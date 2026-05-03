#include "map.h"
#include <math.h>
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

Sector *get_sector_at(Map *map, Vector3 pos) {
    for (int i = 0; i < map->sector_count; i++) {
        Sector *s = &map->sectors[i];
        if (pos.x >= s->x && pos.x <= s->x + s->width &&
            pos.z >= s->y && pos.z <= s->y + s->height) {
            return s;
        }
    }
    return NULL;
}

int is_on_sector_floor(Vector3 position, Map *map, float step_height) {
    Sector *sector = get_sector_at(map, position);
    if (!sector)
        return 0;
    float dy = sector->floor_height - position.y;
    return dy >= -step_height && dy <= step_height;
}

void apply_sector_collision(Vector3 *position, Vector3 *velocity, Map *map,
                            float step_height) {
    Sector *sector = get_sector_at(map, *position);
    if (!sector)
        return;

    float floor_y = sector->floor_height;
    float dy = floor_y - position->y;

    if (dy > 0 && dy <= step_height) {
        // Below floor, within step height → step up (stairs)
        position->y = floor_y;
        velocity->y = 0;
    } else if (dy < 0 && dy >= -step_height && velocity->y <= 0) {
        // Above floor, within step height, moving down → land on floor
        position->y = floor_y;
        velocity->y = 0;
    } else if (position->y < floor_y) {
        // Below floor and NOT within step range → collide horizontally
        // Push player out of sector bounds
        float cx = sector->x + sector->width / 2.0f;
        float cz = sector->y + sector->height / 2.0f;
        float dx = position->x - cx;
        float dz = position->z - cz;

        // Find closest edge
        float half_w = sector->width / 2.0f;
        float half_h = sector->height / 2.0f;

        if (fabsf(dx) / half_w > fabsf(dz) / half_h) {
            // Push out on X axis
            if (dx > 0)
                position->x = sector->x + sector->width + 0.1f;
            else
                position->x = sector->x - 0.1f;
            velocity->x = 0;
        } else {
            // Push out on Z axis
            if (dz > 0)
                position->z = sector->y + sector->height + 0.1f;
            else
                position->z = sector->y - 0.1f;
            velocity->z = 0;
        }
    }
}
