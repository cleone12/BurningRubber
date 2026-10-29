/*
 * gametools.c - translation of GameTools.p (and the variables of
 * GameGlobals.p).
 */
#include "br.h"

/* GameGlobals.p */
bool AccKey, BrakKey, LKey, RKey;
GrafPtr ScreenWorld, Backworld;
INTEGER UDSpeed, LRSpeed, UDPos, LRPos, CarDirection;
Car MyCar;
RoadInfo *MyRoad;
INTEGER Scroll, OldScroll, BackScroll;
LevelInfo *Info;
bool BrakeDone;
Animation Animations[11];
bool Dead, GameOver;
INTEGER OldSpeed, WaitCount;
OBJ *Items;
int ItemCount;
ObjSoft ItemInfo[MAX_OBJS + 1];
INTEGER LastObj;
Rect CarRect;
INTEGER Jump, MaxJump;
INTEGER Lives, DeadCount, DoneCount, GameOCount;
LONGINT Score;
INTEGER MultiBonus, BonPos, BonLR;
REB Rebirthes[16];
Rect URects[16];
INTEGER RectCount;
GrafPtr Clip;
const uint8_t *RoadColData;
Sprite Sprites[LAST_SPRITE + 1];
INTEGER oldScore, oldLives, oldBonus, Lastlive;
bool StandartCLUT;

static bool Toggle;
static INTEGER (*gLevel)[5];
static int gLevelRows;

/* ScreenCT's seed: CopyBits copies indices unchanged while the device
 * table carries the same seed (see BlitToScreen). */
#define SCREEN_CT_SEED 0u

const INTEGER *LevelData(int row)
{
    static const INTEGER empty[5] = { 0, 0, 320, 320, 640 };
    if (!gLevel || row < 0 || row >= gLevelRows) return empty;
    return gLevel[row];
}

const OBJDef *GetODEF(int id)
{
    static OBJDef cache[512];
    static bool have[512];
    static OBJDef none;
    int k = id - 1000;
    if (k >= 0 && k < 512 && have[k]) return &cache[k];
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('O', 'D', 'E', 'F'), id, &size);
    if (!d || size < 24 || k < 0 || k >= 512) return &none;
    OBJDef *o = &cache[k];
    o->icon = rd16s(d);
    o->UD = rd16s(d + 2);
    o->LR = rd16s(d + 4);
    o->Brakes = rd16s(d + 6);
    o->Die = rd16s(d + 8);
    o->Flags1 = d[10];
    o->V = rd16s(d + 12);
    o->H = rd16s(d + 14);
    o->weigth = rd16s(d + 16);
    o->Crash = rd16s(d + 18);
    o->Jump = rd16s(d + 20);
    o->Score = rd16s(d + 22);
    have[k] = true;
    return o;
}

/* ------------------------------------------------------------------ */

void SetCLUT(void)
{
    for (int i = 0; i <= 255; i++) {
        gDeviceCT[i].red = (INTEGER)gSystemPalette[i].red;
        gDeviceCT[i].blue = (INTEGER)gSystemPalette[i].blue;
        gDeviceCT[i].green = (INTEGER)gSystemPalette[i].green;
    }
    SetEntries(0, 255 - 1, gDeviceCT);
    StandartCLUT = true;
}

/* One channel of ScreenBlood: the INTEGER comparisons treat colour values
 * above $7FFF as negative, exactly as the Pascal did. */
static INTEGER Bleed(INTEGER v, bool fade)
{
    if (fade) {
        if (v > 2000 || v < 0) v = (INTEGER)(v - 2000);
    } else if (v >= 0 || v < -6000) {
        v = (INTEGER)(v + 1000);
    }
    return v;
}

void ScreenBlood(int pro, bool R, bool G, bool B)
{
    StandartCLUT = false;
    for (int i2 = 1; i2 <= pro; i2++)
        for (int i = 0; i <= 255; i++) {
            gDeviceCT[i].red = Bleed(gDeviceCT[i].red, R);
            gDeviceCT[i].green = Bleed(gDeviceCT[i].green, G);
            gDeviceCT[i].blue = Bleed(gDeviceCT[i].blue, B);
        }
    SetEntries(0, 255 - 1, gDeviceCT);
}

static INTEGER Heal(INTEGER fade, INTEGER old)
{
    if (fade < old || (fade < 0 && old > 0)) return (INTEGER)(fade + 2000);
    return old;
}

void RemoveBlood(int pro)
{
    for (int i2 = 1; i2 <= pro; i2++)
        for (int i = 0; i <= 255; i++) {
            gDeviceCT[i].red = Heal(gDeviceCT[i].red, (INTEGER)gSystemPalette[i].red);
            gDeviceCT[i].green = Heal(gDeviceCT[i].green, (INTEGER)gSystemPalette[i].green);
            gDeviceCT[i].blue = Heal(gDeviceCT[i].blue, (INTEGER)gSystemPalette[i].blue);
        }
    SetEntries(0, 255 - 1, gDeviceCT);
}

