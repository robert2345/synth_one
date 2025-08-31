#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_render.h>
#include <malloc.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "low_pass_filter.h"
#include "cosine.h"

#define WIDTH (640)
#define HEIGHT (480)
#define X_STEP (5)
#define WAVEFORM_LEN (WIDTH/X_STEP)

#define min(x, y) ((x) < (y) ? x : y)
#define max(x, y) ((x) < (y) ? y : x)

static bool abort = false;

static void pr_sdl_err() {
	fprintf(stderr, "%s", SDL_GetError());
	SDL_ClearError();
}


int octave = -1;

int key= 0; // 0 is off, 1 is a C

unsigned cutoff = 1500;
float resonance = 0.8;

float pianokey_per_scancode[SDL_SCANCODE_COUNT] = {
	[SDL_SCANCODE_Z] = 12,
	[SDL_SCANCODE_S] = 13,
	[SDL_SCANCODE_X] = 14,
	[SDL_SCANCODE_D] = 15,
	[SDL_SCANCODE_C] = 16,
	[SDL_SCANCODE_V] = 17,
	[SDL_SCANCODE_G] = 18,
	[SDL_SCANCODE_B] = 19,
	[SDL_SCANCODE_H] = 20,
	[SDL_SCANCODE_N] = 21,
	[SDL_SCANCODE_J] = 22,
	[SDL_SCANCODE_M] = 23,
	[SDL_SCANCODE_Q] = 24,
	[SDL_SCANCODE_2] = 25,
	[SDL_SCANCODE_W] = 26,
	[SDL_SCANCODE_3] = 27,
	[SDL_SCANCODE_E] = 28,
	[SDL_SCANCODE_R] = 29,
	[SDL_SCANCODE_5] = 30,
	[SDL_SCANCODE_T] = 31,
	[SDL_SCANCODE_6] = 32,
	[SDL_SCANCODE_Y] = 33,
	[SDL_SCANCODE_7] = 34,
	[SDL_SCANCODE_U] = 35,
	[SDL_SCANCODE_I] = 36,
	[SDL_SCANCODE_9] = 37,
	[SDL_SCANCODE_O] = 38,
	[SDL_SCANCODE_0] = 39,

};

float key_to_freq[88] =
{

};

void init_key_to_freq()
{
	for (int key = 0; key < 88; key++)
	{
	key_to_freq[key] =  440.0* pow (2, ((key-49)/12.0));
	}
}



pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int waveform_written = 0;
SDL_FPoint points [WIDTH/X_STEP];
SDL_AudioStream *stream;
char *buf;
long long current_frame = 0;
int buffer_frames;
size_t frame_size;
const SDL_AudioSpec input_spec = {
	.channels = 1, .format = SDL_AUDIO_S16, .freq = 44100};

static size_t calc_frame_size(const SDL_AudioSpec *spec) {
	return spec->channels * SDL_AUDIO_BYTESIZE(spec->format);
}

static float render_pulse(const long long current_frame,
		const SDL_AudioSpec *spec, float freq, float width) {
	int frames_per_period = spec->freq / freq;
	int frames_past_last_period = current_frame % frames_per_period;
	if (frames_past_last_period > (int)(width * frames_per_period))
		return -1.0;
	else
		return 1.0;
}

static float render_sample(const long long current_frame,
		const SDL_AudioSpec *spec) {
	float sample;
	float width = 0.25 + 0.2*cosine_render_sample(current_frame, spec, 0.3);
	sample = 0.8*render_pulse(current_frame, spec, key_to_freq[key], width);
	if (!key) sample = 0.0;
	sample = low_pass_filter_get_output(sample);
	return sample;
}

static void write_sample(float sample, char **buf, const SDL_AudioSpec *spec) {
	int16_t *out = (int16_t *)*buf;
	*out = (int16_t)(sample * 0x7FFF);
	*buf += 2;
}

static bool render_sample_frames(long long *current_frame, int frames,
		char *buf, const SDL_AudioSpec *spec) {
	int s, c = 0;
	float sample;
	for (s = 0; s < frames; s++) {

		if (*current_frame%1000 == 0) {
			int cut_freq =  max(100, cutoff + 110 * cosine_render_sample(*current_frame, spec, 0.1));
			low_pass_filter_configure(cut_freq, resonance, spec->freq);
		}


		// what is going on with channels here? only one buffer so it seems a bit broken if multiple channels.
		for (c = 0; c < spec->channels; c++) {
			sample = render_sample(*current_frame, spec);
			write_sample(sample, &buf, spec);
		}

		// write to visualisation buffer
		{
			int samples_per_period = spec->freq / key_to_freq[key];
			bool on_grid = (*current_frame % (2 * samples_per_period / WAVEFORM_LEN)== 0) && waveform_written < WAVEFORM_LEN && waveform_written != 0;
			bool first_sample = ((*current_frame % samples_per_period) == 0) && (waveform_written == 0);
			if (first_sample || on_grid) {
				points[waveform_written].y = HEIGHT/2 + HEIGHT/2 * sample;
				waveform_written++;
			}
		}

		*current_frame += 1;
	}

	return true;
}
static unsigned calc_frames_queued(SDL_AudioStream *stream,
		const SDL_AudioSpec *spec) {
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
	if (!SDL_PutAudioStreamData(stream, buf, frame_size * frames)) {
		pr_sdl_err();
	}
	pthread_mutex_unlock(&mutex);


}

