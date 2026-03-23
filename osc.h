#pragma once
#include "slide_controller.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>

#define MAX_OSC_COUNT (4) // per voice
#define MAX_GROUPS (3)

enum osc_type
{
    OSC_TYPE_PULSE,
    OSC_TYPE_SAW,
    OSC_TYPE_FM,
    OSC_TYPE_COUNT,
};

struct osc_state
{
    SDL_FRect location;
    enum osc_type type;
    struct slide_controller *slc_arr[MAX_PARAMS_PER_GROUP * MAX_GROUPS];

    struct ctrl_param base_width;
    struct ctrl_param pwm_freq;
    struct ctrl_param pwm_amount;
    struct ctrl_param_group pwm_ctrls;

    struct ctrl_param osc_cnt;
    struct ctrl_param osc_detune_step;
    struct ctrl_param_group detune_ctrls;

    struct ctrl_param_group *param_groups[MAX_GROUPS];
};

float osc_render_sample(long long current_frame, struct osc_state *state, const SDL_AudioSpec *spec, int key,
                        enum osc_type type);

void osc_init(struct osc_state *state, int x_in, int y_in, int width, int height);

void osc_draw(struct osc_state *state, SDL_Renderer *renderer);

void osc_click(struct osc_state *state, int x, int y);
void osc_unclick(struct osc_state *state);
void osc_move(struct osc_state *state, int x, int y);
