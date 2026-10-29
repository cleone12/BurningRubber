/*
 * text.c - QuickDraw text rendering (see text.h).
 */
#include "text.h"
#include "qd.h"
#include "resources.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>

typedef struct {
    INTEGER adv;            /* pen advance */
    INTEGER x0, y0;         /* image origin relative to pen / baseline */
    INTEGER w, h;
    uint8_t *bits;          /* w*h, non-zero = ink */
} Glyph;

typedef struct {
    int family, size;
    FontInfo info;
    Glyph g[256];
} Strike;

static Strike *gStrikes;
static int gStrikeCount;
static char gTTFPath[1200];

/* Mac Roman 0x80-0xFF to Unicode */
static const uint16_t kMacRoman[128] = {
    0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1, 0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,
    0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3, 0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC,
    0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF, 0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,
    0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211, 0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,
    0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB, 0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153,
    0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA, 0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,
    0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1, 0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
    0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC, 0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7,
};

/* The Apple logo (Chicago 0x14, used in the Apple menu title) at 12 pt. */
static const char *kApple[12] = {
    "      ##   ",
    "     ##    ",
    "     #     ",
    "  ###  ### ",
    " ##########",
    "########## ",
    "########## ",
    "########## ",
    " ##########",
    " ##########",
    "  ######## ",
    "   ##  ##  ",
};

/* The Command key symbol (Chicago 0x11). */
static const char *kCommand[9] = {
    " ##   ## ",
    "#  # #  #",
    "#  # #  #",
    " #######",
    "   # #   ",
    " ####### ",
    "#  # #  #",
    "#  # #  #",
    " ##   ## ",
};

bool Text_Init(void)
{
    if (TTF_Init() != 0) return false;
    const char *dir = Res_DataDir();
    const char *cands[] = { "fonts/ChicagoFLF.ttf", "ChicagoFLF.ttf", "../fonts/ChicagoFLF.ttf" };
    for (int i = 0; i < 3; i++) {
        snprintf(gTTFPath, sizeof gTTFPath, "%s%s", dir, cands[i]);
        FILE *f = fopen(gTTFPath, "rb");
        if (f) { fclose(f); return true; }
    }
    fprintf(stderr, "ChicagoFLF.ttf not found next to the game data\n");
    gTTFPath[0] = 0;
    return true;
}

void Text_Shutdown(void)
{
    for (int s = 0; s < gStrikeCount; s++)
        for (int c = 0; c < 256; c++) free(gStrikes[s].g[c].bits);
    free(gStrikes);
    gStrikes = NULL;
    gStrikeCount = 0;
    TTF_Quit();
}

static void BuildPattern(Glyph *g, const char **rows, int rw, int rh, int w, int h, int baselineRows)
{
    g->bits = calloc((size_t)w * h, 1);
    if (!g->bits) return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const char *row = rows[y * rh / h];
            int sx = x * rw / w;
            g->bits[y * w + x] = sx < (int)strlen(row) && row[sx] == '#';
        }
    g->w = (INTEGER)w;
    g->h = (INTEGER)h;
    g->x0 = 1;
    g->y0 = (INTEGER)-(h - baselineRows);
    g->adv = (INTEGER)(w + 2);
}

static void BuildApple(Glyph *g, int size)
{
    int h = size, w = (11 * size + 6) / 12;
    g->bits = calloc((size_t)w * h, 1);
    if (!g->bits) return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            g->bits[y * w + x] = kApple[y * 12 / h][x * 11 / w] == '#';
    g->w = (INTEGER)w;
    g->h = (INTEGER)h;
    g->x0 = 1;
    g->y0 = (INTEGER)-(h - 1);
    g->adv = (INTEGER)(w + 2);
}

