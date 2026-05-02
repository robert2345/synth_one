#include "fm.h"
#include "envelope.h"
#include "linear_control.h"
#include "slide_controller.h"
#include "text.h"
#include "util.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

enum op_param
{
    OP_PARAM_AMP = 0,
    OP_PARAM_FREQ,
    OP_PARAM_DETUNE,
    OP_PARAM_A,
    OP_PARAM_D,
    OP_PARAM_S,
    OP_PARAM_R,
    OP_PARAM_NBR_OF,
};

#define NBR_OPS (8)
#define MAX_GROUPS (NBR_OPS + 4) // choose algo and # operators.

static SDL_FRect fm_location;

static struct slide_controller *slc_arr[MAX_GROUPS][MAX_PARAMS_PER_GROUP] = {};

static struct ctrl_param_group *param_groups[MAX_GROUPS];

static struct ctrl_param algorithm = {
    .label = "ALGORITHM",
    .value = 0,
    .min = 0,
    .max = 5,
    .quantized_to_int = true,
    .show_num = true,
};

static struct ctrl_param mod_param = {
    .label = "MODULATION",
    .value = 1.0,
    .min = 0.0,
    .max = 10.0,
};

static struct ctrl_param fb_param = {
    .label = "FEEDBACK",
    .value = 0.99,
    .min = 0.0,
    .max = 10.0,
};

static const struct ctrl_param op_amp = {
    .label = "OPX AMP",
    .value = 0.001,
    .min = 0.000,
    .max = 0.1,
};

static const struct ctrl_param op_freq = {
    .label = "OPX FREQ",
    .value = 1,
    .min = 0.5,
    .max = 2.0,
    .show_num = true,
};

static const struct ctrl_param op_detune = {
    .label = "OPX DETUNE",
    .value = 0.01,
    .min = 0.001,
    .max = 2.0,
};

static const struct ctrl_param op_A = {
    .label = "OPX A",
    .value = 0.001,
    .min = 0.001,
    .max = 2.0,
};
static const struct ctrl_param op_D = {
    .label = "OPX D",
    .value = 0.001,
    .min = 0.001,
    .max = 2.0,
};
static const struct ctrl_param op_S = {
    .label = "OPX S",
    .value = 0.0,
    .min = 0.000,
    .max = 2.0,
};
static const struct ctrl_param op_R = {
    .label = "OPX R",
    .value = 0.01,
    .min = 0.001,
    .max = 2.0,
};

static struct ctrl_param ops[2 * OP_PARAM_NBR_OF * NBR_OPS + 1] = {
    [OP_PARAM_AMP] = op_amp, [OP_PARAM_FREQ] = op_freq, [OP_PARAM_DETUNE] = op_detune, [OP_PARAM_A] = op_A,
    [OP_PARAM_D] = op_D,     [OP_PARAM_S] = op_S,       [OP_PARAM_R] = op_R,
};

static struct ctrl_param_group algorithm_group = {
    .params = {&algorithm, &mod_param, &fb_param},
};

static struct ctrl_param_group ops_param_groups[MAX_GROUPS];

static float get_op(int op, enum op_param par)
{
    return ops[par + (op - 1) * OP_PARAM_NBR_OF].value;
}

struct operator
{
    int input_ops[NBR_OPS];
    int feedback_op;
    float last_value;
};

struct algorithm
{
    int nbr_carriers;
    int carriers[NBR_OPS];
    struct operator ops[NBR_OPS]; // this will host all the operators regardless of how they are
                                  // connected. Index +1 will be op number
};

struct algorithm algos[32] = {
    {.nbr_carriers = 2,
     .carriers = {1, 3},
     .ops =
         {
             {.input_ops = {2}}, // carrier
             {.input_ops = {0}},
             {.input_ops = {4}}, // carrier
             {.input_ops = {5}},
             {.input_ops = {6}},
             {.feedback_op = 6},
         }},
    {.nbr_carriers = 2,
     .carriers = {1, 3},
     .ops =
         {
             {.input_ops = {2}}, // carrier
             {.input_ops = {0}, .feedback_op = 2},
             {.input_ops = {4}}, // carrier
             {.input_ops = {5}},
             {.input_ops = {6}},
         }},
    {.nbr_carriers = 2,
     .carriers = {1, 4},
     .ops =
         {
             {.input_ops = {2}}, // carrier
             {.input_ops = {3}},
             {.input_ops = {0}},
             {.input_ops = {5}}, // carrier
             {.input_ops = {6}},
             {.feedback_op = 6},
         }},
    {.nbr_carriers = 1,
     .carriers = {1},
     .ops =
         {
             {.input_ops = {0}, .feedback_op = 1}, // carrier
         }},
};

