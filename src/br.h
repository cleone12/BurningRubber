/*
 * br.h - Burning Rubber game declarations: the C translation of the Pascal
 * units Globals.p and GameGlobals.p, plus the interfaces of the other units.
 *
 * Each .c file next to this one is a translation of the .p unit with the
 * same name (Tools.p -> tools.c, ...). Pascal INTEGER is kept as 16 bits.
 * Arrays the original declared 1-based keep their Pascal indices; element 0
 * is unused.
 */
#ifndef BR_H
#define BR_H

#include "mac.h"
#include "qd.h"
#include "text.h"
#include "platform.h"
#include "toolbox.h"
#include "resources.h"
#include "prefs.h"
#include "sound.h"

/* ------------------------------------------------------------------ */
/* Globals.p                                                           */
/* ------------------------------------------------------------------ */

extern WindowPtr ShellWindow, RaceWindow;
extern MenuHandle AppleMenu, FileMenu, OptMenu;
extern bool GamePlay, GamePause;
extern INTEGER CarID, Level;
extern uint8_t ErrFlags;
extern INTEGER Depth;
extern bool SoundDisable;
extern bool Cheater;
extern RGBColor Sb, Sf;

/* GrafFlags bits (BitTst numbering) */
enum { kFlagEmboss = 0, kFlagBlankScreen = 1, kFlagRoadTex = 2, kFlagBackTex = 3,
       kFlagSoundFX = 4, kFlagEngine = 5, kFlagSpeed = 6 };
static inline bool PrefFlag(int bit) { return BitTstWord(GamePrefs.GrafFlags, bit); }

/* ------------------------------------------------------------------ */
/* GameGlobals.p                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    INTEGER BaseIcon;
    INTEGER Crash, Drawn;
    INTEGER Speed, Acc, Brakes;
    INTEGER Steering;
    INTEGER Frames;
    INTEGER Bumping, OffRoad;
} Car;

typedef struct {
    INTEGER DataID, RoadID, ObjID, StopAt, Bonus;
    Str255 Name;
} LevelInfo;

typedef struct {
    INTEGER RoadPat, BackPat, BarrPat, ClutCol;
    RGBColor Road;
    INTEGER X2;
    RGBColor Back;
    INTEGER X3;
    RGBColor Barr;
    INTEGER Slide, Acc, Speed;
    uint8_t Flags;      /* BitTst bits: 0 kill, 1 drown, 2 brake, 3 don't draw road */
} RoadInfo;

/* 'LEVL': one row per scroll unit, [1] left edge, [2]/[3] the gap between
 * two lanes (equal when there is none), [4] right edge. */
typedef INTEGER LevelRow[5];

typedef struct {
    bool On;
    INTEGER Frame;
    Rect Pos;
    const uint8_t *Data;    /* 'ANIM' resource */
    int32_t DataSize;
} Animation;

typedef struct { INTEGER ID, Scroll, LR; } OBJ;
typedef struct { INTEGER LR, UD; } ObjSoft;
typedef struct { INTEGER SCR, LRP; } REB;

typedef struct {
    GrafPtr SpriteWorld;
    GrafPtr MaskPort;
} Sprite;

/* 'ODEF' (declared locally in Objects.p and RoadDraw.p as OBJDef) */
typedef struct {
    INTEGER icon;
    INTEGER UD, LR;
    INTEGER Brakes;
    INTEGER Die;
    uint8_t Flags1;     /* BitTst bits: 0 crash car, 1 collect, 3 navigate,
                           4 slide, 6 jump, 7 crashes into the roadside */
    INTEGER V, H;
    INTEGER weigth, Crash, Jump, Score;
} OBJDef;

#define MAX_OBJS 150
#define FIRST_SPRITE 1001
#define LAST_SPRITE 1821

extern bool AccKey, BrakKey, LKey, RKey;
extern GrafPtr ScreenWorld, Backworld;
extern INTEGER UDSpeed, LRSpeed, UDPos, LRPos, CarDirection;
extern Car MyCar;
extern RoadInfo *MyRoad;
extern INTEGER Scroll, OldScroll, BackScroll;
extern LevelInfo *Info;
extern bool BrakeDone;
extern Animation Animations[11];
extern bool Dead, GameOver;
extern INTEGER OldSpeed, WaitCount;
extern OBJ *Items;
extern int ItemCount;
extern ObjSoft ItemInfo[MAX_OBJS + 1];
extern INTEGER LastObj;
extern Rect CarRect;
extern INTEGER Jump, MaxJump;
extern INTEGER Lives, DeadCount, DoneCount, GameOCount;
extern LONGINT Score;
extern INTEGER MultiBonus, BonPos, BonLR;
extern REB Rebirthes[16];
extern Rect URects[16];
extern INTEGER RectCount;
extern GrafPtr Clip;
extern const uint8_t *RoadColData;
extern Sprite Sprites[LAST_SPRITE + 1];
extern INTEGER oldScore, oldLives, oldBonus, Lastlive;
extern bool StandartCLUT;

/* Level data access; rows outside the level read as an empty full-width road. */
const INTEGER *LevelData(int row);
#define Data(row, col) (LevelData(row)[col])
const OBJDef *GetODEF(int id);

/* ------------------------------------------------------------------ */
/* unit interfaces                                                     */
/* ------------------------------------------------------------------ */

/* Tools.p */
void InitProg(void);
void InitSound(void);
void Quit(void);
void ExitToShell(void);
void DoError(int id);
void HideMenuBar(void);
void ShowMenuBar(void);
void ReHideMenuBar(void);
void EnterHighScore(LONGINT points);
void DisplayScores(void);
void OffsetPt(Point *pt, int h, int v);

/* GameTools.p */
bool CheckSect(INTEGER SCR, INTEGER LRP, INTEGER tolerance, INTEGER *Idea);
void AddUpRect(Rect r);
void EnterLevel(INTEGER level);
void DisposeLevel(void);
void DoControl(void);
void ShowMessage(const unsigned char *TheMessage, int h, int v, int Size);
void ShowBgMessage(const unsigned char *TheMessage, int h, int v, int Size, RGBColor bg, RGBColor fg);
void PreLoadSprites(void);
void PlotSprite(INTEGER ID, Rect Where);
void BlitToScreen(void);
void DrawInstr(void);
void SetCLUT(void);
void ScreenBlood(int pro, bool R, bool G, bool B);
void RemoveBlood(int pro);
void KillEngine(void);
void PlaySound(INTEGER ID);
void Engine(INTEGER Speed);
void DisposeSprites(void);

/* Shell.p */
void DoShell(void);

/* PowerShell.p */
void PowerShell(void);
void HandlePSClick(Point Hit, INTEGER *reply);
void PSUpdate(void);
void PSAbout(void);
void PSHelp(void);

/* Game.p */
void DoGame(void);
void BeginGame(void);
void EndGame(void);
void GameUpdate(void);

/* Car.p, Objects.p, Animations.p, Display.p, RoadDraw.p */
void DoCar(void);
void DoObjects(void);
void Animate(INTEGER ID, Rect where);
void DoAnimations(void);
void DoDisplay(void);
void DoBackground(void);

#endif
