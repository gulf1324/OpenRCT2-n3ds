/* n3ds port: SDL_ttf stand-in.
 * The 3DS build has no SDL2_ttf/FreeType. The seven functions the game uses are implemented in
 * src/platform/n3ds_font.c over bitmap fonts that are part of the program: a font set that names
 * them (Korean, interface/Fonts.cpp) is drawn through the game's TrueType text code; any other
 * fails to open, so ttf_initialise() returns false and the game falls back to its built-in
 * sprite font (Fonts.cpp: LoadSpriteFont). This keeps drawing/font.h and drawing/string.c as
 * they are. Only on the 3DS include path. */
#pragma once
#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TTF_Font TTF_Font;

int TTF_Init(void);
void TTF_Quit(void);
TTF_Font *TTF_OpenFont(const char *file, int ptsize);
void TTF_CloseFont(TTF_Font *font);
int TTF_GlyphIsProvided(const TTF_Font *font, Uint16 ch);
SDL_Surface *TTF_RenderUTF8_Solid(TTF_Font *font, const char *text, SDL_Color fg);
int TTF_SizeUTF8(TTF_Font *font, const char *text, int *w, int *h);

#ifdef __cplusplus
}
#endif
