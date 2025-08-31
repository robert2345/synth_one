#include <math.h>

#include "low_pass_filter.h"

// Source https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
static float v2 = 0;
static float g = 0;
static float v1 = 0;
static float ic2eq = 0;
static float ic1eq = 0;
static float a2 = 0;
static float a1 = 0.0;
static int cutoff = 4000;


void low_pass_filter_configure(float cut_freq, float res, int samplerate)
{
	cutoff = cut_freq;
	g = tan(M_PI * cutoff / samplerate);
	float k = 2.0-2*res;
	a1 = 1.0/(1.0+g*(g+k));
	a2 = g * a1;


}

float low_pass_filter_get_output(float v0)
{

	v1 = a1*ic1eq + a2*(v0 - ic2eq);
	v2 = ic2eq + g*v1;
	ic1eq = 2*v1 - ic1eq;
	ic2eq = 2*v2 - ic2eq;

	return v2;
}
