/*
 * display.c - translation of Display.p.
 */
#include "br.h"

/* 'Sped' 1000: needle offsets for each speed (the original declared 71
 * entries; the resource has 47 and the rest read as zero). */
static Point SpeedometerConsts[71];
static uint32_t BlinckTick;

static Str255 NumStr, ScoreString;
static Rect BonRect;

static void LoadSpeedometer(void)
{
    int32_t size;
    const uint8_t *d = GetResource(FOURCC('S', 'p', 'e', 'd'), 1000, &size);
    memset(SpeedometerConsts, 0, sizeof SpeedometerConsts);
    for (int k = 0; d && k < 71 && k * 4 + 4 <= size; k++) {
        SpeedometerConsts[k].h = rd16s(d + k * 4);
        SpeedometerConsts[k].v = rd16s(d + k * 4 + 2);
    }
}

static void Waitproc(void)
{
    if (wait != 0) {
        LoadSpeedometer();
        BlinckTick = 0;
        if (wait > 38) {
            GetIndString(ScoreString, 2000, 6);
            NumToString(Level, NumStr);
            ShowMessage(ScoreString, 310 - 80, 200, 36);
            ShowMessage(NumStr, 310 + 43, 200, 36);
            ShowMessage(Info->Name, 310 - StringWidth(Info->Name) / 2, 250, 36);
        } else if (wait > 1) {
            if ((wait + 2) % 10 == 0) PlaySound(1100);
            SetRect(&BonRect, 620 - 38, 480 - 62, 620 - 6, 480 - 30);
            if (wait > 8) PlotSprite(1021, BonRect);
            else PlotSprite(1022, BonRect);
        } else {
            PlaySound(1101);
            SetRect(&BonRect, 620 - 38, 480 - 62, 620 - 6, 480 - 30);
            PlotSprite(1023, BonRect);
        }
        wait = (INTEGER)(wait - 1);
    }
}

static void GameOProc(void)
{
    if (GameOver) {
        GetIndString(ScoreString, 2000, 3);
        ShowMessage(ScoreString, 310 - 100, 240, 36);
        GameOCount = (INTEGER)(GameOCount + 1);
    }
}

static void DoneProc(void)
{
    long timer;
    if (DoneCount != 0) {
        if (DoneCount > 0) {
            GetIndString(ScoreString, 2000, 4);
            ShowMessage(ScoreString, 310 - 120, 240, 36);
        }
        if ((DoneCount < 0) && (DoneCount > -10)) {
            if (DoneCount <= -1) {
                GetIndString(ScoreString, 2001, 1);
                NumToString(Level, NumStr);
                if (NumStr[0] == 2) {
                    ScoreString[7] = NumStr[1];
                    ScoreString[8] = NumStr[2];
                } else {
                    ScoreString[8] = NumStr[1];
                }
                ShowMessage(ScoreString, 310 - 120, 240 - 100, 24);
                if (DoneCount == -1) PlaySound(1105);
                Delay(15, &timer);
                KillEngine();
            }
            if (DoneCount <= -4) {
                GetIndString(ScoreString, 2001, 3);
                ShowMessage(ScoreString, 310 - 120, 240 - 60, 12);
                NumToString(Score, ScoreString);
                ShowMessage(ScoreString, 310 - 105, 240 - 40, 18);
                if (DoneCount == -4) PlaySound(1105);
                Delay(15, &timer);
            }
            if (DoneCount <= -7) {
                GetIndString(ScoreString, 2001, 4);
                ShowMessage(ScoreString, 310 - 120, 240 - 0, 12);
                NumToString(Info->Bonus, ScoreString);
                ShowMessage(ScoreString, 310 - 105, 240 + 20, 18);
                if (MultiBonus < 0) {
                    SetRect(&BonRect, 310 - 40, 240 + 2, 310 - 16, 240 + 26);
                    PlotSprite((INTEGER)(1030 + abs(MultiBonus)), BonRect);
                    AddUpRect(BonRect);
                    NumToString(Info->Bonus * (abs(MultiBonus) + 1), ScoreString);
                    ShowMessage(ScoreString, 310, 240 + 20, 18);
                    if (DoneCount == -9) Info->Bonus = (INTEGER)(Info->Bonus * (abs(MultiBonus) + 1));
                }
                if (DoneCount == -7) PlaySound(1105);
                Delay(15, &timer);
            }
        }
        if (DoneCount == -11) {
            GetIndString(ScoreString, 2001, 5);
            ShowMessage(ScoreString, 310 - 80, 240 - 29, 24);
            DoneCount = -10;
            if (Info->Bonus > 15) {
                Score = Score + 15;
                Info->Bonus = (INTEGER)(Info->Bonus - 15);
            } else {
                Score = Score + Info->Bonus;
                Info->Bonus = 0;
                DoneCount = -20;
            }
            if (BlinckTick + 41 < TickCount()) PlaySound(1106);
            NumToString(Score, NumStr);
            ShowMessage(NumStr, 310 - 50, 240 + 21, 36);
        }
        if (DoneCount == -20) {
            DisposeLevel();
            Level = (INTEGER)(Level + 1);
            EnterLevel(Level);
        }
    }
}

