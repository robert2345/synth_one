#pragma once
#include <SDL3/SDL_render.h>
void sequencer_init(int x, int y, int w, int h, void (*callback)(int on_key, int off_key));
void sequencer_relocate(int x, int y, int w, int h);
void sequencer_draw(SDL_Renderer *renderer);
void sequencer_toggle_run();
void sequencer_toggle_edit();
void sequencer_input(int key);
