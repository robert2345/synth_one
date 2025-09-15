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
#include "low_pass_filter.h"
#include "midi.h"
#include "slide_controller.h"
#include "square_controller.h"
#include "text.h"

#define WIDTH (1024)
#define HEIGHT (768)
#define X_STEP (5)
#define WAVEFORM_LEN (WIDTH / X_STEP)

#define NBR_VOICES (8)
#define MAX_OSC_COUNT (4) // per voice

#define min(x, y) ((x) < (y) ? x : y)
#define max(x, y) ((x) < (y) ? y : x)

#define MAX_WIDTH (0.99)
#define MIN_WIDTH (0.01)

#define NBR_KEYS (88)

#define MAX_DELAY_MS (750)

#define MAX_PARAMS_PER_GROUP (8)
#define MAX_GROUPS (8)

#define LINE_LEN (100)

#define DEFAULT_SETTINGS_FILE_NAME "saved_settings.txt"

enum osc_type
{
    OSC_TYPE_PULSE,
    OSC_TYPE_SAW,
    OSC_TYPE_COUNT,
};

static bool synth_abort = false;

static void pr_sdl_err()
{
    fprintf(stderr, "%s", SDL_GetError());
    SDL_ClearError();
}

struct voice
{
    int key; // 0 is off, 1 is a C
    float period_position[MAX_OSC_COUNT];
    long long released;
    long long pressed;
    struct env_state env;
    struct filter_state filter;
};

struct voice voices[NBR_VOICES] = {};

struct ctrl_param
{
    const char *label;
    float value;
    float min;
    float max;
    bool quantized_to_int;
};

struct ctrl_param_group
{
    struct ctrl_param *params[MAX_PARAMS_PER_GROUP];
};

struct ctrl_param amplitude = {
    .label = "LINEAR GAIN",
    .value = 0.7,
    .min = 0.1,
    .max = 3,
};

struct ctrl_param octave = {
    .label = "OCTAVE",
    .value = 0,
    .min = 0,
    .max = 5,
    .quantized_to_int = true,
};

struct ctrl_param osc_type = {
    .label = "SAW/PULSE",
    .value = OSC_TYPE_PULSE,
    .min = OSC_TYPE_SAW,
    .max = OSC_TYPE_PULSE,
    .quantized_to_int = true,
};

struct ctrl_param osc_cnt = {
    .label = "OSC COUNT",
    .value = 1,
    .min = 1,
    .max = MAX_OSC_COUNT,
    .quantized_to_int = true,
};

struct ctrl_param osc_detune_step = {
    .label = "DETUNE CENTS",
    .value = 0,
    .min = 0,
    .max = 50,
};

struct ctrl_param cutoff = {
    .label = "CUTOFF",
    .value = 17000,
    .min = 50,
    .max = 17000,
};
struct ctrl_param resonance = {
    .label = "RESONANCE",
    .value = 0.0,
    .min = 0.0,
    .max = 0.98,
};
struct ctrl_param base_width = {
    .label = "PULSE WIDTH",
    .value = 0.5,
    .min = MIN_WIDTH,
    .max = MAX_WIDTH,
};
struct ctrl_param pwm_freq = {
    .label = "PWM FREQ",
    .value = 0.3,
    .min = 0.001,
    .max = 10.0,
};
struct ctrl_param pwm_amount = {
    .label = "PWM AMOUNT",
    .value = 0.0,
    .min = 0.0,
    .max = 0.5,
};

struct ctrl_param bend_target = {
    .label = "BEND",
    .value = 1.0,
    .min = -2.0,
    .max = 2.0,
};
float bend = 1.0;

struct ctrl_param dist_level = {
    .label = "DIST THRESHOLD",
    .value = 1.0,
    .min = 0.01,
    .max = 1.0,
};
struct ctrl_param flip_level = {
    .label = "FLIP THRESHOLD",
    .value = 1.1,
    .min = 0.01,
    .max = 1.1,
};

struct ctrl_param A = {
    .label = "A",
    .value = 0.1,
    .min = 0.1,
    .max = 500.0,
};
struct ctrl_param D = {
    .label = "D",
    .value = 25,
    .min = 0.1,
    .max = 500,
};
struct ctrl_param S = {
    .label = "S",
    .value = 1,
    .min = 0,
    .max = 1,
};
struct ctrl_param R = {
    .label = "R",
    .value = 0,
    .min = 0,
    .max = 1000,
};

struct ctrl_param env_to_cutoff = {
    .label = "ENV TO CUTOFF",
    .value = 0,
    .min = 0,
    .max = 10000,
};
struct ctrl_param env_to_amp = {
    .label = "ENV TO AMP",
    .value = 1.0,
    .min = 0,
    .max = 1.0,
};

