#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_scancode.h>
#include <malloc.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "cosine.h"
#include "delay.h"
#include "distortion.h"
#include "envelope.h"
#include "fm.h"
#include "low_pass_filter.h"
#include "midi.h"
#include "osc.h"
#include "sequencer.h"
#include "slide_controller.h"
#include "square_controller.h"
#include "text.h"
#include "util.h"

#define WIDTH (1024)
#define HEIGHT (768)
#define WAVEFORM_LEN (1024)

#define MAX_DELAY_MS (750)

#define LINE_LEN (100)
#define MAX_GROUPS (9)

#define DEFAULT_SETTINGS_FILE_NAME "saved_settings.txt"

#define NBR_BALLS (20)

int window_width;
int window_height;
static SDL_FRect main_location;
static bool synth_abort = false;
struct osc_state saw_state;
struct osc_state pulse_state;
static void pr_sdl_err()
{
    fprintf(stderr, "%s", SDL_GetError());
    SDL_ClearError();
}

struct voice
{
    int key; // 0 is off, 1 is a C
    long long released;
    long long pressed;
    struct env_state env;
    struct filter_state filter;
};

struct voice voices[NBR_VOICES] = {};

static struct ctrl_param op_amp = {
    .label = "OP1 AMP",
    .value = 0.01,
    .min = 0.001,
    .max = 0.1,
};

static struct ctrl_param op_freq = {
    .label = "OP1 FREQ",
    .value = 0.01,
    .min = 0.001,
    .max = 2.0,
};

static struct ctrl_param amplitude = {
    .label = "LINEAR GAIN",
    .value = 0.7,
    .min = 0.1,
    .max = 3,
};

static struct ctrl_param octave = {
    .label = "OCTAVE",
    .value = 0,
    .min = 0,
    .max = 5,
    .quantized_to_int = true,
};

static struct ctrl_param pulse1 = {
    .label = "ENABLE PULSE",
    .value = 1,
    .min = 0,
    .max = 1,
    .quantized_to_int = true,
};
static struct ctrl_param saw1 = {
    .label = "ENABLE SAW",
    .value = 1,
    .min = 0,
    .max = 1,
    .quantized_to_int = true,
};
static struct ctrl_param fm1 = {
    .label = "ENABLE FM",
    .value = 1,
    .min = 0,
    .max = 1,
    .quantized_to_int = true,
};

static struct ctrl_param cutoff = {
    .label = "CUTOFF",
    .value = 17000,
    .min = 50,
    .max = 17000,
};
static struct ctrl_param resonance = {
    .label = "RESONANCE",
    .value = 0.0,
    .min = 0.0,
    .max = 0.98,
};
static struct ctrl_param key_to_cutoff = {
    .label = "KEY TO CUTOFF",
    .value = 0.0,
    .min = 0.0,
    .max = 100.0,
};

static struct ctrl_param cutoff_lfo_freq = {
    .label = "CUTOFF LFO FREQ",
    .value = 0.0,
    .min = 0.0,
    .max = 5.0,
};

static struct ctrl_param cutoff_lfo_amp = {
    .label = "CUTOFF LFO AMP",
    .value = 0.0,
    .min = 0.0,
    .max = 5000.0,
};

static struct ctrl_param bend_target = {
    .label = "BEND",
    .value = 1.0,
    .min = -2.0,
    .max = 2.0,
};
static float bend = 1.0;

static struct ctrl_param dist_level = {
    .label = "DIST THRESHOLD",
    .value = 1.0,
    .min = 0.01,
    .max = 1.0,
};
static struct ctrl_param flip_level = {
    .label = "FLIP THRESHOLD",
    .value = 1.1,
    .min = 0.01,
    .max = 1.1,
};

static struct ctrl_param A = {
    .label = "A",
    .value = 0.1,
    .min = 0.1,
    .max = 500.0,
};
static struct ctrl_param D = {
    .label = "D",
    .value = 25,
    .min = 0.1,
    .max = 500,
};
static struct ctrl_param S = {
    .label = "S",
    .value = 1,
    .min = 0,
    .max = 1,
};
static struct ctrl_param R = {
    .label = "R",
    .value = 0,
    .min = 0,
    .max = 1000,
};

