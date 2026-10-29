/*
 * main.c - translation of Race.p, plus start-up of the port's platform layer.
 */
#include "br.h"

#include <SDL.h>
#include <stdio.h>

static void QuitFromDialog(void) { ExitToShell(); }

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    if (!Res_Init()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Burning Rubber",
                                 "Could not find Race.rsrc and Levels.rsrc next to the game.", NULL);
        return 1;
    }
    QD_Init();
    Prefs_Load();
    if (!Plat_Init()) {
        fprintf(stderr, "SDL initialisation failed: %s\n", SDL_GetError());
        return 1;
    }
    Text_Init();
    TB_Init();
    gQuitHook = QuitFromDialog;

    InitProg();
    for (;;) {
        DoShell();
        PowerShell();
        DoGame();
        DoGame();
        DoGame();
        DoGame();
        DoGame();
        DoGame();
    }
}
