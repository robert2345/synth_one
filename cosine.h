#pragma once

#include <math.h>
#include <SDL3/SDL_audio.h>


static float cosine_render_sample(const long long current_frame, const SDL_AudioSpec *spec, float freq)
{
    float time = current_frame * 1.0 / spec->freq;
    return (cos(freq * 2 * M_PI * time));
}
