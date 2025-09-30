#include "fm.h"
#include "cosine.h"

float render_fm(long long current_frame, float *period_position, const SDL_AudioSpec *spec, float freq,
                struct fm_operator *ops)
{
    *period_position += freq / spec->freq;
    if (*period_position >= 1.0)
    {
        *period_position = 0.0;
    }

    float time = current_frame * 1.0 / spec->freq;
    float mod_one = *ops[0].amp * cos(freq * *ops[0].freq * 2 * M_PI * time);

    return (cos((freq + 0.1 * mod_one) * 2 * M_PI * time));
}
