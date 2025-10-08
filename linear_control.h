#pragma once

#include <stdbool.h>

#define MAX_PARAMS_PER_GROUP (8)

struct linear_control
{
    float *target;
    float max;
    float min;
    bool quantized_to_int;
};

struct ctrl_param
{
    char label[15];
    float value;
    float min;
    float max;
    bool quantized_to_int;
};

struct ctrl_param_group
{
    struct ctrl_param *params[MAX_PARAMS_PER_GROUP];
};
