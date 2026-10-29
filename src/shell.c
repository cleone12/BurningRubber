/*
 * shell.c - translation of Shell.p.
 */
#include "br.h"

static DialogPtr CtrlDialog;
static INTEGER response;

/* Look the key up in the 'STR#' 3000 key name table. */
static bool ScanTest(int Scan, unsigned char *Charcode)
{
    Str255 theStr;
    GetIndString(theStr, 3000, Scan);
    PStrCopy(Charcode, theStr);
    return theStr[0] != 0;
}

static void SetKey(INTEGER *KeyCode, int nameIndex, int Ditem)
{
    Rect RedRect;
    bool Scan[129];
    Str255 ScanString;
    EventRecord KeyTouch;
    GrafPtr CurrPort = thePort;
    SetPort(DialogPort(CtrlDialog));
    GetDialogItemRect(CtrlDialog, response, &RedRect);
    InsetRect(&RedRect, -4, -4);
    PenSize(2, 2);
    ForeColor(redColor);
    FrameRect(&RedRect);
    for (;;) {
        GetKeys(Scan);
        for (int i = 1; i <= 128; i++) {
            if (Scan[i]) {
                *KeyCode = (INTEGER)i;
                if (!ScanTest(i, ScanString)) {
                    if (GetNextEvent(keyDownMask, &KeyTouch)) {
                        ScanString[0] = 1;
                        ScanString[1] = (unsigned char)(KeyTouch.message & 255);
                        UpperString(ScanString);
                    } else {
                        ScanString[0] = 1;
                        ScanString[1] = 0x14;
                    }
                }
                PStrCopy(gKeyNames[nameIndex], ScanString);

                ForeColor(whiteColor);
                FrameRect(&RedRect);
                ForeColor(blackColor);
                PenSize(1, 1);
                SetDialogItemText(CtrlDialog, Ditem, ScanString);
                SetPort(CurrPort);
                FlushEvents(everyEvent, 0);
                return;
            }
        }
        if (Plat_QuitRequested()) ExitToShell();
        Plat_Sleep(5);
    }
}

static void SetControls(void)
{
    CtrlDialog = GetNewDialog(2001);
    SetDialogDefaultItem(CtrlDialog, 1);
    SetDialogCancelItem(CtrlDialog, 2);

    ParamText(gKeyNames[0], gKeyNames[1], gKeyNames[2], gKeyNames[3]);
    do {
        ModalDialog(NULL, &response);
        switch (response) {
        case 3: SetKey(&GameKeys.LCode, 0, 9); break;
        case 4: SetKey(&GameKeys.RCode, 1, 10); break;
        case 5: SetKey(&GameKeys.AccCode, 2, 11); break;
        case 6: SetKey(&GameKeys.BrakCode, 3, 12); break;
        }
        ParamText(gKeyNames[0], gKeyNames[1], gKeyNames[2], gKeyNames[3]);
    } while (response != 1 && response != 2);
    if (response == 1) Prefs_CommitControls();
    else Prefs_RevertControls();
    DisposeDialog(CtrlDialog);
    CtrlDialog = NULL;
    response = 0;
}

static void Prefs(void)
{
    DialogPtr PrefDialog;
    INTEGER Hit;

    PrefDialog = GetNewDialog(2000);
    SetDialogDefaultItem(PrefDialog, 1);
    SetDialogCancelItem(PrefDialog, 2);

    SetControlValue(PrefDialog, 3, PrefFlag(kFlagBlankScreen));
    if (PrefFlag(kFlagBackTex)) SetControlValue(PrefDialog, 6, 1);

    SetControlValue(PrefDialog, 8, PrefFlag(kFlagSoundFX));
    if (SoundDisable) HiliteControl(PrefDialog, 8, 255);

    SetControlValue(PrefDialog, 11, PrefFlag(kFlagEngine));
    if (SoundDisable) HiliteControl(PrefDialog, 11, 255);

    SetControlValue(PrefDialog, 10, PrefFlag(kFlagSpeed));

    do {
        ModalDialog(NULL, &Hit);
        switch (Hit) {
        case 3:
            GamePrefs.GrafFlags ^= 0x4000;
            SetControlValue(PrefDialog, 3, PrefFlag(kFlagBlankScreen));
            break;
        case 6:
            GamePrefs.GrafFlags ^= 0x1000;
            SetControlValue(PrefDialog, 6, PrefFlag(kFlagBackTex));
            break;
        case 8:
            GamePrefs.GrafFlags ^= 0x0800;
            SetControlValue(PrefDialog, 8, PrefFlag(kFlagSoundFX));
            break;
        case 10:
            GamePrefs.GrafFlags ^= 0x0200;
            SetControlValue(PrefDialog, 10, PrefFlag(kFlagSpeed));
            break;
        case 11:
            GamePrefs.GrafFlags ^= 0x0400;
            SetControlValue(PrefDialog, 11, PrefFlag(kFlagEngine));
            break;
        case 7:
            SetControls();
            break;
        }
    } while (Hit != 1 && Hit != 2);
    if (Hit == 1) {
        Prefs_CommitSettings();
        InitSound();
    } else {
        Prefs_RevertSettings();
    }
    DisposeDialog(PrefDialog);
}

