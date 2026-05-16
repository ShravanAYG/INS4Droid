#include "map.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include "stb_image_write.h"

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static void map_put_pixel(SDL_Surface *s, int x, int y, Uint32 c)
{
	if (x < 0 || x >= s->w || y < 0 || y >= s->h)
		return;
	((Uint32 *)s->pixels)[(y * s->w) + x] = c;
}

static void map_draw_line(SDL_Surface *s, int x0, int y0, int x1, int y1,
			  Uint32 c)
{
	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy, e2;

	while (1) {
		map_put_pixel(s, x0, y0, c);
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

/* Draw a filled circle (Bresenham midpoint) */
static void map_fill_circle(SDL_Surface *s, int cx, int cy, int r, Uint32 c)
{
	for (int dy2 = -r; dy2 <= r; dy2++) {
		for (int dx2 = -r; dx2 <= r; dx2++) {
			if (dx2 * dx2 + dy2 * dy2 <= r * r)
				map_put_pixel(s, cx + dx2, cy + dy2, c);
		}
	}
}

/* Draw a thick line (draw the line multiple times with small offsets) */
static void map_draw_thick_line(SDL_Surface *s, int x0, int y0, int x1,
				int y1, Uint32 c, int thickness)
{
	int half = thickness / 2;
	for (int ox = -half; ox <= half; ox++) {
		for (int oy = -half; oy <= half; oy++) {
			if (ox * ox + oy * oy <= half * half + 1)
				map_draw_line(s, x0 + ox, y0 + oy,
					      x1 + ox, y1 + oy, c);
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Create / Destroy                                                   */
/* ------------------------------------------------------------------ */

INSMap *map_create(double pixelsPerMeter, Uint32 pixfmt)
{
	INSMap *m = calloc(1, sizeof(INSMap));
	if (!m)
		return NULL;

	m->width = MAP_INIT_W;
	m->height = MAP_INIT_H;
	m->pixfmt = pixfmt;
	m->pixelsPerMeter = pixelsPerMeter;

	m->surface =
	    SDL_CreateRGBSurfaceWithFormat(0, m->width, m->height, 32, pixfmt);
	if (!m->surface) {
		free(m);
		return NULL;
	}

	/* Start in the centre */
	m->originX = m->width / 2;
	m->originY = m->height / 2;

	m->posX = m->posY = 0.0;
	m->velX = m->velY = 0.0;

	m->trailCapacity = 4096;
	m->trail = malloc(sizeof(SDL_Point) * m->trailCapacity);
	m->trailCount = 0;

	m->started = false;
	m->lastTick = 0;

	return m;
}

void map_destroy(INSMap *map)
{
	if (!map)
		return;
	if (map->surface)
		SDL_FreeSurface(map->surface);
	free(map->trail);
	free(map);
}

/* ------------------------------------------------------------------ */
/*  Expand canvas                                                      */
/* ------------------------------------------------------------------ */

static void map_expand(INSMap *map, int needX, int needY)
{
	int newW = map->width;
	int newH = map->height;
	int shiftX = 0, shiftY = 0;

	/* Expand right/bottom */
	if (needX >= newW - MAP_EDGE_MARGIN)
		newW = needX + newW / 2;
	if (needY >= newH - MAP_EDGE_MARGIN)
		newH = needY + newH / 2;

	/* Expand left/top (need to shift origin) */
	if (needX < MAP_EDGE_MARGIN) {
		shiftX = newW / 2;
		newW = newW + shiftX;
	}
	if (needY < MAP_EDGE_MARGIN) {
		shiftY = newH / 2;
		newH = newH + shiftY;
	}

	if (newW == map->width && newH == map->height && shiftX == 0
	    && shiftY == 0)
		return;

	SDL_Surface *newSurf =
	    SDL_CreateRGBSurfaceWithFormat(0, newW, newH, 32, map->pixfmt);
	if (!newSurf)
		return;

	/* Clear new surface */
	SDL_FillRect(newSurf, NULL, SDL_MapRGB(newSurf->format, 20, 20, 25));

	/* Blit old surface at the shifted position */
	SDL_Rect dst = { shiftX, shiftY, map->width, map->height };
	SDL_BlitSurface(map->surface, NULL, newSurf, &dst);

	SDL_FreeSurface(map->surface);
	map->surface = newSurf;

	/* Update origin and all trail points */
	map->originX += shiftX;
	map->originY += shiftY;
	for (int i = 0; i < map->trailCount; i++) {
		map->trail[i].x += shiftX;
		map->trail[i].y += shiftY;
	}

	map->width = newW;
	map->height = newH;
}

/* ------------------------------------------------------------------ */
/*  Update (dead reckoning)                                            */
/* ------------------------------------------------------------------ */

void map_update(INSMap *map, double ax, double ay, double heading)
{
	if (!map)
		return;

	uint64_t now = SDL_GetPerformanceCounter();
	if (!map->started) {
		map->lastTick = now;
		map->started = true;
		/* Record starting point */
		if (map->trailCount < map->trailCapacity) {
			map->trail[map->trailCount].x = map->originX;
			map->trail[map->trailCount].y = map->originY;
			map->trailCount++;
		}
		return;
	}

	double freq = (double)SDL_GetPerformanceFrequency();
	double dt = (double)(now - map->lastTick) / freq;
	map->lastTick = now;

	/* Clamp dt to avoid huge jumps */
	if (dt > 0.1)
		dt = 0.1;
	if (dt <= 0.0)
		return;

	/* Rotate acceleration by heading to get global acceleration */
	double rad = heading * M_PI / 180.0;
	double cosR = cos(rad);
	double sinR = sin(rad);
	
	/* Assuming heading is degrees from North, and our map coordinate system is X=East, Y=North */
	/* Standard rotation: x' = x*cos - y*sin, y' = x*sin + y*cos */
	double gax = ax * cosR - ay * sinR;
	double gay = ax * sinR + ay * cosR;

	/* Simple threshold: ignore tiny accelerations (noise) */
	double mag = sqrt(gax * gax + gay * gay);
	if (mag < 0.25) {  /* Increased threshold to reduce idle drift */
		gax = 0;
		gay = 0;
		/* Decay velocity when stationary */
		map->velX *= 0.85;
		map->velY *= 0.85;
	} else {
		/* Integrate acceleration -> velocity */
		map->velX += gax * dt;
		map->velY += gay * dt;
	}

	/* Apply velocity damping to control drift */
	map->velX *= 0.99;
	map->velY *= 0.99;

	if (fabs(map->velX) < 0.005) map->velX = 0;
	if (fabs(map->velY) < 0.005) map->velY = 0;

	/* Integrate velocity -> position */
	map->posX += map->velX * dt;
	map->posY += map->velY * dt;

	/* Convert position (meters) to pixel coordinates */
	int px = map->originX + (int)(map->posX * map->pixelsPerMeter);
	int py = map->originY - (int)(map->posY * map->pixelsPerMeter);	/* Y flipped */

	/* Auto-expand if near the edge */
	if (px < MAP_EDGE_MARGIN || px >= map->width - MAP_EDGE_MARGIN ||
	    py < MAP_EDGE_MARGIN || py >= map->height - MAP_EDGE_MARGIN) {
		map_expand(map, px, py);
		/* Recalculate px/py after expansion (origin may have shifted) */
		px = map->originX + (int)(map->posX * map->pixelsPerMeter);
		py = map->originY - (int)(map->posY * map->pixelsPerMeter);
	}

	/* Add to trail */
	if (map->trailCount >= map->trailCapacity) {
		int newCap = map->trailCapacity * 2;
		if (newCap > MAP_MAX_TRAIL)
			newCap = MAP_MAX_TRAIL;
		if (map->trailCapacity < newCap) {
			SDL_Point *tmp =
			    realloc(map->trail, sizeof(SDL_Point) * newCap);
			if (tmp) {
				map->trail = tmp;
				map->trailCapacity = newCap;
			}
		}
	}

	/* Only add a new point if it moved at least 1 pixel */
	if (map->trailCount > 0) {
		SDL_Point last = map->trail[map->trailCount - 1];
		if (abs(px - last.x) >= 1 || abs(py - last.y) >= 1) {
			if (map->trailCount < map->trailCapacity) {
				map->trail[map->trailCount].x = px;
				map->trail[map->trailCount].y = py;
				map->trailCount++;
			}
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Render                                                             */
/* ------------------------------------------------------------------ */

void map_render(INSMap *map, TTF_Font *font)
{
	if (!map || !map->surface)
		return;

	SDL_Surface *s = map->surface;

	/* Dark background */
	SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 20, 20, 25));

	Uint32 gridColor = SDL_MapRGB(s->format, 40, 40, 50);
	Uint32 trailColor = SDL_MapRGB(s->format, 0, 220, 100);
	Uint32 startColor = SDL_MapRGB(s->format, 80, 80, 255);
	Uint32 curColor = SDL_MapRGB(s->format, 255, 60, 60);
	Uint32 axisColor = SDL_MapRGB(s->format, 60, 60, 80);

	/* Draw grid */
	for (int x = map->originX % MAP_GRID_SPACING; x < s->w;
	     x += MAP_GRID_SPACING) {
		for (int y = 0; y < s->h; y++)
			map_put_pixel(s, x, y, gridColor);
	}
	for (int y = map->originY % MAP_GRID_SPACING; y < s->h;
	     y += MAP_GRID_SPACING) {
		for (int x = 0; x < s->w; x++)
			map_put_pixel(s, x, y, gridColor);
	}

	/* Draw axis lines through origin */
	if (map->originX >= 0 && map->originX < s->w) {
		for (int y = 0; y < s->h; y++)
			map_put_pixel(s, map->originX, y, axisColor);
	}
	if (map->originY >= 0 && map->originY < s->h) {
		for (int x = 0; x < s->w; x++)
			map_put_pixel(s, x, map->originY, axisColor);
	}

	/* Draw trail */
	for (int i = 1; i < map->trailCount; i++) {
		map_draw_thick_line(s, map->trail[i - 1].x,
				    map->trail[i - 1].y, map->trail[i].x,
				    map->trail[i].y, trailColor, 3);
	}

	/* Draw start point (blue circle) */
	map_fill_circle(s, map->originX, map->originY, 6, startColor);

	/* Draw current position (red circle) */
	if (map->trailCount > 0) {
		SDL_Point cur = map->trail[map->trailCount - 1];
		map_fill_circle(s, cur.x, cur.y, 8, curColor);
	}

	/* Scale bar (bottom-left) */
	if (font) {
		int barMeters = 1;
		int barPx = (int)(barMeters * map->pixelsPerMeter);
		int bx = 20, by = s->h - 30;

		Uint32 white = SDL_MapRGB(s->format, 200, 200, 200);
		map_draw_line(s, bx, by, bx + barPx, by, white);
		map_draw_line(s, bx, by - 5, bx, by + 5, white);
		map_draw_line(s, bx + barPx, by - 5, bx + barPx, by + 5,
			      white);

		char label[64];
		snprintf(label, sizeof(label), "%dm", barMeters);
		SDL_Color wc = { 200, 200, 200, 255 };
		SDL_Surface *lblSurf =
		    TTF_RenderUTF8_Blended(font, label, wc);
		if (lblSurf) {
			SDL_Rect dst =
			    { bx + barPx / 2 - lblSurf->w / 2, by - 20,
				lblSurf->w, lblSurf->h
			};
			SDL_BlitSurface(lblSurf, NULL, s, &dst);
			SDL_FreeSurface(lblSurf);
		}

		/* Position info (top-left) */
		snprintf(label, sizeof(label), "X:%.2fm Y:%.2fm  Trail:%d",
			 map->posX, map->posY, map->trailCount);
		lblSurf = TTF_RenderUTF8_Blended(font, label, wc);
		if (lblSurf) {
			SDL_Rect dst =
			    { 10, 10, lblSurf->w, lblSurf->h };
			SDL_BlitSurface(lblSurf, NULL, s, &dst);
			SDL_FreeSurface(lblSurf);
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Save to PNG                                                        */
/* ------------------------------------------------------------------ */

int map_save_png(INSMap *map, const char *filepath)
{
	if (!map || !map->surface)
		return -1;

	/* Re-render before saving */
	map_render(map, NULL);

	SDL_Surface *s = map->surface;

	/* Convert to RGBA for stb_image_write */
	SDL_Surface *rgba =
	    SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
	if (!rgba)
		return -1;

	SDL_UnlockSurface(rgba);
	SDL_FreeSurface(rgba);

	return -1;
}

/* ------------------------------------------------------------------ */
/*  Reset                                                              */
/* ------------------------------------------------------------------ */

void map_reset(INSMap *map)
{
	if (!map)
		return;

	/* Free old surface and create fresh */
	if (map->surface)
		SDL_FreeSurface(map->surface);

	map->width = MAP_INIT_W;
	map->height = MAP_INIT_H;
	map->surface =
	    SDL_CreateRGBSurfaceWithFormat(0, map->width, map->height, 32,
					  map->pixfmt);

	map->originX = map->width / 2;
	map->originY = map->height / 2;
	map->posX = map->posY = 0.0;
	map->velX = map->velY = 0.0;
	map->trailCount = 0;
	map->started = false;
	map->lastTick = 0;
}
