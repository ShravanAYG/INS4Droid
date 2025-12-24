cmake_minimum_required(VERSION 3.6)

include_directories(
        ${CMAKE_SOURCE_DIR}/SDL2/include
        ${CMAKE_SOURCE_DIR}/SDL2_ttf
)


project(MY_APP)

find_library(SDL2 SDL2)

add_library(main SHARED)

target_sources(main PRIVATE main.c)

target_link_libraries(main
        SDL2
        SDL2_ttf
        log)


