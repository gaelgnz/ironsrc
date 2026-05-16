/*
map.c - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#include "map.h"
#include "game.h"
#include "raymath.h"
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
        game_log("Invalid sector_count: %d, resetting to 0",
               map->sector_count);
        map->sector_count = 0;
    }
    if (map->entity_count < 0 || map->entity_count > MAX_ENTITIES) {
        game_log("Invalid entity_count: %d, resetting to 0",
               map->entity_count);
        map->entity_count = 0;
    }

    return map;
}

float raycast_sectors(Vector3 origin, Vector3 dir, float max_dist, Map *map) {
    float closest = max_dist;

    for (int i = 0; i < map->sector_count; i++) {
        Sector *s = &map->sectors[i];

        // Ceiling plane
        if (s->ceiling_enabled && fabsf(dir.y) > 0.0001f) {
            float t = (s->ceiling_height - origin.y) / dir.y;
            if (t > 0.01f && t < closest) {
                Vector3 p = Vector3Add(origin, Vector3Scale(dir, t));
                if (p.x >= s->x && p.x <= s->x + s->width &&
                    p.z >= s->y && p.z <= s->y + s->height)
                    closest = t;
            }
        }

        // Floor plane
        if (fabsf(dir.y) > 0.0001f) {
            float t = (s->floor_height - origin.y) / dir.y;
            if (t > 0.01f && t < closest) {
                Vector3 p = Vector3Add(origin, Vector3Scale(dir, t));
                if (p.x >= s->x && p.x <= s->x + s->width &&
                    p.z >= s->y && p.z <= s->y + s->height)
                    closest = t;
            }
        }

        // Wall faces — skip edges shared with an adjacent sector at the same floor height
        struct { float pos; int is_x; int positive; } faces[4] = {
            {s->x,           1, 0},
            {s->x + s->width, 1, 1},
            {s->y,           0, 0},
            {s->y + s->height, 0, 1},
        };
        for (int f = 0; f < 4; f++) {
            int adjacent = 0;
            for (int j = 0; j < map->sector_count; j++) {
                if (i == j) continue;
                Sector *a = &map->sectors[j];
                if (faces[f].is_x) {
                    if ((!faces[f].positive && a->x + a->width == faces[f].pos) ||
                        (faces[f].positive && a->x == faces[f].pos)) {
                        if (a->y < s->y + s->height && a->y + a->height > s->y) {
                            if (a->floor_height == s->floor_height) {
                                adjacent = 1;
                                break;
                            }
                        }
                    }
                } else {
                    if ((!faces[f].positive && a->y + a->height == faces[f].pos) ||
                        (faces[f].positive && a->y == faces[f].pos)) {
                        if (a->x < s->x + s->width && a->x + a->width > s->x) {
                            if (a->floor_height == s->floor_height) {
                                adjacent = 1;
                                break;
                            }
                        }
                    }
                }
            }
            if (adjacent) continue;

            float t;
            if (faces[f].is_x) {
                if (fabsf(dir.x) < 0.0001f) continue;
                t = (faces[f].pos - origin.x) / dir.x;
            } else {
                if (fabsf(dir.z) < 0.0001f) continue;
                t = (faces[f].pos - origin.z) / dir.z;
            }
            if (t > 0.01f && t < closest) {
                Vector3 p = Vector3Add(origin, Vector3Scale(dir, t));
                int hit = 1;
                if (faces[f].is_x) {
                    if (p.z < s->y || p.z > s->y + s->height) hit = 0;
                } else {
                    if (p.x < s->x || p.x > s->x + s->width) hit = 0;
                }
                float ceil_y = s->ceiling_enabled ? (float)s->ceiling_height : INFINITY;
                if (p.y < s->floor_height || p.y > ceil_y) hit = 0;
                if (hit) closest = t;
            }
        }
    }
    return closest;
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
