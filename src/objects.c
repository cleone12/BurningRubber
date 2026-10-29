/*
 * objects.c - translation of Objects.p.
 *
 * The Pascal nested procedures share DoObjects' loop variable i and the
 * current object's definition Def; they are file statics here.
 */
#include "br.h"

static INTEGER i;
static OBJDef Def;
static Rect Recy;

static inline int ObjTop(int k) { return -((Items[k].Scroll * 2 - Scroll * 2) - 480); }

static void ObjRectOf(int k, const OBJDef *d, Rect *r)
{
    SetRect(r, Items[k].LR, ObjTop(k), Items[k].LR + d->V + 32, ObjTop(k) + d->H + 32);
}

static void DoBonus(void)
{
    Rect BonRect, Ignore;
    if (Scroll + 480 >= BonPos) {
        SetRect(&BonRect, BonLR, 480 - (BonPos - Scroll) * 2, BonLR + 24, 480 - (BonPos - Scroll) * 2 + 24);
        PlotSprite((INTEGER)(1030 + MultiBonus), BonRect);
        InsetRect(&BonRect, -5, -5);
        if (SectRect(&BonRect, &CarRect, &Ignore)) {
            BonPos = -1000;
            PlaySound(1107);
            MultiBonus = (INTEGER)(MultiBonus * (-1));
            AddUpRect(BonRect);
        }
    }
}

static void CrashObjs(INTEGER *AUD, INTEGER *ALR, INTEGER *BUD, INTEGER *BLR, INTEGER *AMUD, INTEGER *AMLR,
                      INTEGER AMASS, INTEGER BMASS, Rect ARec, Rect BRec)
{
    INTEGER BUOLD, BLOLD;
    if (abs(*AUD - *BUD) > abs(*ALR - *BLR) * 2) {
        if (*AUD < *BUD) *AMUD = (INTEGER)(*AMUD + (ARec.bottom - BRec.top));
        else *AMUD = (INTEGER)(*AMUD + (BRec.bottom - ARec.top));
    } else {
        if (*ALR < *BLR) *AMLR = (INTEGER)(*AMLR + (ARec.right - BRec.left));
        else *AMLR = (INTEGER)(*AMLR + (ARec.left - BRec.right));
    }

    BUOLD = *BUD;
    BLOLD = *BLR;

    *BUD = (INTEGER)PRound(*AUD * ((double)AMASS / BMASS));
    *BLR = (INTEGER)PRound(*ALR * ((double)AMASS / BMASS));
    *AUD = (INTEGER)PRound(BUOLD * ((double)BMASS / AMASS));
    *ALR = (INTEGER)PRound(BLOLD * ((double)BMASS / AMASS));
}

static void PoliceNav(void)
{
    INTEGER ignore;
    if ((Items[i].Scroll < Scroll + GamePrefs.Offset / 2) && (ItemInfo[i].UD < 16))
        ItemInfo[i].UD = (INTEGER)(ItemInfo[i].UD + 1);
    if ((Items[i].Scroll > Scroll + GamePrefs.Offset / 2) && (ItemInfo[i].UD > 8))
        ItemInfo[i].UD = (INTEGER)(ItemInfo[i].UD - 3);
    if (ItemInfo[i].UD < 9) ItemInfo[i].UD = 9;
    if (BitTstByte(Def.Flags1, 3)) {
        ItemInfo[i].LR = 0;
        if ((Items[i].LR < LRPos) && !CheckSect(Items[i].Scroll, (INTEGER)(Items[i].LR + 10), 20, &ignore))
            ItemInfo[i].LR = (INTEGER)(ItemInfo[i].UD / 2);
        else if ((Items[i].LR > LRPos) && !CheckSect(Items[i].Scroll, (INTEGER)(Items[i].LR - 10), 20, &ignore))
            ItemInfo[i].LR = (INTEGER)(-ItemInfo[i].UD / 2);
    } else {
        if (Items[i].LR < LRPos) ItemInfo[i].LR = (INTEGER)(ItemInfo[i].UD / 2);
        else ItemInfo[i].LR = (INTEGER)(-ItemInfo[i].UD / 2);
    }
}

/* NavProc; returns true where the Pascal did Exit(DoNvgte). */
static bool NavProc(int y)
{
    INTEGER Direction;
    if (CheckSect((INTEGER)(Items[i].Scroll + y * 4 - (Def.H + 32) / 2), (INTEGER)(Items[i].LR + (Def.V + 32) / 2), 22, &Direction)) {
        if (y > 22) ItemInfo[i].LR = (INTEGER)(Direction * abs(Def.UD) / 2);
        else ItemInfo[i].LR = (INTEGER)(Direction * abs(Def.UD));
        return true;
    }
    return false;
}

static void doNvgte(void)
{
    if (Def.UD > 0) {
        for (int y = 2; y <= 25; y++)
            if (NavProc(y)) return;
    } else {
        for (int y = -2; y >= -25; y--)
            if (NavProc(y)) return;
    }
}

