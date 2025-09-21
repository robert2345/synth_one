#include "ball.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int win_width;
static int win_height;
static int waveform_stepsize;

struct ball_state
{
    float x;
    float y;
    float y_speed;
    float x_speed;
    long long previous_frame;
};

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

void ball_init(int width, int height, int stepsize)
{
    win_width = width;
    win_height = height;
    waveform_stepsize = stepsize;
}

struct ball_state *ball_create(float x, float y, float x_speed, float y_speed, long long current_frame)
{
    struct ball_state *bs = malloc(sizeof(*bs));
    if (!bs)
    {
        fprintf(stderr, "Failed to allocate memory for ball\n");
        return NULL;
    }
    bs->x = x;
    bs->y = y;
    bs->x_speed = x_speed;
    bs->y_speed = y_speed;
    bs->previous_frame = current_frame;
    return bs;
}

void ball_destroy(struct ball_state **bs)
{
    if (!bs)
        fprintf(stderr, "Can't destroy a null pointer.\n");
    if (*bs)
        free(*bs);
    *bs = NULL;
}

void ball_draw(SDL_Renderer *renderer, struct ball_state *bs, SDL_FPoint *waveform, long long current_frame)
{
    int frames_past = max(current_frame - bs->previous_frame, 1);
    bs->previous_frame = current_frame;

    bs->y += bs->y_speed * frames_past;
    bs->x += bs->x_speed * frames_past;
    bs->y_speed += 0.005;

    if (bs->x > (win_width - 1))
    {
        bs->x = (win_width - 1) - (bs->x - (win_width - 1));
        bs->x_speed = -bs->x_speed;
    }
    else if (bs->x < 0)
    {
        bs->x = -bs->x;
        bs->x_speed = -bs->x_speed;
    }

    // Think about how come x can still be out of bounds after the bounce...
    bs->x = max(min((win_width - 1), bs->x), 0);

    float wave_y = waveform[((int)round(bs->x)) / waveform_stepsize].y;

    if (bs->y >= wave_y)
    {
        int x_low = floor(bs->x);
        int x_high = ceil(bs->x);
        if (x_low == x_high)
        {
            if (x_low > (win_width / 2))
                x_low -= 1;
            else
                x_high += 1;
        }
        double y_line = waveform[x_high].y - waveform[x_low].y;

        bs->y_speed += min(0, (wave_y - bs->y) / frames_past);
        float wave_angle = atan(y_line);
        float speed_angle;
        if (bs->x_speed > 0)
            speed_angle = atan(bs->y_speed / bs->x_speed);
        else if (bs->x_speed < 0)
            speed_angle = M_PI - atan(bs->y_speed / -bs->x_speed);
        else
            speed_angle = M_PI / 2;
        float out_angle = 2 * wave_angle - speed_angle;

        float speed_magnitude = 0.7 * sqrt(bs->y_speed * bs->y_speed + bs->x_speed * bs->x_speed);

        bs->y_speed = sin(out_angle) * speed_magnitude;
        bs->x_speed = cos(out_angle) * speed_magnitude;

        bs->y = wave_y - 0.00001;
    }

    SDL_SetRenderDrawColor(renderer, 10, 255, 10, 255);
    SDL_RenderPoint(renderer, bs->x, bs->y);
    SDL_RenderPoint(renderer, bs->x, bs->y + 1);
    SDL_RenderPoint(renderer, bs->x + 1, bs->y + 1);
    SDL_RenderPoint(renderer, bs->x + 1, bs->y);
}
