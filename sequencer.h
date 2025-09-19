#pragma once
#include <SDL3/SDL_render.h>
void sequencer_init();
void sequencer_draw(SDL_Renderer *renderer);
void sequencer_toggle_run();
void sequencer_toggle_edit();
void sequencer_input(int key);
