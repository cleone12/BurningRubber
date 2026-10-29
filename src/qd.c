/*
 * qd.c - 8-bit indexed QuickDraw subset (see qd.h).
 */
#include "qd.h"
#include "resources.h"
#include "text.h"

#include <stdio.h>

GrafPtr thePort;
RGBColor gSystemPalette[256];
CSpec gDeviceCT[256];
uint32_t gDeviceSeed = 1;
uint8_t gHWPalette[256][3];
bool gPaletteDirty = true;

/* ------------------------------------------------------------------ */
/* palette                                                             */
/* ------------------------------------------------------------------ */

static uint16_t gInverse[32768];   /* 5-5-5 RGB -> index + 1, 0 = not yet computed */

void QD_Init(void)
{
    /* Standard Mac 8-bit palette: a 6x6x6 cube (without black), then ramps
     * of red, green, blue and grey that skip multiples of 0x33, then black. */
    static const uint8_t cube[6] = { 0xFF, 0xCC, 0x99, 0x66, 0x33, 0x00 };
    static const uint8_t ramp[10] = { 0xEE, 0xDD, 0xBB, 0xAA, 0x88, 0x77, 0x55, 0x44, 0x22, 0x11 };
    int n = 0;
    for (int r = 0; r < 6; r++)
        for (int g = 0; g < 6; g++)
            for (int b = 0; b < 6; b++)
                if (n < 215) {
                    gSystemPalette[n].red = (uint16_t)(cube[r] * 0x101);
                    gSystemPalette[n].green = (uint16_t)(cube[g] * 0x101);
                    gSystemPalette[n].blue = (uint16_t)(cube[b] * 0x101);
                    n++;
                }
    for (int c = 0; c < 4; c++)
        for (int i = 0; i < 10; i++, n++) {
            uint16_t v = (uint16_t)(ramp[i] * 0x101);
            gSystemPalette[n].red = (c == 0 || c == 3) ? v : 0;
            gSystemPalette[n].green = (c == 1 || c == 3) ? v : 0;
            gSystemPalette[n].blue = (c == 2 || c == 3) ? v : 0;
        }
    gSystemPalette[255].red = gSystemPalette[255].green = gSystemPalette[255].blue = 0;
    SetSystemPaletteEntries();
}

void SetSystemPaletteEntries(void)
{
    for (int i = 0; i < 256; i++) {
        gDeviceCT[i].red = (INTEGER)gSystemPalette[i].red;
        gDeviceCT[i].green = (INTEGER)gSystemPalette[i].green;
        gDeviceCT[i].blue = (INTEGER)gSystemPalette[i].blue;
        gHWPalette[i][0] = (uint8_t)(gSystemPalette[i].red >> 8);
        gHWPalette[i][1] = (uint8_t)(gSystemPalette[i].green >> 8);
        gHWPalette[i][2] = (uint8_t)(gSystemPalette[i].blue >> 8);
    }
    gDeviceSeed++;
    gPaletteDirty = true;
}

void SetEntries(int start, int count, const CSpec *table)
{
    for (int i = start; i <= start + count && i < 256; i++) {
        if (i < 0) continue;
        gDeviceCT[i] = table[i - start];
        gHWPalette[i][0] = (uint8_t)((uint16_t)table[i - start].red >> 8);
        gHWPalette[i][1] = (uint8_t)((uint16_t)table[i - start].green >> 8);
        gHWPalette[i][2] = (uint8_t)((uint16_t)table[i - start].blue >> 8);
    }
    gDeviceSeed++;
    gPaletteDirty = true;
}

uint8_t Color2Index(const RGBColor *c)
{
    int key = ((c->red >> 11) << 10) | ((c->green >> 11) << 5) | (c->blue >> 11);
    if (gInverse[key]) return (uint8_t)(gInverse[key] - 1);
    int best = 0;
    long bestD = -1;
    for (int i = 0; i < 256; i++) {
        long dr = (long)(c->red >> 8) - (gSystemPalette[i].red >> 8);
        long dg = (long)(c->green >> 8) - (gSystemPalette[i].green >> 8);
        long db = (long)(c->blue >> 8) - (gSystemPalette[i].blue >> 8);
        long d = dr * dr + dg * dg + db * db;
        if (bestD < 0 || d < bestD) { bestD = d; best = i; }
    }
    /* Like QuickDraw's inverse table, colours are matched per 5-bit cell. */
    gInverse[key] = (uint16_t)(best + 1);
    return (uint8_t)best;
}