static bool LoadTTFStrike(Strike *s)
{
    if (!gTTFPath[0]) return false;
    TTF_Font *f = TTF_OpenFont(gTTFPath, s->size);
    if (!f) return false;
    s->info.ascent = (INTEGER)TTF_FontAscent(f);
    s->info.descent = (INTEGER)-TTF_FontDescent(f);
    s->info.leading = (INTEGER)(TTF_FontLineSkip(f) - TTF_FontAscent(f) + TTF_FontDescent(f));
    if (s->info.leading < 0) s->info.leading = 0;
    SDL_Color white = { 255, 255, 255, 255 };
    int widMax = 0;
    for (int c = 32; c < 256; c++) {
        Glyph *g = &s->g[c];
        uint16_t uc = c < 128 ? (uint16_t)c : kMacRoman[c - 128];
        int minx, maxx, miny, maxy, adv;
        if (!TTF_GlyphIsProvided(f, uc) || TTF_GlyphMetrics(f, uc, &minx, &maxx, &miny, &maxy, &adv) != 0) continue;
        g->adv = (INTEGER)adv;
        if (adv > widMax) widMax = adv;
        SDL_Surface *surf = TTF_RenderGlyph_Solid(f, uc, white);
        if (!surf) continue;
        g->w = (INTEGER)surf->w;
        g->h = (INTEGER)surf->h;
        g->x0 = 0;
        g->y0 = (INTEGER)-TTF_FontAscent(f);
        g->bits = calloc((size_t)surf->w * surf->h, 1);
        if (g->bits) {
            SDL_LockSurface(surf);
            for (int y = 0; y < surf->h; y++)
                for (int x = 0; x < surf->w; x++)
                    g->bits[y * surf->w + x] = ((uint8_t *)surf->pixels)[y * surf->pitch + x] != 0;
            SDL_UnlockSurface(surf);
        }
        SDL_FreeSurface(surf);
    }
    BuildApple(&s->g[0x14], s->size);
    BuildPattern(&s->g[0x11], kCommand, 9, 9, (9 * s->size + 6) / 12, (9 * s->size + 6) / 12, 0);
    s->info.widMax = (INTEGER)widMax;
    /* Lay text out with Chicago's bitmap metrics (12 pt: ascent 12,
     * descent 3, leading 1) so dialog text lines up as it did. */
    s->info.ascent = (INTEGER)s->size;
    s->info.descent = (INTEGER)(s->size / 4);
    s->info.leading = (INTEGER)(s->size / 12);
    TTF_CloseFont(f);
    return true;
}

/* 'FONT' resource (Inside Macintosh: Text, 4-66). */
static bool LoadBitmapStrike(Strike *s, const uint8_t *d, int32_t size)
{
    if (size < 26) return false;
    int first = rd16s(d + 2), last = rd16s(d + 4);
    int kernMax = rd16s(d + 8), fRectH = rd16s(d + 14);
    int ascent = rd16s(d + 18), descent = rd16s(d + 20), leading = rd16s(d + 22);
    int rowWords = rd16s(d + 24);
    int n = last - first + 3;
    int32_t img = 26, loc = img + rowWords * 2 * fRectH, ow = loc + n * 2;
    if (first < 0 || last > 255 || last < first || ow + n * 2 > size) return false;
    s->info.ascent = (INTEGER)ascent;
    s->info.descent = (INTEGER)descent;
    s->info.leading = (INTEGER)leading;
    s->info.widMax = rd16s(d + 6);
    int rowBytes = rowWords * 2;
    for (int c = 0; c < 256; c++) {
        int k = (c >= first && c <= last) ? c - first : -1;
        if (k >= 0 && rd16(d + ow + k * 2) == 0xFFFF) k = -1;
        if (k < 0) {
            if (c < 32) continue;   /* control characters draw nothing */
            k = last - first + 1;   /* the font's missing-character glyph */
        }
        uint16_t owv = rd16(d + ow + k * 2);
        if (owv == 0xFFFF) continue;
        int x0 = rd16(d + loc + k * 2), x1 = rd16(d + loc + (k + 1) * 2);
        Glyph *g = &s->g[c];
        g->adv = (INTEGER)(owv & 0xFF);
        g->x0 = (INTEGER)(kernMax + (owv >> 8));
        g->y0 = (INTEGER)-ascent;
        g->w = (INTEGER)(x1 - x0);
        g->h = (INTEGER)fRectH;
        if (g->w <= 0) continue;
        g->bits = calloc((size_t)g->w * fRectH, 1);
        if (!g->bits) continue;
        for (int y = 0; y < fRectH; y++)
            for (int x = 0; x < g->w; x++) {
                int bit = x0 + x;
                g->bits[y * g->w + x] = (d[img + y * rowBytes + bit / 8] >> (7 - bit % 8)) & 1;
            }
    }
    return true;
}