static void CarTouch(void)
{
    Rect ObjRect;
    INTEGER X, Y;
    ObjRectOf(i, &Def, &ObjRect);
    if (SectRect(&ObjRect, &CarRect, &Recy) && (Jump == 0) && !Dead) {
        if (Def.Brakes != 0) {
            if (UDSpeed > (Def.Brakes * MyCar.OffRoad) / 100) {
                UDSpeed = (INTEGER)((Def.Brakes * MyCar.OffRoad) / 100);
                LRSpeed = (INTEGER)(LRSpeed / 2);
            }
        }
        if (BitTstByte(Def.Flags1, 1)) {
            Animate(Def.Die, ObjRect);
            Items[i].Scroll = -1000;
            Score = Score + Def.Score;
        }
        if (BitTstByte(Def.Flags1, 0) && !Dead)
            if ((sign(Def.Crash) * UDSpeed > ItemInfo[i].UD + Def.Crash) && (DoneCount == 0)) {
                Animate(MyCar.Crash, CarRect);
                Dead = true;
            }
        if (BitTstByte(Def.Flags1, 6)) {
            MaxJump = (INTEGER)((UDSpeed * Def.Jump) / 100);
            if (MaxJump > 0) Jump = 1;
        }
        if (BitTstByte(Def.Flags1, 4)) {
            X = LRSpeed;
            Y = UDSpeed;
            CrashObjs(&ItemInfo[i].UD, &ItemInfo[i].LR, &Y, &X, &Items[i].Scroll, &Items[i].LR, Def.weigth,
                      MyCar.Bumping, ObjRect, CarRect);
            UDSpeed = (INTEGER)(UDSpeed - (UDSpeed - Y) / 4);
            LRSpeed = (INTEGER)(LRSpeed - (LRSpeed - X) / 4);
            PlaySound(1102);
        }
    }
}

static void MoveObj(void)  /* Move */
{
    Items[i].LR = (INTEGER)(Items[i].LR + ItemInfo[i].LR);
    Items[i].Scroll = (INTEGER)(Items[i].Scroll + ItemInfo[i].UD);
    if (Def.Die == 1) {
        PoliceNav();
    } else {
        if (ItemInfo[i].LR > Def.LR) ItemInfo[i].LR = (INTEGER)(ItemInfo[i].LR - 1);
        if (ItemInfo[i].LR < Def.LR) ItemInfo[i].LR = (INTEGER)(ItemInfo[i].LR + 1);
        if (ItemInfo[i].UD > Def.UD) ItemInfo[i].UD = (INTEGER)(ItemInfo[i].UD - 1);
        if (ItemInfo[i].UD < Def.UD) ItemInfo[i].UD = (INTEGER)(ItemInfo[i].UD + 1);
        if (BitTstByte(Def.Flags1, 3)) doNvgte();
    }
}

static void BackCrash(void)
{
    Rect DeathRect;
    INTEGER ignore;
    if (BitTstByte(Def.Flags1, 7))
        if (CheckSect(Items[i].Scroll, Items[i].LR, 0, &ignore)) {
            ObjRectOf(i, &Def, &DeathRect);
            Animate(Def.Die, DeathRect);
            Items[i].Scroll = -1000;
            Score = Score + Def.Score;
        }
}

static void ObjTouch(void)
{
    Rect TestRect, Test2Rect, ignore;
    if (BitTstByte(Def.Flags1, 4)) {
        ObjRectOf(i, &Def, &TestRect);
        SetRect(&Test2Rect, 0, 0, 620, 480);
        if (SectRect(&TestRect, &Test2Rect, &ignore)) {
            for (INTEGER Obj = (INTEGER)(i + 1); Obj <= LastObj; Obj++) {
                const OBJDef *CODef = GetODEF(Items[Obj].ID);
                if (BitTstByte(CODef->Flags1, 4)) {
                    ObjRectOf(Obj, CODef, &Test2Rect);
                    if (SectRect(&TestRect, &Test2Rect, &ignore)) {
                        CrashObjs(&ItemInfo[i].UD, &ItemInfo[i].LR, &ItemInfo[Obj].UD, &ItemInfo[Obj].LR,
                                  &Items[i].Scroll, &Items[i].LR, CODef->weigth, Def.weigth, TestRect, Test2Rect);
                        PlaySound(1108);
                    }
                }
            }
        }
    }
}

void DoObjects(void)
{
    if (DoneCount >= 0) {
        DoBonus();
        if (ItemCount > LastObj)
            while (LastObj < ItemCount && Items[LastObj + 1].Scroll < Scroll + 240 + 15) {
                LastObj = (INTEGER)(LastObj + 1);
                ItemInfo[LastObj].UD = 0;
                ItemInfo[LastObj].LR = GetODEF(Items[LastObj].ID)->LR;
            }
        for (i = 1; i <= LastObj; i++) {
            if ((Items[i].Scroll * 2 > Scroll * 2 - 200) && (Items[i].Scroll * 2 < Scroll * 2 + 700)) {
                Def = *GetODEF(Items[i].ID);
                MoveObj();
                BackCrash();
                ObjTouch();
                CarTouch();
                ObjRectOf(i, &Def, &Recy);
                PlotSprite(Def.icon, Recy);
            } else if (ItemInfo[i].UD != 0) {
                Items[i].Scroll = -1000;
            }
        }
    }
}