RGBColor RGB(int r, int g, int b)
{
    RGBColor c = { (uint16_t)r, (uint16_t)g, (uint16_t)b };
    return c;
}

uint8_t ClassicColorIndex(long classic)
{
    RGBColor c;
    switch (classic) {
    case whiteColor: return kWhiteIndex;
    case redColor: c = RGB(0xDD6B, 0x08C2, 0x06A2); break;
    case greenColor: c = RGB(0x0000, 0x8000, 0x11B0); break;
    case blueColor: c = RGB(0x0000, 0x0000, 0xD400); break;
    case cyanColor: c = RGB(0x0241, 0xAB54, 0xEAFF); break;
    case magentaColor: c = RGB(0xF2D7, 0x0856, 0x84EC); break;
    case yellowColor: c = RGB(0xFC00, 0xF37D, 0x052F); break;
    default: return kBlackIndex;
    }
    return Color2Index(&c);
}

void RGBForeColor(const RGBColor *c) { thePort->fg = Color2Index(c); }
void RGBBackColor(const RGBColor *c) { thePort->bg = Color2Index(c); }
void ForeColor(long classic) { thePort->fg = ClassicColorIndex(classic); }
void BackColor(long classic) { thePort->bg = ClassicColorIndex(classic); }

/* ------------------------------------------------------------------ */
/* ports                                                               */
/* ------------------------------------------------------------------ */

void SetPort(GrafPtr port) { thePort = port; }

void PortReset(GrafPtr p)
{
    p->fg = kBlackIndex;
    p->bg = kWhiteIndex;
    p->pnW = p->pnH = 1;
    p->pnLoc.h = p->pnLoc.v = 0;
    p->txFont = 0;
    p->txSize = 12;
    p->txMode = srcOr;
}

GrafPtr NewGWorld(const Rect *bounds)
{
    GrafPtr gw = calloc(1, sizeof(GrafPort));
    if (!gw) return NULL;
    int w = bounds->right - bounds->left, h = bounds->bottom - bounds->top;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    gw->base = calloc((size_t)w * (size_t)h, 1);
    if (!gw->base) { free(gw); return NULL; }
    gw->stride = w;
    gw->bounds = *bounds;
    gw->portRect = *bounds;
    gw->clip = *bounds;
    gw->ownsPixels = true;
    PortReset(gw);
    return gw;
}

void DisposeGWorld(GrafPtr gw)
{
    if (!gw) return;
    if (thePort == gw) thePort = NULL;
    if (gw->ownsPixels) free(gw->base);
    free(gw);
}

static inline bool VisibleRect(GrafPtr p, Rect *r)
{
    Rect c;
    if (!SectRect(&p->clip, &p->bounds, &c)) return false;
    return SectRect(r, &c, r);
}

static inline uint8_t *PixAt(GrafPtr p, int x, int y)
{
    return p->base + (y - p->bounds.top) * p->stride + (x - p->bounds.left);
}

void ClipToRect(const Rect *r)
{
    Rect c = *r;
    if (!SectRect(&c, &thePort->portRect, &c)) SetRect(&c, 0, 0, 0, 0);
    thePort->clip = c;
}

/* ------------------------------------------------------------------ */
/* shapes                                                              */
/* ------------------------------------------------------------------ */

void FillRectIndex(const Rect *r, uint8_t index)
{
    Rect v = *r;
    if (!thePort || !VisibleRect(thePort, &v)) return;
    for (int y = v.top; y < v.bottom; y++)
        memset(PixAt(thePort, v.left, y), index, (size_t)(v.right - v.left));
}

