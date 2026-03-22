#pragma once
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_render.h>
#include <stdbool.h>

struct fm_operator
{
    float *amp;
    float *freq;
};

void fm_draw(SDL_Renderer *renderer);
void fm_click(int x, int y);
void fm_unclick();
void fm_move(int x, int y);
void fm_relocate(int x, int y, int width, int height);
void fm_init(int x, int y, int width, int height);
float fm_render_sample(int frame_since_on, int frame_since_release, const SDL_AudioSpec *spec, float freq);
bool fm_read_setting(char *line);
void fm_save_settings(FILE *f);
