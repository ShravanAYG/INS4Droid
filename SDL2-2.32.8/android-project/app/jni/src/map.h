#ifndef MAP_H
#define MAP_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>

/* Maximum trail points before we start dropping old ones */
#define MAP_MAX_TRAIL 100000

/* Initial map canvas dimensions */
#define MAP_INIT_W 1024
#define MAP_INIT_H 1024

/* How close to the edge (px) before we expand */
#define MAP_EDGE_MARGIN 80

/* Grid spacing in pixels */
#define MAP_GRID_SPACING 50

typedef struct {
	SDL_Surface *surface;	/* The drawable map canvas */
	int width, height;	/* Current canvas dimensions */
	int originX, originY;	/* Pixel coords of the starting point (0,0) */

	/* Dead-reckoning state */
	double posX, posY;	/* Position in meters (2D horizontal plane) */
	double velX, velY;	/* Velocity estimate in m/s */

	/* Trail storage */
	SDL_Point *trail;	/* Array of pixel positions */
	int trailCount;
	int trailCapacity;

	/* Timing */
	uint64_t lastTick;	/* SDL_GetPerformanceCounter value */
	bool started;		/* Has first update happened? */

	/* Scale */
	double pixelsPerMeter;

	/* Pixel format (matches window) */
	Uint32 pixfmt;
} INSMap;

/* Create a new map. pixelsPerMeter controls zoom level. */
INSMap *map_create(double pixelsPerMeter, Uint32 pixfmt);

/* Destroy and free all resources. */
void map_destroy(INSMap *map);

/* Update dead-reckoning with new sensor data.
 * ax, ay: zeroed horizontal acceleration (m/s^2)
 * heading: compass heading in degrees
 * This also auto-expands the canvas if needed. */
void map_update(INSMap *map, double ax, double ay, double heading);

/* Re-render the full map surface (grid + trail + current pos). */
void map_render(INSMap *map, TTF_Font *font);

/* Save the map to a PNG file. Returns 0 on success. */
int map_save_png(INSMap *map, const char *filepath);

/* Reset the map (clear trail, re-center). */
void map_reset(INSMap *map);

#endif /* MAP_H */
