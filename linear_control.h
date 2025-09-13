#pragma once

#include <stdbool.h>

struct linear_control
{
    float *target;
    float max;
    float min;
    bool quantized_to_int;
};
