#pragma once

#include <SDL3/SDL_audio.h>

struct env_state
{
    long long start_frame;
    long long release_frame;
    float release_level;
    int sample_rate;
};

void envelope_init(struct env_state *state, SDL_AudioSpec *spec);
void envelope_start(struct env_state *state, long long frame);
float envelope_get(struct env_state *state, float A, float D, float S, float R, long long frame);
