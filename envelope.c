#include "envelope.h"
#include <stdio.h>
#include <stdlib.h>

#define min(x, y) ((x) < (y) ? x : y)
#define max(x, y) ((x) < (y) ? y : x)

static float ms_to_frames(int sample_rate, float ms)
{
    return ms * sample_rate / 1000;
}

void envelope_init(struct env_state *state, SDL_AudioSpec *spec)
{

    state->sample_rate = spec->freq;
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

float envelope_get(struct env_state *state, float A, float D, float S, float R, long long frame)
{

    int decay_frames = ms_to_frames(state->sample_rate, D);
    float sustain_value = S;

    float ret_level;
    if (frame < state->start_frame)
    {
        printf("Bad startframe %lld or frame %lld\n", state->start_frame, frame);
        exit(-1);
    }

    if (state->start_frame >= state->release_frame)
    {
        int attack_frames = ms_to_frames(state->sample_rate, A);
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
            ret_level = 0.001 + (0.999 * (frame - state->start_frame)) / attack_frames; // Avoiding 0, because I have hacked 0 envelope to turn off the note.
        }
    }
    else
    {
        // Release
        int release_frames = ms_to_frames(state->sample_rate, R);
        float diff = frame - state->release_frame;
        return max(0.0, state->release_level * (1.0 - diff / (release_frames)));
    }

    state->release_level = ret_level;
    return ret_level;
}
