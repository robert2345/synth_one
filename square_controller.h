#pragma once
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_render.h>

struct linear_controller {
	float *target;
	float max;
	float min;
};

struct square_controller {
	int x;
	int y;
	int width;
	int height;
	struct linear_controller x_control;
	struct linear_controller y_control;
	SDL_FPoint marker_points[5];
	SDL_FPoint border_points[5];
};


void square_controller_click(struct square_controller *sc, int x, int y);

void square_controller_draw(SDL_Renderer *renderer, struct square_controller *sc);

struct square_controller *square_controller_create(int x, int y, int width, int height, struct linear_controller x_control, struct linear_controller y_control);


void square_controller_destroy(struct square_controller *sc);