float evaluate_operator(struct algorithm *algo, int op, float freq, double on_time, double release_time)
{
    float modulation = 0;
    struct operator* op_p = & algo->ops[op - 1];

    for (int i = 0; 0 != op_p->input_ops[i]; i++)
    {
        modulation += mod_param.value * evaluate_operator(algo, op_p->input_ops[i], freq, on_time, release_time);
    }
    if (op_p->feedback_op)
    {
        struct operator* feedback_op_p = & algo->ops[op_p->feedback_op - 1];
        modulation += mod_param.value * feedback_op_p->last_value;
    }

    float env = envelope_get_stateless(get_op(op, OP_PARAM_A), get_op(op, OP_PARAM_D), get_op(op, OP_PARAM_S),
                                       get_op(op, OP_PARAM_R), on_time, release_time);

    op_p->last_value =
        env * (get_op(op, OP_PARAM_AMP) * cos(modulation + (freq * get_op(op, OP_PARAM_FREQ)) * 2 * M_PI * on_time));
    return op_p->last_value;
}

float fm_render_sample(int frame_since_on, int frame_since_release, const SDL_AudioSpec *spec, float freq)
{
    float data = 0;
    double on_time = frame_since_on * 1.0 / spec->freq;
    double release_time = frame_since_release * 1.0 / spec->freq;

    struct algorithm *algo = &algos[(int)algorithm.value];
    for (int i = 0; i < algo->nbr_carriers; i++)
    {
        data += evaluate_operator(algo, algo->carriers[i], freq, on_time, release_time) / algo->nbr_carriers;
    }

    return data;
}

void fm_relocate(int x_in, int y_in, int width, int height)
{
    fm_location.x = x_in;
    fm_location.y = y_in;
    fm_location.w = width;
    fm_location.h = height;
    int i = 0;
    int j = 0;
    struct ctrl_param_group *pg;
    struct ctrl_param *p;
    const int margin = 10;
    const int ctrl_width = 100;
    const int ctrl_height = 10;
    int label_height = text_get_height();
    int tot_height = ctrl_height + label_height;
    int x = margin;
    int y = margin;
    for (i = 0; (pg = param_groups[i]); i++)
    {
        for (j = 0; (p = pg->params[j]); j++)
        {
            struct slide_controller *slc = slc_arr[i][j];
            x = x_in + margin + (y / (height - tot_height)) * (ctrl_width + margin);
            int y_to_set = y_in + y % (height - tot_height);
            slide_controller_relocate(slc, x, y_to_set, ctrl_width, ctrl_height);
            y += (margin + ctrl_height + label_height);
        }
        y += 3 * margin;
    }
}

void fm_init(int x_in, int y_in, int width, int height)
{
    int i, j = 0;

    // Initialize all the parameters for the operators so they can be drawn and tweaked.
    param_groups[0] = &algorithm_group;
    for (i = 0; i < NBR_OPS; i++)
    {
        for (j = 0; j < OP_PARAM_NBR_OF; j++)
        {
            if (i != 0)
            {
                ops[j + OP_PARAM_NBR_OF * i] = ops[j];
            }
            ops[j + OP_PARAM_NBR_OF * i].label[2] = '1' + i;
        }

        ops_param_groups[i] = (struct ctrl_param_group){
            .params = {&ops[OP_PARAM_AMP + i * OP_PARAM_NBR_OF], &ops[OP_PARAM_FREQ + i * OP_PARAM_NBR_OF],
                       &ops[OP_PARAM_A + i * OP_PARAM_NBR_OF], &ops[OP_PARAM_D + i * OP_PARAM_NBR_OF],
                       &ops[OP_PARAM_S + i * OP_PARAM_NBR_OF], &ops[OP_PARAM_R + i * OP_PARAM_NBR_OF]},
        };
        param_groups[i + 1] = &ops_param_groups[i];
    };

    // Initialize all the actual controllers
    {
        int i = 0;
        int j = 0;
        int k = 0;
        const int ctrl_width = 100;
        const int ctrl_height = 10;
        struct ctrl_param_group *pg;
        struct ctrl_param *p;
        for (; (pg = param_groups[i]); i++)
        {
            for (j = 0; (p = pg->params[j]); j++)
            {
                slc_arr[i][j] = slide_controller_create(
                    0, 0, 10, 10, (struct linear_control){&p->value, p->min, p->max, p->quantized_to_int, p->show_num},
                    p->label);
            }
        }
    }
    fm_relocate(x_in, y_in, width, height);
}

