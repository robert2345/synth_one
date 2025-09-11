#pragma once

#include <SDL3/SDL_audio.h>

struct env_state
{
    long long start_frame;
    long long release_frame;
    float release_level;
};

void envelope_init(struct env_state *state, float A, float D, float S, float R, SDL_AudioSpec *spec);
void envelope_start(struct env_state *state, long long frame);
void envelope_release(struct env_state *state, long long frame);
float envelope_get(struct env_state *state, long long frame);