static void DispProc(void)
{
    INTEGER Speed;
    SetRect(&BonRect, 17, 480 - 86, 81, 480 - 22);
    if (!Dead) {
        Speed = (INTEGER)(UDSpeed - abs(LRSpeed) / 5);
        if (Speed < 0) Speed = 0;
        if (Speed > 70) Speed = 70;
        PlotSprite(1002, BonRect);
        MoveTo(18, 480 - 23);
        PenSize(2, 3);
        Line(SpeedometerConsts[Speed].h, -SpeedometerConsts[Speed].v);
        PenSize(1, 1);

        if (DoneCount == wait) Info->Bonus = (INTEGER)(Info->Bonus - 1);
        if (Info->Bonus == 0) {
            Dead = true;
            PlaySound(1020);
        }

        SetPort(WindowPort(RaceWindow));
        Rect oldclip = thePort->clip;
        Rect NewClip;
        SetRect(&NewClip, 0, 0, 620, 480);
        SectRect(&NewClip, &oldclip, &thePort->clip);

        if (oldScore != Score) {
            if ((Score / 2000 > oldScore / 2000) && (Score / 2000 > Lastlive)) {
                Lives = (INTEGER)(Lives + 1);
                Lastlive = (INTEGER)(Score / 2000);
                PlaySound(1500);
                BlinckTick = TickCount();
            }
            NumToString(Score, NumStr);
            ShowBgMessage(NumStr, 620 - 65, 480 - 10, 12, Sb, Sf);
        }

        NumToString(Info->Bonus, NumStr);
        if (NumStr[0] != 4) {
            NumStr[NumStr[0] + 1] = ' ';
            NumStr[NumStr[0] + 2] = ' ';
            NumStr[0] = (unsigned char)(NumStr[0] + 2);
        }
        ShowBgMessage(NumStr, 620 - 170, 480 - 10, 12, Sb, Sf);

        if ((MultiBonus < 0) && (MultiBonus != oldBonus)) {
            SetRect(&BonRect, 620 - 135, 480 - 22, 620 - 117, 480 - 4);
            PlotCIcon(&BonRect, GetCIcon(1040 + abs(MultiBonus)));
        }
        if (oldLives != Lives) {
            NumToString(Lives, NumStr);
            ShowBgMessage(NumStr, 620 - 250, 480 - 10, 12, Sb, Sf);
        }

        thePort->clip = oldclip;
        SetPort(ScreenWorld);
        oldScore = (INTEGER)Score;   /* oldScore is a 16-bit INTEGER in the original */
        oldLives = Lives;
        oldBonus = MultiBonus;
    } else {
        PlotSprite(1003, BonRect);
    }
}

void DoDisplay(void)
{
    DispProc();

    Waitproc();
    DoneProc();
    GameOProc();
}
