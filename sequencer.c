#include <pthread.h>
#include <signal.h>
#include <stdio.h>

#include "text.h"

#define NBR_STEPS (16)

#define MARGIN 2

struct step
{
    int key;
    char label[3];
    SDL_FPoint points[5];
};

static struct step steps[NBR_STEPS];

int *(*note_change_cb)(int on_key, int off_key);

static int step_idx = 0;
static const int step_time_ms = 400;
static timer_t sequencer_timer;
static bool run = false;
static bool running = false;

void sequencer_draw(SDL_Renderer *renderer)
{
    int i = 0;
    for (i = 0; i < NBR_STEPS; i++)
    {
        struct step *step = &steps[i];

        text_draw(renderer, step->label, step->points[4].x + MARGIN, step->points[4].y + MARGIN, false);
        if (step_idx == i)
            SDL_SetRenderDrawColor(renderer, 250, 50, 0, 255);
        else
            SDL_SetRenderDrawColor(renderer, 0, 50, 150, 255);
        SDL_RenderLines(renderer, step->points, 5);
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

    struct itimerspec new_value = {.it_interval = {.tv_nsec = step_time_ms * 1000000ull}};
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

    for (i = 0; i < NBR_STEPS; i++)
    {
        steps[i].key = 1 + ((i + 1) % 3 == 0) * 5 + ((i + 2) % 3 == 0) * 12;
        snprintf(steps[i].label, 3, "%u", steps[i].key);
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

void sequencer_toggle()
{
    run = !run;
}