struct ctrl_param delay_ms = {
    .label = "DELAY [MS]",
    .value = 600,
    .min = 0,
    .max = 1000,
};

struct ctrl_param delay_fb = {
    .label = "DELAY FEEDBACK",
    .value = 0.0,
    .min = 0.0,
    .max = 0.9,
};

struct ctrl_param chorus_amount = {
    .label = "CHORUS AMOUNT",
    .value = 0.0,
    .min = 0.0,
    .max = 1.0,
};

struct ctrl_param chorus_freq = {
    .label = "CHORUS FREQ",
    .value = 1.0,
    .min = 0.1,
    .max = 5.0,
};

struct ctrl_param_group tone_ctrls = {
    .params = {&amplitude, &osc_type, &octave, &osc_cnt, &osc_detune_step, &env_to_amp},
};

struct ctrl_param_group envelope_ctrls = {
    .params = {&A, &D, &S, &R},
};

struct ctrl_param_group filter_ctrls = {
    .params = {&cutoff, &resonance, &env_to_cutoff},
};

struct ctrl_param_group dist_ctrls = {
    .params = {&dist_level, &flip_level},
};

struct ctrl_param_group delay_ctrls = {
    .params = {&delay_fb, &delay_ms},
};

struct ctrl_param_group pwm_ctrls = {
    .params = {&base_width, &pwm_freq, &pwm_amount},
};

struct ctrl_param_group chorus_ctrls = {
    .params = {&chorus_amount, &chorus_freq},
};

struct ctrl_param_group *param_groups[MAX_GROUPS] = {&tone_ctrls, &envelope_ctrls, &filter_ctrls, &pwm_ctrls,
                                                     &dist_ctrls, &delay_ctrls,    &chorus_ctrls};

struct square_controller *sqc_arr[5] = {};
struct slide_controller *slc_arr[MAX_PARAMS_PER_GROUP * MAX_GROUPS] = {};

float pianokey_per_scancode[SDL_SCANCODE_COUNT] = {
    [SDL_SCANCODE_Z] = 1,  [SDL_SCANCODE_S] = 2,  [SDL_SCANCODE_X] = 3,  [SDL_SCANCODE_D] = 4,  [SDL_SCANCODE_C] = 5,
    [SDL_SCANCODE_V] = 6,  [SDL_SCANCODE_G] = 7,  [SDL_SCANCODE_B] = 8,  [SDL_SCANCODE_H] = 9,  [SDL_SCANCODE_N] = 10,
    [SDL_SCANCODE_J] = 11, [SDL_SCANCODE_M] = 12, [SDL_SCANCODE_Q] = 13, [SDL_SCANCODE_2] = 14, [SDL_SCANCODE_W] = 15,
    [SDL_SCANCODE_3] = 16, [SDL_SCANCODE_E] = 17, [SDL_SCANCODE_R] = 18, [SDL_SCANCODE_5] = 19, [SDL_SCANCODE_T] = 20,
    [SDL_SCANCODE_6] = 21, [SDL_SCANCODE_Y] = 22, [SDL_SCANCODE_7] = 23, [SDL_SCANCODE_U] = 24, [SDL_SCANCODE_I] = 25,
    [SDL_SCANCODE_9] = 26, [SDL_SCANCODE_O] = 27, [SDL_SCANCODE_0] = 28, [SDL_SCANCODE_P] = 29,

};

float key_to_freq[NBR_KEYS] = {

};

void init_key_to_freq()
{
    for (int key = 0; key < 88; key++)
    {
        key_to_freq[key] = 440.0 * pow(2, ((key - 49) / 12.0));
    }
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int waveform_written = 0;
SDL_FPoint points[WIDTH / X_STEP];
SDL_AudioStream *stream;
char *buf;
long long current_frame = 0;
int sample_frames;
int buffer_frames;
size_t frame_size;
const SDL_AudioSpec input_spec = {.channels = 1, .format = SDL_AUDIO_S16, .freq = 44100};

static size_t calc_frame_size(const SDL_AudioSpec *spec)
{
    return spec->channels * SDL_AUDIO_BYTESIZE(spec->format);
}

static void save_settings()
{
    char filename[] = DEFAULT_SETTINGS_FILE_NAME;
    FILE *f = fopen(filename, "w");
    if (f)
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
        fclose(f);
    }
}