void PaintRect(const Rect *r) { FillRectIndex(r, thePort->fg); }
void EraseRect(const Rect *r) { FillRectIndex(r, thePort->bg); }

void InvertRect(const Rect *r)
{
    Rect v = *r;
    if (!VisibleRect(thePort, &v)) return;
    for (int y = v.top; y < v.bottom; y++) {
        uint8_t *p = PixAt(thePort, v.left, y);
        for (int x = v.left; x < v.right; x++, p++) *p ^= 0xFF;
    }
}

void FrameRect(const Rect *r)
{
    Rect s;
    int pw = thePort->pnW, ph = thePort->pnH;
    if (pw <= 0 || ph <= 0 || EmptyRect(r)) return;
    if (r->right - r->left <= 2 * pw || r->bottom - r->top <= 2 * ph) { PaintRect(r); return; }
    SetRect(&s, r->left, r->top, r->right, r->top + ph); PaintRect(&s);
    SetRect(&s, r->left, r->bottom - ph, r->right, r->bottom); PaintRect(&s);
    SetRect(&s, r->left, r->top + ph, r->left + pw, r->bottom - ph); PaintRect(&s);
    SetRect(&s, r->right - pw, r->top + ph, r->right, r->bottom - ph); PaintRect(&s);
}

void PenSize(int w, int h) { thePort->pnW = (INTEGER)w; thePort->pnH = (INTEGER)h; }
void PenNormal(void) { thePort->pnW = thePort->pnH = 1; }
void MoveTo(int h, int v) { thePort->pnLoc.h = (INTEGER)h; thePort->pnLoc.v = (INTEGER)v; }
void Move(int dh, int dv) { thePort->pnLoc.h += dh; thePort->pnLoc.v += dv; }

