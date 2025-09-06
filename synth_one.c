#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_scancode.h>
#include <malloc.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "cosine.h"
#include "delay.h"
#include "distortion.h"
#include "envelope.h"
#include "low_pass_filter.h"
#include "slide_controller.h"
#include "square_controller.h"
#include "text.h"

#define WIDTH (640)
#define HEIGHT (480)
#define X_STEP (5)
#define WAVEFORM_LEN (WIDTH / X_STEP)

#define min(x, y) ((x) < (y) ? x : y)
#define max(x, y) ((x) < (y) ? y : x)

#define MAX_WIDTH (0.5)
#define MIN_WIDTH (0.1)

#define MAX_DELAY_MS (750)

static bool abort = false;

static void pr_sdl_err()
{
    fprintf(stderr, "%s", SDL_GetError());
    SDL_ClearError();
}

int octave = 1;

int key = 0; // 0 is off, 1 is a C
bool key_has_been_released = true;

float cutoff = 1500;
float resonance = 0.8;

float base_width = MAX_WIDTH;
float pwm_freq = 1.0;

float bend = 1.0;
float bend_target = 1.0;

float dist_level = 0.6;
float flip_level = 0.8;

float env_to_cutoff = 100;
float env_to_amp = 1.0;

float delay_ms = 100.0;

struct square_controller *sqc_arr[5] = {};
struct slide_controller *slc_arr[2] = {};

float pianokey_per_scancode[SDL_SCANCODE_COUNT] = {
    [SDL_SCANCODE_Z] = 12, [SDL_SCANCODE_S] = 13, [SDL_SCANCODE_X] = 14, [SDL_SCANCODE_D] = 15, [SDL_SCANCODE_C] = 16,
    [SDL_SCANCODE_V] = 17, [SDL_SCANCODE_G] = 18, [SDL_SCANCODE_B] = 19, [SDL_SCANCODE_H] = 20, [SDL_SCANCODE_N] = 21,
    [SDL_SCANCODE_J] = 22, [SDL_SCANCODE_M] = 23, [SDL_SCANCODE_Q] = 24, [SDL_SCANCODE_2] = 25, [SDL_SCANCODE_W] = 26,
    [SDL_SCANCODE_3] = 27, [SDL_SCANCODE_E] = 28, [SDL_SCANCODE_R] = 29, [SDL_SCANCODE_5] = 30, [SDL_SCANCODE_T] = 31,
    [SDL_SCANCODE_6] = 32, [SDL_SCANCODE_Y] = 33, [SDL_SCANCODE_7] = 34, [SDL_SCANCODE_U] = 35, [SDL_SCANCODE_I] = 36,
    [SDL_SCANCODE_9] = 37, [SDL_SCANCODE_O] = 38, [SDL_SCANCODE_0] = 39,

};