void AddUpRect(Rect r)
{
    /* The original wrote URects[16] (past the array) on overflow. */
    if (RectCount >= 15) return;
    RectCount = (INTEGER)(RectCount + 1);
    URects[RectCount] = r;
}

void DrawInstr(void)
{
    Rect DrawRect;
    Str255 ScoreString, NumStr;
    SetPort(WindowPort(RaceWindow));
    SetRect(&DrawRect, 0, 480 - 100, 100, 480);
    DrawPicture(1000, &DrawRect);
    SetRect(&DrawRect, 100, 480 - 32, 620, 480);
    PlotCIcon(&DrawRect, GetCIcon(1000));
    SetRect(&DrawRect, 310, 480 - 28, 610, 480 + 2);
    DrawPicture(1001, &DrawRect);

    GetIndString(ScoreString, 2000, 5);
    ShowBgMessage(ScoreString, 620 - 210, 480 - 10, 12, Sb, Sf);

    GetIndString(ScoreString, 2000, 2);
    NumToString(Lives, NumStr);
    ShowBgMessage(ScoreString, 620 - 290, 480 - 10, 12, Sb, Sf);
    ShowBgMessage(NumStr, 620 - 250, 480 - 10, 12, Sb, Sf);

    GetIndString(ScoreString, 2000, 1);
    NumToString(Score, NumStr);
    ShowBgMessage(ScoreString, 620 - 110, 480 - 10, 12, Sb, Sf);
    ShowBgMessage(NumStr, 620 - 65, 480 - 10, 12, Sb, Sf);

    if (MultiBonus < 0) {
        SetRect(&DrawRect, 620 - 135, 480 - 22, 620 - 117, 480 - 4);
        PlotCIcon(&DrawRect, GetCIcon(1040 + abs(MultiBonus)));
    }
}

void BlitToScreen(void)
{
    Rect BlitRect;
    if (gDeviceSeed != SCREEN_CT_SEED) {
        if (StandartCLUT) SetCLUT();
        gDeviceSeed = SCREEN_CT_SEED;
    }
    SetRect(&BlitRect, 0, 0, 620, 480 - 100);
    SetPort(WindowPort(RaceWindow));
    CopyBits(ScreenWorld, WindowPort(RaceWindow), &BlitRect, &BlitRect);
    SetRect(&BlitRect, 0, 480 - 100, 620, 480 - 20);
    CopyBitsRgn(ScreenWorld, WindowPort(RaceWindow), &BlitRect, &BlitRect, Clip);
}

bool CheckSect(INTEGER SCR, INTEGER LRP, INTEGER tolerance, INTEGER *Idea)
{
    bool r;
    if (SCR > 0) {
        if (Data(SCR, 1) + tolerance >= LRP) {
            r = true;
            *Idea = 1;
        } else if (Data(SCR, 4) - tolerance <= LRP) {
            r = true;
            *Idea = -1;
        } else if ((Data(SCR, 2) - tolerance <= LRP) && (Data(SCR, 2) != Data(SCR, 3)) &&
                   (Data(SCR, 3) + tolerance >= LRP)) {
            r = true;
            if (LRP - Data(SCR, 2) < Data(SCR, 3) - LRP) *Idea = -1;
            else *Idea = 1;
        } else {
            r = false;
        }
    } else {
        r = false;
    }
    return r;
}