#define OP_START_X 600
#define OP_START_Y 250
#define OP_WIDTH 30
float draw_operator(SDL_Renderer *renderer, struct algorithm *algo, int op, SDL_FPoint *op_positions, float left_most,
                    float *right_most, float y)
{
    static SDL_FRect rect = {.w = OP_WIDTH, .h = OP_WIDTH};
    float modulation = 0;
    struct operator* op_p = & algo->ops[op - 1];

    for (int i = 0; 0 != op_p->input_ops[i]; i++)
    {
        if (i > 0)
            *right_most += OP_WIDTH * 2;
        draw_operator(renderer, algo, op_p->input_ops[i], op_positions, *right_most, right_most, y - OP_WIDTH * 2);
    }

    float x = (left_most + *right_most) / 2;
    op_positions[op - 1].x = x + OP_WIDTH / 2;
    op_positions[op - 1].y = y;
    rect.x = x;
    rect.y = y;

    char label[2] = {'0' + op, '\0'};
    SDL_SetRenderDrawColor(renderer, 200, 0, 55, 255);
    text_draw(renderer, label, x + OP_WIDTH / 2 - text_get_width() / 2, y + OP_WIDTH / 2 - text_get_height() / 2,
              false);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &rect);

    return op_p->last_value;
}

#define LINE_DISTANCE (5)
void draw_operator_connections(SDL_Renderer *renderer, struct algorithm *algo, int op, SDL_FPoint *op_positions)
{
    float modulation = 0;
    struct operator* op_p = & algo->ops[op - 1];

    for (int i = 0; 0 != op_p->input_ops[i]; i++)
    {
        draw_operator_connections(renderer, algo, op_p->input_ops[i], op_positions);
        SDL_FPoint tmp_points[2] = {op_positions[op - 1], op_positions[op_p->input_ops[i] - 1]};
        tmp_points[1].y += OP_WIDTH;
        SDL_RenderLines(renderer, tmp_points, 2);
    }
    if (op_p->feedback_op)
    {
        SDL_FPoint tmp_points[6] = {op_positions[op - 1],
                                    op_positions[op - 1],
                                    op_positions[op - 1],
                                    op_positions[op_p->feedback_op - 1],
                                    op_positions[op_p->feedback_op - 1],
                                    op_positions[op_p->feedback_op - 1]};

        tmp_points[1].y -= LINE_DISTANCE;

        tmp_points[2].y -= LINE_DISTANCE;
        tmp_points[2].x += OP_WIDTH / 2 + LINE_DISTANCE;

        // this guy is special, since he should get x coord from one "port" and y coord from another
        tmp_points[3].y += OP_WIDTH + LINE_DISTANCE;
        tmp_points[3].x = tmp_points[2].x;
        ;

        tmp_points[4].y += OP_WIDTH + LINE_DISTANCE;
        tmp_points[5].y += OP_WIDTH;

        SDL_RenderLines(renderer, tmp_points, 6);
    }
}

static int fm_get_nbr_active_operators()
{
    int i = 0;
    struct algorithm *algo = &algos[(int)algorithm.value];
    int max_op = algo->nbr_carriers;

    for (int i = 0; i < NBR_OPS; i++)
    {
        for (int j = 0; j < NBR_OPS; j++)
            max_op = max(max_op, algo->ops[i].input_ops[j]);
    }

    return max_op;
}

