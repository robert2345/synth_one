#include "osc.h"
#include "cosine.h"
#include "linear_control.h"
#include "slide_controller.h"
#include "text.h"
#include "util.h"
#include <stdio.h>

#define MAX_OSC_COUNT (4) // per voice

#define MAX_WIDTH (0.99)
#define MIN_WIDTH (0.01)
#define MAX_GROUPS (3)

static bool initialized = false;

static struct slide_controller *slc_arr[MAX_PARAMS_PER_GROUP * MAX_GROUPS] = {};

static struct ctrl_param base_width = {
    .label = "PULSE WIDTH",
    .value = 0.5,
    .min = MIN_WIDTH,
    .max = MAX_WIDTH,
};
static struct ctrl_param pwm_freq = {
    .label = "PWM FREQ",
    .value = 0.3,
    .min = 0.001,
    .max = 10.0,
};
static struct ctrl_param pwm_amount = {
    .label = "PWM AMOUNT",
    .value = 0.0,
    .min = 0.0,
    .max = 0.5,
};
static struct ctrl_param_group pwm_ctrls = {
    .params = {&base_width, &pwm_freq, &pwm_amount},
};

static struct ctrl_param osc_cnt = {
    .label = "OSC COUNT",
    .value = 1,
    .min = 1,
    .max = MAX_OSC_COUNT,
    .quantized_to_int = true,
};

static struct ctrl_param osc_detune_step = {
    .label = "DETUNE CENTS",
    .value = 0,
    .min = 0,
    .max = 50,
};

static struct ctrl_param_group detune_ctrls = {
    .params = {&osc_cnt, &osc_detune_step},
};

static struct ctrl_param_group *param_groups[MAX_GROUPS] = {&detune_ctrls, &pwm_ctrls};

static float render_pulse(const long long current_frame, float *period_pos, const SDL_AudioSpec *spec, float freq,
                          float width)
{
    *period_pos += freq / spec->freq;
    if (*period_pos > 1.0)
    {
        *period_pos = 0.0;
    }

    if (*period_pos > width)
        return -1.0;
    else
        return 1.0;
}

static float render_saw(const long long current_frame, float *period_pos, const SDL_AudioSpec *spec, float freq)
{
    *period_pos += freq / spec->freq;
    if (*period_pos > 1.0)
    {
        *period_pos = 0.0;
    }

    return -1.0 + 2.0 * *period_pos;
}

float osc_render_sample(long long current_frame, struct osc_state *state, const SDL_AudioSpec *spec, int key,
                        enum osc_type type)
{
    float sample = 0.0;
    float width = base_width.value + pwm_amount.value * cosine_render_sample(current_frame, spec, pwm_freq.value);

    width = max(MIN_WIDTH, width);
    width = min(MAX_WIDTH, width);

    int detune_cents = -((int)osc_cnt.value * osc_detune_step.value) / 2;
    for (int osc = 0; osc < (int)osc_cnt.value; osc++)
    {
        float freq = key_to_freq[key][detune_cents + osc * (int)osc_detune_step.value];

        if (type == OSC_TYPE_PULSE)
            sample += 1.0 / NBR_VOICES * render_pulse(current_frame, &state->period_position[osc], spec, freq, width);
        else if (type == OSC_TYPE_SAW)
            sample += 1.0 / NBR_VOICES * render_saw(current_frame, &state->period_position[osc], spec, freq);
        else
        {
            fprintf(stderr, "Invalid oscillator type %d\n", type);
        }
    }
    return sample;
}

void osc_draw(SDL_Renderer *renderer)
{
    int i;
    struct slide_controller *slc;

    for (i = 0; (slc = slc_arr[i]); i++)
    {
        slide_controller_draw(renderer, slc);
    }
}

void osc_click(int x, int y)
{
    int i;
    struct slide_controller *slc;
    for (i = 0; (slc = slc_arr[i]); i++)
    {
        slide_controller_click(slc, x, y);
    }
}

void osc_unclick()
{
    int i;
    struct slide_controller *slc;
    for (i = 0; (slc = slc_arr[i]); i++)
    {
        slide_controller_unclick(slc);
    }
}

void osc_move(int x, int y)
{
    int i;
    struct slide_controller *slc;
    for (i = 0; (slc = slc_arr[i]); i++)
    {
        slide_controller_move(slc, x, y);
    }
}

void osc_init(struct osc_state *state, int x_in, int y_in)
{
    if (!state)
    {
        fprintf(stderr, "%s: Null pointer\n", __func__);
    }
    else
    {
        memset(state, 0, sizeof(*state));
    }
    // Initialize all the actual controllers
    if (!initialized)
    {
        initialized = true;
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
}