static void LoadLevelResources(void)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('I', 'N', 'F', 'O'), 1000 - 1 + Level % 10, &size);
    Info = calloc(1, sizeof(LevelInfo));
    if (d && size >= 10) {
        Info->DataID = rd16s(d);
        Info->RoadID = rd16s(d + 2);
        Info->ObjID = rd16s(d + 4);
        Info->StopAt = rd16s(d + 6);
        Info->Bonus = rd16s(d + 8);
        if (size > 10 && d[10] < size - 10) memcpy(Info->Name, d + 10, (size_t)d[10] + 1);
    }
    RoadColData = GetResource(FOURCC('C', 'C', 'O', 'L'), Info->RoadID, &size);
    if (size < 8) RoadColData = NULL;

    memset(Rebirthes, 0, sizeof Rebirthes);
    d = GetResource(FOURCC('R', 'S', 'E', 'T'), Info->DataID, &size);
    for (int i = 1; i <= 15 && d && i * 4 <= size; i++) {
        Rebirthes[i].SCR = rd16s(d + (i - 1) * 4);
        Rebirthes[i].LRP = rd16s(d + (i - 1) * 4 + 2);
    }

    MyRoad = calloc(1, sizeof(RoadInfo));
    d = GetResource(FOURCC('R', 'O', 'A', 'D'), Info->RoadID, &size);
    if (d && size >= 37) {
        MyRoad->RoadPat = rd16s(d);
        MyRoad->BackPat = rd16s(d + 2);
        MyRoad->BarrPat = rd16s(d + 4);
        MyRoad->ClutCol = rd16s(d + 6);
        MyRoad->Road = RGB(rd16(d + 8), rd16(d + 10), rd16(d + 12));
        MyRoad->X2 = rd16s(d + 14);
        MyRoad->Back = RGB(rd16(d + 16), rd16(d + 18), rd16(d + 20));
        MyRoad->X3 = rd16s(d + 22);
        MyRoad->Barr = RGB(rd16(d + 24), rd16(d + 26), rd16(d + 28));
        MyRoad->Slide = rd16s(d + 30);
        MyRoad->Acc = rd16s(d + 32);
        MyRoad->Speed = rd16s(d + 34);
        MyRoad->Flags = d[36];
    }

    /* 'OBJS', 1-based, with a sentinel after the last object: the original
     * read on past the end of the handle when spawning objects. */
    d = GetResource(FOURCC('O', 'B', 'J', 'S'), Info->ObjID, &size);
    ItemCount = d ? size / 6 : 0;
    if (ItemCount > MAX_OBJS) ItemCount = MAX_OBJS;
    Items = calloc((size_t)ItemCount + 2, sizeof(OBJ));
    for (int i = 1; i <= ItemCount; i++) {
        Items[i].ID = rd16s(d + (i - 1) * 6);
        Items[i].Scroll = rd16s(d + (i - 1) * 6 + 2);
        Items[i].LR = rd16s(d + (i - 1) * 6 + 4);
    }
    Items[ItemCount + 1].Scroll = 32767;

    d = GetResource(FOURCC('L', 'E', 'V', 'L'), Info->DataID, &size);
    gLevelRows = d ? size / 8 : 0;
    gLevel = calloc((size_t)gLevelRows + 1, sizeof *gLevel);
    for (int i = 0; i < gLevelRows; i++)
        for (int k = 1; k <= 4; k++) gLevel[i][k] = rd16s(d + i * 8 + (k - 1) * 2);
}

void EnterLevel(INTEGER level)
{
    Rect DrawingRect;
    (void)level;
    Scroll = 0;
    BackScroll = 0;
    Jump = 0;
    MaxJump = 0;
    OldScroll = -240;
    LastObj = 0;
    CarDirection = 0;
    WaitCount = 45;
    OldSpeed = 0;
    UDSpeed = 0;
    LRSpeed = 0;
    UDPos = (INTEGER)(480 - GamePrefs.Offset - 16);
    LRPos = 310 - 16;
    for (int i = 1; i <= 10; i++) Animations[i].On = false;
    for (int i = 1; i <= 100; i++) { ItemInfo[i].LR = 0; ItemInfo[i].UD = 0; }
    DeadCount = 0;
    Dead = false;
    BrakeDone = true;
    DoneCount = 0;
    oldBonus = -1;
    GamePause = false;
    SetRect(&CarRect, 0, 0, 0, 0);

    LoadLevelResources();
    for (int i = 1; i <= (Level - 1) / 10; i++) Info->Bonus = (INTEGER)(Info->Bonus - 120 / i);

    MultiBonus = (INTEGER)(abs(Random() % 3) + 1);
    BonPos = (INTEGER)((long)(abs(Random()) * ((Info->StopAt - 360) / 32767.0)) + 200);
    if (!(Data(BonPos, 2) == Data(BonPos, 3))) {
        if (Data(BonPos, 2) - Data(BonPos, 1) > Data(BonPos, 4) - Data(BonPos, 3))
            BonLR = (INTEGER)(Data(BonPos, 1) + (Data(BonPos, 2) - Data(BonPos, 1)) / 2 - 16);
        else
            BonLR = (INTEGER)(Data(BonPos, 3) + (Data(BonPos, 4) - Data(BonPos, 3)) / 2 - 16);
    } else {
        BonLR = (INTEGER)(Data(BonPos, 1) + (Data(BonPos, 4) - Data(BonPos, 1)) / 2);
    }
    DrawInstr();

    if (PrefFlag(kFlagBackTex)) {
        SetPort(Backworld);
        SetRect(&DrawingRect, 0, -44, GamePrefs.TextureSize, 480);
        FillCRect(&DrawingRect, GetPixPat(MyRoad->BackPat));
    }
}

void DisposeLevel(void)
{
    free(Info); Info = NULL;
    free(Items); Items = NULL;
    ItemCount = 0;
    free(MyRoad); MyRoad = NULL;
    free(gLevel); gLevel = NULL;
    gLevelRows = 0;
}

