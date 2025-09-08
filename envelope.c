#include "envelope.h"
#include <stdio.h>

#define min(x, y) ((x) < (y) ? x : y)
#define max(x, y) ((x) < (y) ? y : x)

long long start_frame = -88200;
long long release_frame = -44200;

int attack_frames = 10;
int decay_frames = 10;
float sustain_value = 0.7;
float release_level = 0.5;
int release_frames = 1;

static float ms_to_frames(SDL_AudioSpec *spec, float ms)
{
    return ms * spec->freq / 1000;
}

void envelope_init(float A, float D, float S, float R, SDL_AudioSpec *spec)
{
    attack_frames = ms_to_frames(spec, A);
    decay_frames = ms_to_frames(spec, D);
    sustain_value = S;
    release_frames = ms_to_frames(spec, R);
}

void envelope_start(long long frame)
{
    start_frame = frame;
}

void envelope_release(long long frame)
{
    release_frame = frame;
}

float envelope_get(long long frame)
{
    float ret_level;
    if (frame < start_frame)
    {
        printf("Bad startframe %lld or frame %lld\n", start_frame, frame);
        return 0.1;
    }

    if (start_frame > release_frame)
    {
        long long decay_start = start_frame + attack_frames;
        long long sustain_start = start_frame + attack_frames + decay_frames;
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
            ret_level = (1.0 * (frame - start_frame)) / attack_frames;
        }
    }
    else
    {
        // Release
        int diff = frame - release_frame;
        return max(0.0, release_level * (1.0 - 1.0 * diff / (release_frames)));
    }

    release_level = ret_level;
    return ret_level;
}
