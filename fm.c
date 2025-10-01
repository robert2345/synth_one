#include "fm.h"
#include "slide_controller.h"
#include "text.h"
#include <math.h>
#include <stdio.h>

enum op_param
{
    OP_PARAM_AMP = 0,
    OP_PARAM_FREQ,
    OP_PARAM_A,
    OP_PARAM_D,
    OP_PARAM_S,
    OP_PARAM_R,
    OP_PARAM_NBR_OF,
};

#define NBR_OPS (8)
#define MAX_PARAMS_PER_GROUP (OP_PARAM_NBR_OF + 1)
#define MAX_GROUPS (NBR_OPS + 2) // choose algo and # operators.

static struct slide_controller *slc_arr[MAX_PARAMS_PER_GROUP * MAX_GROUPS] = {};

struct ctrl_param
{
    char label[25];
    float value;
    float min;
    float max;
    bool quantized_to_int;
};

struct ctrl_param_group
{
    struct ctrl_param *params[MAX_PARAMS_PER_GROUP];
};

static struct ctrl_param_group *param_groups[MAX_GROUPS];

static struct ctrl_param algorithm = {
    .label = "ALGORITHM",
    .value = 0,
    .min = 0,
    .max = 5,
    .quantized_to_int = true,
};

static const struct ctrl_param op_amp = {
    .label = "OPX AMP",
    .value = 0.01,
    .min = 0.001,
    .max = 0.1,
};

static const struct ctrl_param op_freq = {
    .label = "OPX FREQ",
    .value = 0.01,
    .min = 0.001,
    .max = 2.0,
};

static const struct ctrl_param op_A = {
    .label = "OPX A",
    .value = 0.01,
    .min = 0.001,
    .max = 2.0,
};
static const struct ctrl_param op_D = {
    .label = "OPX D",
    .value = 0.01,
    .min = 0.001,
    .max = 2.0,
};
static const struct ctrl_param op_S = {
    .label = "OPX S",
    .value = 0.01,
    .min = 0.001,
    .max = 2.0,
};
static const struct ctrl_param op_R = {
    .label = "OPX R",
    .value = 0.01,
    .min = 0.001,
    .max = 2.0,
};

static struct ctrl_param ops[2 * OP_PARAM_NBR_OF * NBR_OPS + 1] = {
    [OP_PARAM_AMP] = op_amp, [OP_PARAM_FREQ] = op_freq, [OP_PARAM_A] = op_A,
    [OP_PARAM_D] = op_D,     [OP_PARAM_S] = op_S,       [OP_PARAM_R] = op_R,
};

static struct ctrl_param_group algorithm_group = {
    .params = {&algorithm},
};

static struct ctrl_param_group ops_param_groups[MAX_GROUPS];

static float get_op(int op, enum op_param par)
{
    return ops[par + op * OP_PARAM_NBR_OF].value;
}

float fm_render_sample(long long current_frame, float *period_position, const SDL_AudioSpec *spec, float freq)
{
    *period_position += freq / spec->freq;
    if (*period_position >= 1.0)
    {
        *period_position = 0.0;
    }
    float time = current_frame * 1.0 / spec->freq;
    float mod_one;

    int algo = algorithm.value;
    switch (algo)
    {
    case 0:
        mod_one = get_op(1, OP_PARAM_AMP) * cos(freq * get_op(1, OP_PARAM_FREQ) * 2 * M_PI * time);
        break;
    case 1:
        mod_one = 0.0;
        break;
    default:
        printf("Algo %d not implemented!\n", algo);
    }

    return (get_op(0, OP_PARAM_AMP) * cos((freq + get_op(0, OP_PARAM_FREQ) + mod_one) * 2 * M_PI * time));
}

void fm_init(int x_in, int y_in)
{
    int i, j = 0;

    printf("init start");
    // Initialize all the parameters for the operators so they can be drawn and tweaked.
    for (i = 0; i < NBR_OPS; i++)
    {
        for (j = 0; j < OP_PARAM_NBR_OF; j++)
        {
            if (i != 0)
            {
                ops[j + OP_PARAM_NBR_OF * i] = ops[j];
            }
            ops[j + OP_PARAM_NBR_OF * i].label[2] = '0' + i;
        }

        ops_param_groups[i] = (struct ctrl_param_group){
            .params = {&ops[OP_PARAM_AMP + i * OP_PARAM_NBR_OF], &ops[OP_PARAM_FREQ + i * OP_PARAM_NBR_OF],
                       &ops[OP_PARAM_A + i * OP_PARAM_NBR_OF], &ops[OP_PARAM_D + i * OP_PARAM_NBR_OF],
                       &ops[OP_PARAM_S + i * OP_PARAM_NBR_OF], &ops[OP_PARAM_R + i * OP_PARAM_NBR_OF]},
        };
        param_groups[i] = &ops_param_groups[i];
    };
    param_groups[i] = &algorithm_group;

    // Initialize all the actual controllers
    {
#define WIDTH (1024)
#define HEIGHT (768)
        int i = 0;
        int j = 0;
        int k = 0;
        struct ctrl_param_group *pg;
        struct ctrl_param *p;
        const int margin = 10;
        const int width = 100;
        const int height = 10;
        int label_height = text_get_height();
        int tot_height = height + label_height;
        int x = margin;
        int y = margin;
        while ((pg = param_groups[i++]))
        {
            j = 0;
            while ((p = pg->params[j++]))
            {
                x = x_in + margin + (y + y_in) / (HEIGHT - tot_height) * (width + margin);
                slc_arr[k++] = slide_controller_create(
                    x, (y + y_in) % (HEIGHT - tot_height), width, height,
                    (struct linear_control){&p->value, p->min, p->max, p->quantized_to_int}, p->label);
                y += (margin + height + label_height);
            }
            y += 3 * margin;
        }
    }
    printf("init done");
}

void fm_draw(SDL_Renderer *renderer)
{
    int i;
    struct slide_controller *slc;
    for (i = 0; (slc = slc_arr[i]); i++)
    {
        slide_controller_draw(renderer, slc);
    }
}

void fm_click(int x, int y)
{
    int i;
    struct slide_controller *slc;
    for (i = 0; (slc = slc_arr[i]); i++)
    {
        slide_controller_click(slc, x, y);
    }
}

void fm_unclick()
{
    int i;
    struct slide_controller *slc;
    for (i = 0; (slc = slc_arr[i]); i++)
    {
        slide_controller_unclick(slc);
    }
}

void fm_move(int x, int y)
{
    int i;
    struct slide_controller *slc;
    for (i = 0; (slc = slc_arr[i]); i++)
    {
        slide_controller_move(slc, x, y);
    }
}
