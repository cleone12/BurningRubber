/*
 * prefs.h - persistent state.
 *
 * The original stored its settings ('SETT'), controls ('CTLS' plus the key
 * name 'STR 's) and high scores ('HIGH') by writing back into its own
 * resource fork with ChangedResource/WriteResource, and threw away unsaved
 * edits with ReleaseResource. Those semantics are kept here: the game edits
 * a working copy, Prefs_Commit*() plays the part of WriteResource and
 * Prefs_Revert*() the part of ReleaseResource. The committed state lives in
 * a text file in the per-user preferences directory instead of the game file.
 */
#ifndef PREFS_H
#define PREFS_H

#include "mac.h"

/* 'SETT' 1000 (Globals.p: Preferences). GrafFlags is a Pascal Byte, which
 * occupies a 16-bit word in an unpacked record; BitTst(@GrafFlags, n) tests
 * mask 0x8000 >> n. */
typedef struct {
    INTEGER Width, Height;
    uint16_t GrafFlags;
    INTEGER TextureSize, Speed, X, Offset;
} Preferences;

/* 'CTLS' 1000: key indices into GetKeys (Mac key code + 1). */
typedef struct { INTEGER AccCode, BrakCode, LCode, RCode; } Keys;

typedef struct { LONGINT Score; Str255 Name; } ScoreEntry;

typedef struct { int windowW, windowH; bool fullscreen; } PortPrefs;

extern Preferences GamePrefs;
extern Keys GameKeys;
extern Str255 gKeyNames[4];        /* 'STR ' 11000..11003: left, right, accelerate, brake */
extern ScoreEntry gHighScores[11]; /* 'HIGH' 128..138 */
extern ScoreEntry gHighDefault;    /* 'HIGH' 1000 */
extern PortPrefs gPortPrefs;

void Prefs_Load(void);
void Prefs_Save(void);             /* writes the committed state to disk */

void Prefs_CommitSettings(void);
void Prefs_RevertSettings(void);
void Prefs_CommitControls(void);
void Prefs_RevertControls(void);
void Prefs_CommitScores(void);

#endif
