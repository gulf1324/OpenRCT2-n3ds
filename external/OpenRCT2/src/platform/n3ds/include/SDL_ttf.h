/* n3ds port: SDL_ttf stand-in.
 * The 3DS build has no SDL2_ttf/FreeType. Every call reports failure, so ttf_initialise()
 * returns false and the game falls back to its built-in sprite font (Fonts.cpp: LoadSpriteFont).
 * This keeps drawing/font.h and drawing/string.c unchanged. Only on the 3DS include path. */
#pragma once
#include <SDL.h>

typedef struct _TTF_Font TTF_Font;

static inline int TTF_Init(void) { SDL_SetError("SDL_ttf is not available on 3DS"); return -1; }
static inline void TTF_Quit(void) { }
static inline TTF_Font *TTF_OpenFont(const char *file, int ptsize) { (void)file; (void)ptsize; return NULL; }
static inline void TTF_CloseFont(TTF_Font *font) { (void)font; }
static inline int TTF_GlyphIsProvided(const TTF_Font *font, Uint16 ch) { (void)font; (void)ch; return 0; }
static inline SDL_Surface *TTF_RenderUTF8_Solid(TTF_Font *font, const char *text, SDL_Color fg)
{ (void)font; (void)text; (void)fg; return NULL; }
static inline int TTF_SizeUTF8(TTF_Font *font, const char *text, int *w, int *h)
{ (void)font; (void)text; if (w) *w = 0; if (h) *h = 0; return -1; }
