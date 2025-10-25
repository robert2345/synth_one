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
void fm_init(int x, int y);
float fm_render_sample(long long current_frame, const SDL_AudioSpec *spec, float freq);
bool fm_read_setting(char *line);
void fm_save_settings(FILE *f);