void LineTo(int h, int v)
{
    int x0 = thePort->pnLoc.h, y0 = thePort->pnLoc.v;
    int pw = thePort->pnW, ph = thePort->pnH;
    thePort->pnLoc.h = (INTEGER)h;
    thePort->pnLoc.v = (INTEGER)v;
    if (pw <= 0 || ph <= 0) return;
    int dx = abs(h - x0), sx = x0 < h ? 1 : -1;
    int dy = -abs(v - y0), sy = y0 < v ? 1 : -1;
    int err = dx + dy;
    Rect r;
    for (;;) {
        SetRect(&r, x0, y0, x0 + pw, y0 + ph);
        PaintRect(&r);
        if (x0 == h && y0 == v) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Line(int dh, int dv) { LineTo(thePort->pnLoc.h + dh, thePort->pnLoc.v + dv); }

/* Horizontal inset of a rounded corner at row distance dy from the corner. */
static int CornerInset(int y, const Rect *r, int rx, int ry)
{
    if (rx <= 0 || ry <= 0) return 0;
    double d;
    if (y < r->top + ry) d = (r->top + ry) - y - 0.5;
    else if (y >= r->bottom - ry) d = y - (r->bottom - ry) + 0.5;
    else return 0;
    double t = 1.0 - (d * d) / ((double)ry * ry);
    if (t < 0) t = 0;
    return (int)(rx - rx * sqrt(t) + 0.5);
}

void PaintRoundRect(const Rect *r, int ovalW, int ovalH, uint8_t index)
{
    int rx = ovalW / 2, ry = ovalH / 2;
    Rect s;
    for (int y = r->top; y < r->bottom; y++) {
        int in = CornerInset(y, r, rx, ry);
        SetRect(&s, r->left + in, y, r->right - in, y + 1);
        FillRectIndex(&s, index);
    }
}

void FrameRoundRect(const Rect *r, int ovalW, int ovalH, int thick, uint8_t index)
{
    int rx = ovalW / 2, ry = ovalH / 2;
    Rect inner = *r, s;
    InsetRect(&inner, thick, thick);
    for (int y = r->top; y < r->bottom; y++) {
        int in = CornerInset(y, r, rx, ry);
        int l = r->left + in, rt = r->right - in;
        if (EmptyRect(&inner) || y < inner.top || y >= inner.bottom) {
            SetRect(&s, l, y, rt, y + 1);
            FillRectIndex(&s, index);
            continue;
        }
        int iin = CornerInset(y, &inner, rx - thick, ry - thick);
        int il = inner.left + iin, ir = inner.right - iin;
        SetRect(&s, l, y, il, y + 1); FillRectIndex(&s, index);
        SetRect(&s, ir, y, rt, y + 1); FillRectIndex(&s, index);
    }
}

void ScrollRect(const Rect *r, int dh, int dv)
{
    Rect area = *r;
    if (!SectRect(&area, &thePort->bounds, &area)) return;
    int w = area.right - area.left, h = area.bottom - area.top;
    uint8_t *tmp = malloc((size_t)w * (size_t)h);
    if (!tmp) return;
    for (int y = 0; y < h; y++) memcpy(tmp + y * w, PixAt(thePort, area.left, area.top + y), (size_t)w);
    FillRectIndex(&area, thePort->bg);
    for (int y = 0; y < h; y++) {
        int dy = area.top + y + dv;
        if (dy < area.top || dy >= area.bottom) continue;
        for (int x = 0; x < w; x++) {
            int dx = area.left + x + dh;
            if (dx < area.left || dx >= area.right) continue;
            *PixAt(thePort, dx, dy) = tmp[y * w + x];
        }
    }
    free(tmp);
}

/* ------------------------------------------------------------------ */
/* bit transfer                                                        */
/* ------------------------------------------------------------------ */

static void Blit(GrafPtr src, GrafPtr mask, GrafPtr dst, const Rect *sr, const Rect *dr,
                 GrafPtr rgn)
{
    int sw = sr->right - sr->left, sh = sr->bottom - sr->top;
    int dw = dr->right - dr->left, dh = dr->bottom - dr->top;
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    Rect v = *dr;
    if (!VisibleRect(dst, &v)) return;
    for (int y = v.top; y < v.bottom; y++) {
        int sy = sr->top + (int)((long)(y - dr->top) * sh / dh);
        if (sy < src->bounds.top || sy >= src->bounds.bottom) continue;
        uint8_t *d = PixAt(dst, v.left, y);
        for (int x = v.left; x < v.right; x++, d++) {
            int sx = sr->left + (int)((long)(x - dr->left) * sw / dw);
            if (sx < src->bounds.left || sx >= src->bounds.right) continue;
            if (mask && !*PixAt(mask, sx, sy)) continue;
            if (rgn && PtInRect((Point){ (INTEGER)y, (INTEGER)x }, &rgn->bounds) && !*PixAt(rgn, x, y)) continue;
            *d = *PixAt(src, sx, sy);
        }
    }
}

void CopyBits(GrafPtr src, GrafPtr dst, const Rect *sr, const Rect *dr) { Blit(src, NULL, dst, sr, dr, NULL); }
void CopyBitsRgn(GrafPtr src, GrafPtr dst, const Rect *sr, const Rect *dr, GrafPtr rgn) { Blit(src, NULL, dst, sr, dr, rgn); }
void CopyMask(GrafPtr src, GrafPtr mask, GrafPtr dst, const Rect *sr, const Rect *dr) { Blit(src, mask, dst, sr, dr, NULL); }

/* Draw a w*h index image (with optional 0/1 coverage) scaled into dr. */
static void DrawImage(const uint8_t *pix, const uint8_t *cover, int w, int h, const Rect *dr)
{
    int dw = dr->right - dr->left, dh = dr->bottom - dr->top;
    if (dw <= 0 || dh <= 0 || w <= 0 || h <= 0) return;
    Rect v = *dr;
    if (!VisibleRect(thePort, &v)) return;
    for (int y = v.top; y < v.bottom; y++) {
        int sy = (int)((long)(y - dr->top) * h / dh);
        uint8_t *d = PixAt(thePort, v.left, y);
        for (int x = v.left; x < v.right; x++, d++) {
            int sx = (int)((long)(x - dr->left) * w / dw);
            int i = sy * w + sx;
            if (cover && !cover[i]) continue;
            *d = pix[i];
        }
    }
}

/* ------------------------------------------------------------------ */
/* PICT decoding                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    int id, w, h;
    uint8_t *pix, *cover;
} PictCache;

static PictCache *gPicts;
static int gPictCount;

static bool UnpackBits(const uint8_t *src, int32_t srcLen, uint8_t *dst, int dstLen, int unit)
{
    int32_t o = 0;
    int n = 0;
    while (n < dstLen && o < srcLen) {
        int c = src[o++];
        if (c < 128) {
            int k = (c + 1) * unit;
            if (o + k > srcLen) return false;
            for (int i = 0; i < k && n < dstLen; i++) dst[n++] = src[o + i];
            o += k;
        } else if (c > 128) {
            int k = 257 - c;
            if (o + unit > srcLen) return false;
            for (int i = 0; i < k; i++)
                for (int u = 0; u < unit && n < dstLen; u++) dst[n++] = src[o + u];
            o += unit;
        }
    }
    return n == dstLen;
}

/* Parses a ColorTable; fills map[256] with system palette indices. */
static int32_t ReadCTab(const uint8_t *d, int32_t o, int32_t size, uint8_t map[256])
{
    if (o + 8 > size) return -1;
    uint16_t flags = rd16(d + o + 4);
    int n = rd16s(d + o + 6) + 1;
    o += 8;
    for (int i = 0; i < 256; i++) map[i] = (uint8_t)i;
    for (int i = 0; i < n; i++, o += 8) {
        if (o + 8 > size) return -1;
        int value = (flags & 0x8000) ? i : (rd16(d + o) & 0xFF);
        RGBColor c = { rd16(d + o + 2), rd16(d + o + 4), rd16(d + o + 6) };
        map[value] = Color2Index(&c);
    }
    return o;
}

static inline int GetBits(const uint8_t *row, int x, int depth)
{
    if (depth == 8) return row[x];
    int per = 8 / depth;
    return (row[x / per] >> (8 - depth * (x % per + 1))) & ((1 << depth) - 1);
}

static void DecodePict(PictCache *pc, const uint8_t *d, int32_t size)
{
    Rect frame;
    SetRect(&frame, rd16s(d + 4), rd16s(d + 2), rd16s(d + 8), rd16s(d + 6));
    pc->w = frame.right - frame.left;
    pc->h = frame.bottom - frame.top;
    if (pc->w <= 0 || pc->h <= 0 || pc->w > 4096 || pc->h > 4096) { pc->w = pc->h = 0; return; }
    pc->pix = calloc((size_t)pc->w * pc->h, 1);
    pc->cover = calloc((size_t)pc->w * pc->h, 1);
    if (!pc->pix || !pc->cover) { pc->w = pc->h = 0; return; }

    Rect space = frame;   /* coordinate space of the opcodes */
    int32_t o = 10;
    while (o + 2 <= size) {
        if (o & 1) o++;
        uint16_t op = rd16(d + o);
        o += 2;
        if (op == 0x00FF) break;
        switch (op) {
        case 0x0000: case 0x001E: case 0x0038: case 0x0039: case 0x003A: case 0x003B: case 0x003C:
            break;
        case 0x0011: case 0x0003: case 0x0005: case 0x0008: case 0x000D: case 0x0015:
        case 0x0016: case 0x0023: case 0x00A0:
            o += 2; break;
        case 0x0004: o += 1; break;
        case 0x0007: case 0x0021: o += 4; break;
        case 0x001A: case 0x001B: case 0x001F: case 0x0022: o += 6; break;
        case 0x0009: case 0x000A: case 0x0010: case 0x0020:
        case 0x0030: case 0x0031: case 0x0032: case 0x0033: case 0x0034:
            o += 8; break;
        case 0x0001: o += rd16(d + o); break;
        case 0x00A1: o += 4 + rd16(d + o + 2); break;
        case 0x0C00:
            if (rd16s(d + o) == -2)
                SetRect(&space, rd16s(d + o + 14), rd16s(d + o + 12), rd16s(d + o + 18), rd16s(d + o + 16));
            o += 24;
            break;
        case 0x0090: case 0x0091: case 0x0098: case 0x0099: case 0x009A: case 0x009B: {
            bool direct = op == 0x009A || op == 0x009B;
            bool packed = op != 0x0090 && op != 0x0091;
            bool hasRgn = op & 1;
            if (direct) o += 4;
            if (o + 10 > size) return;
            uint16_t rbf = rd16(d + o);
            int rowBytes = rbf & 0x3FFF;
            Rect bnd;
            SetRect(&bnd, rd16s(d + o + 4), rd16s(d + o + 2), rd16s(d + o + 8), rd16s(d + o + 6));
            int depth = 1, packType = 0;
            uint8_t map[256];
            map[0] = kWhiteIndex;
            map[1] = kBlackIndex;
            if (direct || (rbf & 0x8000)) {
                if (o + 46 > size) return;
                packType = rd16(d + o + 12);
                depth = rd16s(d + o + 28);
                o += 46;
                if (!direct && (o = ReadCTab(d, o, size, map)) < 0) return;
            } else {
                o += 10;
            }
            if (o + 18 > size) return;
            Rect sr, dr;
            SetRect(&sr, rd16s(d + o + 2), rd16s(d + o), rd16s(d + o + 6), rd16s(d + o + 4));
            SetRect(&dr, rd16s(d + o + 10), rd16s(d + o + 8), rd16s(d + o + 14), rd16s(d + o + 12));
            int mode = rd16s(d + o + 16);
            /* srcOr and transparent leave white (the background colour) alone */
            bool skipWhite = (mode & 0x3F) == srcOr || mode == 36;
            o += 18;
            if (hasRgn) o += rd16(d + o);
            int w = bnd.right - bnd.left, h = bnd.bottom - bnd.top;
            if (w <= 0 || h <= 0 || rowBytes <= 0) return;
            int unit = (direct && depth == 16) ? 2 : 1;
            uint8_t *rows = malloc((size_t)rowBytes * h);
            if (!rows) return;
            bool ok = true;
            for (int y = 0; y < h && ok; y++) {
                if (rowBytes < 8 || !packed || packType == 1) {
                    if (o + rowBytes > size) { ok = false; break; }
                    memcpy(rows + y * rowBytes, d + o, (size_t)rowBytes);
                    o += rowBytes;
                } else {
                    int n = rowBytes > 250 ? rd16(d + o) : d[o];
                    o += rowBytes > 250 ? 2 : 1;
                    if (o + n > size) { ok = false; break; }
                    ok = UnpackBits(d + o, n, rows + y * rowBytes, rowBytes, unit);
                    o += n;
                }
            }
            if (ok && (depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16)) {
                /* map dr from opcode space into the canvas */
                int sw = space.right - space.left, shh = space.bottom - space.top;
                if (sw <= 0 || shh <= 0) { sw = pc->w; shh = pc->h; space = frame; }
                Rect cr;
                SetRect(&cr, (int)((long)(dr.left - space.left) * pc->w / sw),
                        (int)((long)(dr.top - space.top) * pc->h / shh),
                        (int)((long)(dr.right - space.left) * pc->w / sw),
                        (int)((long)(dr.bottom - space.top) * pc->h / shh));
                int cw = cr.right - cr.left, ch = cr.bottom - cr.top;
                int srw = sr.right - sr.left, srh = sr.bottom - sr.top;
                for (int y = 0; y < ch && srw > 0 && srh > 0; y++) {
                    int cy = cr.top + y;
                    if (cy < 0 || cy >= pc->h) continue;
                    int by = sr.top + (int)((long)y * srh / ch) - bnd.top;
                    if (by < 0 || by >= h) continue;
                    const uint8_t *row = rows + by * rowBytes;
                    for (int x = 0; x < cw; x++) {
                        int cx = cr.left + x;
                        if (cx < 0 || cx >= pc->w) continue;
                        int bx = sr.left + (int)((long)x * srw / cw) - bnd.left;
                        if (bx < 0 || bx >= w) continue;
                        uint8_t idx;
                        if (depth == 16) {
                            if (bx * 2 + 1 >= rowBytes) continue;
                            uint16_t px = rd16(row + bx * 2);
                            RGBColor c = { (uint16_t)(((px >> 10) & 31) * 0x0842),
                                           (uint16_t)(((px >> 5) & 31) * 0x0842),
                                           (uint16_t)((px & 31) * 0x0842) };
                            idx = Color2Index(&c);
                        } else {
                            if ((bx * depth) / 8 >= rowBytes) continue;
                            idx = map[GetBits(row, bx, depth)];
                        }
                        if (skipWhite && idx == kWhiteIndex) continue;
                        pc->pix[cy * pc->w + cx] = idx;
                        pc->cover[cy * pc->w + cx] = 1;
                    }
                }
            }
            free(rows);
            if (!ok) return;
            break;
        }
        default:
            fprintf(stderr, "PICT: unsupported opcode 0x%04X, stopping\n", op);
            return;
        }
    }
}

static PictCache *FindPict(int id)
{
    for (int i = 0; i < gPictCount; i++)
        if (gPicts[i].id == id) return &gPicts[i];
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('P', 'I', 'C', 'T'), id, &size);
    if (!d || size < 12) return NULL;
    PictCache *n = realloc(gPicts, sizeof(PictCache) * (size_t)(gPictCount + 1));
    if (!n) return NULL;
    gPicts = n;
    PictCache *pc = &gPicts[gPictCount++];
    memset(pc, 0, sizeof *pc);
    pc->id = id;
    DecodePict(pc, d, size);
    return pc;
}

bool GetPictFrame(int id, Rect *frame)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('P', 'I', 'C', 'T'), id, &size);
    if (!d || size < 10) return false;
    SetRect(frame, rd16s(d + 4), rd16s(d + 2), rd16s(d + 8), rd16s(d + 6));
    return true;
}

