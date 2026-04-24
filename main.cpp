#include <SDL2/SDL.h>
#include <cmath>
#include <cstdint>

// ── Window ──────────────────────────────────────────────────────────────────
const int W = 1280;
const int H = 720;
const double HALF_H = H / 2.0;

// ── Map ──────────────────────────────────────────────────────────────────────
const int MAP_W = 16;
const int MAP_H = 16;

int MAP[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,2,0,0,0,0,0,2,2,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,2,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,3,3,3,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,3,0,3,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,3,0,3,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,2,0,0,0,1},
    {1,0,0,2,2,0,0,0,0,0,2,2,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

// ── Wall colours by tile type ────────────────────────────────────────────────
struct Color { uint8_t r, g, b; };

Color wallColor(int tile, bool dark) {
    Color c = {0, 0, 0};
    switch (tile) {
        case 1: c = {180, 100, 60};  break; // brown
        case 2: c = {80,  140, 200}; break; // blue
        case 3: c = {60,  180, 90};  break; // green
    }
    if (dark) { c.r /= 2; c.g /= 2; c.b /= 2; }
    return c;
}

// ── Player ───────────────────────────────────────────────────────────────────
struct Player {
    double x = 8.0, y = 8.0;
    double angle = 0.0;
    double moveSpeed = 0.05;
    double rotSpeed  = 0.04;
};

// ── DDA Raycasting ───────────────────────────────────────────────────────────
void castRay(double px, double py, double angle, double& dist, int& tile, bool& hitNS) {
    double rayDirX = cos(angle);
    double rayDirY = sin(angle);

    int mapX = (int)px;
    int mapY = (int)py;

    double deltaDistX = fabs(1.0 / rayDirX);
    double deltaDistY = fabs(1.0 / rayDirY);

    double sideDistX, sideDistY;
    int stepX, stepY;

    if (rayDirX < 0) { stepX = -1; sideDistX = (px - mapX) * deltaDistX; }
    else             { stepX =  1; sideDistX = (mapX + 1.0 - px) * deltaDistX; }

    if (rayDirY < 0) { stepY = -1; sideDistY = (py - mapY) * deltaDistY; }
    else             { stepY =  1; sideDistY = (mapY + 1.0 - py) * deltaDistY; }

    bool hit = false;
    int side = 0;

    while (!hit) {
        if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
        else                       { sideDistY += deltaDistY; mapY += stepY; side = 1; }

        if (mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H) { dist = 64; return; }
        if (MAP[mapY][mapX] > 0) hit = true;
    }

    tile   = MAP[mapY][mapX];
    hitNS  = (side == 1);
    dist   = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
    if (dist < 0.01) dist = 0.01;
}

// ── Draw a vertical stripe ───────────────────────────────────────────────────
void drawStripe(SDL_Renderer* ren, int x, double dist, int tile, bool hitNS) {
    int lineH = (int)(H / dist);
    int top    = (int)(HALF_H - lineH / 2.0);
    int bottom = (int)(HALF_H + lineH / 2.0);

    Color c = wallColor(tile, hitNS);

    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
    SDL_RenderDrawLine(ren, x, top < 0 ? 0 : top, x, bottom > H ? H : bottom);
}

// ── Mini-map ─────────────────────────────────────────────────────────────────
void drawMinimap(SDL_Renderer* ren, const Player& p) {
    const int TILE = 8;
    const int OX = 10, OY = 10;

    for (int row = 0; row < MAP_H; row++) {
        for (int col = 0; col < MAP_W; col++) {
            SDL_Rect r = { OX + col * TILE, OY + row * TILE, TILE - 1, TILE - 1 };
            if (MAP[row][col])
                SDL_SetRenderDrawColor(ren, 200, 200, 200, 200);
            else
                SDL_SetRenderDrawColor(ren, 30, 30, 30, 180);
            SDL_RenderFillRect(ren, &r);
        }
    }

    // Player dot
    int px = OX + (int)(p.x * TILE);
    int py = OY + (int)(p.y * TILE);
    SDL_SetRenderDrawColor(ren, 255, 80, 80, 255);
    SDL_Rect dot = { px - 2, py - 2, 5, 5 };
    SDL_RenderFillRect(ren, &dot);

    // Direction line
    SDL_SetRenderDrawColor(ren, 255, 200, 0, 255);
    SDL_RenderDrawLine(ren, px, py,
        px + (int)(cos(p.angle) * 12),
        py + (int)(sin(p.angle) * 12));
}

// ── Main ─────────────────────────────────────────────────────────────────────
int main() {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window*   win = SDL_CreateWindow("C++ Raycaster",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    Player player;
    bool running = true;
    SDL_Event e;

    const double FOV      = M_PI / 3.0; // 60°
    const double HALF_FOV = FOV / 2.0;
    const double MOUSE_SENSITIVITY = 0.0015;

    // Capture mouse for look
    SDL_SetRelativeMouseMode(SDL_TRUE);

    while (running) {
        // ── Events ──────────────────────────────────────────────────────────
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
            // Mouse look
            if (e.type == SDL_MOUSEMOTION)
                player.angle += e.motion.xrel * MOUSE_SENSITIVITY;
        }

        // ── Input ────────────────────────────────────────────────────────────
        const uint8_t* keys = SDL_GetKeyboardState(nullptr);

        double nx = player.x, ny = player.y;

        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
            nx += cos(player.angle) * player.moveSpeed;
            ny += sin(player.angle) * player.moveSpeed;
        }
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
            nx -= cos(player.angle) * player.moveSpeed;
            ny -= sin(player.angle) * player.moveSpeed;
        }
        if (keys[SDL_SCANCODE_A]) {
            nx += cos(player.angle - M_PI / 2) * player.moveSpeed;
            ny += sin(player.angle - M_PI / 2) * player.moveSpeed;
        }
        if (keys[SDL_SCANCODE_D]) {
            nx += cos(player.angle + M_PI / 2) * player.moveSpeed;
            ny += sin(player.angle + M_PI / 2) * player.moveSpeed;
        }

        // Collision: only move if new cell is empty
        if (MAP[(int)ny][(int)player.x] == 0) player.y = ny;
        if (MAP[(int)player.y][(int)nx] == 0) player.x = nx;

        // ── Clear ────────────────────────────────────────────────────────────
        // Ceiling
        SDL_SetRenderDrawColor(ren, 30, 30, 50, 255);
        SDL_Rect ceiling = { 0, 0, W, H / 2 };
        SDL_RenderFillRect(ren, &ceiling);

        // Floor
        SDL_SetRenderDrawColor(ren, 60, 50, 40, 255);
        SDL_Rect floor = { 0, H / 2, W, H / 2 };
        SDL_RenderFillRect(ren, &floor);

        // ── Raycasting ───────────────────────────────────────────────────────
        for (int x = 0; x < W; x++) {
            double rayAngle = player.angle - HALF_FOV + FOV * ((double)x / W);
            double dist;
            int tile;
            bool hitNS;
            castRay(player.x, player.y, rayAngle, dist, tile, hitNS);

            // Fish-eye fix
            dist *= cos(rayAngle - player.angle);

            drawStripe(ren, x, dist, tile, hitNS);
        }

        // ── Minimap ──────────────────────────────────────────────────────────
        drawMinimap(ren, player);

        SDL_RenderPresent(ren);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
