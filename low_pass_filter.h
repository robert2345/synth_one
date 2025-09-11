#pragma once

struct filter_state {
float v2;
float g;
float v1;
float ic2eq;
float ic1eq;
float a2;
float a1;
float cutoff;
};

void low_pass_filter_init(struct filter_state *state, float res, float cutoff, int sample_rate);
void low_pass_filter_configure(struct filter_state *state, float cut_freq, float res, int samplerate);

float low_pass_filter_get_output(struct filter_state *state, float v0);