static Strike *GetStrike(int family, int size)
{
    if (size <= 0) size = 12;
    if (family == applFont || family == 3) family = systemFont;
    for (int i = 0; i < gStrikeCount; i++)
        if (gStrikes[i].family == family && gStrikes[i].size == size) return &gStrikes[i];
    Strike *n = realloc(gStrikes, sizeof(Strike) * (size_t)(gStrikeCount + 1));
    if (!n) return gStrikeCount ? &gStrikes[0] : NULL;
    gStrikes = n;
    Strike *s = &gStrikes[gStrikeCount++];
    memset(s, 0, sizeof *s);
    s->family = family;
    s->size = size;
    bool ok = false;
    if (family != systemFont) {
        /* exact size first, else the nearest strike of the family */
        int best = -1;
        for (int sz = 1; sz < 128; sz++)
            if (HasResource(FOURCC('F', 'O', 'N', 'T'), family * 128 + sz) &&
                (best < 0 || abs(sz - size) < abs(best - size))) best = sz;
        if (best > 0) {
            int32_t len;
            const uint8_t *d = GetResource(FOURCC('F', 'O', 'N', 'T'), family * 128 + best, &len);
            ok = LoadBitmapStrike(s, d, len);
        }
    }
    if (!ok) ok = LoadTTFStrike(s);
    if (!ok) { s->info.ascent = (INTEGER)size; s->info.descent = (INTEGER)(size / 4); }
    return s;
}

void TextFont(int font) { thePort->txFont = (INTEGER)font; }
void TextSize(int size) { thePort->txSize = (INTEGER)size; }
void TextMode(int mode) { thePort->txMode = (INTEGER)mode; }
void TextFace(int face) { (void)face; }

void GetFontInfo(FontInfo *info)
{
    Strike *s = GetStrike(thePort->txFont, thePort->txSize);
    if (s) *info = s->info;
    else memset(info, 0, sizeof *info);
}

INTEGER CharWidth(unsigned char c)
{
    Strike *s = GetStrike(thePort->txFont, thePort->txSize);
    return s ? s->g[c].adv : 0;
}

INTEGER TextWidth(const void *text, int first, int count)
{
    const unsigned char *t = (const unsigned char *)text + first;
    Strike *s = GetStrike(thePort->txFont, thePort->txSize);
    long w = 0;
    for (int i = 0; s && i < count; i++) w += s->g[t[i]].adv;
    return (INTEGER)w;
}

INTEGER StringWidth(const unsigned char *pstr) { return TextWidth(pstr, 1, pstr[0]); }

static inline void PutPixel(int x, int y, uint8_t index, int xorMode)
{
    GrafPtr p = thePort;
    if (x < p->clip.left || x >= p->clip.right || y < p->clip.top || y >= p->clip.bottom) return;
    if (x < p->bounds.left || x >= p->bounds.right || y < p->bounds.top || y >= p->bounds.bottom) return;
    uint8_t *d = p->base + (y - p->bounds.top) * p->stride + (x - p->bounds.left);
    *d = xorMode ? (uint8_t)(*d ^ 0xFF) : index;
}

void DrawText(const void *text, int first, int count)
{
    const unsigned char *t = (const unsigned char *)text + first;
    Strike *s = GetStrike(thePort->txFont, thePort->txSize);
    if (!s) return;
    int mode = thePort->txMode;
    uint8_t ink = thePort->fg, paper = thePort->bg;
    if (mode == srcBic) ink = thePort->bg;
    if (mode == notSrcCopy) { ink = thePort->bg; paper = thePort->fg; }
    int base = thePort->pnLoc.v;
    int pen = thePort->pnLoc.h;
    Rect clipTB = thePort->clip;
    for (int i = 0; i < count; i++) {
        Glyph *g = &s->g[t[i]];
        if (pen > clipTB.right) { pen += g->adv; continue; }  /* skip work far off-screen */
        if (mode == srcCopy || mode == notSrcCopy) {
            Rect cell;
            SetRect(&cell, pen, base - s->info.ascent, pen + g->adv, base + s->info.descent);
            FillRectIndex(&cell, paper);
        }
        if (g->bits && pen + g->x0 + g->w >= clipTB.left)
            for (int y = 0; y < g->h; y++)
                for (int x = 0; x < g->w; x++)
                    if (g->bits[y * g->w + x])
                        PutPixel(pen + g->x0 + x, base + g->y0 + y, ink, mode == srcXor);
        pen += g->adv;
    }
    thePort->pnLoc.h = (INTEGER)pen;
}

void DrawString(const unsigned char *pstr) { DrawText(pstr, 1, pstr[0]); }
void DrawChar(unsigned char c) { DrawText(&c, 0, 1); }
