#include <stdio.h>

#include "text.h"

static SDL_Surface *text_surface;
static SDL_Texture *text_texture;

void text_init(SDL_Renderer *renderer)
{
    text_surface = SDL_LoadBMP("ascii.bmp");
    if (!text_surface)
    {
        printf("Failed to load font image\n");
    }
    text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
    if (!text_texture)
    {
        printf("Failed to create texture\n");
    }
}

void text_exit()
{
    SDL_DestroyTexture(text_texture);
    SDL_DestroySurface(text_surface);
}

void text_draw(SDL_Renderer *renderer, const char *text, int x, int y, bool vertical)
{
    int i;
    char c;
    SDL_FRect dstrect = {.x = x, .y = y, .w = 16, .h = 16};
    SDL_FRect srcrect = {.x = x, .y = y, .w = 16, .h = 16};

    if (vertical)
    {
        for (i = 0; (c = text[i]); i++)
        {
            // Source image is a representation of ascii chars from 32 and up, 32 per "line". each is 16*16 pixels
            srcrect.x = 16 * (c % 32);
            srcrect.y = 16 * (c / 32 - 1);
            if (!SDL_RenderTextureRotated(renderer, text_texture, &srcrect, &dstrect, -90, NULL, SDL_FLIP_NONE))
                printf("Failed to render text!\n");
            dstrect.y -= 16;
        }
    }
    else
    {
        for (i = 0; (c = text[i]); i++)
        {
            srcrect.x = 16 * (c % 32);
            srcrect.y = 16 * (c / 32 - 1);
            if (!SDL_RenderTexture(renderer, text_texture, &srcrect, &dstrect))
                printf("Failed to render text!\n");
            dstrect.x += 16;
        }
    }
}
