#include <pthread.h>
#include <signal.h>
#include <stdio.h>

#include "text.h"

#define NBR_STEPS (16)

#define MARGIN 2

struct step
{
    int key;
    SDL_FPoint points[5];
};

static struct step steps[NBR_STEPS];
static SDL_FPoint big_square[5];

int *(*note_change_cb)(int on_key, int off_key);

static int step_idx = 0;
static timer_t sequencer_timer;
static bool edit = false;
static bool run = false;
static bool running = false;
static float bpm = 120.0;
static int steps_per_beat = 4;

#define LABEL_LEN (3)

void sequencer_draw(SDL_Renderer *renderer)
{
    int i = 0;
    for (i = 0; i < NBR_STEPS; i++)
    {
        struct step *step = &steps[i];
        char label[LABEL_LEN];
        snprintf(label, LABEL_LEN, "%u", steps[i].key);

        text_draw(renderer, label, step->points[4].x + MARGIN, step->points[4].y + MARGIN, false);
        if (step_idx == i)
            SDL_SetRenderDrawColor(renderer, 250, 50, 0, 255);
        else
            SDL_SetRenderDrawColor(renderer, 0, 50, 150, 255);
        SDL_RenderLines(renderer, step->points, 5);
    }
    if (edit)
    {
        SDL_SetRenderDrawColor(renderer, 250, 50, 0, 255);
        SDL_RenderLines(renderer, big_square, 5);
    }
}

static void step(union sigval)
{
    if (run)
    {
        int off_key = steps[step_idx].key;
        step_idx = (step_idx + 1) % NBR_STEPS;
        int on_key = steps[step_idx].key;
        note_change_cb(on_key, off_key);
    }
    else if (running)
    {
        int off_key = steps[step_idx].key;
        int on_key = 0;
        step_idx = (step_idx + 1) % NBR_STEPS;
        running = false;
        note_change_cb(on_key, off_key);
    }
}

static int setup_timer()
{
    struct sigevent sevnt = {.sigev_notify = SIGEV_THREAD, .sigev_notify_function = step};

    float steps_per_second = bpm / 60 * steps_per_beat;
    struct itimerspec new_value = {.it_interval = {.tv_nsec = 1000000000 / steps_per_second}};
    new_value.it_value = new_value.it_interval;

    int ret = timer_create(CLOCK_MONOTONIC, &sevnt, &sequencer_timer);
    if (ret)
    {
        perror("Failed to create sequencer timer!");
        return -1;
    }

    timer_settime(sequencer_timer, 0, &new_value, NULL);

    return 0;
}

void sequencer_init(int *(*callback)(int on_key, int off_key))
{
    int i = 0;
    int x = 350;
    int y = 350;
    const int width = MARGIN * 2 + 2 * text_get_width();
    const int height = MARGIN * 2 + text_get_height();
    const int spacing = 2;

    note_change_cb = callback;

    big_square[0].x = x - MARGIN;
    big_square[0].y = y - MARGIN;
    big_square[1].x = x + NBR_STEPS * (width + spacing);
    big_square[1].y = y - MARGIN;
    big_square[2].x = x + NBR_STEPS * (width + spacing);
    big_square[2].y = y + height + MARGIN;
    big_square[3].x = x - MARGIN;
    big_square[3].y = y + height + MARGIN;
    big_square[4].x = x - MARGIN;
    big_square[4].y = y - MARGIN;

    for (i = 0; i < NBR_STEPS; i++)
    {
        steps[i].key = 1 + ((i + 1) % 3 == 0) * 5 + ((i + 2) % 3 == 0) * 12;
        steps[i].points[0].x = x;
        steps[i].points[0].y = y;
        steps[i].points[1].x = x + width;
        steps[i].points[1].y = y;
        steps[i].points[2].x = x + width;
        steps[i].points[2].y = y + height;
        steps[i].points[3].x = x;
        steps[i].points[3].y = y + height;
        steps[i].points[4].x = x;
        steps[i].points[4].y = y;

        x += width + spacing;
    }
    setup_timer();
}

void sequencer_toggle_run()
{
    run = !run;
}

void sequencer_toggle_edit()
{
    edit = !edit;
}

void sequencer_input(int key)
{
    if (edit)
    {
        steps[step_idx].key = key;
        step_idx = (step_idx + 1) % NBR_STEPS;
    }
}
