/*
 * text.h - QuickDraw text: the game's bitmap 'FONT' resources (Digital,
 * KidPix) and the Mac system font (Chicago), which is not in the game files
 * and is substituted with the public-domain ChicagoFLF via SDL_ttf.
 */
#ifndef TEXT_H
#define TEXT_H

#include "mac.h"

enum { systemFont = 0, applFont = 1 };

typedef struct { INTEGER ascent, descent, widMax, leading; } FontInfo;

bool Text_Init(void);
void Text_Shutdown(void);

void TextFont(int font);
void TextSize(int size);
void TextMode(int mode);
void TextFace(int face);

void DrawChar(unsigned char c);
void DrawString(const unsigned char *pstr);
void DrawText(const void *text, int first, int count);
INTEGER CharWidth(unsigned char c);
INTEGER StringWidth(const unsigned char *pstr);
INTEGER TextWidth(const void *text, int first, int count);
void GetFontInfo(FontInfo *info);

#endif
