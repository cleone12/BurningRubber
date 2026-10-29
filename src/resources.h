/*
 * resources.h - read-only access to the game's classic Mac resource files.
 *
 * Race.rsrc and Levels.rsrc hold resource forks that were moved into the data
 * fork for git. Every resource is plain big-endian data; nothing is compressed.
 */
#ifndef RESOURCES_H
#define RESOURCES_H

#include "mac.h"

bool Res_Init(void);
void Res_Shutdown(void);

/* Directory the game data was found in (ends with a path separator). */
const char *Res_DataDir(void);

/* Returns the resource bytes, or NULL if missing. *size may be NULL. */
const uint8_t *GetResource(uint32_t type, int id, int32_t *size);
bool HasResource(uint32_t type, int id);

/* 'STR ' and 'STR#' helpers; missing strings come back empty. */
void GetString(unsigned char *out, int id);
void GetIndString(unsigned char *out, int id, int index);

#endif
