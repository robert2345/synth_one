
#pragma once
#include <SDL3/SDL_render.h>

#include "linear_control.h"

struct slide_controller
{
    bool clicked;
    int x;
    int y;
    int width;
    int height;
    const char *label;
    struct linear_control control;
    SDL_FPoint marker_points[5];
    SDL_FPoint border_points[5];
};

void slide_controller_click(struct slide_controller *sc, int x, int y);
void slide_controller_unclick(struct slide_controller *sc);
void slide_controller_move(struct slide_controller *sc, int x, int y);

void slide_controller_draw(SDL_Renderer *renderer, struct slide_controller *sc);

struct slide_controller *slide_controller_create(int x, int y, int width, int height, struct linear_control control,
                                                 const char *label);

void slide_controller_destroy(struct slide_controller *sc);
void slide_controller_set_pos_from_value(struct slide_controller *sc);