static struct ctrl_param env_to_cutoff = {
    .label = "ENV TO CUTOFF",
    .value = 0,
    .min = 0,
    .max = 10000,
};
static struct ctrl_param env_to_amp = {
    .label = "ENV TO AMP",
    .value = 1.0,
    .min = 0,
    .max = 1.0,
    .quantized_to_int = true,
};

static struct ctrl_param delay_ms = {
    .label = "DELAY [MS]",
    .value = 600,
    .min = 0,
    .max = 1000,
};

static struct ctrl_param delay_fb = {
    .label = "DELAY FEEDBACK",
    .value = 0.0,
    .min = 0.0,
    .max = 0.9,
};

static struct ctrl_param chorus_amount = {
    .label = "CHORUS AMOUNT",
    .value = 0.0,
    .min = 0.0,
    .max = 1.0,
};

static struct ctrl_param chorus_freq = {
    .label = "CHORUS FREQ",
    .value = 1.0,
    .min = 0.1,
    .max = 5.0,
};

static struct ctrl_param_group tone_ctrls = {
    .params = {&amplitude, &pulse1, &saw1, &fm1, &octave, &env_to_amp, NULL},
};

static struct ctrl_param_group envelope_ctrls = {
    .params = {&A, &D, &S, &R, NULL},
};

static struct ctrl_param_group filter_ctrls = {
    .params = {&cutoff, &resonance, &env_to_cutoff, &key_to_cutoff, &cutoff_lfo_freq, &cutoff_lfo_amp, NULL},
};

static struct ctrl_param_group dist_ctrls = {
    .params = {&dist_level, &flip_level, NULL},
};

static struct ctrl_param_group delay_ctrls = {
    .params = {&delay_fb, &delay_ms, NULL},
};

static struct ctrl_param_group chorus_ctrls = {
    .params = {&chorus_amount, &chorus_freq, NULL},
};

struct ctrl_param_group *param_groups[MAX_GROUPS] = {&tone_ctrls,  &envelope_ctrls, &filter_ctrls, &dist_ctrls,
                                                     &delay_ctrls, &chorus_ctrls,   NULL};

static struct square_controller *sqc_arr[5] = {};
static struct slide_controller *slc_arr[MAX_PARAMS_PER_GROUP * MAX_GROUPS] = {};

static float pianokey_per_scancode[SDL_SCANCODE_COUNT] = {
    [SDL_SCANCODE_Z] = 1,  [SDL_SCANCODE_S] = 2,  [SDL_SCANCODE_X] = 3,  [SDL_SCANCODE_D] = 4,  [SDL_SCANCODE_C] = 5,
    [SDL_SCANCODE_V] = 6,  [SDL_SCANCODE_G] = 7,  [SDL_SCANCODE_B] = 8,  [SDL_SCANCODE_H] = 9,  [SDL_SCANCODE_N] = 10,
    [SDL_SCANCODE_J] = 11, [SDL_SCANCODE_M] = 12, [SDL_SCANCODE_Q] = 13, [SDL_SCANCODE_2] = 14, [SDL_SCANCODE_W] = 15,
    [SDL_SCANCODE_3] = 16, [SDL_SCANCODE_E] = 17, [SDL_SCANCODE_R] = 18, [SDL_SCANCODE_5] = 19, [SDL_SCANCODE_T] = 20,
    [SDL_SCANCODE_6] = 21, [SDL_SCANCODE_Y] = 22, [SDL_SCANCODE_7] = 23, [SDL_SCANCODE_U] = 24, [SDL_SCANCODE_I] = 25,
    [SDL_SCANCODE_9] = 26, [SDL_SCANCODE_O] = 27, [SDL_SCANCODE_0] = 28, [SDL_SCANCODE_P] = 29,

};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int waveform_written = 0;
static SDL_FPoint points[WAVEFORM_LEN];
static SDL_AudioStream *stream;
static char *buf;
static long long current_frame = 0;
static int sample_frames;
static int buffer_frames;
static size_t frame_size;
static int fill_target;
static int num_frames_to_gen_in_one_go;
static SDL_AudioSpec input_spec = {.channels = 1, .format = SDL_AUDIO_S16, .freq = 44100};

