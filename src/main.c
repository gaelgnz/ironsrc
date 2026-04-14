#include "assets.h"
#include "entity.h"
#include "protocol.h"
#include "raylib.h"
#include "raymath.h"
#include "render.h"
#include "stdio.h"
#include <unistd.h>
void render_entity(Camera *camera, Assets *assets, Entity entity) {
    if (entity.type == ENT_NPC_GENERIC) {
        printf("%f", entity.position.x);
        DrawBillboard(*camera, get_texture(assets, "player"), entity.position,
                      1., WHITE);
        DrawCubeWires(entity.position, 30.f, 30.f, 30.f, BLACK);
    } else {
    }
}

typedef enum GameMode {
    GM_MENU,
    GM_CONNECTING,
    GM_INGAME,
} GameMode;

typedef struct Global {
    GameMode gamemode;
    Assets assets;
    union {};
} Global;

void menu_loop(Global *global) {
    while (global->gamemode == GM_MENU) {
        BeginDrawing();

        ClearBackground(WHITE);

        EndDrawing();
    }
}

void game_loop(Global *global) {}

void connect(Global *global) {}
int main() {

    InitWindow(800, 600, "IronSrc");
    SetTargetFPS(144);
    sleep(5);

    if (!IsWindowReady()) {
        printf("Window init failed\n");
        return 1;
    }
    Global global = {0};
    global.assets = assets_load();
    global.gamemode = GM_MENU;

    Font font = LoadFont("assets/font.ttf");

    Assets assets = assets_load();

    switch (global.gamemode) {
    case GM_MENU:
        menu_loop(&global);
        break;
    case GM_CONNECTING:
        connect(&global);
        break;
    case GM_INGAME:
        game_loop(&global);
        break;
    };
    const float dt = 1.0f / 60.0f;
    float accumulator = 0.0f;

    Vector3 velocity = (Vector3){0};
    Vector3 position = (Vector3){0};

    float yaw;
    float pitch;
    DisableCursor();

    Entity local_entities[256]; // render only

    while (!WindowShouldClose()) {
        float frameTime = GetFrameTime();
        if (frameTime > 0.25f)
            frameTime = 0.25f;
        accumulator += frameTime;

        UserCmd cmd = {0};
        float speed = 5.0f;
        Vector3 forward = {sinf(yaw * DEG2RAD), 0, cosf(yaw * DEG2RAD)};
        Vector3 right = {cosf(yaw * DEG2RAD), 0, -sinf(yaw * DEG2RAD)};

        if (IsKeyDown(KEY_W))
            velocity =
                Vector3Add(velocity, Vector3Scale(forward, speed * frameTime));
        if (IsKeyDown(KEY_S))
            velocity =
                Vector3Add(velocity, Vector3Scale(forward, -speed * frameTime));
        if (IsKeyDown(KEY_A))
            velocity =
                Vector3Add(velocity, Vector3Scale(right, -speed * frameTime));
        if (IsKeyDown(KEY_D))
            velocity =
                Vector3Add(velocity, Vector3Scale(right, speed * frameTime));

        velocity.y -= 20.0f * frameTime;
        if (position.y < 0.0f) {
            position.y = 0.0f;
            velocity.y = 0.0f;
        }
        position = Vector3Add(position, Vector3Scale(velocity, frameTime));

        // Damping
        velocity.x *= 0.8f;
        velocity.z *= 0.8f;

        if (IsKeyPressed(KEY_SPACE))
            velocity.y -= 10;
        cmd.position = position;
        cmd.current_velocity = velocity;

        BeginDrawing();
        ClearBackground(BLUE);
        DrawFPS(10, 10);

        float ry = yaw * DEG2RAD;
        float rp = pitch * DEG2RAD;

        Camera3D camera = {0};
        camera.position = Vector3Add(position, (Vector3){0, 1.0f, 0});
        camera.target = (Vector3){camera.position.x + sinf(ry) * cosf(rp),
                                  camera.position.y + sinf(rp),
                                  camera.position.z + cosf(ry) * cosf(rp)};
        camera.up = (Vector3){0.0f, 1.0f, 0.0f};
        camera.fovy = 120.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        BeginMode3D(camera);

        DrawCubeTexture(get_texture(&assets, "brick_01"),
                        (Vector3){0, -0.5f, 0}, 100.0f, 1.0f, 100.0f, WHITE);

        EndMode3D();
        DrawTextEx(font, "IRONSRC ENGINE - SERVER TICK: 60Hz",
                   (Vector2){20, 20}, 30, 0, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
