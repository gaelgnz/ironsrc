/*
render.h - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#ifndef RENDER_H
#define RENDER_H

#include "assets.h"
#include "entity.h"
#include "global.h"
#include "map.h"
#include "protocol.h"
#include "raylib.h"
void DrawCubeTextureRec(Texture2D texture, Rectangle source, Vector3 position,
                        float width, float height, float length, Color color);
void DrawCubeTexture(Texture2D texture, Vector3 position, float width,
                     float height, float length, Color color);
void render_net_entity(Camera *camera, Assets *assets, NetEntity entity,
                       Global *global);
void draw_username_billboard(Camera3D camera, Font font, Vector3 world_pos,
                              const char *name);
void draw_map(Map *map, Assets *assets);
void draw_shots(Shot *shots, uint16_t count);
#endif
