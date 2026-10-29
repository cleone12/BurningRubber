/*
 * animations.c - translation of Animations.p.
 *
 * 'ANIM': Scroll (Boolean word), Sound, then frames. A frame below 1000 is a
 * palette command followed by its argument: -1 resets the palette, 0 heals
 * it by the argument's steps, anything else bleeds it, its low three bits
 * (BTst numbering) choosing red, green and blue.
 */
#include "br.h"

static INTEGER Frames(const Animation *a, int k) { return rd16s(a->Data + 4 + (k - 1) * 2); }
static int FrameCount(const Animation *a) { return (a->DataSize - 4) / 2; }

static void DoPolice(void)
{
    PlaySound(1208);
    KillEngine();
    SetPort(WindowPort(RaceWindow));
    if (Alert(1004, NULL) == 1) {
        Score = Score - 500;
        if (Score < 0) Score = 0;
        DrawInstr();
    } else if (Random() > 10000) {
        Dead = true;
        PlaySound(1207);
        Animate(1002, CarRect);
    }
    if (PrefFlag(kFlagBlankScreen)) ReHideMenuBar();
    SetPort(ScreenWorld);
    LKey = false;
    RKey = false;
    BrakKey = false;
    AccKey = false;
    UDSpeed = 0;
    LRSpeed = 0;
}

void Animate(INTEGER ID, Rect where)
{
    if (ID != 1) {
        for (int i = 1; i <= 10; i++) {
            if (!Animations[i].On) {
                if (ID == 0) {
                    if (BitTstByte(MyRoad->Flags, 0)) ID = 1020;
                    else ID = 1021;
                }
                int32_t size;
                const uint8_t *d = GetResource(FOURCC('A', 'N', 'I', 'M'), ID, &size);
                if (!d || size < 4) return;
                Animations[i].On = true;
                Animations[i].Frame = 1;
                Animations[i].Pos = where;
                Animations[i].Data = d;
                Animations[i].DataSize = size;
                PlaySound(rd16s(d + 2));
                return;
            }
        }
    } else {
        DoPolice();
    }
}

void DoAnimations(void)
{
    StandartCLUT = true;
    for (int i = 1; i <= 10; i++)
        if (Animations[i].On) {
            Animation *a = &Animations[i];
            for (;;) {   /* label 1 */
                if (a->Frame < FrameCount(a)) {
                    INTEGER f = Frames(a, a->Frame);
                    if (f < 1000) {
                        if (f == -1) SetCLUT();
                        if (f == 0) RemoveBlood(Frames(a, a->Frame + 1));
                        else ScreenBlood(Frames(a, a->Frame + 1), BTst(f, 0), BTst(f, 1), BTst(f, 2));
                        a->Frame = (INTEGER)(a->Frame + 2);
                        continue;   /* goto 1 */
                    }
                    PlotSprite(f, a->Pos);
                    OffsetRect(&a->Pos, 0, UDSpeed);
                } else {
                    a->On = false;
                }
                a->Frame = (INTEGER)(a->Frame + 1);
                break;
            }
        }
}
