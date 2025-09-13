#include <math.h>
#include <stdlib.h>

#include "slide_controller.h"
#include "text.h"

static void set_marker(struct slide_controller *sc, int x, int y)
{
    int marker_width = 2;
    int marker_height = 2;
    if (sc->width > sc->height)
        marker_height = sc->height;
    else
        marker_width = sc->width;

    sc->marker_points[0].x = x - marker_width / 2;
    sc->marker_points[0].y = y - marker_height / 2;
    sc->marker_points[1].x = x - marker_width / 2;
    sc->marker_points[1].y = y + marker_height / 2;
    sc->marker_points[2].x = x + marker_width / 2;
    sc->marker_points[2].y = y + marker_height / 2;
    sc->marker_points[3].x = x + marker_width / 2;
    sc->marker_points[3].y = y - marker_height / 2;
    sc->marker_points[4].x = x - marker_width / 2;
    sc->marker_points[4].y = y - marker_height / 2;
}

static void linear_control_set(struct linear_control *lc, float setting)
{
    float range = lc->max - lc->min;
    *lc->target = lc->min + (range * setting);
}

void slide_controller_move(struct slide_controller *sc, int x, int y)
{
    // fix offset with text here.
    if (sc->clicked  )
    {
        if (sc->width > sc->height&& x >= sc->x  && x <= (sc->x + sc->width))
        {
            linear_control_set(&sc->control, 1.0 * (x - sc->x) / sc->width);
            set_marker(sc, x, sc->y + sc->height / 2);
        }
        else if (sc->height > sc->width && y >= sc->y && y <= (sc->y + sc->height))
        {
            linear_control_set(&sc->control, 1.0 * (y - sc->y) / sc->height);
            set_marker(sc, sc->x + sc->width / 2, y);
        }
    }
}

void slide_controller_click(struct slide_controller *sc, int x, int y)
{
    if (x >= sc->x && x <= (sc->x + sc->width) && y >= sc->y && y <= (sc->y + sc->height))
    {
        if (sc->width > sc->height)
        {
            linear_control_set(&sc->control, 1.0 * (x - sc->x) / sc->width);
            set_marker(sc, x, sc->y + sc->height / 2);
        }
        else
        {
            linear_control_set(&sc->control, 1.0 * (y - sc->y) / sc->height);
            set_marker(sc, sc->x + sc->width / 2, y);
        }
        sc->clicked = true;
    }
}

void slide_controller_unclick(struct slide_controller *sc)
{
    sc->clicked = false;
}

void slide_controller_draw(SDL_Renderer *renderer, struct slide_controller *sc)
{

    if (sc->width > sc->height)
        text_draw(renderer, sc->label, sc->x, sc->y + sc->height, false);
    else

        text_draw(renderer, sc->label, sc->x + sc->width, sc->y + sc->height, true);

    SDL_SetRenderDrawColor(renderer, 0, 50, 150, 255);
    SDL_RenderLines(renderer, sc->border_points, 5);
    SDL_SetRenderDrawColor(renderer, 250, 50, 0, 255);
    SDL_RenderLines(renderer, sc->marker_points, 5);
}

struct slide_controller *slide_controller_create(int x, int y, int width, int height, struct linear_control control,
                                                 const char *label)
{
    struct slide_controller *sc = malloc(sizeof(*sc));
    int text_margin = 16;
    int mx = 0;
    int my = 0;
    sc->x = x;
    sc->y = y;
    sc->width = width;
    sc->height = height;
    sc->control = control;
    sc->clicked = false;
    sc->label = label;

    // calculate initial marker position
    if (width > height) // x slider
    {
        mx = round(((*control.target - control.min) / (control.max - control.min)) * width + x);
        my = y + height / 2;
    }
    else
    {
        mx = x + width / 2;
        my = round(((*control.target - control.min) / (control.max - control.min)) * height + y);
    }
    set_marker(sc, mx, my);

    sc->border_points[0].x = x;
    sc->border_points[0].y = y;
    sc->border_points[1].x = x + width;
    sc->border_points[1].y = y;
    sc->border_points[2].x = x + width;
    sc->border_points[2].y = y + height;
    sc->border_points[3].x = x;
    sc->border_points[3].y = y + height;
    sc->border_points[4].x = x;
    sc->border_points[4].y = y;

    return sc;
}

void slide_controller_destroy(struct slide_controller *sc)
{
    free(sc);
}
