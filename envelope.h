#pragma once

#include <SDL3/SDL_audio.h>

void envelope_init(float A, float D, float S, float R, SDL_AudioSpec *spec);
void envelope_start(long long frame);
void envelope_release(long long frame);
float envelope_get(long long frame);
void envelope_start(long long frame);
