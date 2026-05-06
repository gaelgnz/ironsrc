/*
render.c - Copyright (C) 2026 gaelgnz <gaelgnz06@gmail.com>
Licensed under the GNU GPL v3. See LICENSE for details.
*/
#include "render.h"
#include "assets.h"
#include "entity.h"
#include "global.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "stdio.h"
void render_net_entity(Camera *camera, Assets *assets, NetEntity entity,
                       Global *global) {
    if (!entity.active)
        return;

    Vector3 pos = entity.position;
    if (entity.type == ENT_PLAYER) {
        pos.y += 0.5f;
        DrawBillboard(*camera, get_texture(assets, "player"), pos, 1.f, WHITE);
    }
}
void draw_username_billboard(Camera3D camera, Font font, Vector3 world_pos,
                             const char *name) {
    Vector3 above = {world_pos.x, world_pos.y + 2.2f, world_pos.z};

    // Use Raylib's own matrix stack instead of GetWorldToScreen
    Vector2 screen = GetWorldToScreen(above, camera);

    // Only draw if in front of camera (z > 0 check)
    Vector3 toCam = Vector3Subtract(camera.position, above);
    Vector3 camDir = Vector3Subtract(camera.target, camera.position);
    if (Vector3DotProduct(toCam, camDir) > 0)
        return; // behind camera, skip

    Vector2 size = MeasureTextEx(font, name, 18, 0);
    DrawTextEx(font, name,
               (Vector2){screen.x - size.x / 2.f, screen.y - size.y / 2.f}, 18,
               0, WHITE);
}
void DrawCubeTexture(Texture2D texture, Vector3 position, float width,
                     float height, float length, Color color) {
    float x = position.x;
    float y = position.y;
    float z = position.z;

    // Set desired texture to be enabled while drawing following vertex data
    rlSetTexture(texture.id);

    // Vertex data transformation can be defined with the commented lines,
    // but in this example we calculate the transformed vertex data directly
    // when calling rlVertex3f()
    // rlPushMatrix();
    // NOTE: Transformation is applied in inverse order (scale -> rotate ->
    // translate)
    // rlTranslatef(2.0f, 0.0f, 0.0f);
    // rlRotatef(45, 0, 1, 0);
    // rlScalef(2.0f, 2.0f, 2.0f);

    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    // Front Face
    rlNormal3f(0.0f, 0.0f, 1.0f); // Normal Pointing Towards Viewer
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2,
               z + length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2,
               z + length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2,
               z + length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2,
               z + length / 2); // Top Left Of The Texture and Quad
    // Back Face
    rlNormal3f(0.0f, 0.0f, -1.0f); // Normal Pointing Away From Viewer
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2,
               z - length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2,
               z - length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2,
               z - length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2,
               z - length / 2); // Bottom Left Of The Texture and Quad
    // Top Face
    rlNormal3f(0.0f, 1.0f, 0.0f); // Normal Pointing Up
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2,
               z - length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x - width / 2, y + height / 2,
               z + length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x + width / 2, y + height / 2,
               z + length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2,
               z - length / 2); // Top Right Of The Texture and Quad
    // Bottom Face
    rlNormal3f(0.0f, -1.0f, 0.0f); // Normal Pointing Down
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x - width / 2, y - height / 2,
               z - length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x + width / 2, y - height / 2,
               z - length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2,
               z + length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2,
               z + length / 2); // Bottom Right Of The Texture and Quad
    // Right face
    rlNormal3f(1.0f, 0.0f, 0.0f); // Normal Pointing Right
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2,
               z - length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2,
               z - length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2,
               z + length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2,
               z + length / 2); // Bottom Left Of The Texture and Quad
    // Left Face
    rlNormal3f(-1.0f, 0.0f, 0.0f); // Normal Pointing Left
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2,
               z - length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2,
               z + length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2,
               z + length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2,
               z - length / 2); // Top Left Of The Texture and Quad
    rlEnd();
    // rlPopMatrix();

    rlSetTexture(0);
}

