#include <math.h>

#include "low_pass_filter.h"

// Source https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
float v2 = 0;
float g = 0;
float v1 = 0;
float ic2eq = 0;
float ic1eq = 0;
float a2 = 0;
float a1 = 0.0;
int cutoff = 4000;


void low_pass_filter_get_configure(float cut_freq, float res, int samplerate)
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
