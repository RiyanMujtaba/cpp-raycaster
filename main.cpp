#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <algorithm>

// ─── Internal render resolution (window is 2× this = 1280×720) ───────────────
static const int W   = 640;
static const int H   = 360;
static const int TEX = 64;
static const float CAM  = 0.66f;
static const float SENS = 0.0006f;

// ─── Pixel helper ────────────────────────────────────────────────────────────
inline uint32_t rgb(int r, int g, int b) {
    r=r<0?0:r>255?255:r; g=g<0?0:g>255?255:g; b=b<0?0:b>255?255:b;
    return (0xFFu<<24)|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
}
inline uint32_t dimC(uint32_t c, float d) {
    float f = 1.f/(1.f+d*d*0.09f); if(f>1.f)f=1.f;
    return rgb(int(((c>>16)&0xFF)*f),int(((c>>8)&0xFF)*f),int((c&0xFF)*f));
}

// ─── Map ─────────────────────────────────────────────────────────────────────
static const int MW=24, MH=24;
static int MAP[MH][MW]={
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

// ─── Wall textures ────────────────────────────────────────────────────────────
static uint32_t T_WALL[4][TEX*TEX];
static uint32_t T_SPRITE[TEX*TEX]; // 0 = transparent

static void genTextures() {
    srand(42);
    int wr[]={160,80,55,180}, wg[]={75,140,60,50}, wb[]={40,200,175,40};
    for(int t=0;t<4;t++) {
        for(int y=0;y<TEX;y++) for(int x=0;x<TEX;x++) {
            int bx=x+((y/8)%2)*(TEX/2);
            bool mortar=(y%8==0)||(bx%TEX==0&&y%8!=0);
            int n=(rand()%21)-10;
            T_WALL[t][y*TEX+x]=mortar?rgb(88+n,82+n,78+n):rgb(wr[t]+n,wg[t]+n,wb[t]+n);
        }
    }
    // Friendly robot sprite (brighter, more cartoon-like)
    memset(T_SPRITE,0,sizeof(T_SPRITE));
    // Body: bright yellow
    for(int y=32;y<58;y++) for(int x=20;x<44;x++) T_SPRITE[y*TEX+x]=rgb(240,200,50);
    // Arms
    for(int y=34;y<52;y++) {
        for(int x=14;x<20;x++) T_SPRITE[y*TEX+x]=rgb(220,180,40);
        for(int x=44;x<50;x++) T_SPRITE[y*TEX+x]=rgb(220,180,40);
    }
    // Neck
    for(int y=28;y<33;y++) for(int x=27;x<37;x++) T_SPRITE[y*TEX+x]=rgb(200,170,40);
    // Head: round, bright orange
    for(int y=6;y<30;y++) for(int x=14;x<50;x++) {
        int dx=x-32,dy=y-18;
        if(dx*dx+dy*dy<15*15) T_SPRITE[y*TEX+x]=rgb(255,140,30);
    }
    // Eyes: big white circles with pupils
    for(int y=12;y<21;y++) for(int x=20;x<27;x++) { int dx=x-23,dy=y-16; if(dx*dx+dy*dy<14) T_SPRITE[y*TEX+x]=rgb(255,255,255); }
    for(int y=12;y<21;y++) for(int x=37;x<44;x++) { int dx=x-40,dy=y-16; if(dx*dx+dy*dy<14) T_SPRITE[y*TEX+x]=rgb(255,255,255); }
    // Pupils
    for(int y=14;y<18;y++) for(int x=22;x<25;x++) T_SPRITE[y*TEX+x]=rgb(30,30,30);
    for(int y=14;y<18;y++) for(int x=39;x<42;x++) T_SPRITE[y*TEX+x]=rgb(30,30,30);
    // Big smile
    for(int x=24;x<40;x++) T_SPRITE[24*TEX+x]=rgb(80,30,10);
    for(int x=22;x<25;x++) T_SPRITE[23*TEX+x]=rgb(80,30,10);
    for(int x=39;x<42;x++) T_SPRITE[23*TEX+x]=rgb(80,30,10);
    // Antenna: pink
    for(int y=0;y<7;y++) T_SPRITE[y*TEX+32]=rgb(255,100,180);
    for(int y=0;y<4;y++) for(int x=30;x<35;x++) T_SPRITE[y*TEX+x]=rgb(255,50,150);
    // Legs
    for(int y=58;y<64;y++) {
        for(int x=22;x<30;x++) T_SPRITE[y*TEX+x]=rgb(200,160,30);
        for(int x=34;x<42;x++) T_SPRITE[y*TEX+x]=rgb(200,160,30);
    }
    srand((unsigned)time(nullptr));
}

// ─── Audio ────────────────────────────────────────────────────────────────────
static Mix_Chunk* sndShoot=nullptr, *sndHit=nullptr,
                 *sndEnemyStep=nullptr, *sndHello=nullptr;
static std::vector<int16_t> shootBuf, hitBuf, eStepBuf, helloBuf;

static Mix_Chunk* makeChunk(std::vector<int16_t>& buf) {
    auto* c=new Mix_Chunk(); c->allocated=0; c->volume=MIX_MAX_VOLUME;
    c->alen=(uint32_t)(buf.size()*sizeof(int16_t)); c->abuf=(uint8_t*)buf.data();
    return c;
}

static void genAudio() {
    // Shoot: sharp laser zap
    shootBuf.resize(44100/10);
    for(int i=0;i<(int)shootBuf.size();i++){
        float t=(float)i/shootBuf.size();
        float freq=1200.f-t*900.f;
        float env=(1.f-t)*(1.f-t);
        shootBuf[i]=(int16_t)(sinf(2.f*M_PI*freq*i/44100.f)*env*7000.f);
    }
    sndShoot=makeChunk(shootBuf);

    // Hit: fun boing
    hitBuf.resize(44100/5);
    for(int i=0;i<(int)hitBuf.size();i++){
        float t=(float)i/hitBuf.size();
        float freq=600.f+sinf(t*M_PI)*400.f;
        float env=t<0.1f?t/0.1f:(1.f-t);
        hitBuf[i]=(int16_t)(sinf(2.f*M_PI*freq*i/44100.f)*env*5000.f);
    }
    sndHit=makeChunk(hitBuf);

    // Enemy footstep: lighter tap
    eStepBuf.resize(44100*6/100);
    for(int i=0;i<(int)eStepBuf.size();i++){
        float t=(float)i/eStepBuf.size();
        float env=(1.f-t)*(1.f-t);
        float n=((rand()%2001)-1000)/1000.f;
        eStepBuf[i]=(int16_t)(n*env*2000.f);
    }
    sndEnemyStep=makeChunk(eStepBuf);

    // Hello beep: rising friendly tone
    helloBuf.resize(44100/4);
    for(int i=0;i<(int)helloBuf.size();i++){
        float t=(float)i/helloBuf.size();
        float freq=400.f+t*300.f;
        float env=t<0.1f?t/0.1f:(t>0.8f?(1.f-t)/0.2f:1.f);
        helloBuf[i]=(int16_t)(sinf(2.f*M_PI*freq*i/44100.f)*env*4000.f);
    }
    sndHello=makeChunk(helloBuf);
}

// ─── Player ───────────────────────────────────────────────────────────────────
struct Player {
    double x=2.5, y=2.5, angle=0.0;
    int    pitch=0;
    double moveSpeed=0.055;
};

// ─── Enemy ────────────────────────────────────────────────────────────────────
struct Enemy {
    double x, y;
    double patrolAngle=0; float patrolTimer=0;
    bool   chasing=false, helloed=false, alive=true;
    float  stepTimer=0;
    float  bobTime=0;      // for up/down bounce animation
};

static std::vector<Enemy> enemies={
    {4.5,10.5},{10.5,2.5},{18.5,5.5},
    {14.5,10.5},{6.5,19.5},{20.5,14.5},{11.5,20.5},
};

static void updateEnemies(Player& player, float dt) {
    for(auto& e:enemies) {
        if(!e.alive) continue;
        double dx=player.x-e.x, dy=player.y-e.y;
        double dist=sqrt(dx*dx+dy*dy);
        bool moving=false;
        e.bobTime+=dt*6.f;

        if(dist<7.0) {
            e.chasing=true;
            if(!e.helloed&&sndHello){Mix_PlayChannel(2,sndHello,0);e.helloed=true;}
            double spd=0.018;
            double nx=e.x+(dx/dist)*spd, ny=e.y+(dy/dist)*spd;
            if(MAP[(int)ny][(int)e.x]==0) e.y=ny;
            if(MAP[(int)e.y][(int)nx]==0) e.x=nx;
            moving=true;
        } else {
            e.chasing=false; e.helloed=false;
            e.patrolTimer-=dt;
            if(e.patrolTimer<=0){e.patrolAngle=(rand()%628)/100.0;e.patrolTimer=1.2f+(rand()%2000)/1000.f;}
            double spd=0.010;
            double nx=e.x+cos(e.patrolAngle)*spd, ny=e.y+sin(e.patrolAngle)*spd;
            if(MAP[(int)ny][(int)e.x]==0) e.y=ny;
            if(MAP[(int)e.y][(int)nx]==0) e.x=nx;
            moving=true;
        }

        // Enemy footstep
        if(moving){ e.stepTimer-=dt; if(e.stepTimer<=0&&sndEnemyStep&&dist<12.0){
            // Volume based on distance
            int vol=(int)(MIX_MAX_VOLUME*(1.0-dist/12.0));
            Mix_VolumeChunk(sndEnemyStep,vol);
            Mix_PlayChannel(3,sndEnemyStep,0);
            e.stepTimer=0.40f;
        }}
    }
}

// ─── Shoot ────────────────────────────────────────────────────────────────────
static float zBuf[W];

static void shoot(Player& player) {
    if(sndShoot) Mix_PlayChannel(1,sndShoot,0);
    double dirX=cos(player.angle),dirY=sin(player.angle);
    double plnX=-dirY*CAM,plnY=dirX*CAM;
    double inv=1.0/(plnX*dirY-dirX*plnY);
    int    best=-1; double bestD=999;
    for(int i=0;i<(int)enemies.size();i++){
        auto& e=enemies[i]; if(!e.alive) continue;
        double sx=e.x-player.x,sy=e.y-player.y;
        double tX=inv*(dirY*sx-dirX*sy);
        double tY=inv*(-plnY*sx+plnX*sy);
        if(tY<=0) continue;
        if(fabs(tX/tY)<0.18&&tY<bestD){bestD=tY;best=i;}
    }
    if(best>=0){
        enemies[best].alive=false;
        if(sndHit) Mix_PlayChannel(4,sndHit,0);
    }
}

// ─── DDA ─────────────────────────────────────────────────────────────────────
static double castRay(double px,double py,double rdx,double rdy,int&tile,int&side){
    int mx=(int)px,my=(int)py;
    double ddx=fabs(1.0/rdx),ddy=fabs(1.0/rdy),sdx,sdy;
    int sx,sy;
    if(rdx<0){sx=-1;sdx=(px-mx)*ddx;}else{sx=1;sdx=(mx+1.0-px)*ddx;}
    if(rdy<0){sy=-1;sdy=(py-my)*ddy;}else{sy=1;sdy=(my+1.0-py)*ddy;}
    while(true){
        if(sdx<sdy){sdx+=ddx;mx+=sx;side=0;}else{sdy+=ddy;my+=sy;side=1;}
        if(mx<0||mx>=MW||my<0||my>=MH) return 64;
        if(MAP[my][mx]){tile=MAP[my][mx];break;}
    }
    return (side==0)?(sdx-ddx):(sdy-ddy);
}

// ─── Render ───────────────────────────────────────────────────────────────────
static void renderFrame(uint32_t* px, const Player& p, float bobOffset) {
    int hor=H/2+p.pitch;

    double dirX=cos(p.angle),dirY=sin(p.angle);
    double plnX=-dirY*CAM, plnY=dirX*CAM;

    // ── Floor & ceiling: simple gradient (fast, no texture lookup) ───────────
    for(int y=0;y<H;y++){
        if(y>=hor){
            // Floor: warm grey
            float t=(float)(y-hor)/(H-hor+1);
            int sh=35+(int)(t*55);
            uint32_t col=rgb(sh,sh-3,sh-6);
            for(int x=0;x<W;x++) px[y*W+x]=col;
        } else {
            // Ceiling: cool dark blue-grey
            float t=(float)(hor-y)/(hor+1);
            int sh=12+(int)(t*22);
            uint32_t col=rgb(sh,sh+4,sh+12);
            for(int x=0;x<W;x++) px[y*W+x]=col;
        }
    }

    // ── Walls ────────────────────────────────────────────────────────────────
    for(int x=0;x<W;x++){
        double cam=2.0*x/W-1.0;
        double rdx=dirX+plnX*cam, rdy=dirY+plnY*cam;
        int tile=1,side=0;
        double dist=castRay(p.x,p.y,rdx,rdy,tile,side);
        zBuf[x]=(float)dist;

        int lh=(int)(H/dist);
        int ds=hor-lh/2, de=hor+lh/2;
        double wallX=(side==0)?p.y+dist*rdy:p.x+dist*rdx;
        wallX-=floor(wallX);
        int tx=(int)(wallX*TEX);
        if((side==0&&rdx>0)||(side==1&&rdy<0)) tx=TEX-tx-1;
        tx=tx<0?0:tx>=TEX?TEX-1:tx;

        uint32_t* wall=T_WALL[(tile-1)%4];
        for(int y=(ds<0?0:ds);y<=(de>=H?H-1:de);y++){
            int ty=(y-ds)*TEX/lh; ty=ty<0?0:ty>=TEX?TEX-1:ty;
            uint32_t col=wall[ty*TEX+tx];
            col=(side==1)?dimC(col,(float)dist+1.2f):dimC(col,(float)dist);
            px[y*W+x]=col;
        }
    }

    // ── Sprites ──────────────────────────────────────────────────────────────
    struct SD{double dist;int idx;};
    std::vector<SD> sorted;
    for(int i=0;i<(int)enemies.size();i++){
        if(!enemies[i].alive) continue;
        double dx=enemies[i].x-p.x,dy=enemies[i].y-p.y;
        sorted.push_back({dx*dx+dy*dy,i});
    }
    std::sort(sorted.begin(),sorted.end(),[](const SD&a,const SD&b){return a.dist>b.dist;});

    double inv=1.0/(plnX*dirY-dirX*plnY);
    for(auto& s:sorted){
        const Enemy& e=enemies[s.idx];
        double sx=e.x-p.x,sy=e.y-p.y;
        double tX=inv*(dirY*sx-dirX*sy);
        double tY=inv*(-plnY*sx+plnX*sy);
        if(tY<=0.1) continue;

        int scrX=(int)((W/2)*(1.0+tX/tY));
        int sprH=abs((int)(H/tY));
        int sprW=sprH;

        // Bob up/down
        int bob=(int)(sinf(e.bobTime)*3);
        int dys=hor-sprH/2+bob, dye=hor+sprH/2+bob;
        int dxs=scrX-sprW/2,    dxe=scrX+sprW/2;

        for(int cx=dxs;cx<dxe;cx++){
            if(cx<0||cx>=W||tY>=(double)zBuf[cx]) continue;
            int texX=(cx-dxs)*TEX/sprW;
            for(int cy=dys;cy<dye;cy++){
                if(cy<0||cy>=H) continue;
                int texY=(cy-dys)*TEX/sprH;
                uint32_t col=T_SPRITE[texY*TEX+texX];
                if((col&0x00FFFFFF)==0) continue;
                px[cy*W+cx]=dimC(col,(float)tY);
            }
        }
    }

    // ── Gun (bottom centre, bobs with movement) ───────────────────────────────
    int gx=W/2, gy=(int)(H-40+bobOffset);
    // Barrel
    for(int y=gy-6;y<gy+6;y++) for(int x=gx-3;x<gx+22;x++) {
        if(y>=0&&y<H&&x>=0&&x<W) px[y*W+x]=rgb(80,80,80);
    }
    // Body
    for(int y=gy-3;y<gy+18;y++) for(int x=gx-14;x<gx+12;x++) {
        if(y>=0&&y<H&&x>=0&&x<W) px[y*W+x]=rgb(90,70,55);
    }
    // Highlight
    for(int x=gx-13;x<gx+11;x++) { if(gy-3>=0&&gy-3<H) px[(gy-3)*W+x]=rgb(130,100,80); }
}

// ─── Minimap ─────────────────────────────────────────────────────────────────
static void drawMinimap(SDL_Renderer* ren, const Player& p) {
    const int S=7,OX=10,OY=10;
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    for(int r=0;r<MH;r++) for(int c=0;c<MW;c++){
        SDL_Rect rect={OX+c*S,OY+r*S,S-1,S-1};
        if(MAP[r][c]) SDL_SetRenderDrawColor(ren,160,150,140,200);
        else          SDL_SetRenderDrawColor(ren,20,20,20,160);
        SDL_RenderFillRect(ren,&rect);
    }
    for(auto& e:enemies) if(e.alive){
        SDL_SetRenderDrawColor(ren,80,220,100,220);
        SDL_Rect r={OX+(int)(e.x*S)-2,OY+(int)(e.y*S)-2,5,5};
        SDL_RenderFillRect(ren,&r);
    }
    int px2=OX+(int)(p.x*S),py2=OY+(int)(p.y*S);
    SDL_SetRenderDrawColor(ren,255,80,80,255);
    SDL_Rect dot={px2-3,py2-3,6,6}; SDL_RenderFillRect(ren,&dot);
    SDL_SetRenderDrawColor(ren,255,220,0,255);
    SDL_RenderDrawLine(ren,px2,py2,px2+(int)(cos(p.angle)*14),py2+(int)(sin(p.angle)*14));
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
}

// ─── Crosshair ───────────────────────────────────────────────────────────────
static void drawCrosshair(SDL_Renderer* ren){
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,255,255,255,180);
    SDL_RenderDrawLine(ren,W/2-8,H/2,W/2+8,H/2);
    SDL_RenderDrawLine(ren,W/2,H/2-8,W/2,H/2+8);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
}

