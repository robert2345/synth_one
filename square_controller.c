#include <stdlib.h>

#include "square_controller.h"
#include "text.h"
#include <math.h>

static void set_marker(struct square_controller *sc, int x, int y)
{
    static const int marker_width = 4;

    sc->marker_points[0].x = x - marker_width / 2.0;
    sc->marker_points[0].y = y - marker_width / 2.0;
    sc->marker_points[1].x = x - marker_width / 2.0;
    sc->marker_points[1].y = y + marker_width / 2.0;
    sc->marker_points[2].x = x + marker_width / 2.0;
    sc->marker_points[2].y = y + marker_width / 2.0;
    sc->marker_points[3].x = x + marker_width / 2.0;
    sc->marker_points[3].y = y - marker_width / 2.0;
    sc->marker_points[4].x = x - marker_width / 2.0;
    sc->marker_points[4].y = y - marker_width / 2.0;
}

static void linear_control_set(struct linear_control *lc, float setting)
{
    float range = lc->max - lc->min;
    *lc->target = lc->min + (range * setting);
}

void square_controller_move(struct square_controller *sc, int x, int y)
{
    // fix offset with text here.
    if (sc->clicked && x >= sc->x && x <= (sc->x + sc->width) && y >= sc->y && y <= (sc->y + sc->height))
    {
        linear_control_set(&sc->x_control, 1.0 * (x - sc->x) / sc->width);
        linear_control_set(&sc->y_control, 1.0 * (y - sc->y) / sc->height);
        set_marker(sc, x, y);
    }
}

void square_controller_click(struct square_controller *sc, int x, int y)
{
    if (x >= sc->x && x <= (sc->x + sc->width) && y >= sc->y && y <= (sc->y + sc->height))
    {
        linear_control_set(&sc->x_control, 1.0 * (x - sc->x) / sc->width);
        linear_control_set(&sc->y_control, 1.0 * (y - sc->y) / sc->height);
        set_marker(sc, x, y);
        sc->clicked = true;
    }
}

void square_controller_unclick(struct square_controller *sc)
{
    sc->clicked = false;
}

void square_controller_draw(SDL_Renderer *renderer, struct square_controller *sc)
{
    text_draw(renderer, sc->x_label, sc->x, sc->y + sc->height, false);
    text_draw(renderer, sc->y_label, sc->x + sc->width, sc->y + sc->height, true);

    SDL_SetRenderDrawColor(renderer, 0, 50, 150, 255);
    SDL_RenderLines(renderer, sc->border_points, 5);
    SDL_SetRenderDrawColor(renderer, 250, 50, 0, 255);
    SDL_RenderLines(renderer, sc->marker_points, 5);
}

struct square_controller *square_controller_create(int x, int y, int width, int height, struct linear_control x_control,
                                                   struct linear_control y_control, const char *x_label,
                                                   const char *y_label)
{
    struct square_controller *sc = malloc(sizeof(*sc));
    int text_margin = 16;
    int mx = 0;
    int my = 0;
    sc->x = x;
    sc->y = y;
    sc->width = width;
    sc->height = height;
    sc->x_control = x_control;
    sc->y_control = y_control;
    sc->clicked = false;
    sc->x_label = x_label;
    sc->y_label = y_label;

    // calculate initial marker position
    mx = round(((*x_control.target - x_control.min) / (x_control.max - x_control.min)) * width + x);
    my = round(((*y_control.target - y_control.min) / (y_control.max - y_control.min)) * height + y);
    set_marker(sc, mx, my);

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

void square_controller_destroy(struct square_controller *sc)
{
    free(sc);
}
