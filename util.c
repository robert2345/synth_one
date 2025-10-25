#include "util.h"
#include <math.h>

float key_to_freq[NBR_KEYS][100] = {};

void init_key_to_freq()
{
    for (int key = 0; key < NBR_KEYS; key++)
    {
        for (int cent = 0; cent < 100; cent++)
		key_to_freq[key][cent] = 440.0 * pow(2, (((1.0*key+(cent/100.0) - 49)) / 12.0));
    }
}