void fm_draw(SDL_Renderer *renderer)
{
    int i = 0;
    int j = 0;
    struct slide_controller *slc;

    SDL_SetRenderDrawColor(renderer, 123, 234, 012, 123);
    SDL_RenderRect(renderer, &fm_location);

    i = 0;
    for (j = 0; (slc = slc_arr[i][j]); j++)
    {
        slide_controller_draw(renderer, slc);
    }

    int ops = fm_get_nbr_active_operators();

    for (i = 1; i <= ops; i++)
    {
        for (j = 0; (slc = slc_arr[i][j]); j++)
        {
            slide_controller_draw(renderer, slc);
        }
    }
    slc = slc_arr[i - 1][j - 1];
    float op_start_y = fm_location.y + fm_location.h - OP_WIDTH * 2;
    float op_start_x = slc->x + slc->width + 10;

    SDL_FPoint op_positions[NBR_OPS];

    struct algorithm *algo = &algos[(int)algorithm.value];
    float right_most = op_start_x;
    for (i = 0; i < algo->nbr_carriers; i++)
    {
        draw_operator(renderer, algo, algo->carriers[i], op_positions, right_most, &right_most, op_start_y);
        right_most += OP_WIDTH * 2;

        SDL_FPoint *pos = &op_positions[algo->carriers[i] - 1];
        SDL_RenderLine(renderer, pos->x, pos->y + OP_WIDTH, pos->x, pos->y + OP_WIDTH + LINE_DISTANCE);
        if (i == algo->nbr_carriers - 1)
        {
            SDL_FPoint *first_pos = &op_positions[algo->carriers[0] - 1];
            SDL_FPoint *second_pos = &op_positions[algo->carriers[i] - 1];
            SDL_RenderLine(renderer, first_pos->x, first_pos->y + OP_WIDTH + LINE_DISTANCE, second_pos->x,
                           second_pos->y + OP_WIDTH + LINE_DISTANCE);
            SDL_RenderLine(renderer, (first_pos->x + second_pos->x) / 2, first_pos->y + OP_WIDTH + LINE_DISTANCE,
                           (first_pos->x + second_pos->x) / 2, first_pos->y + OP_WIDTH + 2 * LINE_DISTANCE);
        }
    }

    for (int i = 0; i < algo->nbr_carriers; i++)
    {
        draw_operator_connections(renderer, algo, algo->carriers[i], op_positions);
    }
}

void fm_click(int x, int y)
{
    int i;
    int j;
    struct slide_controller *slc;
    for (i = 0, j = 0; (slc = slc_arr[i][j]); i++)
    {
        for (; (slc = slc_arr[i][j]); j++)
        {
            slide_controller_click(slc, x, y);
        }
        j = 0;
    }
}

void fm_unclick()
{
    int i;
    int j;
    struct slide_controller *slc;
    for (i = 0, j = 0; (slc = slc_arr[i][j]); i++)
    {
        for (; (slc = slc_arr[i][j]); j++)
        {
            slide_controller_unclick(slc);
        }
        j = 0;
    }
}

void fm_move(int x, int y)
{
    int i;
    int j;
    struct slide_controller *slc;
    for (i = 0, j = 0; (slc = slc_arr[i][j]); i++)
    {
        for (; (slc = slc_arr[i][j]); j++)
        {
            slide_controller_move(slc, x, y);
        }
        j = 0;
    }
}

bool fm_read_setting(char *line)
{
    int i = 0;
    int j = 0;
    int k = 0;
    int l = 0;
    struct ctrl_param_group *pg;
    bool ret = false;
    for (; (pg = param_groups[i]); i++)
    {
        struct ctrl_param *p;
        int j = 0;
        for (; (p = pg->params[j]); j++)
        {
            int len = strlen(p->label);
            char tmp = line[len];
            if (line[len + 1] != '=')
                continue;
            line[len] = '\0';
            if (0 == strcmp(line, p->label))
            {
                int s = 0;
                struct slide_controller *slc;
                p->value = atof(&line[len + 3]);
                line[len] = tmp;
                printf("%s: %s Read %s with value %f\n", __func__, line, p->label, p->value);
                ret = true;
                slc = slc_arr[i][j];
                slide_controller_set_pos_from_value(slc);
                break;
            }
            line[len] = tmp;
        }
    }
    return ret;
}

void fm_save_settings(FILE *f)
{
    int i = 0;
    struct ctrl_param_group *pg;
    while ((pg = param_groups[i++]))
    {
        struct ctrl_param *p;
        int j = 0;
        while ((p = pg->params[j++]))
        {
            fprintf(f, "%s = %f\n", p->label, p->value);
        }
    }
}
