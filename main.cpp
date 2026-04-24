#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <algorithm>

// ─── Screen ──────────────────────────────────────────────────────────────────
static const int W    = 1280;
static const int H    = 720;
static const int TEX  = 64;
static const float CAM  = 0.66f;   // camera plane half-width (~66° FOV)
static const float SENS = 0.0012f; // mouse sensitivity

// ─── Pixel helpers ───────────────────────────────────────────────────────────
inline uint32_t rgb(int r, int g, int b) {
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
inline uint32_t dimRGB(uint32_t c, float dist) {
    float f = 1.0f / (1.0f + dist * dist * 0.08f);
    if (f > 1.f) f = 1.f;
    return rgb(int(((c>>16)&0xFF)*f), int(((c>>8)&0xFF)*f), int((c&0xFF)*f));
}

// ─── Map (24×24) ─────────────────────────────────────────────────────────────
static const int MW = 24, MH = 24;
// 0=open  1=brick  2=blue stone  3=green stone  4=red pillar
static int MAP[MH][MW] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,2,2,0,0,0,2,2,1,0,0,2,0,0,0,0,2,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,2,0,0,0,1},
    {1,0,0,0,0,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,3,0,0,0,3,0,0,1,1,1,1,0,1,1,1,1,0,0,1},
    {1,0,0,0,0,3,0,0,0,3,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
    {1,0,0,0,0,3,0,0,0,3,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
    {1,0,0,0,0,3,3,3,3,3,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
    {1,1,1,0,1,1,1,1,1,1,1,0,1,0,0,0,0,0,0,0,1,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,0,0,1,0,0,1},
    {1,0,4,4,0,0,0,0,0,0,0,0,0,0,0,0,4,4,0,0,1,1,1,1},
    {1,0,4,4,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,3,0,0,0,3,0,0,0,0,1,0,0,0,0,0,0,4,0,0,0,1},
    {1,0,0,3,0,0,0,3,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

// ─── Textures ────────────────────────────────────────────────────────────────
static uint32_t T_WALL[4][TEX * TEX];
static uint32_t T_FLOOR[TEX * TEX];
static uint32_t T_CEIL[TEX * TEX];
static uint32_t T_SPRITE[TEX * TEX]; // enemy sprite (0 = transparent)

static void genTextures() {
    // Wall textures: brick with 4 colour variants
    int wr[] = {160, 80,  55,  180};
    int wg[] = { 75, 140,  60,  50};
    int wb[] = { 40, 200, 175,  40};
    for (int t = 0; t < 4; t++) {
        for (int y = 0; y < TEX; y++) {
            for (int x = 0; x < TEX; x++) {
                int bx = x + ((y/8)%2) * (TEX/2);
                bool mortar = (y % 8 == 0) || (bx % TEX == 0 && y % 8 != 0);
                int n = (rand()%21) - 10;
                T_WALL[t][y*TEX+x] = mortar
                    ? rgb(88+n, 82+n, 78+n)
                    : rgb(wr[t]+n, wg[t]+n, wb[t]+n);
            }
        }
    }
    // Floor: stone tiles
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            bool line = (x%16==0)||(y%16==0);
            int n = (rand()%15)-7;
            T_FLOOR[y*TEX+x] = line ? rgb(48+n,43+n,38+n) : rgb(88+n,82+n,72+n);
        }
    // Ceiling: wooden planks
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            bool grain = (x%4 < 1);
            int n = (rand()%13)-6;
            T_CEIL[y*TEX+x] = grain ? rgb(48+n,33+n,18+n) : rgb(68+n,48+n,28+n);
        }

    // Enemy sprite: friendly robot, centre 32×32 on 64×64, rest transparent
    memset(T_SPRITE, 0, sizeof(T_SPRITE));
    // Body
    for (int y=32; y<58; y++) for (int x=20; x<44; x++) T_SPRITE[y*TEX+x] = rgb(50,190,80);
    // Arms
    for (int y=34; y<52; y++) {
        for (int x=14; x<20; x++) T_SPRITE[y*TEX+x] = rgb(40,170,70);
        for (int x=44; x<50; x++) T_SPRITE[y*TEX+x] = rgb(40,170,70);
    }
    // Neck
    for (int y=28; y<33; y++) for (int x=27; x<37; x++) T_SPRITE[y*TEX+x] = rgb(60,170,90);
    // Head circle
    for (int y=8; y<30; y++) for (int x=14; x<50; x++) {
        int dx=x-32, dy=y-19;
        if (dx*dx+dy*dy < 15*15) T_SPRITE[y*TEX+x] = rgb(100,215,225);
    }
    // Eyes
    for (int y=14; y<20; y++) {
        for (int x=22; x<27; x++) T_SPRITE[y*TEX+x] = rgb(15,15,50);
        for (int x=37; x<42; x++) T_SPRITE[y*TEX+x] = rgb(15,15,50);
    }
    // Eye shine
    for (int x=23; x<25; x++) T_SPRITE[15*TEX+x] = rgb(220,220,255);
    for (int x=38; x<40; x++) T_SPRITE[15*TEX+x] = rgb(220,220,255);
    // Smile
    for (int x=26; x<38; x++) T_SPRITE[25*TEX+x] = rgb(10,60,10);
    T_SPRITE[24*TEX+25] = rgb(10,60,10);
    T_SPRITE[24*TEX+38] = rgb(10,60,10);
    // Antenna
    for (int y=1; y<9; y++) T_SPRITE[y*TEX+32] = rgb(210,210,60);
    for (int y=0; y<4; y++) for (int x=30; x<35; x++) T_SPRITE[y*TEX+x] = rgb(255,90,90);
    // Legs
    for (int y=58; y<64; y++) {
        for (int x=22; x<30; x++) T_SPRITE[y*TEX+x] = rgb(40,140,60);
        for (int x=34; x<42; x++) T_SPRITE[y*TEX+x] = rgb(40,140,60);
    }
}

// ─── Audio ───────────────────────────────────────────────────────────────────
static Mix_Chunk* sndStep  = nullptr;
static Mix_Chunk* sndHello = nullptr;

static Mix_Chunk* makeChunk(std::vector<int16_t>& buf) {
    Mix_Chunk* c = new Mix_Chunk();
    c->allocated = 0;
    c->volume    = MIX_MAX_VOLUME;
    c->alen      = (uint32_t)(buf.size() * sizeof(int16_t));
    c->abuf      = (uint8_t*)buf.data();
    return c;
}

static std::vector<int16_t> stepBuf, helloBuf;

static void genAudio() {
    // Footstep: short noise thud (~90ms @ 44100Hz)
    int n = 44100 * 9 / 100;
    stepBuf.resize(n);
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        float env = (1.f - t) * (1.f - t);
        float noise = ((rand()%2001) - 1000) / 1000.f;
        // Low-pass ish: mix with previous sample
        float s = (i > 0)
            ? 0.5f * noise + 0.5f * (stepBuf[i-1] / 4000.f)
            : noise;
        stepBuf[i] = (int16_t)(s * env * 4000.f);
    }
    sndStep = makeChunk(stepBuf);

    // Hello beep: friendly rising tone when enemy spots player (~250ms)
    int m = 44100 / 4;
    helloBuf.resize(m);
    for (int i = 0; i < m; i++) {
        float t  = (float)i / m;
        float freq = 400.f + t * 300.f; // 400→700 Hz rising
        float env  = t < 0.1f ? t/0.1f : (t > 0.8f ? (1.f-t)/0.2f : 1.f);
        helloBuf[i] = (int16_t)(sinf(2.f * M_PI * freq * i / 44100.f) * env * 5000.f);
    }
    sndHello = makeChunk(helloBuf);
}

// ─── Player ──────────────────────────────────────────────────────────────────
struct Player {
    double x = 2.5, y = 2.5;
    double angle = 0.0;
    int    pitch = 0;         // vertical look offset in pixels
    double moveSpeed = 0.055;
};

// ─── Enemy ───────────────────────────────────────────────────────────────────
struct Enemy {
    double x, y;
    double patrolAngle = 0;
    float  patrolTimer = 0;
    bool   chasing     = false;
    bool   helloed     = false;
};

static std::vector<Enemy> enemies = {
    {4.5, 10.5}, {10.5, 2.5}, {18.5, 5.5},
    {14.5, 10.5}, {6.5, 19.5}, {20.5, 14.5}, {11.5, 20.5},
};

static void updateEnemies(Player& player, float dt) {
    for (auto& e : enemies) {
        double dx = player.x - e.x;
        double dy = player.y - e.y;
        double dist = sqrt(dx*dx + dy*dy);

        if (dist < 7.0) {
            e.chasing = true;
            if (!e.helloed && sndHello) {
                Mix_PlayChannel(-1, sndHello, 0);
                e.helloed = true;
            }
            double speed = 0.018;
            double nx = e.x + (dx/dist) * speed;
            double ny = e.y + (dy/dist) * speed;
            if (MAP[(int)ny][(int)e.x] == 0) e.y = ny;
            if (MAP[(int)e.y][(int)nx] == 0) e.x = nx;
        } else {
            e.chasing   = false;
            e.helloed   = false;
            e.patrolTimer -= dt;
            if (e.patrolTimer <= 0) {
                e.patrolAngle = (rand() % 628) / 100.0;
                e.patrolTimer = 1.2f + (rand()%2000)/1000.f;
            }
            double speed = 0.010;
            double nx = e.x + cos(e.patrolAngle) * speed;
            double ny = e.y + sin(e.patrolAngle) * speed;
            if (MAP[(int)ny][(int)e.x] == 0) e.y = ny;
            if (MAP[(int)e.y][(int)nx] == 0) e.x = nx;
        }
    }
}

// ─── Z-buffer ────────────────────────────────────────────────────────────────
static float zBuf[W];

// ─── DDA Raycasting ──────────────────────────────────────────────────────────
// Returns perpendicular wall distance, sets tile and side
static double castRay(double px, double py, double rdx, double rdy, int& tile, int& side) {
    int mx = (int)px, my = (int)py;
    double ddx = fabs(1.0/rdx), ddy = fabs(1.0/rdy);
    double sdx, sdy;
    int sx, sy;
    if (rdx < 0) { sx=-1; sdx=(px-mx)*ddx; } else { sx=1; sdx=(mx+1.0-px)*ddx; }
    if (rdy < 0) { sy=-1; sdy=(py-my)*ddy; } else { sy=1; sdy=(my+1.0-py)*ddy; }
    while (true) {
        if (sdx < sdy) { sdx+=ddx; mx+=sx; side=0; }
        else           { sdy+=ddy; my+=sy; side=1; }
        if (mx<0||mx>=MW||my<0||my>=MH) return 64;
        if (MAP[my][mx]) { tile=MAP[my][mx]; break; }
    }
    return (side==0) ? (sdx-ddx) : (sdy-ddy);
}

// ─── Render frame into pixel buffer ──────────────────────────────────────────
static void renderFrame(uint32_t* px, const Player& p) {
    double dirX  = cos(p.angle), dirY = sin(p.angle);
    double plnX  = -dirY * CAM,  plnY = dirX * CAM;
    int    hor   = H/2 + p.pitch; // horizon row

    // ── Floor & ceiling ──────────────────────────────────────────────────────
    double rdx0 = dirX - plnX, rdy0 = dirY - plnY; // leftmost ray
    double rdx1 = dirX + plnX, rdy1 = dirY + plnY; // rightmost ray

    for (int y = 0; y < H; y++) {
        int rel = y - hor; // signed distance from horizon
        if (rel == 0) rel = 1;

        float rowDist = (0.5f * H) / (float)rel;
        if (rowDist < 0) rowDist = -rowDist; // above horizon (ceiling side)

        float fsx = rowDist * (float)(rdx1 - rdx0) / W;
        float fsy = rowDist * (float)(rdy1 - rdy0) / W;
        float fx  = (float)p.x + rowDist * (float)rdx0;
        float fy  = (float)p.y + rowDist * (float)rdy0;

        for (int x = 0; x < W; x++) {
            int tx = (int)(fx * TEX) & (TEX-1);
            int ty = (int)(fy * TEX) & (TEX-1);
            uint32_t col;
            if (y > hor) col = dimRGB(T_FLOOR[ty*TEX+tx], rowDist);   // floor
            else         col = dimRGB(T_CEIL [ty*TEX+tx], rowDist*1.4f); // ceiling
            px[y*W+x] = col;
            fx += fsx; fy += fsy;
        }
    }

    // ── Walls ────────────────────────────────────────────────────────────────
    for (int x = 0; x < W; x++) {
        double cam  = 2.0 * x / W - 1.0;
        double rdx  = dirX + plnX * cam;
        double rdy  = dirY + plnY * cam;
        int tile=1, side=0;
        double dist = castRay(p.x, p.y, rdx, rdy, tile, side);
        zBuf[x] = (float)dist;

        int   lh  = (int)(H / dist);
        int   ds  = hor - lh/2;
        int   de  = hor + lh/2;

        // Texture X coordinate
        double wallX = (side==0) ? p.y + dist*rdy : p.x + dist*rdx;
        wallX -= floor(wallX);
        int tx = (int)(wallX * TEX);
        if ((side==0 && rdx>0)||(side==1 && rdy<0)) tx = TEX-tx-1;
        tx = tx < 0 ? 0 : tx >= TEX ? TEX-1 : tx;

        uint32_t* wall = T_WALL[(tile-1)%4];
        for (int y = (ds<0?0:ds); y <= (de>=H?H-1:de); y++) {
            int ty = (y - ds) * TEX / lh;
            ty = ty < 0 ? 0 : ty >= TEX ? TEX-1 : ty;
            uint32_t col = wall[ty*TEX+tx];
            if (side==1) col = dimRGB(col, (float)dist + 1.5f);
            else         col = dimRGB(col, (float)dist);
            px[y*W+x] = col;
        }
    }

    // ── Sprites (enemies) ────────────────────────────────────────────────────
    // Sort farthest first
    struct SD { double dist; int idx; };
    std::vector<SD> sorted;
    for (int i = 0; i < (int)enemies.size(); i++) {
        double dx = enemies[i].x - p.x, dy = enemies[i].y - p.y;
        sorted.push_back({dx*dx+dy*dy, i});
    }
    std::sort(sorted.begin(), sorted.end(), [](const SD& a, const SD& b){ return a.dist > b.dist; });

    double invDet = 1.0 / (plnX*dirY - dirX*plnY);
    for (auto& s : sorted) {
        const Enemy& e = enemies[s.idx];
        double sx = e.x - p.x, sy = e.y - p.y;
        double tX = invDet * ( dirY*sx - dirX*sy);
        double tY = invDet * (-plnY*sx + plnX*sy);
        if (tY <= 0.1) continue;

        int scrX = (int)((W/2) * (1.0 + tX/tY));
        int sprH = abs((int)(H / tY));
        int sprW = sprH;

        int drawXs = scrX - sprW/2;
        int drawXe = scrX + sprW/2;
        int drawYs = hor  - sprH/2;
        int drawYe = hor  + sprH/2;

        for (int sx2 = drawXs; sx2 < drawXe; sx2++) {
            if (sx2 < 0 || sx2 >= W) continue;
            if (tY >= zBuf[sx2]) continue;
            int texX = (sx2 - drawXs) * TEX / sprW;
            for (int sy2 = drawYs; sy2 < drawYe; sy2++) {
                if (sy2 < 0 || sy2 >= H) continue;
                int texY = (sy2 - drawYs) * TEX / sprH;
                uint32_t col = T_SPRITE[texY*TEX+texX];
                if ((col & 0x00FFFFFF) == 0) continue; // transparent
                px[sy2*W+sx2] = dimRGB(col, (float)tY);
            }
        }
    }
}

// ─── Minimap ─────────────────────────────────────────────────────────────────
static void drawMinimap(SDL_Renderer* ren, const Player& p) {
    const int S = 7, OX = 10, OY = 10;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for (int row = 0; row < MH; row++) {
        for (int col = 0; col < MW; col++) {
            SDL_Rect r = {OX+col*S, OY+row*S, S-1, S-1};
            if (MAP[row][col]) SDL_SetRenderDrawColor(ren,180,170,160,200);
            else               SDL_SetRenderDrawColor(ren, 20, 20, 20,160);
            SDL_RenderFillRect(ren, &r);
        }
    }
    // Enemies on minimap
    for (auto& e : enemies) {
        SDL_SetRenderDrawColor(ren, 80, 220, 100, 220);
        SDL_Rect r = {OX+(int)(e.x*S)-2, OY+(int)(e.y*S)-2, 5,5};
        SDL_RenderFillRect(ren, &r);
    }
    // Player
    int px2 = OX + (int)(p.x*S), py2 = OY + (int)(p.y*S);
    SDL_SetRenderDrawColor(ren, 255, 80, 80, 255);
    SDL_Rect dot = {px2-3, py2-3, 6, 6};
    SDL_RenderFillRect(ren, &dot);
    SDL_SetRenderDrawColor(ren, 255, 220, 0, 255);
    SDL_RenderDrawLine(ren, px2, py2,
        px2 + (int)(cos(p.angle)*14), py2 + (int)(sin(p.angle)*14));
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

// ─── Crosshair ───────────────────────────────────────────────────────────────
static void drawCrosshair(SDL_Renderer* ren) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 160);
    SDL_RenderDrawLine(ren, W/2-10, H/2, W/2+10, H/2);
    SDL_RenderDrawLine(ren, W/2, H/2-10, W/2, H/2+10);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    srand((unsigned)time(nullptr));

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    Mix_OpenAudio(44100, AUDIO_S16SYS, 1, 512);
    Mix_AllocateChannels(8);

    SDL_Window*   win = SDL_CreateWindow("C++ Raycaster",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture*  scr = SDL_CreateTexture(ren,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);

    std::vector<uint32_t> pixels(W * H);

    genTextures();
    genAudio();

    Player player;
    SDL_SetRelativeMouseMode(SDL_TRUE);

    bool   running      = true;
    SDL_Event ev;
    float  stepTimer    = 0.f;
    const float STEP_INTERVAL = 0.32f;
    uint32_t lastTick   = SDL_GetTicks();

    while (running) {
        uint32_t now = SDL_GetTicks();
        float dt = (now - lastTick) / 1000.f;
        lastTick = now;

        // ── Events ──────────────────────────────────────────────────────────
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (ev.type == SDL_MOUSEMOTION) {
                player.angle += ev.motion.xrel * SENS;
                player.pitch -= ev.motion.yrel * 1; // vertical look
                if (player.pitch >  250) player.pitch =  250;
                if (player.pitch < -250) player.pitch = -250;
            }
        }

        // ── Movement ─────────────────────────────────────────────────────────
        const uint8_t* k = SDL_GetKeyboardState(nullptr);
        double nx = player.x, ny = player.y;
        bool moving = false;

        if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) {
            nx += cos(player.angle) * player.moveSpeed;
            ny += sin(player.angle) * player.moveSpeed;
            moving = true;
        }
        if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) {
            nx -= cos(player.angle) * player.moveSpeed;
            ny -= sin(player.angle) * player.moveSpeed;
            moving = true;
        }
        if (k[SDL_SCANCODE_A]) {
            nx += cos(player.angle - M_PI/2) * player.moveSpeed;
            ny += sin(player.angle - M_PI/2) * player.moveSpeed;
            moving = true;
        }
        if (k[SDL_SCANCODE_D]) {
            nx += cos(player.angle + M_PI/2) * player.moveSpeed;
            ny += sin(player.angle + M_PI/2) * player.moveSpeed;
            moving = true;
        }

        // Collision (with slight wall offset so you don't clip)
        const double MARGIN = 0.25;
        if (MAP[(int)ny][(int)(player.x)] == 0 &&
            MAP[(int)(ny+MARGIN)][(int)(player.x)] == 0 &&
            MAP[(int)(ny-MARGIN)][(int)(player.x)] == 0) player.y = ny;
        if (MAP[(int)(player.y)][(int)nx] == 0 &&
            MAP[(int)(player.y+MARGIN)][(int)nx] == 0 &&
            MAP[(int)(player.y-MARGIN)][(int)nx] == 0) player.x = nx;

        // Footstep sound
        if (moving) {
            stepTimer -= dt;
            if (stepTimer <= 0 && sndStep) {
                Mix_PlayChannel(0, sndStep, 0);
                stepTimer = STEP_INTERVAL;
            }
        } else {
            stepTimer = 0;
        }

        updateEnemies(player, dt);

        // ── Render ───────────────────────────────────────────────────────────
        renderFrame(pixels.data(), player);

        SDL_UpdateTexture(scr, nullptr, pixels.data(), W * sizeof(uint32_t));
        SDL_RenderCopy(ren, scr, nullptr, nullptr);
        drawMinimap(ren, player);
        drawCrosshair(ren);
        SDL_RenderPresent(ren);
    }

    Mix_CloseAudio();
    SDL_DestroyTexture(scr);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
