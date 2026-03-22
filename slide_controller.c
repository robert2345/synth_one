#include <math.h>
#include <stdio.h>
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
    float value = lc->min + (range * setting);
    if (lc->quantized_to_int)
    {
        value = round(value);
    }
    *lc->target = value;
}

void slide_controller_relocate(struct slide_controller *sc, int x, int y, int width, int height)
{
    sc->x = x;
    sc->y = y;
    sc->width = width;
    sc->height = height;
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
    slide_controller_set_pos_from_value(sc);
}
void slide_controller_set_pos_from_value(struct slide_controller *sc)
{
    int mx, my;
    struct linear_control *control = &sc->control;
    if (sc->width > sc->height) // x slider
    {
        mx = round(((*control->target - control->min) / (control->max - control->min)) * sc->width + sc->x);
        my = sc->y + sc->height / 2;
    }
    else
    {
        mx = sc->x + sc->width / 2;
        my = round(((*control->target - control->min) / (control->max - control->min)) * sc->height + sc->y);
    }
    set_marker(sc, mx, my);
}

void slide_controller_move(struct slide_controller *sc, int x, int y)
{
    // fix offset with text here.
    if (sc->clicked)
    {
        if (sc->width > sc->height && x >= sc->x && x <= (sc->x + sc->width))
        {
            linear_control_set(&sc->control, 1.0 * (x - sc->x) / sc->width);
        }
        else if (sc->height > sc->width && y >= sc->y && y <= (sc->y + sc->height))
        {
            linear_control_set(&sc->control, 1.0 * (y - sc->y) / sc->height);
        }
        slide_controller_set_pos_from_value(sc);
    }
}

void slide_controller_click(struct slide_controller *sc, int x, int y)
{
    if (x >= sc->x && x <= (sc->x + sc->width) && y >= sc->y && y <= (sc->y + sc->height))
    {
        if (sc->width > sc->height)
        {
            linear_control_set(&sc->control, 1.0 * (x - sc->x) / sc->width);
        }
        else
        {
            linear_control_set(&sc->control, 1.0 * (y - sc->y) / sc->height);
        }
        slide_controller_set_pos_from_value(sc);
        sc->clicked = true;
    }
}

void slide_controller_unclick(struct slide_controller *sc)
{
    sc->clicked = false;
}

void slide_controller_draw(SDL_Renderer *renderer, struct slide_controller *sc)
{
#define VAL_SIZE 10
    char val_text[VAL_SIZE + 1];
    int label_len = strlen(sc->label);
    if (sc->control.show_num)
    {
        int n = 0;
        if (sc->control.quantized_to_int)
            n = snprintf(val_text, VAL_SIZE, "%-.f", *sc->control.target);
        else
            n = snprintf(val_text, VAL_SIZE, "%-.2f", *sc->control.target);
        if (n > VAL_SIZE)
            fprintf(stderr, "Value label too small!\n");
    }

    if (sc->width > sc->height)
    {
        text_draw(renderer, sc->label, sc->x, sc->y + sc->height, false);
        if (sc->control.show_num)
        {
            text_draw(renderer, val_text, sc->x + (label_len + 1) * text_get_width(), sc->y + sc->height, false);
        }
    }
    else
    {
        text_draw(renderer, sc->label, sc->x + sc->width, sc->y + sc->height, true);
        if (sc->control.show_num)
        {
            text_draw(renderer, val_text, sc->x, sc->y + (label_len + 1) * text_get_height() + sc->height, false);
        }
    }

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
    sc->control = control;
    sc->clicked = false;
    sc->label = label;

    slide_controller_relocate(sc, x, y, width, height);

    // calculate initial marker position
    slide_controller_set_pos_from_value(sc);

    return sc;
}

void slide_controller_destroy(struct slide_controller *sc)
{
    free(sc);
}