static INTEGER LevelSelect(void)
{
    DialogPtr ChooseDlog;
    Str255 ChooseString;
    long r;
    INTEGER Hit, result = 1;
    ChooseDlog = GetNewDialog(2002);
    do {
        ModalDialog(NULL, &Hit);
    } while (Hit != 2 && Hit != 1);
    if (Hit == 1) {
        GetDialogItemText(ChooseDlog, 3, ChooseString);
        StringToNum(ChooseString, &r);
        result = (INTEGER)r;
    }
    DisposeDialog(ChooseDlog);
    Cheater = true;
    return result;
}

static void KillScores(void)
{
    if (Alert(1009, NULL) == 2) {
        for (int i = 1; i <= 10; i++) gHighScores[i - 1] = gHighDefault;
        Prefs_CommitScores();
    }
}

static EventRecord MyEvent;

static void DoMenu(int Menu, int Item)
{
    switch (Menu) {
    case 1000:
        if (Item == 1) PSAbout();
        break;
    case 1001:
        switch (Item) {
        case 1:
            Level = 1;
            Cheater = false;
            if (MyEvent.modifiers & optionKey) Level = LevelSelect();
            BeginGame();
            break;
        case 2:
            EndGame();
            break;
        case 4:
            GamePause = !GamePause;
            if (GamePause) ShowMenuBar();
            else if (PrefFlag(kFlagBlankScreen)) HideMenuBar();
            break;
        case 6:
            Quit();
            break;
        }
        break;
    case 1002:
        switch (Item) {
        case 1: Prefs(); break;
        case 2: SetControls(); break;
        case 4: DisplayScores(); break;
        case 5: KillScores(); break;
        case 7: PSHelp(); break;
        }
        break;
    }
    HiliteMenu(0);
}

void DoShell(void)
{
    INTEGER ClickReply;
    WindowPtr WhichWindow;
    long r;
    if (GamePlay) ObscureCursor();
    if (Plat_QuitRequested()) Quit();
    if (WaitNextEvent(everyEvent, &MyEvent, 0)) {
        switch (MyEvent.what) {
        case mouseDown:
            switch (FindWindow(MyEvent.where, &WhichWindow)) {
            case inMenuBar:
                r = MenuSelect(MyEvent.where);
                DoMenu((int)(r >> 16), (int)(r & 0xFFFF));
                break;
            case inDrag:
                DragWindow(WhichWindow, MyEvent.where);
                break;
            case inContent:
                if (WhichWindow && WhichWindow == ShellWindow) {
                    SetPort(WindowPort(ShellWindow));
                    GlobalToLocal(&MyEvent.where);
                    HandlePSClick(MyEvent.where, &ClickReply);
                    switch (ClickReply) {
                    case 1:
                        Level = 1;
                        if (MyEvent.modifiers & optionKey) Level = LevelSelect();
                        BeginGame();
                        break;
                    case 2:
                        Prefs();
                        break;
                    case 3:
                        Quit();
                        break;
                    }
                }
                break;
            }
            break;

        case keyDown:
            if (MyEvent.modifiers & cmdKey) {
                r = MenuKey((unsigned char)(MyEvent.message & charCodeMask));
                DoMenu((int)(r >> 16), (int)(r & 0xFFFF));
            }
            break;

        case updateEvt:
            if (ShellWindow && EventWindow(&MyEvent) == ShellWindow) PSUpdate();
            else if (RaceWindow && EventWindow(&MyEvent) == RaceWindow) GameUpdate();
            break;

        case osEvt:
            if (GamePlay)
                if (((MyEvent.message & 0xFF000000u) >> 24) == 1)   /* suspendResumeMessage */
                    if ((MyEvent.message & 1) == 0) {              /* suspend */
                        GamePause = true;
                        if (PrefFlag(kFlagBlankScreen)) ShowMenuBar();
                    }
            break;
        }
    }
}