float key_to_freq[88] = {

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
int buffer_frames;
size_t frame_size;
const SDL_AudioSpec input_spec = {.channels = 1, .format = SDL_AUDIO_S16, .freq = 44100};

static size_t calc_frame_size(const SDL_AudioSpec *spec)
{
    return spec->channels * SDL_AUDIO_BYTESIZE(spec->format);
}

static float render_pulse(const long long current_frame, const SDL_AudioSpec *spec, float freq, float width,
                          bool *new_period)
{
    static float period_position = 0.0;
    period_position += freq / spec->freq;
    if (period_position > 1.0)
    {
        period_position -= 1.0;
        *new_period = true;
    }
    else
    {
        *new_period = false;
    }

    if (period_position > width)
        return -1.0;
    else
        return 1.0;
}

static float render_sample(const long long current_frame, const SDL_AudioSpec *spec, bool *new_period)
{
    float sample;
    float width = base_width + 0.1 * cosine_render_sample(current_frame, spec, pwm_freq);

    width = max(MIN_WIDTH, width);
    width = min(MAX_WIDTH, width);
    sample = 0.3 * render_pulse(current_frame, spec, bend * key_to_freq[key], width, new_period);

    // envelope
    sample = sample * (1.0 - env_to_amp) + sample * env_to_amp * env_to_amp * envelope_get(current_frame);

    // filter
    sample = low_pass_filter_get_output(sample);

    // distort

    sample = distort(sample, dist_level, flip_level);

    // echo
    sample += 0.2 * delay_get_sample(delay_ms, spec);
    delay_put_sample(sample);

    // chorus
    float chorus_delay_ms = 3.0 + 1.0 * cosine_render_sample(current_frame, spec, 3);
    sample += 0.2 * delay_get_sample(chorus_delay_ms, spec);

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

        if (*current_frame % 1000 == 0)
        {
            int cut_freq = max(100, cutoff + env_to_cutoff * envelope_get(*current_frame) +
                                        110 * cosine_render_sample(*current_frame, spec, 0.1));
            low_pass_filter_configure(cut_freq, resonance, spec->freq);
        }

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
            float bend_diff = bend_target - bend;
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
                bend = bend_target;
            }
        }

        // write to visualisation buffer
        {
            int samples_per_period = spec->freq / (bend * key_to_freq[key]);
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
    int frames = buffer_frames - calc_frames_queued(stream, &input_spec);
    render_sample_frames(&current_frame, frames, buf, &input_spec);
    if (!SDL_PutAudioStreamData(stream, buf, frame_size * frames))
    {
        pr_sdl_err();
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
    abort = true;
}

static int setup_audio_timer(timer_t *t)
{
    struct sigevent sevnt = {.sigev_notify = SIGEV_THREAD, .sigev_notify_function = fill_audio_buffer};
    struct itimerspec new_value = {.it_interval = {.tv_nsec = buffer_frames / 8 * 1000000000ull / input_spec.freq}};
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
    int sample_frames;
    SDL_Window *window;
    timer_t audio_timer;
    timer_t video_timer;

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_AUDIO | SDL_INIT_VIDEO))
    {
        pr_sdl_err();
        return -2;
    }

    signal(SIGINT, sig_handler);

    // VIDEO STUFF
    window = SDL_CreateWindow("Synth One",      // window title
                              WIDTH,            // width, in pixels
                              480,              // height, in pixels
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

    sqc_arr[0] = square_controller_create(10, 10, 100, 100, (struct linear_control){&resonance, 0.9, 0.01},
                                          (struct linear_control){&cutoff, input_spec.freq / 2.5, 50}, "RES", "CUTOFF");
    sqc_arr[1] = square_controller_create(230, 10, 100, 100, (struct linear_control){&base_width, MIN_WIDTH, MAX_WIDTH},
                                          (struct linear_control){&pwm_freq, 0.1, 10.0}, "PWDTH", "MOD FRQ");
    sqc_arr[2] = square_controller_create(450, 10, 100, 100, (struct linear_control){&dist_level, 0.1, 0.99},
                                          (struct linear_control){&flip_level, 0.1, 0.99}, "DIST", "CLIP");
    sqc_arr[3] = square_controller_create(10, HEIGHT - 130, 100, 100, (struct linear_control){&env_to_amp, 0.0, 1.0},
                                          (struct linear_control){&env_to_cutoff, 0.0, 110}, "TO AMP", "ENV TO CUT");

    slc_arr[0] =
        slide_controller_create(130, HEIGHT - 130, 10, 100, (struct linear_control){&delay_ms, 0.5, MAX_DELAY_MS}, "DELAY MS");

    text_init(renderer);

    if (res = setup_video_timer(&video_timer))
        return res;

    // AUDIO STUFF

    init_key_to_freq();

    delay_init(&input_spec, MAX_DELAY_MS);

    envelope_init(10, 40, 0.7, 100, &input_spec);

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
    while (!abort)
    {
        if (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                int new_key = pianokey_per_scancode[event.key.scancode];
                if (new_key != 0)
                {
                    new_key += 12 * octave;
                    if (new_key != key && key_has_been_released)
                    {
                        envelope_start(current_frame);
                        key_has_been_released = false;
                    }
                    key = new_key;
                }
            }
            else if (event.type == SDL_EVENT_KEY_UP)
            {
                if (key == 12 * octave + pianokey_per_scancode[event.key.scancode])
                {
                    envelope_release(current_frame);
                    key_has_been_released = true;
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
    }

    timer_delete(video_timer);
    timer_delete(audio_timer);

    SDL_DestroyAudioStream(stream);
    SDL_CloseAudioDevice(devId);

    free(buf);

    return 0;
}
