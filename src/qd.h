/*
 * qd.h - the subset of Color QuickDraw that Burning Rubber uses, rebuilt on
 * 8-bit indexed pixel buffers.
 *
 * The game ran in 256 colours with the standard Mac system palette (its
 * 'clut' 1008 is identical to it), so every port here stores palette indices.
 * Colour effects (the red "blood" flash, fades) are done the original way:
 * by editing the device colour table, which only changes how indices are
 * displayed.
 */
#ifndef QD_H
#define QD_H

#include "mac.h"

typedef struct GrafPort {
    uint8_t *base;      /* pixel at local (bounds.left, bounds.top) */
    int stride;
    Rect bounds;        /* local coordinates covered by the pixel buffer */
    Rect portRect;      /* local coordinates of the port */
    Rect clip;          /* local coordinates drawing is limited to */
    uint8_t fg, bg;     /* palette indices */
    Point pnLoc;
    INTEGER pnW, pnH;
    INTEGER txFont, txSize, txMode;
    bool ownsPixels;
} GrafPort, *GrafPtr;

/* Transfer modes */
enum { srcCopy = 0, srcOr = 1, srcXor = 2, srcBic = 3, notSrcCopy = 4 };

/* Classic QuickDraw colours (ForeColor/BackColor) */
enum {
    blackColor = 33, whiteColor = 30, redColor = 205, greenColor = 341,
    blueColor = 409, cyanColor = 273, magentaColor = 137, yellowColor = 69
};

/* System palette indices of pure white and black */
enum { kWhiteIndex = 0, kBlackIndex = 255 };

extern GrafPtr thePort;
void SetPort(GrafPtr port);
static inline GrafPtr GetPort(void) { return thePort; }

GrafPtr NewGWorld(const Rect *bounds);
void DisposeGWorld(GrafPtr gw);
void PortReset(GrafPtr port);    /* PenNormal + default colours and text */

/* ---- colour ---- */
extern RGBColor gSystemPalette[256];
void QD_Init(void);
uint8_t Color2Index(const RGBColor *c);
RGBColor RGB(int r, int g, int b);
void RGBForeColor(const RGBColor *c);
void RGBBackColor(const RGBColor *c);
void ForeColor(long classic);
void BackColor(long classic);
uint8_t ClassicColorIndex(long classic);

/* Device colour table, as seen through gdPMap^^.pmTable. RGB fields are
 * signed like Pascal's RGBColor INTEGER fields, which the fade code relies on. */
typedef struct { INTEGER red, green, blue; } CSpec;
extern CSpec gDeviceCT[256];
extern uint32_t gDeviceSeed;
/* SetEntries(start, count, table): count is "number of entries minus 1". */
void SetEntries(int start, int count, const CSpec *table);
void SetSystemPaletteEntries(void);
/* Palette currently displayed (8-bit RGB triples), for the presenter. */
extern uint8_t gHWPalette[256][3];
extern bool gPaletteDirty;

/* ---- pen and shapes ---- */
void PenSize(int w, int h);
void PenNormal(void);
void MoveTo(int h, int v);
void Move(int dh, int dv);
void LineTo(int h, int v);
void Line(int dh, int dv);
void FillRectIndex(const Rect *r, uint8_t index);
void PaintRect(const Rect *r);
void EraseRect(const Rect *r);
void FrameRect(const Rect *r);
void InvertRect(const Rect *r);
void PaintRoundRect(const Rect *r, int ovalW, int ovalH, uint8_t index);
void FrameRoundRect(const Rect *r, int ovalW, int ovalH, int thick, uint8_t index);
void ScrollRect(const Rect *r, int dh, int dv);
void ClipToRect(const Rect *r);

/* ---- bit transfer ----
 * All copies scale srcRect onto dstRect like CopyBits and honour the
 * destination's clip and bounds. */
void CopyBits(GrafPtr src, GrafPtr dst, const Rect *srcRect, const Rect *dstRect);
/* Copies only where rgn (in dst local coordinates) has a non-zero pixel;
 * pixels outside rgn's bounds are copied. */
void CopyBitsRgn(GrafPtr src, GrafPtr dst, const Rect *srcRect, const Rect *dstRect, GrafPtr rgn);
/* CopyMask: mask shares src's coordinates; non-zero mask pixels are copied. */
void CopyMask(GrafPtr src, GrafPtr mask, GrafPtr dst, const Rect *srcRect, const Rect *dstRect);

/* ---- pictures, icons, patterns ---- */
bool GetPictFrame(int id, Rect *frame);
void DrawPicture(int id, const Rect *dst);

typedef struct CIcon {
    int w, h;
    uint8_t *pix;   /* palette indices */
    uint8_t *mask;  /* iconMask, 0/1 */
    uint8_t *bmap;  /* iconBMap (the 1-bit image), 0/1 */
} CIcon;
const CIcon *GetCIcon(int id);
void PlotCIcon(const Rect *r, const CIcon *icon);
void PlotIconID(const Rect *r, int id);    /* 'ICON' 1-bit, srcCopy */

typedef struct PixPat { int w, h; uint8_t *pix; } PixPat;
const PixPat *GetPixPat(int id);
void FillCRect(const Rect *r, const PixPat *pat);

#endif