void DrawPicture(int id, const Rect *dst)
{
    PictCache *pc = FindPict(id);
    if (pc && pc->w > 0) DrawImage(pc->pix, pc->cover, pc->w, pc->h, dst);
}

/* ------------------------------------------------------------------ */
/* colour icons                                                        */
/* ------------------------------------------------------------------ */

typedef struct { int id; CIcon icon; bool valid; } CIconCache;
static CIconCache *gIcons;
static int gIconCount;

static void DecodeCIcon(CIcon *ic, const uint8_t *d, int32_t size, bool *valid)
{
    /* PixMap (50 bytes, baseAddr included), iconMask BitMap (14),
     * iconBMap BitMap (14), iconData (4), mask bits, bmap bits, ctab, pixels */
    *valid = false;
    if (size < 82) return;
    int rowBytes = rd16(d + 4) & 0x3FFF;
    int top = rd16s(d + 6), left = rd16s(d + 8), bottom = rd16s(d + 10), right = rd16s(d + 12);
    int depth = rd16s(d + 32);
    int w = right - left, h = bottom - top;
    int maskRB = rd16(d + 54), bmapRB = rd16(d + 68);
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024) return;
    if (depth != 1 && depth != 2 && depth != 4 && depth != 8) return;
    int32_t o = 82;
    const uint8_t *mask = d + o;
    o += maskRB * h;
    const uint8_t *bmap = d + o;
    o += bmapRB * h;
    uint8_t map[256];
    if ((o = ReadCTab(d, o, size, map)) < 0) return;
    if (o + rowBytes * h > size || (rowBytes * 8) / depth < w) return;
    ic->w = w;
    ic->h = h;
    ic->pix = malloc((size_t)w * h);
    ic->mask = malloc((size_t)w * h);
    ic->bmap = malloc((size_t)w * h);
    if (!ic->pix || !ic->mask || !ic->bmap) return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            ic->pix[y * w + x] = map[GetBits(d + o + y * rowBytes, x, depth)];
            ic->mask[y * w + x] = maskRB ? (uint8_t)GetBits(mask + y * maskRB, x, 1) : 1;
            ic->bmap[y * w + x] = bmapRB ? (uint8_t)GetBits(bmap + y * bmapRB, x, 1) : 1;
        }
    *valid = true;
}

