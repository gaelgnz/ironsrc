#ifndef ASSETS_H
#define ASSETS_H
#include "raylib.h"

#define TEXTURES_MAX 256
#define NAME_MAX 64

typedef struct NamedTexture {
    char name[NAME_MAX];
    Texture2D texture;
} NamedTexture;

typedef struct Assets {
    NamedTexture textures[TEXTURES_MAX];
    int count;
    Font default_font;
} Assets;

Assets assets_load();
Texture2D get_texture(Assets *assets, const char *name);

#endif