static void trigger_draw_video_event(union sigval)
{
	SDL_Event user_event;
	SDL_zero(user_event);  /* SDL will copy this entire struct! Initialize to keep memory checkers happy. */
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
	if (waveform_written == WAVEFORM_LEN) {
		SDL_SetRenderDrawColor(renderer, 0,0,0,255);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer, 255,255,255,255);
		SDL_RenderLines(renderer, points, WIDTH/X_STEP);
		SDL_RenderPresent(renderer);
		waveform_written = 0;
	}
	pthread_mutex_unlock(&mutex);


}

static void sig_handler(int signum) { printf("ABORT!\n"); abort = true; }

static int setup_audio_timer()
{
	struct sigevent sevnt = { .sigev_notify = SIGEV_THREAD, .sigev_notify_function = fill_audio_buffer };
	timer_t t;
	struct itimerspec new_value = {.it_interval = {.tv_nsec = buffer_frames / 8 * 1000000000ull / input_spec.freq}};
	new_value.it_value = new_value.it_interval;

	int ret = timer_create(CLOCK_MONOTONIC, &sevnt, &t);
	if (ret) {
		perror("Failed to create audio timer!");
		return -1;
	}

	timer_settime( t, 0, &new_value, NULL);

	return 0;
}

static int setup_video_timer()
{
	int i;
	struct sigevent sevnt = { .sigev_notify = SIGEV_THREAD, .sigev_notify_function = trigger_draw_video_event };
	timer_t t;

	// initialize the points that shall be drawn
	for (i = 0; i < WAVEFORM_LEN; i++)
		points[i].x = i*X_STEP;


	struct itimerspec new_value = {.it_interval = {.tv_nsec = 1000000000ull / 8}};
	new_value.it_value = new_value.it_interval;

	int ret = timer_create(CLOCK_MONOTONIC, &sevnt, &t);
	if (ret) {
		perror("Failed to create video timer!");
		return -1;
	}

	timer_settime( t, 0, &new_value, NULL);

	return 0;
}

int main(int argc, char **argv) {
	SDL_AudioDeviceID devId;
	bool res;
	SDL_AudioSpec output_spec;
	int sample_frames;
	SDL_Window *window;

	if (!SDL_Init(SDL_INIT_EVENTS |SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
		pr_sdl_err();
		return -2;
	}

	signal(SIGINT, sig_handler);

	// VIDEO STUFF
	window = SDL_CreateWindow(
			"Synth One",                  // window title
			WIDTH,                               // width, in pixels
			480,                               // height, in pixels
			SDL_WINDOW_OPENGL                  // flags - see below
			);
	if (!window) {
		pr_sdl_err();
		return -1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window,NULL);
	if (!renderer) {
		pr_sdl_err();
		return 1;
	}

	if (res = setup_video_timer())
		return res;


	//AUDIO STUFF
	
	init_key_to_freq();

	devId = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
	if (!devId) {
		pr_sdl_err();
		return 2;
	}

	if (!SDL_GetAudioDeviceFormat(devId, &output_spec, &sample_frames)) {
		pr_sdl_err();
		return 3;
	}
	buffer_frames = sample_frames * 2;
	frame_size = calc_frame_size(&input_spec);
	printf("Frame size %ld\n", frame_size);
	buf = malloc(buffer_frames * frame_size);

	printf("Audiodriver %s, id %u, channels %d, freq %d, frames %d, \n",
			SDL_GetCurrentAudioDriver(), devId, output_spec.channels,
			output_spec.freq, sample_frames);


	if (!(stream = SDL_CreateAudioStream(&input_spec, &output_spec))) {
		pr_sdl_err();
		return 4;
	}

	if (!SDL_BindAudioStream(devId, stream)) {
		pr_sdl_err();
		return 5;
	}

	// render 2xsample_frames
	pthread_mutex_lock(&mutex);
	render_sample_frames(&current_frame, buffer_frames, buf, &input_spec);

	// write to stream
	if (!SDL_PutAudioStreamData(stream, buf, frame_size * buffer_frames)) {
		pr_sdl_err();
		return 6;
	}

	if (!SDL_ResumeAudioStreamDevice(stream)) {
		pr_sdl_err();
		return 7;
	}
	pthread_mutex_unlock(&mutex);

	if (res = setup_audio_timer())
		return res;

	SDL_Event event;
	while (!abort) {
		if (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_KEY_DOWN) {
				if (event.key.scancode == SDL_SCANCODE_UP)
						cutoff = min (20000, cutoff + 500);
				else if (event.key.scancode == SDL_SCANCODE_DOWN) {
						int reduction = min (500, cutoff);
						cutoff = max (100, cutoff - reduction);
				} else {
					key = pianokey_per_scancode[event.key.scancode];
					if (key != 0)
						key += 12*octave;
				}
			} else if(event.type == SDL_EVENT_KEY_UP) {
				if (key == 12*octave +pianokey_per_scancode[event.key.scancode])
					key = 0;
			} else if (event.type == SDL_EVENT_USER) {
				draw_waveform(renderer);
			}
		}
	}


	SDL_DestroyAudioStream(stream);
	SDL_CloseAudioDevice(devId);

	return 0;
}
