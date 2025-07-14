#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <malloc.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static bool abort = false;

static void pr_sdl_err() {
  fprintf(stderr, "%s", SDL_GetError());
  SDL_ClearError();
}

static size_t calc_frame_size(const SDL_AudioSpec *spec) {
  return spec->channels * SDL_AUDIO_BYTESIZE(spec->format);
}

struct trigger {
	char note;
	float freq;
	long long start_frame;
	long long stop_frame; // 0 if not set yet
};

#define MAX_TRIGGERS (1)

struct trigger triggers[MAX_TRIGGERS] = {};


static void create_note(char note, long long current_frame);
{
	int i;
	for (i = 0; i < MAX_TRIGGERS; i++)
	{
		if (stop_frame && stop_frame <= current_frame) {
			triggers[i].note = c;
			triggers[i].start_frame = current_frame;
			triggers[i].stop_frame = current_frame + 410;
			triggers[i].freq = note_to_freq(c);
		}

	}

}

/*
 * width - [0.0 1.0]
 * freq - integer Hz
 * current_frame - incremental frame number */
static float render_pulse(const long long current_frame,
                          const SDL_AudioSpec *spec, float freq, float width) {
  int frames_per_period = spec->freq / freq;
  int frames_past_last_period = current_frame % frames_per_period;

  if (width > 1.0)
    width = 1.0;
  if (width < 0.0)
    width = 0.0;

  if (frames_past_last_period > (int)(width * frames_per_period))
    return -1.0;
  else
    return 1.0;
}
static float render_sine(const long long current_frame,
                         const SDL_AudioSpec *spec, float freq) {
  float time = current_frame * 1.0 / spec->freq;
  return 1.0 - 0.4 * (1.0 + cos(freq * 2 * M_PI * time));
}

static float render_sample(const long long current_frame,
                           const SDL_AudioSpec *spec) {
  float width = 0.5 * render_sine(current_frame, spec, 0.3);
  return render_pulse(current_frame, spec, 150, width);
}

static void write_sample(float sample, char **buf, const SDL_AudioSpec *spec) {
  int16_t *out = (int16_t *)*buf;
  *out = (int16_t)sample * 0x7FFF;
  // printf("out 0x%hx\n", *out);
  *buf += 2;
}

static bool render_sample_frames(long long *current_frame, int frames,
                                 char *buf, const SDL_AudioSpec *spec) {
  int s, c = 0;
  float sample;
  for (s = 0; s < frames; s++) {
    for (c = 0; c < spec->channels; c++) {
      sample = render_sample(*current_frame, spec); // sin(2*pi*f*t)
      write_sample(sample, &buf, spec);
      *current_frame += 1;
    }
  }
  // printf("Render sample frames, last value %f current frame %lld, frames to
  // render%d\n", sample, *current_frame, frames);

  return true;
}
static unsigned calc_frames_queued(SDL_AudioStream *stream,
                                   const SDL_AudioSpec *spec) {
  int bytes_queued = SDL_GetAudioStreamQueued(stream);
  return bytes_queued / calc_frame_size(spec);
}

static void sig_handler(int signum) { abort = true; }

int main(int argc, char **argv) {
  SDL_AudioDeviceID devId;
  bool res;
  const SDL_AudioSpec input_spec = {
      .channels = 1, .format = SDL_AUDIO_S16, .freq = 44100};
  SDL_AudioSpec output_spec;
  int sample_frames;
  SDL_AudioStream *stream;
  char *buf;
  long long current_frame = 0;
  int buffer_frames;
  int frames;
  size_t frame_size;

  if (!SDL_Init(SDL_INIT_AUDIO)) {
    pr_sdl_err();
    return 1;
  }

  signal(SIGINT, sig_handler);
  signal(SIGINT, sig_handler);

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
  frames = buffer_frames;
  render_sample_frames(&current_frame, frames, buf, &input_spec);
  //
  // write to stream
  if (!SDL_PutAudioStreamData(stream, buf, frame_size * frames)) {
    pr_sdl_err();
    return 6;
  }

  if (!SDL_ResumeAudioStreamDevice(stream)) {
    pr_sdl_err();
    return 7;
  }

  // wait for time it takes to play sample_frames
  for (int i = 0; i < 100; i++) {
    usleep(buffer_frames / 2 * 1000000 / input_spec.freq);
    // check how much is in buffer
    // render rest
    frames = buffer_frames - calc_frames_queued(stream, &input_spec);
    render_sample_frames(&current_frame, frames, buf, &input_spec);
    if (!SDL_PutAudioStreamData(stream, buf, frame_size * frames)) {
      pr_sdl_err();
      return 8;
    }
    if (abort)
      break;
  }

  SDL_DestroyAudioStream(stream);
  SDL_CloseAudioDevice(devId);

  return 0;
}