static size_t calc_frame_size(const SDL_AudioSpec *spec)
{
    return spec->channels * SDL_AUDIO_BYTESIZE(spec->format);
}

static void main_save_settings(FILE *f)
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
static void save_settings()
{
    char filename[] = DEFAULT_SETTINGS_FILE_NAME;
    FILE *f = fopen(filename, "w");
    if (f)
    {
        main_save_settings(f);
        fm_save_settings(f);
        fclose(f);
    }
}

static bool main_read_setting(char *line)
{
    int i = 0;
    struct ctrl_param_group *pg;
    bool ret = false;
    while ((pg = param_groups[i++]))
    {
        struct ctrl_param *p;
        int j = 0;
        while ((p = pg->params[j++]))
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
                for (s = 0; (slc = slc_arr[s]); s++)
                {
                    slide_controller_set_pos_from_value(slc);
                }
                break;
            }
            line[len] = tmp;
        }
    }
    return ret;
}

static void load_settings(char *filename)
{
    char c;
    int i = 0;
    char line[LINE_LEN + 1];
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        printf("Failed to open \"%s\"\n", filename);
        return;
    }
    while (fread(&c, 1, 1, f) && c != EOF)
    {
        if (c == '\n')
        {
            line[i] = '\0';
            if (main_read_setting(line) || fm_read_setting(line))
            {
            }
            i = 0;
            continue;
        }
        line[i++] = c;
    }
}

static void key_press(int key)
{
    struct voice *oldest_voice = &voices[0];

    // notes higher that 0x53 are really bad so no need to even try
    if (key >= 0x53)
        return;
    pthread_mutex_lock(&mutex);
    // find oldest empty spot and if key is already in the array
    for (int i = 0; i < NBR_VOICES; i++)
    {
        struct voice *voice = &voices[i];
        // compare current to oldest
        if (oldest_voice->released > voice->released)
        {
            oldest_voice = voice;
        }
        else if (oldest_voice->released == voice->released && oldest_voice->pressed > voice->pressed)
        {
            oldest_voice = voice;
        }

        if (voice->key == key)
        {
            if (voice->released <= current_frame)
            {
                oldest_voice = voice;
                break;
            }
            pthread_mutex_unlock(&mutex);
            return;
        }
    }

    envelope_start(&oldest_voice->env, current_frame);
    oldest_voice->released = INT64_MAX;
    oldest_voice->pressed = current_frame;

    oldest_voice->key = key;
    pthread_mutex_unlock(&mutex);
}

static void voice_off(struct voice *voice)
{
    if (env_to_amp.value > 0.5)
    {
        envelope_release(&voice->env, current_frame);
    }
    else
    {
        voice->key = 0;
    }
    voice->released = current_frame;
}