static void parse_and_apply_setting(char *string)
{
    int i = 0;
    struct ctrl_param_group *pg;
    while ((pg = param_groups[i++]))
    {
        struct ctrl_param *p;
        int j = 0;
        while ((p = pg->params[j++]))
        {
            int len = strlen(p->label);
            char tmp = string[len];
            if (string[len] != ' ')
                continue;
            string[len] = '\0';
            if (0 == strcmp(string, p->label))
            {
                p->value = atof(&string[len + 3]);
                string[len] = tmp;
                printf("%s\nRead %s with value %f\n", string, p->label, p->value);
                break;
            }
            string[len] = tmp;
        }
    }
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
            parse_and_apply_setting(line);
            // printf("%s\n", line);
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
    if (key >= 0x35)
        return;
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
            return;
        }
    }

    envelope_start(&oldest_voice->env, current_frame);
    oldest_voice->released = INT64_MAX;
    oldest_voice->pressed = current_frame;

    oldest_voice->key = key;
}

static void key_release(int key)
{
    for (int i = 0; i < NBR_VOICES; i++)
    {
        struct voice *voice = &voices[i];
        if (voice->key == key)
        {
            if (voice->pressed < current_frame)
            {
                envelope_release(&voice->env, current_frame);
                voice->released = current_frame;
                voice->pressed = INT64_MAX;
                return;
            }
        }
    }
}

static float render_pulse(const long long current_frame, float *period_pos, const SDL_AudioSpec *spec, float freq,
                          float width, bool *new_period)
{
    *period_pos += freq / spec->freq;
    if (*period_pos > 1.0)
    {
        *period_pos = 0.0;
        *new_period = true;
    }
    else
    {
        *new_period = false;
    }

    if (*period_pos > width)
        return -1.0;
    else
        return 1.0;
}

static float render_saw(const long long current_frame, float *period_pos, const SDL_AudioSpec *spec, float freq,
                        float width, bool *new_period)
{
    *period_pos += freq / spec->freq;
    if (*period_pos > 1.0)
    {
        *period_pos = 0.0;
        *new_period = true;
    }
    else
    {
        *new_period = false;
    }

    return -1 + 2 * *period_pos;
}

