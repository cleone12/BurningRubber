/*
 * car.c - translation of Car.p.
 */
#include "br.h"

void DoCar(void)
{
    INTEGER i;
    bool ResetSucc;
    Str255 DeadString;
    if (!Dead) {
        Engine((INTEGER)(UDSpeed + abs(LRSpeed) / 5));
        if (Jump != 0) {
            if (Jump < MaxJump) {
                Jump = (INTEGER)(Jump + 2);
            } else {
                MaxJump = 0;
                Jump = (INTEGER)(Jump - 2);
            }
            if (Jump < 0) {
                Jump = 0;
                CarDirection = (INTEGER)(CarDirection + Random() / 14000);
                PlaySound(1103);
            }
        }
        if (Jump == 0) {
            if (LKey) LRSpeed = (INTEGER)(LRSpeed - PRound(UDSpeed / 50.0 * MyCar.Steering));
            if (RKey) LRSpeed = (INTEGER)(LRSpeed + PRound(UDSpeed / 50.0 * MyCar.Steering));
            if (AccKey && (wait == 0)) {
                if (UDSpeed > 8) UDSpeed = (INTEGER)(UDSpeed + MyCar.Acc);
                else UDSpeed = (INTEGER)(UDSpeed + MyCar.Acc * 2);
            }

            if (BrakKey) {
                if ((UDSpeed > MyCar.Speed * 3 / 4) && !BrakeDone) PlaySound(1000);
                BrakeDone = true;
                if (UDSpeed >= MyCar.Brakes) UDSpeed = (INTEGER)(UDSpeed - MyCar.Brakes);
                LRSpeed = (INTEGER)(LRSpeed / (MyCar.Brakes * 2));
            } else {
                BrakeDone = false;
            }

            if (UDSpeed >= 1) UDSpeed = (INTEGER)(UDSpeed - abs(Random() / 12000));
            else if (UDSpeed <= -1) UDSpeed = (INTEGER)(UDSpeed + abs(Random() / 12000));
            if (DoneCount != 0) {
                if (UDSpeed >= 5) UDSpeed = (INTEGER)(UDSpeed - 5);
                else UDSpeed = 0;
            }
            if (UDSpeed > MyCar.Speed) UDSpeed = MyCar.Speed;
            if (abs(LRSpeed) > MyCar.Speed) LRSpeed = (INTEGER)(MyCar.Speed * sign(LRSpeed));
            if (LRSpeed != 0) LRSpeed = (INTEGER)(LRSpeed - 1 * sign(LRSpeed));

            if (CheckSect((INTEGER)(Scroll + GamePrefs.Offset / 2), LRPos, 7, &i)) {
                if (BitTstByte(MyRoad->Flags, 0))
                    if (CheckSect((INTEGER)(Scroll + GamePrefs.Offset / 2), (INTEGER)(LRPos + 16), -16, &i) && (DoneCount == 0)) {
                        Dead = true;
                        Animate(MyCar.Crash, CarRect);
                    }
                if (BitTstByte(MyRoad->Flags, 1))
                    if (CheckSect((INTEGER)(Scroll + GamePrefs.Offset / 2), (INTEGER)(LRPos + 16), -16, &i) && (DoneCount == 0)) {
                        Dead = true;
                        Animate(MyCar.Drawn, CarRect);
                    }
                if (BitTstByte(MyRoad->Flags, 2)) {
                    if (UDSpeed > PRound(16 * (MyCar.OffRoad / 100.0))) UDSpeed = (INTEGER)(UDSpeed - MyCar.Acc * 3);
                    if (abs(LRSpeed) > PRound(8 * (MyCar.OffRoad / 100.0)))
                        LRSpeed = (INTEGER)(LRSpeed - MyCar.Acc * 3 * LRSpeed / abs(LRSpeed));
                }
            }
        }

        Scroll = (INTEGER)(Scroll + UDSpeed / 2);
        LRPos = (INTEGER)(LRPos + LRSpeed);
        if (UDSpeed != 0) CarDirection = (INTEGER)PRound(((double)LRSpeed / UDSpeed) * 6);
        else CarDirection = 0;
        if (abs(CarDirection) > MyCar.Frames) CarDirection = (INTEGER)(MyCar.Frames * sign(CarDirection));
        SetRect(&CarRect, LRPos, UDPos, LRPos + 32, UDPos + 32);
        InsetRect(&CarRect, -Jump / 5, -Jump / 5);
        if (!Dead) PlotSprite((INTEGER)(MyCar.BaseIcon + CarDirection), CarRect);
    } else if (Dead) {
        if (DeadCount == 0) {
            UDSpeed = 0;
            LRSpeed = 0;
            KillEngine();
            OldScroll = Scroll;
            Lives = (INTEGER)(Lives - 1);
            if (Lives == 0) GameOver = true;
            DeadCount = 20;
        }
        if (Info->Bonus == 0) {
            GetIndString(DeadString, 2000, 7);
            ShowMessage(DeadString, 310 - 100, 180, 36);
            Info->Bonus = 500;
        }
        DeadCount = (INTEGER)(DeadCount - 1);
        if ((DeadCount == 0) && !GameOver) {
            Dead = false;
            ResetSucc = false;
            if (Info->Bonus < 0) Info->Bonus = 500;
            for (i = 15; i >= 1; i--)
                if (Rebirthes[i].SCR < Scroll + GamePrefs.Offset) {
                    Scroll = Rebirthes[i].SCR;
                    OldScroll = (INTEGER)(Scroll - 480 + GamePrefs.Offset);
                    LRPos = Rebirthes[i].LRP;
                    ResetSucc = true;
                    break;
                }
            if (!ResetSucc) {
                Scroll = 0;
                OldScroll = (INTEGER)(0 - 480 - GamePrefs.Offset);
                LRPos = 310;
            }
        }
    }
    if (Scroll + GamePrefs.Offset / 2 > Info->StopAt) {
        if (DoneCount == 0) {
            DoneCount = 12;
            PlaySound(1104);
        }
        DoneCount = (INTEGER)(DoneCount - 1);
        if (DoneCount == 0) DoneCount = -1;
    }
}
