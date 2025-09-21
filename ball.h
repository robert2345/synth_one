#pragma once
#include <SDL3/SDL_render.h>

struct ball_state;

void ball_init(int width, int height, int stepsize);

struct ball_state *ball_create(float x, float y, float x_speed, float y_speed, long long current_frame);

void ball_destroy(struct ball_state **bs);

void ball_draw(SDL_Renderer *renderer, struct ball_state *bs, SDL_FPoint *waveform, long long current_frame);
