#ifndef RENDER_H
#define RENDER_H

#include "assets.h"
#include "entity.h"
#include "raylib.h"
void DrawCubeTextureRec(Texture2D texture, Rectangle source, Vector3 position,
                        float width, float height, float length, Color color);
void DrawCubeTexture(Texture2D texture, Vector3 position, float width,
                     float height, float length, Color color);
void render_net_entity(Camera *camera, Assets *assets, NetEntity entity);
#endif