void DrawCubeTextureRec(Texture2D texture, Rectangle source, Vector3 position,
                        float width, float height, float length, Color color) {
    float x = position.x;
    float y = position.y;
    float z = position.z;
    float texWidth = (float)texture.width;
    float texHeight = (float)texture.height;

    // Set desired texture to be enabled while drawing following vertex data
    rlSetTexture(texture.id);

    // We calculate the normalized texture coordinates for the desired
    // texture-source-rectangle It means converting from (tex.width, tex.height)
    // coordinates to [0.0f, 1.0f] equivalent
    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);

    // Front face
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth,
                 (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2);

    // Back face
    rlNormal3f(0.0f, 0.0f, -1.0f);
    rlTexCoord2f((source.x + source.width) / texWidth,
                 (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2);

    // Top face
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth,
                 (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2);

    // Bottom face
    rlNormal3f(0.0f, -1.0f, 0.0f);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth,
                 (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2);

    // Right face
    rlNormal3f(1.0f, 0.0f, 0.0f);
    rlTexCoord2f((source.x + source.width) / texWidth,
                 (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2);

    // Left face
    rlNormal3f(-1.0f, 0.0f, 0.0f);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth,
                 (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2);

    rlEnd();

    rlSetTexture(0);
}
void DrawCubeTexturedTiled(Texture2D tex, Vector3 position, float width,
                           float height, float length, float tileScale,
                           Color tint) {
    float hw = width / 2.0f, hh = height / 2.0f, hl = length / 2.0f;

    // UV repeats every `tileScale` world units
    float uW = width / tileScale;
    float uH = height / tileScale;
    float uL = length / tileScale;

    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);

    // Front face  (z+)
    rlNormal3f(0, 0, 1);
    rlTexCoord2f(0, 0);
    rlVertex3f(position.x - hw, position.y - hh, position.z + hl);
    rlTexCoord2f(uW, 0);
    rlVertex3f(position.x + hw, position.y - hh, position.z + hl);
    rlTexCoord2f(uW, uH);
    rlVertex3f(position.x + hw, position.y + hh, position.z + hl);
    rlTexCoord2f(0, uH);
    rlVertex3f(position.x - hw, position.y + hh, position.z + hl);

    // Back face   (z-)
    rlNormal3f(0, 0, -1);
    rlTexCoord2f(uW, 0);
    rlVertex3f(position.x - hw, position.y - hh, position.z - hl);
    rlTexCoord2f(uW, uH);
    rlVertex3f(position.x - hw, position.y + hh, position.z - hl);
    rlTexCoord2f(0, uH);
    rlVertex3f(position.x + hw, position.y + hh, position.z - hl);
    rlTexCoord2f(0, 0);
    rlVertex3f(position.x + hw, position.y - hh, position.z - hl);

    // Top face    (y+)
    rlNormal3f(0, 1, 0);
    rlTexCoord2f(0, uL);
    rlVertex3f(position.x - hw, position.y + hh, position.z - hl);
    rlTexCoord2f(0, 0);
    rlVertex3f(position.x - hw, position.y + hh, position.z + hl);
    rlTexCoord2f(uW, 0);
    rlVertex3f(position.x + hw, position.y + hh, position.z + hl);
    rlTexCoord2f(uW, uL);
    rlVertex3f(position.x + hw, position.y + hh, position.z - hl);

    // Bottom face (y-)
    rlNormal3f(0, -1, 0);
    rlTexCoord2f(uW, uL);
    rlVertex3f(position.x - hw, position.y - hh, position.z - hl);
    rlTexCoord2f(0, uL);
    rlVertex3f(position.x + hw, position.y - hh, position.z - hl);
    rlTexCoord2f(0, 0);
    rlVertex3f(position.x + hw, position.y - hh, position.z + hl);
    rlTexCoord2f(uW, 0);
    rlVertex3f(position.x - hw, position.y - hh, position.z + hl);

    // Right face  (x+)
    rlNormal3f(1, 0, 0);
    rlTexCoord2f(uL, 0);
    rlVertex3f(position.x + hw, position.y - hh, position.z - hl);
    rlTexCoord2f(uL, uH);
    rlVertex3f(position.x + hw, position.y + hh, position.z - hl);
    rlTexCoord2f(0, uH);
    rlVertex3f(position.x + hw, position.y + hh, position.z + hl);
    rlTexCoord2f(0, 0);
    rlVertex3f(position.x + hw, position.y - hh, position.z + hl);

    // Left face   (x-)
    rlNormal3f(-1, 0, 0);
    rlTexCoord2f(0, 0);
    rlVertex3f(position.x - hw, position.y - hh, position.z - hl);
    rlTexCoord2f(uL, 0);
    rlVertex3f(position.x - hw, position.y - hh, position.z + hl);
    rlTexCoord2f(uL, uH);
    rlVertex3f(position.x - hw, position.y + hh, position.z + hl);
    rlTexCoord2f(0, uH);
    rlVertex3f(position.x - hw, position.y + hh, position.z - hl);

    rlEnd();
    rlSetTexture(0);
}
void draw_map(Map *map, Assets *assets) {
    for (int i = 0; i < map->sector_count; i++) {
        Sector *s = &map->sectors[i];
        Texture2D tex = get_texture(assets, s->texture);
        if (tex.id == 0)
            tex = get_texture(assets, "dirt_01");

        // Floor starts at y=0, goes up to floor_height
        float floor_top = s->floor_height;
        float floor_mid = floor_top / 2.0f;

        Vector3 pos = {s->x + s->width / 2.0f, floor_mid,
                       s->y + s->height / 2.0f};

        DrawCubeTexturedTiled(tex, pos, s->width, floor_top, s->height, 1.f,
                              WHITE);

        // Draw ceiling if enabled (unchanged)
        if (s->ceiling_enabled) {
            Vector3 ceil_pos = {s->x + s->width / 2.0f,
                                s->ceiling_height + 0.05f,
                                s->y + s->height / 2.0f};
            DrawCubeTexture(tex, ceil_pos, s->width, 0.1f, s->height, WHITE);
        }
    }
}