const CIcon *GetCIcon(int id)
{
    for (int i = 0; i < gIconCount; i++)
        if (gIcons[i].id == id) return gIcons[i].valid ? &gIcons[i].icon : NULL;
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('c', 'i', 'c', 'n'), id, &size);
    if (!d) return NULL;
    CIconCache *n = realloc(gIcons, sizeof(CIconCache) * (size_t)(gIconCount + 1));
    if (!n) return NULL;
    gIcons = n;
    CIconCache *c = &gIcons[gIconCount++];
    memset(c, 0, sizeof *c);
    c->id = id;
    DecodeCIcon(&c->icon, d, size, &c->valid);
    return c->valid ? &c->icon : NULL;
}

void PlotCIcon(const Rect *r, const CIcon *ic)
{
    if (ic) DrawImage(ic->pix, ic->mask, ic->w, ic->h, r);
}

void PlotIconID(const Rect *r, int id)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('I', 'C', 'O', 'N'), id, &size);
    if (!d || size < 128) return;
    uint8_t pix[32 * 32];
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            pix[y * 32 + x] = GetBits(d + y * 4, x, 1) ? thePort->fg : thePort->bg;
    DrawImage(pix, NULL, 32, 32, r);
}

/* ------------------------------------------------------------------ */
/* pixel patterns                                                      */
/* ------------------------------------------------------------------ */