static void key_release(int key)
{
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < NBR_VOICES; i++)
    {
        struct voice *voice = &voices[i];
        if (voice->key == key)
        {
            if (voice->released > current_frame)
            {
                voice_off(voice);
                pthread_mutex_unlock(&mutex);
                return;
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}

static void note_change(int key_on, int key_off)
{
    key_release(key_off);
    key_press(key_on);
};

static void notes_off()
{
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < NBR_VOICES; i++)
    {
        struct voice *voice = &voices[i];
        if (voice->released > current_frame)
        {
            voice_off(voice);
        }
    }
    pthread_mutex_unlock(&mutex);
}

static float render_voice(struct voice *voice, long long current_frame, const SDL_AudioSpec *spec)
{
    float sample = 0.0;
    float freq = key_to_freq[voice->key][0];
    struct osc_state *state;
    if (fm1.value > 0.5)
    {
        // non released not is INT64_MAX cause it was a suitable value here, but 0 makes more sense in the fm
        // rendering
        sample = amplitude.value *
                 fm_render_sample(current_frame - voice->pressed,
                                  voice->released == INT64_MAX ? 0 : (current_frame - voice->released), spec, freq);
    }
    if (pulse1.value > 0.5)
    {
        state = &pulse_state;
        sample += amplitude.value * osc_render_sample(current_frame, state, spec, voice->key);
    }

    if (saw1.value > 0.5)
    {
        state = &saw_state;
        sample += amplitude.value * osc_render_sample(current_frame, state, spec, voice->key);
    }

    // envelope
    if (env_to_amp.value > 0.5)
    {
        sample = sample * envelope_get(&voice->env, A.value, D.value, S.value, R.value, current_frame);
    }
    else
    {
        if (0.0 == envelope_get(&voice->env, A.value, D.value, S.value, R.value, current_frame))
            voice->key = 0;
    }
    // filter
    int cut_freq =
        min(17000,
            max(50, key_to_cutoff.value * key_to_freq[voice->key][0] + cutoff.value +
                        env_to_cutoff.value *
                            (envelope_get(&voice->env, A.value, D.value, S.value, R.value, current_frame) - S.value) +
                        cutoff_lfo_amp.value * cosine_render_sample(current_frame, spec, cutoff_lfo_freq.value)));
    low_pass_filter_configure(&voice->filter, cut_freq, resonance.value, spec->freq);
    sample = low_pass_filter_get_output(&voice->filter, sample);
    // distort
    sample += distort(sample, dist_level.value, flip_level.value);
    return sample;
}

static float render_sample(const long long current_frame, const SDL_AudioSpec *spec)
{
    float sample = 0.0;
    for (int i = 0; i < NBR_VOICES; i++)
    {
        struct voice *voice = &voices[i];
        if (voice->key != 0)
        {
            sample += render_voice(voice, current_frame, spec);
        }
    }

    // echo
    sample += delay_fb.value * delay_get_sample(delay_ms.value, spec);
    delay_put_sample(sample);

    // chorus
    float chorus_delay_ms = 3.0 + 1.0 * cosine_render_sample(current_frame, spec, chorus_freq.value);
    sample += chorus_amount.value * delay_get_sample(chorus_delay_ms, spec);

    sample = distort(sample, 0.999, 100.0);

    return sample;
}

static void write_sample(float sample, char **buf, const SDL_AudioSpec *spec)
{
    int16_t *out = (int16_t *)*buf;
    *out = (int16_t)(sample * 0x7FFF);
    *buf += 2;
}

static void render_waveform(union sigval)
{
#define WAVEFORM_KEY 8
    int i;
    static struct voice waveform_voice = {
        .key = 0, // Not initialized - OFF
    };
    const float periods = 3;
    const float periods_per_step = periods / WAVEFORM_LEN;
    const float step_size = 1.0 * window_width / WAVEFORM_LEN;
    const float freq = key_to_freq[WAVEFORM_KEY][0];
    const float frames_per_period = input_spec.freq / freq;
    const float frames_per_step = frames_per_period * periods_per_step;
    if (waveform_voice.key == 0)
    {
        envelope_init(&waveform_voice.env, &input_spec);
        /* just set open filter */
        low_pass_filter_init(&waveform_voice.filter, 0, 20000, input_spec.freq);
        envelope_start(&waveform_voice.env, 0);
        waveform_voice.released = INT64_MAX;
        waveform_voice.pressed = 1;
        waveform_voice.key = WAVEFORM_KEY;
    }

    int frame = floorf(current_frame / frames_per_period) * frames_per_period;
    for (i = 0; i < WAVEFORM_LEN; i++)
    {
        frame += frames_per_step;

        points[i].x = i * window_width / WAVEFORM_LEN;
        points[i].y = 0.5 * window_height * (1 + render_voice(&waveform_voice, input_spec.freq + frame, &input_spec));
    }
}

static bool render_sample_frames(long long *current_frame, int frames, char *buf, const SDL_AudioSpec *spec)
{
    int s, c, i = 0;
    float sample;
    struct voice *lowest_voice = NULL;
    { // Find the key for which we generate the visualization.
        for (i = 0; i < NBR_VOICES; i++)
        {
            if (voices[i].pressed < voices[i].released && (!lowest_voice || lowest_voice->key > voices[i].key))
                lowest_voice = &voices[i];
        }
    }

    for (s = 0; s < frames; s++)
    {

        // what is going on with channels here? only one buffer so it seems a bit
        // broken if multiple channels.
        for (c = 0; c < spec->channels; c++)
        {
            sample = render_sample(*current_frame, spec);
            write_sample(sample, &buf, spec);
        }
        *current_frame += 1;
    }

    return true;
}
static unsigned calc_frames_queued(SDL_AudioStream *stream, const SDL_AudioSpec *spec)
{
    int bytes_queued = SDL_GetAudioStreamQueued(stream);
    return bytes_queued / calc_frame_size(spec);
}

static void fill_audio_buffer(union sigval)
{
    pthread_mutex_lock(&mutex);
    // check how much is in buffer
    // render rest
    int frames_queued = calc_frames_queued(stream, &input_spec);
    if (frames_queued == 0)
    {
        fill_target += 10;
        printf("fill target %d Nothing in queueu!\n", fill_target);
    }
    int frames = fill_target - frames_queued;
    frames = min(frames, buffer_frames);
    if (frames > 0)
    {
        render_sample_frames(&current_frame, frames, buf, &input_spec);
        if (!SDL_PutAudioStreamData(stream, buf, frame_size * frames))
        {
            pr_sdl_err();
        }
    }
    pthread_mutex_unlock(&mutex);
}

static void trigger_draw_video_event(union sigval)
{
    SDL_Event user_event;
    SDL_zero(user_event); /* SDL will copy this entire struct! Initialize to keep
                             memory checkers happy. */
    user_event.type = SDL_EVENT_USER;
    user_event.user.code = 1;
    user_event.user.data1 = NULL;
    user_event.user.data2 = NULL;
    SDL_PushEvent(&user_event);
}

static void main_relocate(int x, int y, int w, int h)
{
    main_location.x = x;
    main_location.y = y;
    main_location.w = w;
    main_location.h = h;
}

static void draw(SDL_Renderer *renderer)
{
    int i;
    float time_scale_factor = 3.0;

    // draw every 5:th pixel of the window in x

    struct square_controller *sqc;
    struct slide_controller *slc;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    pthread_mutex_lock(&mutex); // TODO: Is the locking really good? Think this through!

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderLines(renderer, points, WAVEFORM_LEN);

    // Draw bounding box
    SDL_SetRenderDrawColor(renderer, 100, 180, 210, 100);
    SDL_RenderRect(renderer, &main_location);

    for (i = 0; (sqc = sqc_arr[i]); i++)
    {
        square_controller_draw(renderer, sqc);
    }
    for (i = 0; (slc = slc_arr[i]); i++)
    {
        slide_controller_draw(renderer, slc);
    }

    sequencer_draw(renderer);

    fm_draw(renderer);
    osc_draw(&pulse_state, renderer);
    osc_draw(&saw_state, renderer);

    SDL_RenderPresent(renderer);
    waveform_written = 0;

    pthread_mutex_unlock(&mutex);
}

static void sig_handler(int signum)
{
    printf("ABORT!\n");
    synth_abort = true;
}

static int setup_audio_timer(timer_t *t)
{
    struct sigevent sevnt = {.sigev_notify = SIGEV_THREAD, .sigev_notify_function = fill_audio_buffer};
    struct itimerspec new_value = {
        .it_interval = {.tv_nsec = num_frames_to_gen_in_one_go * 1000000000ull / input_spec.freq}};
    new_value.it_value = new_value.it_interval;

    int ret = timer_create(CLOCK_MONOTONIC, &sevnt, t);
    if (ret)
    {
        perror("Failed to create audio timer!");
        return -1;
    }

    timer_settime(*t, 0, &new_value, NULL);

    return 0;
}

static int setup_video_timer(timer_t *t)
{
    int i;
    struct sigevent sevnt = {.sigev_notify = SIGEV_THREAD, .sigev_notify_function = trigger_draw_video_event};

    struct itimerspec new_value = {.it_interval = {.tv_nsec = 100000000ull / 8}};
    new_value.it_value = new_value.it_interval;

    int ret = timer_create(CLOCK_MONOTONIC, &sevnt, t);
    if (ret)
    {
        perror("Failed to create video timer!");
        return -1;
    }

    timer_settime(*t, 0, &new_value, NULL);

    return 0;
}

static int setup_render_wave_timer(timer_t *t)
{
    int i;
    struct sigevent sevnt = {.sigev_notify = SIGEV_THREAD, .sigev_notify_function = render_waveform};
    struct itimerspec new_value = {.it_interval = {.tv_nsec = 100000000ull / 8}};
    new_value.it_value = new_value.it_interval;

    int ret = timer_create(CLOCK_MONOTONIC, &sevnt, t);
    if (ret)
    {
        perror("Failed to create render wave timer!");
        return -1;
    }

    timer_settime(*t, 0, &new_value, NULL);

    return 0;
}

static void relocate(int w, int h)
{
    const int main_width = 300;

    window_width = w;
    window_height = h;

    main_relocate(0, 0, main_width, h);
    osc_relocate(&saw_state, main_width, 0, (w - main_width) / 2, 200);
    osc_relocate(&pulse_state, main_width + (w - main_width) / 2, 0, (w - main_width) / 2, 200);
    fm_relocate(main_width, 200, w - main_width, h - 300);
    sequencer_relocate(main_width, h - 100, w - main_width, 100);
}

int main(int argc, char **argv)
{
    SDL_AudioDeviceID devId;
    bool res;
    SDL_AudioSpec output_spec;
    SDL_Window *window;
    timer_t audio_timer;
    timer_t video_timer;
    timer_t render_wave_timer;

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_VIDEO))
    {
        pr_sdl_err();
        return -2;
    }

    signal(SIGINT, sig_handler);

    // VIDEO STUFF
    window = SDL_CreateWindow("Synth One",                             // window title
                              WIDTH,                                   // width, in pixels
                              HEIGHT,                                  // height, in pixels
                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE // flags - see below
    );
    if (!window)
    {
        pr_sdl_err();
        return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer)
    {
        pr_sdl_err();
        return 1;
    }

    text_init(renderer);

    pthread_mutex_lock(&mutex);
    {
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
                x = margin + y / (HEIGHT - tot_height) * (margin + width);
                struct linear_control placeholder = {&p->value, p->min, p->max, p->quantized_to_int};
                slc_arr[k++] = slide_controller_create(x, y % (HEIGHT - tot_height), width, height,
                                                       (struct linear_control)placeholder, p->label);
                y += (margin + height + label_height);
            }
            y += 3 * margin;
        }
    }
    pthread_mutex_unlock(&mutex);

    if ((res = setup_video_timer(&video_timer)))
        return res;

    // initialization of sub modules
    main_relocate(0, 0, 300, HEIGHT);
    osc_init(&saw_state, OSC_TYPE_SAW, 300, 0, (WIDTH - 300) / 2, 200);
    osc_init(&pulse_state, OSC_TYPE_PULSE, 300 + (WIDTH - 300) / 2, 0, (WIDTH - 200) / 2, 200);
    fm_init(300, 200, WIDTH - 300, HEIGHT - 300);
    sequencer_init(300, HEIGHT - 100, WIDTH - 300, 100, note_change);
    delay_init(&input_spec, MAX_DELAY_MS);

    relocate(WIDTH, HEIGHT);

    // MIDI STUFF
    snd_rawmidi_t *midi_in = midi_start();

    // SETTINGS
    if (argc == 2)
        load_settings(argv[1]);
    else
        load_settings(DEFAULT_SETTINGS_FILE_NAME);

    // AUDIO STUFF

    init_key_to_freq();

    for (int i = 0; i < NBR_VOICES; i++)
    {
        voices[i].pressed = 0;
        voices[i].released = 0;

        envelope_init(&voices[i].env, &input_spec);
        low_pass_filter_init(&voices[i].filter, res, cutoff.value, input_spec.freq);
    }

    if (res = setup_render_wave_timer(&render_wave_timer))
        return res;

    int count;
    SDL_AudioDeviceID *ids = SDL_GetAudioPlaybackDevices(&count);
    for (int i = 0; i < count; i++)
    {
        printf("%d: %s\n", i, SDL_GetAudioDeviceName(ids[i]));
    }

    devId = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!devId)
    {
        pr_sdl_err();
        return 2;
    }

    if (!SDL_GetAudioDeviceFormat(devId, &output_spec, &sample_frames))
    {
        pr_sdl_err();
        return 3;
    }
    buffer_frames = sample_frames * 2;
    fill_target = sample_frames;
    num_frames_to_gen_in_one_go = sample_frames / 8;
    frame_size = calc_frame_size(&input_spec);
    printf("Frame size %ld\n", frame_size);
    buf = malloc(buffer_frames * frame_size);

    printf("Audiodriver %s, id %u, channels %d, freq %d, frames %d, \n", SDL_GetCurrentAudioDriver(), devId,
           output_spec.channels, output_spec.freq, sample_frames);

    if (!(stream = SDL_CreateAudioStream(&input_spec, &output_spec)))
    {
        pr_sdl_err();
        return 4;
    }

    if (!SDL_BindAudioStream(devId, stream))
    {
        pr_sdl_err();
        return 5;
    }

    // render 2xsample_frames
    pthread_mutex_lock(&mutex);
    render_sample_frames(&current_frame, buffer_frames, buf, &input_spec);

    // write to stream
    if (!SDL_PutAudioStreamData(stream, buf, frame_size * buffer_frames))
    {
        pr_sdl_err();
        return 6;
    }

    if (!SDL_ResumeAudioStreamDevice(stream))
    {
        pr_sdl_err();
        return 7;
    }
    pthread_mutex_unlock(&mutex);

    if (res = setup_audio_timer(&audio_timer))
        return res;

    SDL_Event event;
    while (!synth_abort)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                int new_key;
                switch (event.key.scancode)
                {
                case SDL_SCANCODE_SPACE:
                    sequencer_toggle_run();
                    notes_off();
                    break;
                case SDL_SCANCODE_ESCAPE:
                    sequencer_toggle_edit();
                    break;
                default:
                    new_key = pianokey_per_scancode[event.key.scancode];
                    if (new_key != 0)
                    {
                        new_key += 12 * octave.value;
                        key_press(new_key);
                    }
                    sequencer_input(new_key);
                }
            }
            else if (event.type == SDL_EVENT_KEY_UP)
            {
                int new_key = pianokey_per_scancode[event.key.scancode];
                if (new_key != 0)
                {
                    new_key += 12 * octave.value;
                    key_release(new_key);
                }
            }
            else if (event.type == SDL_EVENT_USER)
            {
                draw(renderer);
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                int i;
                struct square_controller *sqc;
                struct slide_controller *slc;
                for (i = 0; (sqc = sqc_arr[i]); i++)
                {
                    square_controller_click(sqc, event.button.x, event.button.y);
                }
                for (i = 0; (slc = slc_arr[i]); i++)
                {
                    slide_controller_click(slc, event.button.x, event.button.y);
                }

                fm_click(event.button.x, event.button.y);
                osc_click(&saw_state, event.button.x, event.button.y);
                osc_click(&pulse_state, event.button.x, event.button.y);
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
            {
                int i;
                struct square_controller *sqc;
                struct slide_controller *slc;
                for (i = 0; (sqc = sqc_arr[i]); i++)
                {
                    square_controller_unclick(sqc);
                }
                for (i = 0; (slc = slc_arr[i]); i++)
                {
                    slide_controller_unclick(slc);
                }

                fm_unclick();
                osc_unclick(&saw_state);
                osc_unclick(&pulse_state);
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION)
            {
                int i;
                struct square_controller *sqc;
                struct slide_controller *slc;
                for (i = 0; (sqc = sqc_arr[i]); i++)
                {
                    square_controller_move(sqc, event.motion.x, event.motion.y);
                }
                for (i = 0; (slc = slc_arr[i]); i++)
                {
                    slide_controller_move(slc, event.motion.x, event.motion.y);
                }
                fm_move(event.motion.x, event.motion.y);
                osc_move(&saw_state, event.motion.x, event.motion.y);
                osc_move(&pulse_state, event.motion.x, event.motion.y);
            }
            else if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                int w = event.window.data1;
                int h = event.window.data2;
                relocate(w, h);
            }
            else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                synth_abort = true;
            }
        }

        struct midi_message msg;
        while (midi_get(midi_in, &msg))
        {
            if (msg.type == MIDI_MSG_NOTE_ON)
            {
                key_press(msg.note.key);
            }
            else if (msg.type == MIDI_MSG_NOTE_OFF)
            {
                key_release(msg.note.key);
            }
        }
    }

    save_settings();

    midi_stop(midi_in);

    timer_delete(video_timer);
    timer_delete(audio_timer);
    timer_delete(render_wave_timer);

    SDL_DestroyAudioStream(stream);
    SDL_CloseAudioDevice(devId);

    return 0;
}
