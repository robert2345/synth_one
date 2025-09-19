#pragma once

#include <SDL3/SDL_render.h>

void text_init(SDL_Renderer *renderer);
void text_exit();
void text_draw(SDL_Renderer *renderer, const char *text, int x, int y, bool vertical);
int text_get_height();
int text_get_width();
