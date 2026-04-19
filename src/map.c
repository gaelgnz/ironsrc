#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("couldnt read map\n");
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static Vector3 parse_vec3(const char **s) {
    float x, y, z;
    sscanf(*s, "%f %f %f", &x, &y, &z);
    // advance past the three numbers
    for (int i = 0; i < 3; i++) {
        while (**s && **s != ' ' && **s != ')')
            (*s)++;
        while (**s == ' ')
            (*s)++;
    }
    return (Vector3){x, y, z};
}

Map *load_map(const char *file_path) {
    Map *map = calloc(1, sizeof(Map));
    if (!map)
        return NULL;

    char *src = read_file(file_path);
    printf("load_map: read_file returned %p\n", src);
    fflush(stdout);
    if (!src) {
        fprintf(stderr, "load_map: could not open %s\n", file_path);
        return map;
    }

    char *p = src;

    while (*p) {
        // skip whitespace and comments
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
        if (*p == '/' && *(p + 1) == '/') {
            while (*p && *p != '\n')
                p++;
            continue;
        }

        if (*p == '{') {
            p++; // enter entity block

            // check if worldspawn or other entity by reading key/values
            bool is_worldspawn = false;
            char classname[64] = {0};

            while (*p) {
                while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
                    p++;
                if (*p == '/' && *(p + 1) == '/') {
                    while (*p && *p != '\n')
                        p++;
                    continue;
                }

                if (*p == '}') {
                    p++;
                    break;
                }

                if (*p == '{') {
                    // brush block
                    p++;
                    if (map->brush_count >= 1024) {
                        while (*p && *p != '}')
                            p++;
                        p++;
                        continue;
                    }
                    Brush *brush = &map->worldspawn[map->brush_count++];
                    brush->face_count = 0;

                    while (*p) {
                        while (*p == ' ' || *p == '\t' || *p == '\r' ||
                               *p == '\n')
                            p++;
                        if (*p == '/' && *(p + 1) == '/') {
                            while (*p && *p != '\n')
                                p++;
                            continue;
                        }
                        if (*p == '}') {
                            p++;
                            break;
                        }

                        if (*p == '(' && brush->face_count < 64) {
                            BrushFace *face =
                                &brush->faces[brush->face_count++];

                            // ( x y z )
                            p++; // skip '('
                            while (*p == ' ')
                                p++;
                            face->p1 = parse_vec3(&p);
                            while (*p && *p != ')')
                                p++;
                            p++; // skip ')'

                            while (*p == ' ')
                                p++;
                            p++; // skip '('
                            while (*p == ' ')
                                p++;
                            face->p2 = parse_vec3(&p);
                            while (*p && *p != ')')
                                p++;
                            p++;

                            while (*p == ' ')
                                p++;
                            p++; // skip '('
                            while (*p == ' ')
                                p++;
                            face->p3 = parse_vec3(&p);
                            while (*p && *p != ')')
                                p++;
                            p++;

                            // texture name
                            while (*p == ' ')
                                p++;
                            int ti = 0;
                            while (*p && *p != ' ' && *p != '\n' && ti < 63)
                                face->texture[ti++] = *p++;
                            face->texture[ti] = '\0';

                            // skip rest of line (offset rotation scale)
                            while (*p && *p != '\n')
                                p++;
                        } else {
                            while (*p && *p != '\n')
                                p++;
                        }
                    }
                } else if (*p == '"') {
                    // key "value" pair
                    char key[64] = {0}, value[256] = {0};
                    p++; // skip opening quote
                    int i = 0;
                    while (*p && *p != '"')
                        key[i++] = *p++;
                    key[i] = '\0';
                    if (*p)
                        p++; // skip closing quote

                    while (*p == ' ')
                        p++;
                    if (*p == '"')
                        p++; // skip opening quote of value
                    i = 0;
                    while (*p && *p != '"')
                        value[i++] = *p++;
                    value[i] = '\0';
                    if (*p)
                        p++;

                    if (strcmp(key, "classname") == 0) {
                        strncpy(classname, value, sizeof(classname) - 1);
                        is_worldspawn = strcmp(value, "worldspawn") == 0;
                    }

                    // non-worldspawn entity: parse known keys
                    if (!is_worldspawn && map->entity_count < 256) {
                        Entity *ent = &map->entities[map->entity_count];
                        if (strcmp(key, "origin") == 0) {
                            float x, y, z;
                            sscanf(value, "%f %f %f", &x, &y, &z);
                            ent->position = (Vector3){x, y, z};
                        }
                        if (strcmp(key, "classname") == 0) {
                            // you can map classname to EntityType here
                            ent->type = ENT_PROP; // default, extend as
                            ent->active = true;
                            ent->client_id = NOT_PLAYER;
                        }
                    }
                } else {
                    p++;
                }
            }

            // commit non-worldspawn entity
            if (!is_worldspawn && map->entity_count < 256)
                map->entity_count++;

        } else {
            p++;
        }
    }

    free(src);
    printf("load_map: %d brushes, %d entities\n", map->brush_count,
           map->entity_count);
    return map;
}
