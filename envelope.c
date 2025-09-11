#include "envelope.h"
#include <stdio.h>
#include <stdlib.h>

#define min(x, y) ((x) < (y) ? x : y)
#define max(x, y) ((x) < (y) ? y : x)

int attack_frames = 10;
int decay_frames = 10;
float sustain_value = 0.7;
int release_frames = 1;

static float ms_to_frames(SDL_AudioSpec *spec, float ms)
{
    return ms * spec->freq / 1000;
}

void envelope_init(struct env_state *state, float A, float D, float S, float R, SDL_AudioSpec *spec)
{
    attack_frames = ms_to_frames(spec, A);
    decay_frames = ms_to_frames(spec, D);
    sustain_value = S;
    release_frames = ms_to_frames(spec, R);

    state->start_frame = -88200;
    state->release_frame = -44200;
    state->release_level = 0.5;
}

void envelope_start(struct env_state *state, long long frame)
{
    state->start_frame = frame;
}

void envelope_release(struct env_state *state, long long frame)
{
    state->release_frame = frame;
}

float envelope_get(struct env_state *state, long long frame)
{
    float ret_level;
    if (frame < state->start_frame)
    {
        printf("Bad startframe %lld or frame %lld\n", state->start_frame, frame);
        exit(-1);
    }

    if (state->start_frame > state->release_frame)
    {
        long long decay_start = state->start_frame + attack_frames;
        long long sustain_start = state->start_frame + attack_frames + decay_frames;
        if (frame > sustain_start)
        {
            ret_level = sustain_value;
        }
        else if (frame > decay_start)
        {
            // decay
            ret_level = (1.0 - ((1.0 - sustain_value) * (frame - decay_start)) / decay_frames);
        }
        else
        {
            // attack
            ret_level = (1.0 * (frame - state->start_frame)) / attack_frames;
        }
    }
    else
    {
        // Release
        float diff = frame - state->release_frame;
        return max(0.0, state->release_level * (1.0 - diff / (release_frames)));
    }

    state->release_level = ret_level;
    return ret_level;
}
