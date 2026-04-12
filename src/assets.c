#include "assets.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

Assets assets_load() {
    Assets assets = {0};
    const char *dir = "assets/textures";

    if (!DirectoryExists(dir)) {
        printf("WARNING: Texture directory %s not found!\n", dir);
        return assets;
    }

    FilePathList files = LoadDirectoryFiles(dir);

    for (int i = 0; i < files.count && assets.count < TEXTURES_MAX; i++) {
        // Check if it's a PNG
        if (IsFileExtension(files.paths[i], ".png")) {
            // 1. Load the texture
            Texture2D tex = LoadTexture(files.paths[i]);

            // 2. Get the filename without the path or extension for the name
            // GetFileNameWithoutExt returns "brick" from
            // "assets/textures/brick.png"
            const char *fileName = GetFileNameWithoutExt(files.paths[i]);

            // 3. Store it
            strncpy(assets.textures[assets.count].name, fileName, NAME_MAX);
            assets.textures[assets.count].texture = tex;

            printf("Loaded texture [%s] from %s\n", fileName, files.paths[i]);
            assets.count++;
        }
    }

    UnloadDirectoryFiles(files);
    return assets;
}

Texture2D get_texture(Assets *assets, const char *name) {
    for (int i = 0; i < assets->count; i++) {
        if (strcmp(assets->textures[i].name, name) == 0) {
            return assets->textures[i].texture;
        }
    }

    // Return a blank texture or an error texture if not found
    printf("ERROR: Texture [%s] not found!\n", name);
    return (Texture2D){0};
}
