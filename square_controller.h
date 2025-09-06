#pragma once
#include "linear_control.h"
#include <SDL3/SDL_render.h>

struct square_controller
{
    bool clicked;
    int x;
    int y;
    int width;
    int height;
    const char *x_label;
    const char *y_label;
    struct linear_control x_control;
    struct linear_control y_control;
    SDL_FPoint marker_points[5];
    SDL_FPoint border_points[5];
};

void square_controller_click(struct square_controller *sc, int x, int y);
void square_controller_unclick(struct square_controller *sc);
void square_controller_move(struct square_controller *sc, int x, int y);

void square_controller_draw(SDL_Renderer *renderer, struct square_controller *sc);

struct square_controller *square_controller_create(int x, int y, int width, int height, struct linear_control x_control,
                                                   struct linear_control y_control, const char *x_label,
                                                   const char *y_label);

void square_controller_destroy(struct square_controller *sc);