static float render_sample(const long long current_frame, const SDL_AudioSpec *spec, bool *new_period)
{
    float sample = 0.0;
    float width = base_width.value + pwm_amount.value * cosine_render_sample(current_frame, spec, pwm_freq.value);

    width = max(MIN_WIDTH, width);
    width = min(MAX_WIDTH, width);
    for (int i = 0; i < NBR_VOICES; i++)
    {
        struct voice *voice = &voices[i];
        if (voice->key != 0)
        {
            float raw_sample = 0.0;
            float up_offset_per_osc =
                (key_to_freq[voice->key + 1] - key_to_freq[voice->key]) / 100 * osc_detune_step.value;
            float down_offset_per_osc =
                (key_to_freq[voice->key] - key_to_freq[voice->key - 1]) / 100 * osc_detune_step.value;
            int osc_detune = -((int)osc_cnt.value) / 2;
            for (int osc = 0; osc < (int)osc_cnt.value; osc++)
            {

                float freq = key_to_freq[voice->key];
                if (osc_detune < 0)
                    freq += osc_detune * down_offset_per_osc;
                else
                    freq += osc_detune * down_offset_per_osc;

                freq = bend * freq;

                if (osc_type.value == OSC_TYPE_PULSE)
                {
                    raw_sample +=
                        amplitude.value / NBR_VOICES *
                        render_pulse(current_frame, &voice->period_position[osc], spec, freq, width, new_period);
                }
                else if (osc_type.value == OSC_TYPE_SAW)
                {
                    raw_sample +=
                        amplitude.value / NBR_VOICES *
                        render_saw(current_frame, &voice->period_position[osc], spec, freq, width, new_period);
                }
                else
                {
                    fprintf(stderr, "Invalid oscillator type\n");
                }

                osc_detune += 1;
            }
            // envelope
            raw_sample = raw_sample * (1.0 - env_to_amp.value) +
                         raw_sample * env_to_amp.value *
                             envelope_get(&voice->env, A.value, D.value, S.value, R.value, current_frame);
            // filter
            int cut_freq = min(17000, max(50, cutoff.value +
                                       env_to_cutoff.value * envelope_get(&voice->env, A.value, D.value, S.value,
                                                                          R.value, current_frame) +
                                       110 * cosine_render_sample(current_frame, spec, 0.1)));
            low_pass_filter_configure(&voice->filter, cut_freq, resonance.value, spec->freq);
            sample += low_pass_filter_get_output(&voice->filter, raw_sample);
        }
    }

    // distort

    sample = distort(sample, dist_level.value, flip_level.value);

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

static bool render_sample_frames(long long *current_frame, int frames, char *buf, const SDL_AudioSpec *spec)
{
    int s, c = 0;
    float sample;
    for (s = 0; s < frames; s++)
    {
        bool new_period;

        // what is going on with channels here? only one buffer so it seems a bit
        // broken if multiple channels.
        for (c = 0; c < spec->channels; c++)
        {
            sample = render_sample(*current_frame, spec, &new_period);
            write_sample(sample, &buf, spec);
        }

        if (new_period)
        {
#define BEND_STEP (0.001)
            float bend_diff = bend_target.value - bend;
            if (bend_diff > BEND_STEP)
            {
                bend += BEND_STEP;
            }
            else if (bend_diff < -BEND_STEP)
            {
                bend -= BEND_STEP;
            }
            else
            {
                bend = bend_target.value;
            }
        }

        // write to visualisation buffer
        {
            int samples_per_period = max(WAVEFORM_LEN, spec->freq / (bend * key_to_freq[voices[0].key]));
            bool on_grid = (*current_frame % (2 * samples_per_period / WAVEFORM_LEN) == 0) &&
                           waveform_written < WAVEFORM_LEN && waveform_written != 0;
            if ((new_period && (waveform_written == 0)) || on_grid)
            {
                points[waveform_written].y = HEIGHT / 2 + HEIGHT / 2 * sample;
                waveform_written++;
            }
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
    int frames = sample_frames - calc_frames_queued(stream, &input_spec);
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

static void draw_waveform(SDL_Renderer *renderer)
{
    int i;
    float time_scale_factor = 3.0;

    // draw every 5:th pixel of the window in x

    pthread_mutex_lock(&mutex);
    if (waveform_written == WAVEFORM_LEN)
    {
        struct square_controller *sqc;
        struct slide_controller *slc;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderLines(renderer, points, WIDTH / X_STEP);
        for (i = 0; (sqc = sqc_arr[i]); i++)
        {
            square_controller_draw(renderer, sqc);
        }
        for (i = 0; (slc = slc_arr[i]); i++)
        {
            slide_controller_draw(renderer, slc);
        }

        text_draw(renderer, "THIS IS A SYNTHESIZER!", WIDTH / 2 - 12 * 16, HEIGHT / 2 - 8, false);

        SDL_RenderPresent(renderer);
        waveform_written = 0;
    }

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
    struct itimerspec new_value = {.it_interval = {.tv_nsec = buffer_frames / 2 * 1000000000ull / input_spec.freq}};
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

    // initialize the points that shall be drawn
    for (i = 0; i < WAVEFORM_LEN; i++)
        points[i].x = i * X_STEP;

    struct itimerspec new_value = {.it_interval = {.tv_nsec = 1000000000ull / 8}};
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

int main(int argc, char **argv)
{
    SDL_AudioDeviceID devId;
    bool res;
    SDL_AudioSpec output_spec;
    SDL_Window *window;
    timer_t audio_timer;
    timer_t video_timer;

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_VIDEO))
    {
        pr_sdl_err();
        return -2;
    }

    signal(SIGINT, sig_handler);

    // SETTINGS
    if (argc == 2)
        load_settings(argv[1]);
    else
        load_settings(DEFAULT_SETTINGS_FILE_NAME);

    // VIDEO STUFF
    window = SDL_CreateWindow("Synth One",      // window title
                              WIDTH,            // width, in pixels
                              HEIGHT,           // height, in pixels
                              SDL_WINDOW_OPENGL // flags - see below
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

    {
        int i, j, k = 0;
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
                x = margin + y / (HEIGHT - tot_height) * (WIDTH - 2 * margin - width);
                slc_arr[k++] = slide_controller_create(
                    x, y % (HEIGHT - tot_height), width, height,
                    (struct linear_control){&p->value, p->min, p->max, p->quantized_to_int}, p->label);
                y += (margin + height + label_height);
            }
            y += 3 * margin;
        }
    }

    text_init(renderer);

    if ((res = setup_video_timer(&video_timer)))
        return res;

    // MIDI STUFF
    snd_rawmidi_t *midi_in = midi_start();

    // AUDIO STUFF

    init_key_to_freq();

    delay_init(&input_spec, MAX_DELAY_MS);

    for (int i = 0; i < NBR_VOICES; i++)
    {
        voices[i].pressed = 0;
        voices[i].released = 0;

        envelope_init(&voices[i].env, &input_spec);
        low_pass_filter_init(&voices[i].filter, res, cutoff.value, input_spec.freq);
    }

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
    buffer_frames = sample_frames / 2;
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
                int new_key = pianokey_per_scancode[event.key.scancode];
                if (new_key != 0)
                {
                    new_key += 12 * octave.value;
                    key_press(new_key);
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
                draw_waveform(renderer);
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
        usleep(750);
    }

    save_settings();

    midi_stop(midi_in);

    timer_delete(video_timer);
    timer_delete(audio_timer);

    SDL_DestroyAudioStream(stream);
    SDL_CloseAudioDevice(devId);

    free(buf);

    return 0;
}