void DoControl(void)
{
    bool Keys[129];
    GetKeys(Keys);
#define KEY(code) ((code) >= 1 && (code) <= 128 ? Keys[(code)] : false)
    AccKey = KEY(GameKeys.AccCode);
    BrakKey = KEY(GameKeys.BrakCode);
    LKey = KEY(GameKeys.LCode);
    RKey = KEY(GameKeys.RCode);
#undef KEY
}

void ShowMessage(const unsigned char *TheMessage, int h, int v, int Size)
{
    Rect update;
    TextSize(Size);
    TextFont(systemFont);
    RGBColor c = RGB(1074, 2015, 21724);
    RGBForeColor(&c);
    SetRect(&update, h, v - Size, h + StringWidth(TheMessage), v + Size / 4);
    AddUpRect(update);
    MoveTo(h, v);
    DrawString(TheMessage);
    ForeColor(blackColor);
}

void ShowBgMessage(const unsigned char *TheMessage, int h, int v, int Size, RGBColor bg, RGBColor fg)
{
    TextSize(Size);
    TextMode(srcCopy);
    TextFont(200);
    RGBBackColor(&bg);
    RGBForeColor(&fg);
    MoveTo(h, v);
    DrawString(TheMessage);
    TextSize(12);
    TextMode(srcOr);
    BackColor(whiteColor);
    ForeColor(blackColor);
}

/* A sprite is the 'cicn' pixel image plus a mask made by plotting the icon
 * into a 1-bit port. On a 1-bit port PlotCIcon draws the icon's 1-bit
 * image through its mask, so white pixels end up transparent. */
static void LoadSprite(INTEGER ID)
{
    if (ID < FIRST_SPRITE || ID > LAST_SPRITE || Sprites[ID].SpriteWorld) return;
    const CIcon *myIcon = GetCIcon(ID);
    if (!myIcon) return;
    Rect b;
    SetRect(&b, 0, 0, myIcon->w, myIcon->h);
    Sprites[ID].MaskPort = NewGWorld(&b);
    Sprites[ID].SpriteWorld = NewGWorld(&b);
    if (!Sprites[ID].MaskPort || !Sprites[ID].SpriteWorld) return;
    for (int i = 0; i < myIcon->w * myIcon->h; i++) {
        Sprites[ID].MaskPort->base[i] = myIcon->mask[i] & myIcon->bmap[i];
        Sprites[ID].SpriteWorld->base[i] = myIcon->pix[i];
    }
}

void DisposeSprites(void)
{
    for (int id = FIRST_SPRITE; id <= LAST_SPRITE; id++) {
        DisposeGWorld(Sprites[id].SpriteWorld);
        DisposeGWorld(Sprites[id].MaskPort);
        Sprites[id].SpriteWorld = Sprites[id].MaskPort = NULL;
    }
}

void PreLoadSprites(void)
{
    int32_t size;
    const uint8_t *LoadList = GetResource(FOURCC('P', 'r', 'L', 'd'), 1000, &size);
    ShowCursor();
    for (int count = 1; LoadList && count * 2 <= size; count++) LoadSprite(rd16s(LoadList + (count - 1) * 2));
    ObscureCursor();
}

void PlotSprite(INTEGER ID, Rect Where)
{
    Rect ScreenSize;
    SetRect(&ScreenSize, 0, 0, 620, 480);
    if (SectRect(&Where, &ScreenSize, &ScreenSize)) {
        if (ID < FIRST_SPRITE || ID > LAST_SPRITE) return;
        if (!Sprites[ID].SpriteWorld) LoadSprite(ID);
        if (!Sprites[ID].SpriteWorld) return;
        CopyMask(Sprites[ID].SpriteWorld, Sprites[ID].MaskPort, ScreenWorld,
                 &Sprites[ID].MaskPort->bounds, &Where);
    }
}

void PlaySound(INTEGER ID)
{
    if (PrefFlag(kFlagSoundFX) && !SoundDisable) {
        int chan = Toggle ? kEffectAChannel : kEffectBChannel;
        Snd_Quiet(chan);
        Snd_Play(chan, ID);
        Toggle = !Toggle;
    }
}

void Engine(INTEGER Speed)
{
    if (PrefFlag(kFlagEngine) && !SoundDisable) {
        Snd_Flush(kEngineChannel);
        Snd_SetRate(kEngineChannel, PRound(((Speed + Random() % 2) / 50.0) * 8000 + 1000));
        Snd_SetAmp(kEngineChannel, (int)PRound(((Speed + Random() % 3) / 50.0) * 205 + 50));
        if (Speed != 0) Snd_Play(kEngineChannel, 2000);
    }
}

void KillEngine(void)
{
    Snd_Quiet(kEngineChannel);
}
