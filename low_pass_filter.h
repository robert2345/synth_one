#pragma once

void low_pass_filter_get_configure(float cut_freq, float res, int samplerate);

float low_pass_filter_get_output(float v0);
