#include "osc.h"
#include "cosine.h"
#include "linear_control.h"
#include "osc.h"
#include "slide_controller.h"
#include "text.h"
#include "util.h"
#include <stdio.h>

#define MAX_WIDTH (0.99)
#define MIN_WIDTH (0.01)

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

static float render_pulse(const long long current_frame, const SDL_AudioSpec *spec, float freq, float width)
{
    float t = (double)current_frame / spec->freq;
    float periods = t * freq;
    float period_pos = modff(periods, &t);

    if (period_pos > width)
        return -1.0;
    else
        return 1.0;
}

static float render_saw(const long long current_frame, const SDL_AudioSpec *spec, float freq)
{
    float t = (double)current_frame / spec->freq;
    float periods = t * freq;
    float period_pos = modff(periods, &t);

    return -1.0 + 2.0 * period_pos;
}

float osc_render_sample(long long current_frame, struct osc_state *state, const SDL_AudioSpec *spec, int key,
                        enum osc_type type)
{
    float sample = 0.0;
    float width = state->base_width.value +
                  state->pwm_amount.value * cosine_render_sample(current_frame, spec, state->pwm_freq.value);

    width = max(MIN_WIDTH, width);
    width = min(MAX_WIDTH, width);

    int detune_cents = -((int)state->osc_cnt.value * state->osc_detune_step.value) / 2;
    for (int osc = 0; osc < (int)state->osc_cnt.value; osc++)
    {
        float freq = key_to_freq[key][detune_cents + osc * (int)state->osc_detune_step.value];

        if (type == OSC_TYPE_PULSE)
            sample += 1.0 / NBR_VOICES * render_pulse(current_frame, spec, freq, width);
        else if (type == OSC_TYPE_SAW)
            sample += 1.0 / NBR_VOICES * render_saw(current_frame, spec, freq);
        else
        {
            fprintf(stderr, "Invalid oscillator type %d\n", type);
        }
    }
    return sample;
}

void osc_draw(struct osc_state *state, SDL_Renderer *renderer)
{
    int i;
    struct slide_controller *slc;

    SDL_SetRenderDrawColor(renderer, 200, 100, 200, 00);
    SDL_RenderRect(renderer, &state->location);

    for (i = 0; (slc = state->slc_arr[i]); i++)
    {
        slide_controller_draw(renderer, slc);
    }
}

void osc_click(struct osc_state *state, int x, int y)
{
    int i;
    struct slide_controller *slc;
    for (i = 0; (slc = state->slc_arr[i]); i++)
    {
        slide_controller_click(slc, x, y);
    }
}

void osc_unclick(struct osc_state *state)
{
    int i;
    struct slide_controller *slc;
    for (i = 0; (slc = state->slc_arr[i]); i++)
    {
        slide_controller_unclick(slc);
    }
}

void osc_move(struct osc_state *state, int x, int y)
{
    int i;
    struct slide_controller *slc;
    for (i = 0; (slc = state->slc_arr[i]); i++)
    {
        slide_controller_move(slc, x, y);
    }
}

void osc_relocate(struct osc_state *state, int x_in, int y_in, int width, int height)
{
    state->location.x = x_in;
    state->location.y = y_in;
    state->location.w = width;
    state->location.h = height;
    int i = 0;
    int j = 0;
    int k = 0;
    struct ctrl_param_group *pg;
    struct ctrl_param *p;
    const int margin = 10;
    const int ctrl_width = 100;
    const int ctrl_height = 10;
    int label_height = text_get_height();
    int tot_height = ctrl_height + label_height;
    int x = margin;
    int y = margin;
    while ((pg = state->param_groups[i++]))
    {
        j = 0;
        while ((p = pg->params[j++]))
        {
            struct slide_controller *slc = state->slc_arr[k++];
            x = x_in + margin + (y + y_in) / (height - tot_height) * (ctrl_width + margin);
            int y_to_set = y_in + y % (height - tot_height);
            slide_controller_relocate(slc, x, y_to_set, ctrl_width, ctrl_height);
            y += (margin + ctrl_height + label_height);
        }
        y += 3 * margin;
    }
}

void osc_init(struct osc_state *state, int x_in, int y_in, int width, int height)
{
    if (!state)
    {
        fprintf(stderr, "%s: Null pointer\n", __func__);
    }
    else
    {
        memset(state, 0, sizeof(*state));
    }

    // Init the parameters
    state->base_width = base_width;
    state->pwm_freq = pwm_freq;
    state->pwm_amount = pwm_amount;
    state->osc_cnt = osc_cnt;
    state->osc_detune_step = osc_detune_step;

    // init the parameter groups
    state->pwm_ctrls.params[0] = &state->base_width;
    state->pwm_ctrls.params[1] = &state->pwm_freq;
    state->pwm_ctrls.params[2] = &state->pwm_amount;
    state->detune_ctrls.params[0] = &state->osc_cnt;
    state->detune_ctrls.params[1] = &state->osc_detune_step;

    // list all   groups
    state->param_groups[0] = &state->detune_ctrls;
    state->param_groups[1] = &state->pwm_ctrls;

    // Initialize all the actual controllers
    {
        int i = 0;
        int j = 0;
        int k = 0;
        const int ctrl_width = 100;
        const int ctrl_height = 10;
        struct ctrl_param_group *pg;
        struct ctrl_param *p;
        while ((pg = state->param_groups[i++]))
        {
            j = 0;
            while ((p = pg->params[j++]))
            {
                state->slc_arr[k++] = slide_controller_create(
                    0, 0, 100, 10, (struct linear_control){&p->value, p->min, p->max, p->quantized_to_int, p->show_num},
                    p->label);
            }
        }
    }

    osc_relocate(state, x_in, y_in, width, height);
}
