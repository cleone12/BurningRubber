/*
 * mac.h - classic Mac OS types and helpers for the Burning Rubber SDL2 port.
 *
 * The original game is Pascal compiled for the 68k/PPC Mac, where INTEGER is
 * 16 bits and LONGINT is 32 bits. Game state is declared with these types so
 * arithmetic truncates exactly like the original.
 */
#ifndef MAC_H
#define MAC_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

typedef int16_t INTEGER;
typedef int32_t LONGINT;

typedef struct { INTEGER top, left, bottom, right; } Rect;
typedef struct { INTEGER v, h; } Point;
typedef struct { uint16_t red, green, blue; } RGBColor;
typedef unsigned char Str255[256];   /* Pascal string: length byte + chars */

#define FOURCC(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

/* ---- big-endian readers ---- */
static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static inline int16_t rd16s(const uint8_t *p) { return (int16_t)rd16(p); }
static inline uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static inline void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static inline void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

/* ---- Rect / Point utilities (QuickDraw semantics) ---- */
static inline void SetRect(Rect *r, int left, int top, int right, int bottom)
{
    r->left = (INTEGER)left; r->top = (INTEGER)top; r->right = (INTEGER)right; r->bottom = (INTEGER)bottom;
}
static inline void SetPt(Point *p, int h, int v) { p->h = (INTEGER)h; p->v = (INTEGER)v; }
static inline void OffsetRect(Rect *r, int dh, int dv)
{
    r->left += dh; r->right += dh; r->top += dv; r->bottom += dv;
}
static inline void InsetRect(Rect *r, int dh, int dv)
{
    r->left += dh; r->right -= dh; r->top += dv; r->bottom -= dv;
    if (r->left >= r->right || r->top >= r->bottom) SetRect(r, 0, 0, 0, 0);
}
static inline bool EmptyRect(const Rect *r) { return r->left >= r->right || r->top >= r->bottom; }
/* SectRect: dst may alias either source. Returns false (and an empty dst) when disjoint. */
static inline bool SectRect(const Rect *a, const Rect *b, Rect *dst)
{
    Rect r;
    r.left = a->left > b->left ? a->left : b->left;
    r.top = a->top > b->top ? a->top : b->top;
    r.right = a->right < b->right ? a->right : b->right;
    r.bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    if (r.left >= r.right || r.top >= r.bottom) { SetRect(dst, 0, 0, 0, 0); return false; }
    *dst = r;
    return true;
}
static inline bool PtInRect(Point p, const Rect *r)
{
    return p.h >= r->left && p.h < r->right && p.v >= r->top && p.v < r->bottom;
}

/* ---- Pascal strings ---- */
static inline void PStrFromC(unsigned char *dst, const char *src)
{
    size_t n = strlen(src);
    if (n > 255) n = 255;
    dst[0] = (unsigned char)n;
    memcpy(dst + 1, src, n);
}
static inline void PStrCopy(unsigned char *dst, const unsigned char *src) { memmove(dst, src, (size_t)src[0] + 1); }
static inline bool PStrEqual(const unsigned char *a, const unsigned char *b) { return a[0] == b[0] && memcmp(a + 1, b + 1, a[0]) == 0; }
void NumToString(long n, unsigned char *out);
void StringToNum(const unsigned char *s, long *out);
void UpperString(unsigned char *s);

/* ---- Toolbox bit operations ----
 * BitTst() numbers bits from the most significant bit of the first byte, so
 * on a 16-bit word bit n is mask 0x8000 >> n. BTst() (the MPW Pascal
 * intrinsic) numbers from the least significant bit. */
static inline bool BitTstByte(uint8_t b, int n) { return (b >> (7 - n)) & 1; }
static inline bool BitTstWord(uint16_t w, int n) { return (w >> (15 - n)) & 1; }
static inline bool BTst(long v, int n) { return (v >> n) & 1; }

/* ---- QuickDraw Random() ---- */
extern int32_t gRandSeed;            /* qd.randSeed */
INTEGER Random(void);

/* Pascal round(): nearest, ties away from zero; result as Pascal integer. */
static inline long PRound(double x) { return lround(x); }
static inline INTEGER sign(INTEGER n) { return n != 0 ? (INTEGER)(n / abs(n)) : 1; }

#endif
