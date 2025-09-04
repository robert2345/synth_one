#pragma once

#include <SDL3/SDL_audio.h>

int delay_init(const SDL_AudioSpec *spec, unsigned max_len_ms);
float delay_get_sample(float delay_ms, const SDL_AudioSpec *spec);
void delay_put_sample(float sample);
void delay_shutdown();
