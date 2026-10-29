/*
 * resources.c - classic Mac resource fork parser.
 *
 * Layout (Inside Macintosh: More Macintosh Toolbox, 1-121):
 *   header: dataOffset, mapOffset, dataLength, mapLength (big-endian)
 *   map:    +24 typeListOffset, +26 nameListOffset
 *   type list: count-1, then {type, count-1, refListOffset}
 *   ref list:  {id, nameOffset, attrs<<24 | dataOffset, reserved}
 *   data:   {length, bytes} at dataOffset + entry offset
 * Each file is validated against its size so a truncated file cannot make us
 * read out of bounds.
 */
#include "resources.h"

#include <SDL.h>
#include <stdio.h>

typedef struct {
    uint32_t type;
    int16_t id;
    const uint8_t *data;
    int32_t size;
} ResEntry;

static uint8_t *gFiles[2];
static ResEntry *gEntries;
static int gCount;
static char gDataDir[1024];

static uint8_t *LoadFile(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = n > 0 ? malloc((size_t)n) : NULL;
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    *size = n;
    return buf;
}

static bool ParseFork(const uint8_t *d, long size)
{
    if (size < 16) return false;
    uint32_t dataOff = rd32(d), mapOff = rd32(d + 4), mapLen = rd32(d + 12);
    if ((uint64_t)mapOff + mapLen > (uint64_t)size || mapLen < 30) return false;
    const uint8_t *map = d + mapOff;
    uint16_t typeListOff = rd16(map + 24);
    if (typeListOff + 2u > mapLen) return false;
    const uint8_t *tl = map + typeListOff;
    int nTypes = rd16s(tl) + 1;
    for (int t = 0; t < nTypes; t++) {
        if (typeListOff + 2u + (unsigned)t * 8 + 8 > mapLen) return false;
        const uint8_t *te = tl + 2 + t * 8;
        uint32_t type = rd32(te);
        int n = rd16s(te + 4) + 1;
        uint16_t refOff = rd16(te + 6);
        for (int i = 0; i < n; i++) {
            if (typeListOff + (unsigned)refOff + (unsigned)i * 12 + 12 > mapLen) return false;
            const uint8_t *re = tl + refOff + i * 12;
            uint32_t off = rd32(re + 4) & 0xFFFFFF;
            uint64_t at = (uint64_t)dataOff + off;
            if (at + 4 > (uint64_t)size) return false;
            uint32_t len = rd32(d + at);
            if (at + 4 + len > (uint64_t)size) return false;
            ResEntry *e = realloc(gEntries, sizeof(ResEntry) * (size_t)(gCount + 1));
            if (!e) return false;
            gEntries = e;
            gEntries[gCount].type = type;
            gEntries[gCount].id = rd16s(re);
            gEntries[gCount].data = d + at + 4;
            gEntries[gCount].size = (int32_t)len;
            gCount++;
        }
    }
    return true;
}

static bool TryDir(const char *dir)
{
    static const char *names[2] = { "Race.rsrc", "Levels.rsrc" };
    char path[1200];
    for (int i = 0; i < 2; i++) {
        long size;
        snprintf(path, sizeof path, "%s%s", dir, names[i]);
        gFiles[i] = LoadFile(path, &size);
        if (!gFiles[i] || !ParseFork(gFiles[i], size)) {
            Res_Shutdown();
            return false;
        }
    }
    snprintf(gDataDir, sizeof gDataDir, "%s", dir);
    return true;
}

bool Res_Init(void)
{
    char *base = SDL_GetBasePath();
    char dir[1100];
    bool ok = false;
    if (base) {
        const char *subs[] = { "", "../Resources/", "data/", "../" };
        for (int i = 0; i < 4 && !ok; i++) {
            snprintf(dir, sizeof dir, "%s%s", base, subs[i]);
            ok = TryDir(dir);
        }
        SDL_free(base);
    }
    if (!ok) ok = TryDir("./");
    return ok;
}

void Res_Shutdown(void)
{
    free(gEntries);
    gEntries = NULL;
    gCount = 0;
    for (int i = 0; i < 2; i++) { free(gFiles[i]); gFiles[i] = NULL; }
}

const char *Res_DataDir(void) { return gDataDir; }

const uint8_t *GetResource(uint32_t type, int id, int32_t *size)
{
    for (int i = 0; i < gCount; i++) {
        if (gEntries[i].type == type && gEntries[i].id == id) {
            if (size) *size = gEntries[i].size;
            return gEntries[i].data;
        }
    }
    if (size) *size = 0;
    return NULL;
}

bool HasResource(uint32_t type, int id) { return GetResource(type, id, NULL) != NULL; }

void GetString(unsigned char *out, int id)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('S', 'T', 'R', ' '), id, &size);
    out[0] = 0;
    if (d && size > 0 && d[0] < size) memcpy(out, d, (size_t)d[0] + 1);
}

void GetIndString(unsigned char *out, int id, int index)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('S', 'T', 'R', '#'), id, &size);
    out[0] = 0;
    if (!d || size < 2 || index < 1 || index > rd16s(d)) return;
    int32_t o = 2;
    for (int i = 1; i < index; i++) {
        if (o >= size) return;
        o += 1 + d[o];
    }
    if (o < size && o + d[o] < size) memcpy(out, d + o, (size_t)d[o] + 1);
}

/* ---- Pascal-string conversions (TextUtils) ---- */

void NumToString(long n, unsigned char *out)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%ld", n);
    PStrFromC(out, buf);
}

void StringToNum(const unsigned char *s, long *out)
{
    long v = 0;
    int i = 1, neg = 0;
    if (i <= s[0] && (s[i] == '-' || s[i] == '+')) { neg = s[i] == '-'; i++; }
    for (; i <= s[0]; i++) v = v * 10 + (s[i] - '0');
    *out = neg ? -v : v;
}

void UpperString(unsigned char *s)
{
    for (int i = 1; i <= s[0]; i++)
        if (s[i] >= 'a' && s[i] <= 'z') s[i] = (unsigned char)(s[i] - 32);
}

/* ---- QuickDraw Random(): Park-Miller minimal standard generator ---- */

int32_t gRandSeed = 1;

INTEGER Random(void)
{
    int64_t seed = gRandSeed;
    if (seed == 0) seed = 0x2F1D;
    seed = (seed * 16807) % 0x7FFFFFFF;
    gRandSeed = (int32_t)seed;
    INTEGER r = (INTEGER)(seed & 0xFFFF);
    if (r == (INTEGER)0x8000) r = 0;
    return r;
}
