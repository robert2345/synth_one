#include <SDL3/SDL_audio.h>
#include "delay.h"

static int pos = 0;
static float *buffer; // ringbuffer with pos as last entered sample
static int delay_buffer_len = 0;

int delay_init(const SDL_AudioSpec *spec, unsigned max_len_ms)
{
	delay_buffer_len = spec->freq*max_len_ms/1000;
	buffer = calloc(delay_buffer_len, sizeof(float));
	if (!buffer) {
		printf("Failed to allocate ringbuffer for delay!\n");
		return -1;
	}
	return 0;
}

float delay_get_sample(float delay_ms, const SDL_AudioSpec *spec)
{
	int delay_samples = spec->freq*delay_ms/1000;
	if (delay_samples >= delay_buffer_len) {
		printf("Too large delay!");
	}

	int ret_pos = pos - delay_samples;
	if(ret_pos < 0)
		ret_pos = delay_buffer_len + ret_pos;

	return buffer[ret_pos];
}

void delay_put_sample(float sample)
{
	pos+=1;
	pos = pos % delay_buffer_len;
	buffer[pos] = sample;
}