// ─── Kill counter ────────────────────────────────────────────────────────────
static void drawKills(SDL_Renderer* ren, int kills, int total){
    // Simple coloured rectangles as score dots
    for(int i=0;i<total;i++){
        SDL_Rect r={W-14-i*12,8,10,10};
        if(i<kills) SDL_SetRenderDrawColor(ren,100,230,100,255);
        else         SDL_SetRenderDrawColor(ren,80,80,80,200);
        SDL_RenderFillRect(ren,&r);
    }
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main(){
    srand((unsigned)time(nullptr));
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
    Mix_OpenAudio(44100,AUDIO_S16SYS,1,512);
    Mix_AllocateChannels(8);

    // Window is 2× the render resolution for crisp pixel look
    SDL_Window*   win=SDL_CreateWindow("C++ Raycaster",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W*2,H*2,0);
    SDL_Renderer* ren=SDL_CreateRenderer(win,-1,
        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    // Nearest-neighbour scaling keeps pixels sharp
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    SDL_Texture* scr=SDL_CreateTexture(ren,
        SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,W,H);
    // Logical size so all SDL draw calls also use internal res
    SDL_RenderSetLogicalSize(ren,W,H);

    std::vector<uint32_t> pixels(W*H);
    genTextures();
    genAudio();

    Player player;
    SDL_SetRelativeMouseMode(SDL_TRUE);

    bool    running=true;
    SDL_Event ev;
    uint32_t lastTick=SDL_GetTicks();
    float   bobTime=0;
    float   bobOffset=0;
    int     kills=0;
    const int TOTAL=(int)enemies.size();

    while(running){
        uint32_t now=SDL_GetTicks();
        float dt=(now-lastTick)/1000.f; if(dt>0.05f)dt=0.05f;
        lastTick=now;

        // ── Events ──────────────────────────────────────────────────────────
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT) running=false;
            if(ev.type==SDL_KEYDOWN&&ev.key.keysym.sym==SDLK_ESCAPE) running=false;
            if(ev.type==SDL_MOUSEMOTION){
                player.angle+=ev.motion.xrel*SENS;
                player.pitch-=ev.motion.yrel*1;
                if(player.pitch> 200) player.pitch= 200;
                if(player.pitch<-200) player.pitch=-200;
            }
            if(ev.type==SDL_MOUSEBUTTONDOWN&&ev.button.button==SDL_BUTTON_LEFT){
                int before=kills;
                shoot(player);
                kills=0; for(auto&e:enemies) if(!e.alive) kills++;
                (void)before;
            }
        }

        // ── Movement ─────────────────────────────────────────────────────────
        const uint8_t* k=SDL_GetKeyboardState(nullptr);
        double nx=player.x,ny=player.y;
        bool moving=false;
        if(k[SDL_SCANCODE_W]||k[SDL_SCANCODE_UP])   {nx+=cos(player.angle)*player.moveSpeed;ny+=sin(player.angle)*player.moveSpeed;moving=true;}
        if(k[SDL_SCANCODE_S]||k[SDL_SCANCODE_DOWN])  {nx-=cos(player.angle)*player.moveSpeed;ny-=sin(player.angle)*player.moveSpeed;moving=true;}
        if(k[SDL_SCANCODE_A]) {nx+=cos(player.angle-M_PI/2)*player.moveSpeed;ny+=sin(player.angle-M_PI/2)*player.moveSpeed;moving=true;}
        if(k[SDL_SCANCODE_D]) {nx+=cos(player.angle+M_PI/2)*player.moveSpeed;ny+=sin(player.angle+M_PI/2)*player.moveSpeed;moving=true;}

        const double M=0.25;
        if(MAP[(int)ny][(int)player.x]==0&&MAP[(int)(ny+M)][(int)player.x]==0&&MAP[(int)(ny-M)][(int)player.x]==0) player.y=ny;
        if(MAP[(int)player.y][(int)nx]==0&&MAP[(int)(player.y+M)][(int)nx]==0&&MAP[(int)(player.y-M)][(int)nx]==0) player.x=nx;

        // Gun bob
        if(moving){ bobTime+=dt*9; bobOffset=sinf(bobTime)*5.f; }
        else      { bobOffset*=0.85f; }

        updateEnemies(player,dt);

        // ── Render ───────────────────────────────────────────────────────────
        renderFrame(pixels.data(),player,bobOffset);
        SDL_UpdateTexture(scr,nullptr,pixels.data(),W*sizeof(uint32_t));
        SDL_RenderCopy(ren,scr,nullptr,nullptr);
        drawMinimap(ren,player);
        drawCrosshair(ren);
        drawKills(ren,kills,TOTAL);
        SDL_RenderPresent(ren);
    }

    Mix_CloseAudio();
    SDL_DestroyTexture(scr); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
