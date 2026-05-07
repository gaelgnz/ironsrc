/*
assets.c - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/

#include "assets.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Assets *assets_load() {
    Assets *assets = calloc(1, sizeof(Assets));

    InitAudioDevice();

    const char *tex_dir = "assets/textures";
    if (DirectoryExists(tex_dir)) {
        FilePathList files = LoadDirectoryFiles(tex_dir);
        for (int i = 0; i < files.count && assets->count < TEXTURES_MAX; i++) {
            if (IsFileExtension(files.paths[i], ".png")) {
                Texture2D tex = LoadTexture(files.paths[i]);
                const char *fileName = GetFileNameWithoutExt(files.paths[i]);
                strncpy(assets->textures[assets->count].name, fileName, NAME_MAX);
                assets->textures[assets->count].texture = tex;
                printf("Loaded texture [%s] from %s\n", fileName, files.paths[i]);
                assets->count++;
            }
        }
        UnloadDirectoryFiles(files);
    } else {
        printf("WARNING: Texture directory %s not found!\n", tex_dir);
    }

    const char *snd_dir = "assets/sounds";
    if (DirectoryExists(snd_dir)) {
        FilePathList files = LoadDirectoryFiles(snd_dir);
        for (int i = 0; i < files.count && assets->sound_count < SOUNDS_MAX; i++) {
            if (IsFileExtension(files.paths[i], ".wav")) {
                Sound snd = LoadSound(files.paths[i]);
                const char *fileName = GetFileNameWithoutExt(files.paths[i]);
                strncpy(assets->sounds[assets->sound_count].name, fileName, NAME_MAX);
                assets->sounds[assets->sound_count].sound = snd;
                printf("Loaded sound [%s] from %s\n", fileName, files.paths[i]);
                assets->sound_count++;
            }
        }
        UnloadDirectoryFiles(files);
    } else {
        printf("WARNING: Sound directory %s not found!\n", snd_dir);
    }

    if (FileExists("assets/fonts/font.ttf"))
        assets->default_font = LoadFont("assets/fonts/font.ttf");

    return assets;
}

Texture2D get_texture(Assets *assets, const char *name) {
    for (int i = 0; i < assets->count; i++) {
        if (strcmp(assets->textures[i].name, name) == 0) {
            return assets->textures[i].texture;
        }
    }

    printf("ERROR: Texture [%s] not found!\n", name);
    return (Texture2D){0};
}

Sound get_sound(Assets *assets, const char *name) {
    for (int i = 0; i < assets->sound_count; i++) {
        if (strcmp(assets->sounds[i].name, name) == 0) {
            return assets->sounds[i].sound;
        }
    }

    printf("ERROR: Sound [%s] not found!\n", name);
    return (Sound){0};
}
