#pragma once
#include <SDL3/SDL_audio.h>

struct fm_operator
{
    float *amp;
    float *freq;
};

#define NBR_FM_OPS (1)
float render_fm(long long current_frame, float *period_position, const SDL_AudioSpec *spec, float freq,
                struct fm_operator *ops);
