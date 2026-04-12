#include "assets.h"
#include "entity.h"
#include "protocol.h"
#include "raylib.h"
#include "raymath.h"
#include "render.h"
#include "server.h"
#include "stdio.h"

void render_entity(Camera *camera, Assets *assets, Entity entity) {
    if (entity.type == ENT_NPC_GENERIC) {
        printf("%f", entity.position.x);
        DrawBillboard(*camera, get_texture(assets, "player"), entity.position,
                      1., WHITE);
        DrawCubeWires(entity.position, 30.f, 30.f, 30.f, BLACK);
    } else {
        printf("not drawing because %s is not npc\n", entity.player.username);
    }
}

int main() {
    // 1. Setup Raylib
    InitWindow(1280, 720, "IronSrc Engine");
    SetTargetFPS(144); // Client can run fast

    Font font = LoadFont("assets/font.ttf");

    Server sv = {0};
    sv_init(&sv);

    Entity *p = &sv.entities[0];

    Assets assets = assets_load();

    // Simulation settings
    const float dt = 1.0f / 60.0f; // Server ticks at 60Hz
    float accumulator = 0.0f;

    DisableCursor();

    while (!WindowShouldClose()) {
        float frameTime = GetFrameTime();
        if (frameTime > 0.25f)
            frameTime = 0.25f; // "Spiral of death" protection
        accumulator += frameTime;

        // --- 3. INPUT (Client Side) ---
        UserCmd cmd = {0};
        if (IsKeyDown(KEY_W))
            cmd.wishVelocity.z = 1;
        if (IsKeyDown(KEY_S))
            cmd.wishVelocity.z = -1;
        if (IsKeyDown(KEY_A))
            cmd.wishVelocity.x = 1;
        if (IsKeyDown(KEY_D))
            cmd.wishVelocity.x = -1;

        if (IsKeyPressed(KEY_SPACE))
            cmd.jumping = true;
        cmd.mouseDelta = GetMouseDelta();

        sv_receive_input(&sv, 0, cmd);

        while (accumulator >= dt) {
            sv_tick(&sv, dt);
            accumulator -= dt;
        }

        BeginDrawing();
        ClearBackground(BLUE);
        DrawFPS(10, 10);

        float ry = p->player.yaw * DEG2RAD;
        float rp = p->player.pitch * DEG2RAD;

        Camera3D camera = {0};
        camera.position = Vector3Add(p->position, (Vector3){0, 1.0f, 0});
        camera.target = (Vector3){camera.position.x + sinf(ry) * cosf(rp),
                                  camera.position.y + sinf(rp),
                                  camera.position.z + cosf(ry) * cosf(rp)};
        camera.up = (Vector3){0.0f, 1.0f, 0.0f};
        camera.fovy = 120.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        BeginMode3D(camera);

        DrawCubeTexture(get_texture(&assets, "brick_01"),
                        (Vector3){0, -0.5f, 0}, 100.0f, 1.0f, 100.0f, WHITE);

        for (int i = 0; i < sv.entity_count; i++) {

            if (!sv.entities[i].active)
                continue;

            render_entity(&camera, &assets, sv.entities[i]);
        }
        EndMode3D();
        DrawTextEx(font, "IRONSRC ENGINE - SERVER TICK: 60Hz",
                   (Vector2){20, 20}, 30, 0, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
