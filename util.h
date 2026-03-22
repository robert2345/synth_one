#pragma once

#define NBR_VOICES (8)
#define NBR_KEYS (88)

#define min(x, y) ((x) < (y) ? x : y)
#define max(x, y) ((x) < (y) ? y : x)

struct draw_area
{
    int x;
    int y;
    int width;
    int height;
};

extern float key_to_freq[NBR_KEYS][100];

void init_key_to_freq();
