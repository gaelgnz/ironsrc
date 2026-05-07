/*
map.c - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
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

static void push_out_of_sector(Vector3 *position, Vector3 *velocity,
                                Sector *sector) {
    float cx = sector->x + sector->width / 2.0f;
    float cz = sector->y + sector->height / 2.0f;
    float dx = position->x - cx;
    float dz = position->z - cz;
    float half_w = sector->width / 2.0f;
    float half_h = sector->height / 2.0f;

    if (fabsf(dx) / half_w > fabsf(dz) / half_h) {
        if (dx > 0)
            position->x = sector->x + sector->width + 0.1f;
        else
            position->x = sector->x - 0.1f;
        velocity->x = 0;
    } else {
        if (dz > 0)
            position->z = sector->y + sector->height + 0.1f;
        else
            position->z = sector->y - 0.1f;
        velocity->z = 0;
    }
}

int apply_sector_collision(Vector3 *position, Vector3 *velocity, Map *map,
                            float step_height, int resolve_floor) {
    int pushed = 0;
    if (resolve_floor) {
        Sector *primary = get_sector_at(map, *position);
        if (primary) {
            float floor_y = (float)primary->floor_height;
            float dy = floor_y - position->y;

            if (dy > 0 && dy <= step_height) {
                position->y = floor_y;
                velocity->y = 0;
                return 0;
            } else if (dy < 0 && dy >= -step_height && velocity->y <= 0) {
                position->y = floor_y;
                velocity->y = 0;
                return 0;
            }
        }
    }

    for (int iter = 0; iter < 4; iter++) {
        int any = 0;
        for (int i = 0; i < map->sector_count; i++) {
            Sector *s = &map->sectors[i];
            if (position->x >= s->x && position->x <= s->x + s->width &&
                position->z >= s->y && position->z <= s->y + s->height) {
                if (position->y < (float)s->floor_height) {
                    push_out_of_sector(position, velocity, s);
                    any = 1;
                    pushed = 1;
                }
            }
        }
        if (!any)
            break;
    }
    return pushed;
}
