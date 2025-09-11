#include <math.h>

#include "low_pass_filter.h"

// Source https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf

void low_pass_filter_init(struct filter_state *state, float res, float cutoff, int sample_rate)
{
    state->v2 = 0;
    state->g = 0;
    state->v1 = 0;
    state->ic2eq = 0;
    state->ic1eq = 0;
    state->a2 = 0;
    state->a1 = 0.0;
    low_pass_filter_configure(state, cutoff, res, sample_rate);
}

void low_pass_filter_configure(struct filter_state *state, float cut_freq, float res, int samplerate)
{
    state->cutoff = cut_freq;
    state->g = tan(M_PI * state->cutoff / samplerate);
    float k = 2.0 - 2 * res;
    state->a1 = 1.0 / (1.0 + state->g * (state->g + k));
    state->a2 = state->g * state->a1;
}

float low_pass_filter_get_output(struct filter_state *state, float v0)
{

    state->v1 = state->a1 * state->ic1eq + state->a2 * (v0 - state->ic2eq);
    state->v2 = state->ic2eq + state->g * state->v1;
    state->ic1eq = 2 * state->v1 - state->ic1eq;
    state->ic2eq = 2 * state->v2 - state->ic2eq;

    return state->v2;
}
