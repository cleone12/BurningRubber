/*
 * roaddraw.c - translation of RoadDraw.p.
 *
 * Each frame the off-screen world is scrolled with ScrollRect and only the
 * rectangles that changed are repainted (background texture plus road);
 * objects that do not move simply scroll along with the picture.
 */
#include "br.h"

/* MakeLine, as in the PowerPC 8-bit path: copies the road colour bytes of
 * 'CCOL' straight into two rows of the off-screen pixel map. */
static void MakeLine(INTEGER fro, INTEGER tow, INTEGER V, INTEGER LLimit, INTEGER RLimit)
{
    if ((fro <= RLimit) && (tow > LLimit) && (V < 462) && (V >= 0) && (RLimit <= 620)) {
        if (fro < LLimit) fro = LLimit;
        if (tow > RLimit) tow = RLimit;
        if (!RoadColData) return;
        for (int row = 0; row < 2; row++) {
            int y = V + row;
            if (y < ScreenWorld->bounds.top || y >= ScreenWorld->bounds.bottom) continue;
            uint8_t *dst = ScreenWorld->base + (y - ScreenWorld->bounds.top) * ScreenWorld->stride;
            int count = tow - fro, x = fro;
            /* 8-, 4-, 2- and 1-byte stores, each from the start of the pattern */
            while (count >= 8) { if (x >= 0 && x + 8 <= 620) memcpy(dst + x, RoadColData, 8); x += 8; count -= 8; }
            if (count >= 4) { if (x >= 0 && x + 4 <= 620) memcpy(dst + x, RoadColData, 4); x += 4; count -= 4; }
            if (count >= 2) { if (x >= 0 && x + 2 <= 620) memcpy(dst + x, RoadColData, 2); x += 2; count -= 2; }
            if (count > 0) { if (x >= 0 && x < 620) dst[x] = RoadColData[0]; }
        }
    }
}

static void ClearRect(Rect position)
{
    Rect texturerect;
    if (PrefFlag(kFlagBackTex)) {
        texturerect = position;
        OffsetRect(&texturerect, 0, BackScroll + 20);
        CopyBits(Backworld, ScreenWorld, &texturerect, &position);
    } else {
        EraseRect(&position);
    }
}

static void DrawRoad(Rect Pos, INTEGER BotScroll)
{
    if (!BitTstByte(MyRoad->Flags, 3))
        for (int i = BotScroll / 2; i <= (BotScroll + Pos.bottom - Pos.top + 1) / 2; i++) {
            INTEGER V = (INTEGER)(480 - (i - Scroll) * 2);
            if (Data(i, 3) == Data(i, 2)) {
                MakeLine(Data(i, 1), Data(i, 4), V, Pos.left, Pos.right);
            } else {
                MakeLine(Data(i, 1), Data(i, 2), V, Pos.left, Pos.right);
                MakeLine(Data(i, 3), Data(i, 4), V, Pos.left, Pos.right);
            }
        }
}

static void UpdateARect(Rect where)
{
    ClearRect(where);
    DrawRoad(where, (INTEGER)(Scroll * 2 + 480 - where.bottom));
}

static void ScrollBack(void)
{
    Rect ScrollingRect;
    if (Scroll != OldScroll) {
        SetPort(ScreenWorld);
        SetRect(&ScrollingRect, 0, 0, 620, 480 - (Scroll - OldScroll));
        ScrollRect(&ScrollingRect, 0, (Scroll - OldScroll) * 2);
        BackScroll = (INTEGER)(BackScroll - (Scroll - OldScroll) * 2);
        while (BackScroll < -64) BackScroll = (INTEGER)(BackScroll + 64);
    }
}

static void UpdateRects(void)
{
    Rect ObRect, IgnoreRect, ScreenRect;
    PenSize(1, 2);
    if (!PrefFlag(kFlagBackTex)) RGBBackColor(&MyRoad->Back);

    if (UDSpeed >= 0) SetRect(&ObRect, 0, 0, 620, (Scroll - OldScroll) * 2);
    else SetRect(&ObRect, 0, 480 - 70, 620, 480);
    UpdateARect(ObRect);
    ObRect = CarRect;
    OffsetRect(&ObRect, 0, (Scroll - OldScroll) * 2);
    UpdateARect(ObRect);

    if (UDSpeed < 0) {
        SetRect(&ObRect, 17, 480 - 85, 81, 480 - 21);
        OffsetRect(&ObRect, 0, (Scroll - OldScroll) * 2);
        UpdateARect(ObRect);
    }

    SetRect(&ScreenRect, 0, 0, 620, 480 - (Scroll - OldScroll));
    for (int i = 1; i <= LastObj; i++) {
        const OBJDef *Def = GetODEF(Items[i].ID);
        int top = -((Items[i].Scroll * 2 - Scroll * 2) - 480);
        SetRect(&ObRect, Items[i].LR, top, Items[i].LR + Def->V + 32, top + Def->H + 32);
        if (SectRect(&ObRect, &ScreenRect, &IgnoreRect) &&
            ((ItemInfo[i].UD != 0) || (ItemInfo[i].LR != 0) || BitTstByte(Def->Flags1, 4)))
            UpdateARect(ObRect);
    }

    for (int i = 1; i <= 10; i++)
        if (Animations[i].On)
            if (SectRect(&Animations[i].Pos, &ScreenRect, &IgnoreRect)) UpdateARect(Animations[i].Pos);

    for (int i = 1; i <= 15; i++)
        if (!EmptyRect(&URects[i]))
            if (SectRect(&URects[i], &ScreenRect, &IgnoreRect)) {
                OffsetRect(&URects[i], 0, (Scroll - OldScroll) * 2);
                UpdateARect(URects[i]);
            }

    SetRect(&ObRect, BonLR, 480 - (BonPos - Scroll) * 2, BonLR + 24, 480 - (BonPos - Scroll) * 2 + 24);
    if (SectRect(&ObRect, &ScreenRect, &IgnoreRect)) UpdateARect(ObRect);

    if (MultiBonus < 0) {
        SetRect(&ObRect, 620 - 130, 7 + (Scroll - OldScroll) * 2, 620 - 112, 25 + (Scroll - OldScroll) * 2);
        UpdateARect(ObRect);
    }

    PenSize(1, 1);
    BackColor(whiteColor);
    ForeColor(blackColor);
}

void DoBackground(void)
{
    ScrollBack();
    UpdateRects();
    OldScroll = Scroll;
    RectCount = 0;
}
