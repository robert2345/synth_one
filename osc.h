#pragma once
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_render.h>

#define MAX_OSC_COUNT (4) // per voice

enum osc_type
{
    OSC_TYPE_PULSE,
    OSC_TYPE_SAW,
    OSC_TYPE_FM,
    OSC_TYPE_COUNT,
};

struct osc_state
{
    float period_position[MAX_OSC_COUNT];
};

float osc_render_sample(long long current_frame, struct osc_state *state, const SDL_AudioSpec *spec, int key,
                        enum osc_type type);

void osc_init(struct osc_state *state, int x_in, int y_in);

void osc_draw(SDL_Renderer *renderer);

void osc_click(int x, int y);
void osc_unclick();
void osc_move(int x, int y);