typedef struct { int id; PixPat pat; bool valid; } PatCache;
static PatCache *gPats;
static int gPatCount;

const PixPat *GetPixPat(int id)
{
    for (int i = 0; i < gPatCount; i++)
        if (gPats[i].id == id) return gPats[i].valid ? &gPats[i].pat : NULL;
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('p', 'p', 'a', 't'), id, &size);
    if (!d) return NULL;
    PatCache *n = realloc(gPats, sizeof(PatCache) * (size_t)(gPatCount + 1));
    if (!n) return NULL;
    gPats = n;
    PatCache *pc = &gPats[gPatCount++];
    memset(pc, 0, sizeof *pc);
    pc->id = id;
    /* PixPat: patType, patMap offset, patData offset, ...; PixMap at patMap */
    if (size < 28 || rd16s(d) != 1) return NULL;
    uint32_t pm = rd32(d + 2), pd = rd32(d + 6);
    if (pm + 50 > (uint32_t)size) return NULL;
    int rowBytes = rd16(d + pm + 4) & 0x3FFF;
    int h = rd16s(d + pm + 10) - rd16s(d + pm + 6), w = rd16s(d + pm + 12) - rd16s(d + pm + 8);
    int depth = rd16s(d + pm + 32);
    uint32_t ct = rd32(d + pm + 42);
    uint8_t map[256];
    if (w <= 0 || h <= 0 || pd + (uint32_t)(rowBytes * h) > (uint32_t)size) return NULL;
    if (depth != 1 && depth != 2 && depth != 4 && depth != 8) return NULL;
    if (ReadCTab(d, (int32_t)ct, size, map) < 0) return NULL;
    pc->pat.w = w;
    pc->pat.h = h;
    pc->pat.pix = malloc((size_t)w * h);
    if (!pc->pat.pix) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) pc->pat.pix[y * w + x] = map[GetBits(d + pd + y * rowBytes, x, depth)];
    pc->valid = true;
    return &pc->pat;
}

void FillCRect(const Rect *r, const PixPat *pat)
{
    if (!pat) { EraseRect(r); return; }
    Rect v = *r;
    if (!VisibleRect(thePort, &v)) return;
    for (int y = v.top; y < v.bottom; y++) {
        uint8_t *d = PixAt(thePort, v.left, y);
        int py = ((y % pat->h) + pat->h) % pat->h;
        for (int x = v.left; x < v.right; x++)
            *d++ = pat->pix[py * pat->w + ((x % pat->w) + pat->w) % pat->w];
    }
}
