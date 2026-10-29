/*
 * prefs.c - persistent state (see prefs.h).
 */
#include "prefs.h"
#include "resources.h"

#include <SDL.h>
#include <stdio.h>

Preferences GamePrefs;
Keys GameKeys;
Str255 gKeyNames[4];
ScoreEntry gHighScores[11];
ScoreEntry gHighDefault;
PortPrefs gPortPrefs;

static Preferences gSavedPrefs;
static Keys gSavedKeys;
static Str255 gSavedKeyNames[4];

static void ParseSETT(const uint8_t *d, int32_t size, Preferences *p)
{
    memset(p, 0, sizeof *p);
    if (size < 14) return;
    p->Width = rd16s(d);
    p->Height = rd16s(d + 2);
    p->GrafFlags = rd16(d + 4);
    p->TextureSize = rd16s(d + 6);
    p->Speed = rd16s(d + 8);
    p->X = rd16s(d + 10);
    p->Offset = rd16s(d + 12);
}

static void ParseHIGH(int id, ScoreEntry *e)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('H', 'I', 'G', 'H'), id, &size);
    memset(e, 0, sizeof *e);
    if (!d || size < 5) return;
    e->Score = (LONGINT)rd32(d);
    int n = d[4] < size - 5 ? d[4] : size - 5;
    e->Name[0] = (unsigned char)n;
    memcpy(e->Name + 1, d + 5, (size_t)n);
}

static void Defaults(void)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('S', 'E', 'T', 'T'), 1000, &size);
    if (d) ParseSETT(d, size, &GamePrefs);
    /* The shipped 'CTLS' used the numeric keypad (8/5/4/6). Most current
     * keyboards have none, so the port defaults to the arrow keys; the
     * Controls dialog still rebinds them exactly as before. */
    GameKeys.AccCode = 0x7E + 1;
    GameKeys.BrakCode = 0x7D + 1;
    GameKeys.LCode = 0x7B + 1;
    GameKeys.RCode = 0x7C + 1;
    GetIndString(gKeyNames[0], 3000, GameKeys.LCode);
    GetIndString(gKeyNames[1], 3000, GameKeys.RCode);
    GetIndString(gKeyNames[2], 3000, GameKeys.AccCode);
    GetIndString(gKeyNames[3], 3000, GameKeys.BrakCode);
    for (int i = 0; i < 11; i++) ParseHIGH(128 + i, &gHighScores[i]);
    ParseHIGH(1000, &gHighDefault);
    gPortPrefs.windowW = gPortPrefs.windowH = 0;
    gPortPrefs.fullscreen = false;
}

static char *PrefsPath(void)
{
    static char path[1200];
    char *dir = SDL_GetPrefPath("Jonas Echterhoff", "Burning Rubber");
    if (!dir) return NULL;
    snprintf(path, sizeof path, "%sBurning Rubber Prefs.txt", dir);
    SDL_free(dir);
    return path;
}

static void ReadPString(const char *s, unsigned char *out)
{
    size_t n = strcspn(s, "\r\n");
    if (n > 255) n = 255;
    out[0] = (unsigned char)n;
    memcpy(out + 1, s, n);
}

void Prefs_Load(void)
{
    Defaults();
    char *path = PrefsPath();
    FILE *f = path ? fopen(path, "r") : NULL;
    if (f) {
        char line[600];
        while (fgets(line, sizeof line, f)) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            const char *k = line, *v = eq + 1;
            int a, b, c, d, i;
            unsigned u;
            long score;
            if (!strcmp(k, "window") && sscanf(v, "%dx%d", &a, &b) == 2) { gPortPrefs.windowW = a; gPortPrefs.windowH = b; }
            else if (!strcmp(k, "fullscreen")) gPortPrefs.fullscreen = atoi(v) != 0;
            else if (!strcmp(k, "flags") && sscanf(v, "%x", &u) == 1) GamePrefs.GrafFlags = (uint16_t)u;
            else if (!strcmp(k, "speed")) GamePrefs.Speed = (INTEGER)atoi(v);
            else if (!strcmp(k, "keys") && sscanf(v, "%d,%d,%d,%d", &a, &b, &c, &d) == 4) {
                GameKeys.AccCode = (INTEGER)a; GameKeys.BrakCode = (INTEGER)b;
                GameKeys.LCode = (INTEGER)c; GameKeys.RCode = (INTEGER)d;
            }
            else if (sscanf(k, "keyname%d", &i) == 1 && i >= 0 && i < 4) ReadPString(v, gKeyNames[i]);
            else if (sscanf(k, "high%d", &i) == 1 && i >= 0 && i < 11) {
                char *comma = strchr(v, ',');
                if (comma && sscanf(v, "%ld", &score) == 1) {
                    gHighScores[i].Score = (LONGINT)score;
                    ReadPString(comma + 1, gHighScores[i].Name);
                }
            }
        }
        fclose(f);
    }
    gSavedPrefs = GamePrefs;
    gSavedKeys = GameKeys;
    memcpy(gSavedKeyNames, gKeyNames, sizeof gKeyNames);
}

static void WritePString(FILE *f, const unsigned char *s)
{
    for (int i = 1; i <= s[0]; i++)
        if (s[i] != '\n' && s[i] != '\r') fputc(s[i], f);
    fputc('\n', f);
}

void Prefs_Save(void)
{
    char *path = PrefsPath();
    FILE *f = path ? fopen(path, "w") : NULL;
    if (!f) return;
    fprintf(f, "window=%dx%d\nfullscreen=%d\n", gPortPrefs.windowW, gPortPrefs.windowH, gPortPrefs.fullscreen ? 1 : 0);
    fprintf(f, "flags=%04x\nspeed=%d\n", gSavedPrefs.GrafFlags, gSavedPrefs.Speed);
    fprintf(f, "keys=%d,%d,%d,%d\n", gSavedKeys.AccCode, gSavedKeys.BrakCode, gSavedKeys.LCode, gSavedKeys.RCode);
    for (int i = 0; i < 4; i++) { fprintf(f, "keyname%d=", i); WritePString(f, gSavedKeyNames[i]); }
    for (int i = 0; i < 11; i++) { fprintf(f, "high%d=%ld,", i, (long)gHighScores[i].Score); WritePString(f, gHighScores[i].Name); }
    fclose(f);
}

void Prefs_CommitSettings(void) { gSavedPrefs = GamePrefs; Prefs_Save(); }
void Prefs_RevertSettings(void) { GamePrefs = gSavedPrefs; }

void Prefs_CommitControls(void)
{
    gSavedKeys = GameKeys;
    memcpy(gSavedKeyNames, gKeyNames, sizeof gKeyNames);
    Prefs_Save();
}

void Prefs_RevertControls(void)
{
    GameKeys = gSavedKeys;
    memcpy(gKeyNames, gSavedKeyNames, sizeof gKeyNames);
}

void Prefs_CommitScores(void) { Prefs_Save(); }
