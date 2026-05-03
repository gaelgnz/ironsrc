#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Map *load_map(const char *file_path) {
    FILE *f = fopen(file_path, "rb");
    if (!f)
        return NULL;

    Map *map = malloc(sizeof(Map));
    if (!map) {
        fclose(f);
        return NULL;
    }

    fread(map, sizeof(Map), 1, f);
    fclose(f);

    // Validate map data
    if (map->sector_count < 0 || map->sector_count > MAX_SECTORS) {
        printf("[MAP] Invalid sector_count: %d, resetting to 0\n",
               map->sector_count);
        map->sector_count = 0;
    }
    if (map->entity_count < 0 || map->entity_count > MAX_ENTITIES) {
        printf("[MAP] Invalid entity_count: %d, resetting to 0\n",
               map->entity_count);
        map->entity_count = 0;
    }

    return map;
}
