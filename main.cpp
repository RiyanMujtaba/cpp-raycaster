#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

// ─── Render resolution (window = 2×) ─────────────────────────────────────────
static const int W = 640, H = 360, TEX = 64;
static const float CAM  = 0.66f;
static const float SENS = 0.0025f;   // mouse sensitivity

static const char* FONT_PATH = "/System/Library/Fonts/Monaco.ttf";

// ─── Game state ───────────────────────────────────────────────────────────────
enum State { MENU, PLAYING, WIN, LOSE };
enum Mode  { SOLO, TEAM };
static State gState = MENU;
static Mode  gMode  = SOLO;
static float gTime  = 0.f;   // seconds since start of current game
static int   gKills = 0;

// ─── Pixel helpers ───────────────────────────────────────────────────────────
inline uint32_t rgb(int r,int g,int b){
    r=r<0?0:r>255?255:r; g=g<0?0:g>255?255:g; b=b<0?0:b>255?255:b;
    return(0xFFu<<24)|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
}
inline uint32_t dimC(uint32_t c,float d){
    float f=1.f/(1.f+d*d*0.09f); if(f>1.f)f=1.f;
    return rgb(int(((c>>16)&0xFF)*f),int(((c>>8)&0xFF)*f),int((c&0xFF)*f));
}

// ─── Map ─────────────────────────────────────────────────────────────────────
static const int MW=28,MH=28;
static int MAP[MH][MW]={
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,0,2,0,0,0,0,0,0,0,1,0,0,0,2,2,2,2,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,2,0,0,0,0,0,0,1},
    {1,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,2,0,0,2,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,3,3,3,3,3,1,0,0,0,2,2,2,2,0,0,0,0,0,0,1},
    {1,1,1,0,1,1,1,0,3,0,0,0,3,1,1,0,1,1,0,1,1,1,0,1,1,1,0,1},
    {1,0,0,0,0,0,1,0,3,0,0,0,3,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1},
    {1,0,4,0,0,0,1,0,3,0,0,0,3,0,0,0,0,0,0,0,0,1,0,0,4,0,0,1},
    {1,0,0,0,0,0,1,0,3,3,3,3,3,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,4,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,4,0,0,0,1},
    {1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,1,1,0,1,1,1,1,1,1,0,1,1,1},
    {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,2,0,0,0,1,0,0,0,4,0,0,0,1,0,0,0,0,2,2,0,0,0,0,1},
    {1,0,0,2,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,2,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,3,0,3,0,0,0,1,0,0,0,4,0,0,0,1,0,0,0,3,0,3,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,1,1,0,1,1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,1},
    {1,0,0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

// ─── Textures ─────────────────────────────────────────────────────────────────
static uint32_t T_WALL[4][TEX*TEX];
static uint32_t T_ENEMY[TEX*TEX];   // orange robot
static uint32_t T_ALLY [TEX*TEX];   // cyan robot
static uint32_t T_BULLET_F[4*4];    // friendly bullet (yellow)
static uint32_t T_BULLET_E[4*4];    // enemy bullet (red)

static void buildRobotSprite(uint32_t* S, int hr, int hg, int hb, int br, int bg, int bb) {
    memset(S, 0, TEX*TEX*sizeof(uint32_t));
    // Body
    for(int y=32;y<58;y++) for(int x=20;x<44;x++) S[y*TEX+x]=rgb(br,bg,bb);
    // Arms
    for(int y=34;y<52;y++){
        for(int x=14;x<20;x++) S[y*TEX+x]=rgb(br-20,bg-20,bb-20);
        for(int x=44;x<50;x++) S[y*TEX+x]=rgb(br-20,bg-20,bb-20);
    }
    // Neck
    for(int y=28;y<33;y++) for(int x=27;x<37;x++) S[y*TEX+x]=rgb(br-30,bg-30,bb-30);
    // Head circle
    for(int y=6;y<30;y++) for(int x=14;x<50;x++){
        int dx=x-32,dy=y-18;
        if(dx*dx+dy*dy<15*15) S[y*TEX+x]=rgb(hr,hg,hb);
    }
    // Eyes
    for(int y=12;y<21;y++) for(int x=20;x<27;x++){int dx=x-23,dy=y-16;if(dx*dx+dy*dy<14)S[y*TEX+x]=rgb(255,255,255);}
    for(int y=12;y<21;y++) for(int x=37;x<44;x++){int dx=x-40,dy=y-16;if(dx*dx+dy*dy<14)S[y*TEX+x]=rgb(255,255,255);}
    for(int y=14;y<18;y++){for(int x=22;x<25;x++)S[y*TEX+x]=rgb(20,20,40);for(int x=39;x<42;x++)S[y*TEX+x]=rgb(20,20,40);}
    // Smile
    for(int x=24;x<40;x++) S[25*TEX+x]=rgb(60,20,10);
    S[24*TEX+23]=S[24*TEX+40]=rgb(60,20,10);
    // Antenna
    for(int y=0;y<7;y++) S[y*TEX+32]=rgb(255,80,180);
    for(int y=0;y<4;y++) for(int x=30;x<35;x++) S[y*TEX+x]=rgb(255,50,140);
    // Legs
    for(int y=58;y<64;y++){for(int x=22;x<30;x++)S[y*TEX+x]=rgb(br-40,bg-40,bb-40);for(int x=34;x<42;x++)S[y*TEX+x]=rgb(br-40,bg-40,bb-40);}
}

static void genTextures(){
    srand(42);
    int wr[]={160,80,55,180},wg[]={75,140,60,50},wb[]={40,200,175,40};
    for(int t=0;t<4;t++) for(int y=0;y<TEX;y++) for(int x=0;x<TEX;x++){
        int bx=x+((y/8)%2)*(TEX/2);
        bool mortar=(y%8==0)||(bx%TEX==0&&y%8!=0);
        int n=(rand()%21)-10;
        T_WALL[t][y*TEX+x]=mortar?rgb(88+n,82+n,78+n):rgb(wr[t]+n,wg[t]+n,wb[t]+n);
    }
    buildRobotSprite(T_ENEMY, 255,140,30,  220,160,40);   // orange enemy
    buildRobotSprite(T_ALLY,   80,230,230,   50,160,220);  // cyan ally
    // Bullet sprites (4x4 — small)
    for(int i=0;i<16;i++) T_BULLET_F[i]=rgb(255,240,80);
    for(int i=0;i<16;i++) T_BULLET_E[i]=rgb(255,60,60);
    srand((unsigned)time(nullptr));
}

// ─── Audio ───────────────────────────────────────────────────────────────────
static Mix_Chunk *sndShoot=nullptr,*sndHit=nullptr,*sndEStep=nullptr,*sndHurt=nullptr;
static std::vector<int16_t> shootBuf,hitBuf,estepBuf,hurtBuf;
static Mix_Chunk* makeChunk(std::vector<int16_t>& b){
    auto* c=new Mix_Chunk();c->allocated=0;c->volume=MIX_MAX_VOLUME;
    c->alen=(uint32_t)(b.size()*sizeof(int16_t));c->abuf=(uint8_t*)b.data();return c;
}
static void genAudio(){
    // Shoot zap
    shootBuf.resize(44100/10);
    for(int i=0;i<(int)shootBuf.size();i++){float t=(float)i/shootBuf.size();float f=1200.f-t*900.f;float e=(1.f-t)*(1.f-t);shootBuf[i]=(int16_t)(sinf(2.f*M_PI*f*i/44100.f)*e*8000.f);}
    sndShoot=makeChunk(shootBuf);
    // Hit boing
    hitBuf.resize(44100/5);
    for(int i=0;i<(int)hitBuf.size();i++){float t=(float)i/hitBuf.size();float f=700.f+sinf(t*M_PI)*500.f;float e=t<0.1f?t/0.1f:(1.f-t);hitBuf[i]=(int16_t)(sinf(2.f*M_PI*f*i/44100.f)*e*5000.f);}
    sndHit=makeChunk(hitBuf);
    // Enemy footstep
    estepBuf.resize(44100*6/100);
    for(int i=0;i<(int)estepBuf.size();i++){float t=(float)i/estepBuf.size();float e=(1.f-t)*(1.f-t);float n=((rand()%2001)-1000)/1000.f;estepBuf[i]=(int16_t)(n*e*1800.f);}
    sndEStep=makeChunk(estepBuf);
    // Player hurt
    hurtBuf.resize(44100/8);
    for(int i=0;i<(int)hurtBuf.size();i++){float t=(float)i/hurtBuf.size();float f=200.f+t*100.f;float e=(1.f-t);hurtBuf[i]=(int16_t)(sinf(2.f*M_PI*f*i/44100.f)*e*6000.f);}
    sndHurt=makeChunk(hurtBuf);
}

// ─── Structs ─────────────────────────────────────────────────────────────────
struct Player {
    double x=2.5,y=2.5,angle=0,pitch=0;
    int hp=100; float shootCD=0; float bobTime=0; float bobOffset=0;
    double moveSpeed=0.055;
};
struct Enemy {
    double x,y,patrolAngle=0;float patrolTimer=0;
    bool alive=true,chasing=false,helloed=false;
    float stepTimer=0,shootTimer=2.f,bobTime=0;
};
struct Ally {
    double x,y,angle=0;bool alive=true;
    float shootTimer=1.f,patrolTimer=0;double patrolAngle=0;float bobTime=0;
};
struct Bullet {
    double x,y,dx,dy;float life=12.f;bool friendly;bool active=true;
};

static Player            player;
static std::vector<Enemy>  enemies;
static std::vector<Ally>   allies;
static std::vector<Bullet> bullets;
static float zBuf[W];

static void resetGame(Mode m){
    gMode=m; gState=PLAYING; gTime=0; gKills=0;
    player={2.5,2.5,0,0,100,0,0,0,0.055};
    enemies={
        {5.5,5.5},{22.5,2.5},{24.5,10.5},
        {15.5,10.5},{6.5,20.5},{22.5,18.5},
        {11.5,22.5},{18.5,24.5},{4.5,15.5},
        {25.5,24.5}  // 10 enemies now
    };
    bullets.clear();
    allies.clear();
    if(m==TEAM){
        allies.push_back({3.5,2.5}); allies.push_back({2.5,4.5});
        allies.push_back({4.5,3.5}); allies.push_back({3.5,5.5});
    }
}

// ─── DDA ─────────────────────────────────────────────────────────────────────
static double castRay(double px,double py,double rdx,double rdy,int&tile,int&side){
    int mx=(int)px,my=(int)py;double ddx=fabs(1./rdx),ddy=fabs(1./rdy),sdx,sdy;int sx,sy;
    if(rdx<0){sx=-1;sdx=(px-mx)*ddx;}else{sx=1;sdx=(mx+1.-px)*ddx;}
    if(rdy<0){sy=-1;sdy=(py-my)*ddy;}else{sy=1;sdy=(my+1.-py)*ddy;}
    while(true){
        if(sdx<sdy){sdx+=ddx;mx+=sx;side=0;}else{sdy+=ddy;my+=sy;side=1;}
        if(mx<0||mx>=MW||my<0||my>=MH)return 64;
        if(MAP[my][mx]){tile=MAP[my][mx];break;}
    }
    return(side==0)?(sdx-ddx):(sdy-ddy);
}
static bool rayHitsWall(double x,double y,double dx,double dy,double dist){
    double tx=x,ty=y;
    for(double d=0;d<dist;d+=0.1){
        tx+=dx*0.1;ty+=dy*0.1;
        int mx=(int)tx,my=(int)ty;
        if(mx<0||mx>=MW||my<0||my>=MH||MAP[my][mx]) return true;
    }
    return false;
}

// ─── Fire bullet ─────────────────────────────────────────────────────────────
static void fireBullet(double x,double y,double angle,bool friendly){
    Bullet b;b.x=x;b.y=y;b.dx=cos(angle)*0.22;b.dy=sin(angle)*0.22;b.friendly=friendly;
    bullets.push_back(b);
}
static void playerShoot(){
    if(player.shootCD>0) return;
    fireBullet(player.x,player.y,player.angle,true);
    if(sndShoot) Mix_PlayChannel(1,sndShoot,0);
    player.shootCD=0.18f;
}

// ─── Update bullets ───────────────────────────────────────────────────────────
static void updateBullets(float dt){
    for(auto& b:bullets){
        if(!b.active) continue;
        b.x+=b.dx; b.y+=b.dy;
        b.life-=0.22f;
        if(b.life<=0){b.active=false;continue;}
        int mx=(int)b.x,my=(int)b.y;
        if(mx<0||mx>=MW||my<0||my>=MH||MAP[my][mx]){b.active=false;continue;}
        if(b.friendly){
            // Hit enemies
            for(auto& e:enemies){
                if(!e.alive)continue;
                double dx=e.x-b.x,dy=e.y-b.y;
                if(dx*dx+dy*dy<0.3){e.alive=false;b.active=false;if(sndHit)Mix_PlayChannel(4,sndHit,0);gKills++;break;}
            }
        } else {
            // Hit player
            double dx=player.x-b.x,dy=player.y-b.y;
            if(dx*dx+dy*dy<0.3){
                player.hp-=10; b.active=false;
                if(sndHurt) Mix_PlayChannel(5,sndHurt,0);
            }
            // Hit allies
            for(auto& a:allies){
                if(!a.alive)continue;
                double ddx=a.x-b.x,ddy=a.y-b.y;
                if(ddx*ddx+ddy*ddy<0.3){a.alive=false;b.active=false;break;}
            }
        }
    }
    bullets.erase(std::remove_if(bullets.begin(),bullets.end(),[](const Bullet&b){return!b.active;}),bullets.end());
}

// ─── AI helpers ──────────────────────────────────────────────────────────────
static void moveAgent(double&x,double&y,double dx,double dy,double spd){
    double nx=x+dx*spd,ny=y+dy*spd;
    const double M=0.25;
    if(MAP[(int)ny][(int)x]==0&&MAP[(int)(ny+M)][(int)x]==0&&MAP[(int)(ny-M)][(int)x]==0)y=ny;
    if(MAP[(int)y][(int)nx]==0&&MAP[(int)(y+M)][(int)nx]==0&&MAP[(int)(y-M)][(int)nx]==0)x=nx;
}

// ─── Update enemies ───────────────────────────────────────────────────────────
static void updateEnemies(float dt){
    for(auto& e:enemies){
        if(!e.alive)continue;
        e.bobTime+=dt*6.f;
        double dx=player.x-e.x,dy=player.y-e.y;
        double dist=sqrt(dx*dx+dy*dy);

        // Also target allies
        int bestAlly=-1; double bestAD=999;
        for(int i=0;i<(int)allies.size();i++){
            if(!allies[i].alive)continue;
            double adx=allies[i].x-e.x,ady=allies[i].y-e.y;
            double ad=sqrt(adx*adx+ady*ady);
            if(ad<bestAD){bestAD=ad;bestAlly=i;}
        }

        bool chasePlayer = dist < 12.0;   // wider aggro range
        bool chaseAlly   = bestAlly>=0 && bestAD < 12.0 && bestAD < dist;

        if(chasePlayer||chaseAlly){
            e.chasing=true;
            double tdx,tdy,td;
            if(chaseAlly&&!chasePlayer){tdx=allies[bestAlly].x-e.x;tdy=allies[bestAlly].y-e.y;td=bestAD;}
            else{tdx=dx;tdy=dy;td=dist;}
            moveAgent(e.x,e.y,tdx/td,tdy/td,0.026); // faster chase
            // Shoot — tighter spread, shorter cooldown
            e.shootTimer-=dt;
            if(e.shootTimer<=0&&td<10.0){
                double spread=((rand()%60)-30)/1000.0; // tighter aim
                double ang=atan2(tdy,tdx)+spread;
                fireBullet(e.x,e.y,ang,false);
                e.shootTimer=1.1f+(rand()%8)/10.f;    // shoots more often
            }
        } else {
            e.chasing=false; e.patrolTimer-=dt;
            if(e.patrolTimer<=0){e.patrolAngle=(rand()%628)/100.;e.patrolTimer=0.8f+(rand()%1200)/1000.f;}
            moveAgent(e.x,e.y,cos(e.patrolAngle),sin(e.patrolAngle),0.014); // faster patrol
        }
        // Step sound
        e.stepTimer-=dt;
        if(e.stepTimer<=0&&sndEStep&&dist<14.0){
            int vol=(int)(MIX_MAX_VOLUME*(1.0-dist/14.0));
            Mix_VolumeChunk(sndEStep,vol); Mix_PlayChannel(3,sndEStep,0);
            e.stepTimer=0.40f;
        }
    }
}

// ─── Update allies ────────────────────────────────────────────────────────────
static void updateAllies(float dt){
    for(auto& a:allies){
        if(!a.alive)continue;
        a.bobTime+=dt*5.f;
        // Always find nearest enemy — no range limit, allies always hunt
        int best=-1; double bestD=999;
        for(int i=0;i<(int)enemies.size();i++){
            if(!enemies[i].alive)continue;
            double dx=enemies[i].x-a.x,dy=enemies[i].y-a.y;
            double d=sqrt(dx*dx+dy*dy);
            if(d<bestD){bestD=d;best=i;}
        }
        if(best>=0){
            double dx=enemies[best].x-a.x,dy=enemies[best].y-a.y;
            a.angle=atan2(dy,dx);
            // Always move toward enemy unless very close
            if(bestD>2.2) moveAgent(a.x,a.y,dx/bestD,dy/bestD,0.022);
            // Shoot when in range — aggressive cooldown
            a.shootTimer-=dt;
            if(a.shootTimer<=0&&bestD<14.0){
                double spread=((rand()%40)-20)/1000.0;
                fireBullet(a.x,a.y,a.angle+spread,true);
                if(sndShoot){Mix_VolumeChunk(sndShoot,MIX_MAX_VOLUME/4);Mix_PlayChannel(6,sndShoot,0);}
                a.shootTimer=0.5f+(rand()%6)/10.f;
            }
        }
        // If no enemies left allies just idle (they won!)
    }
}

// ─── 3D render ───────────────────────────────────────────────────────────────
static void renderFrame(uint32_t* px,const Player& p,float bob){
    int hor=H/2+(int)p.pitch;
    double dirX=cos(p.angle),dirY=sin(p.angle);
    double plnX=-dirY*CAM,plnY=dirX*CAM;

    // Floor / ceiling gradient
    for(int y=0;y<H;y++){
        if(y>=hor){float t=(float)(y-hor)/(H-hor+1);int sh=30+(int)(t*55);uint32_t c=rgb(sh,sh-2,sh-5);for(int x=0;x<W;x++)px[y*W+x]=c;}
        else{float t=(float)(hor-y)/(hor+1);int sh=10+(int)(t*22);uint32_t c=rgb(sh,sh+4,sh+14);for(int x=0;x<W;x++)px[y*W+x]=c;}
    }

    // Walls
    for(int x=0;x<W;x++){
        double cam=2.*x/W-1.;double rdx=dirX+plnX*cam,rdy=dirY+plnY*cam;
        int tile=1,side=0;double dist=castRay(p.x,p.y,rdx,rdy,tile,side);
        zBuf[x]=(float)dist;
        int lh=(int)(H/dist),ds=hor-lh/2,de=hor+lh/2;
        double wallX=(side==0)?p.y+dist*rdy:p.x+dist*rdx;wallX-=floor(wallX);
        int tx=(int)(wallX*TEX);if((side==0&&rdx>0)||(side==1&&rdy<0))tx=TEX-tx-1;tx=tx<0?0:tx>=TEX?TEX-1:tx;
        uint32_t* wall=T_WALL[(tile-1)%4];
        for(int y=(ds<0?0:ds);y<=(de>=H?H-1:de);y++){
            int ty=(y-ds)*TEX/lh;ty=ty<0?0:ty>=TEX?TEX-1:ty;
            uint32_t col=wall[ty*TEX+tx];
            px[y*W+x]=(side==1)?dimC(col,(float)dist+1.2f):dimC(col,(float)dist);
        }
    }

    // Sprites: enemies, allies, bullets — sorted by distance
    struct Spr{double dist;int type;int idx;}; // type 0=enemy,1=ally,2=bullet
    std::vector<Spr> sprList;
    for(int i=0;i<(int)enemies.size();i++){if(!enemies[i].alive)continue;double dx=enemies[i].x-p.x,dy=enemies[i].y-p.y;sprList.push_back({dx*dx+dy*dy,0,i});}
    for(int i=0;i<(int)allies.size();i++){if(!allies[i].alive)continue;double dx=allies[i].x-p.x,dy=allies[i].y-p.y;sprList.push_back({dx*dx+dy*dy,1,i});}
    for(int i=0;i<(int)bullets.size();i++){if(!bullets[i].active)continue;double dx=bullets[i].x-p.x,dy=bullets[i].y-p.y;sprList.push_back({dx*dx+dy*dy,2,i});}
    std::sort(sprList.begin(),sprList.end(),[](const Spr&a,const Spr&b){return a.dist>b.dist;});

    double inv=1./(plnX*dirY-dirX*plnY);
    for(auto& s:sprList){
        double sx,sy; int texW,texH; uint32_t* texData;
        int bobShift=0;
        if(s.type==0){const Enemy& e=enemies[s.idx];sx=e.x-p.x;sy=e.y-p.y;texData=T_ENEMY;texW=texH=TEX;bobShift=(int)(sinf(e.bobTime)*3);}
        else if(s.type==1){const Ally& a=allies[s.idx];sx=a.x-p.x;sy=a.y-p.y;texData=T_ALLY;texW=texH=TEX;bobShift=(int)(sinf(a.bobTime)*3);}
        else{const Bullet& b=bullets[s.idx];sx=b.x-p.x;sy=b.y-p.y;texData=b.friendly?T_BULLET_F:T_BULLET_E;texW=texH=4;}

        double tX=inv*(dirY*sx-dirX*sy),tY=inv*(-plnY*sx+plnX*sy);
        if(tY<=0.05)continue;
        int scrX=(int)((W/2)*(1.+tX/tY));
        int sprH=abs((int)(H/tY)),sprW=(texW==TEX)?sprH:(int)(H/tY/8); // bullets are tiny
        int dys=hor-sprH/2+bobShift,dye=hor+sprH/2+bobShift;
        int dxs=scrX-sprW/2,dxe=scrX+sprW/2;

        for(int cx=dxs;cx<dxe;cx++){
            if(cx<0||cx>=W||tY>=(double)zBuf[cx])continue;
            int ttx=(cx-dxs)*texW/sprW;
            for(int cy=dys;cy<dye;cy++){
                if(cy<0||cy>=H)continue;
                int tty=(cy-dys)*texH/sprH;
                uint32_t col=texData[tty*texW+ttx];
                if((col&0x00FFFFFF)==0)continue;
                if(s.type==2){// Bullets glow: brighten
                    int r=std::min(255,(int)(((col>>16)&0xFF)*2));
                    int g=std::min(255,(int)(((col>>8)&0xFF)*2));
                    int bl=std::min(255,(int)((col&0xFF)*2));
                    col=rgb(r,g,bl);
                } else col=dimC(col,(float)tY);
                px[cy*W+cx]=col;
            }
        }
    }

    // Gun bob at bottom
    int gx=W/2,gy=(int)(H-40+bob);
    for(int y=gy-6;y<gy+6;y++)  for(int x=gx-3;x<gx+22;x++) if(y>=0&&y<H&&x>=0&&x<W)px[y*W+x]=rgb(80,80,80);
    for(int y=gy-3;y<gy+20;y++) for(int x=gx-14;x<gx+12;x++) if(y>=0&&y<H&&x>=0&&x<W)px[y*W+x]=rgb(90,70,55);
    for(int x=gx-13;x<gx+11;x++) if(gy-3>=0&&gy-3<H) px[(gy-3)*W+x]=rgb(130,100,80);

}

// ─── Text helper ──────────────────────────────────────────────────────────────
static void drawText(SDL_Renderer* ren,TTF_Font* font,const std::string& txt,int x,int y,SDL_Color col,bool center=false){
    if(!font)return;
    SDL_Surface* s=TTF_RenderText_Blended(font,txt.c_str(),col);if(!s)return;
    SDL_Texture* t=SDL_CreateTextureFromSurface(ren,s);
    if(center)x-=s->w/2;
    SDL_Rect dst={x,y,s->w,s->h};
    SDL_FreeSurface(s);SDL_RenderCopy(ren,t,nullptr,&dst);SDL_DestroyTexture(t);
}

// ─── Filled rect with alpha ───────────────────────────────────────────────────
static void fillRect(SDL_Renderer* ren,int x,int y,int w,int h,int r,int g,int b,int a){
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,r,g,b,a);
    SDL_Rect rc={x,y,w,h};SDL_RenderFillRect(ren,&rc);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
}
static void drawRect(SDL_Renderer* ren,int x,int y,int w,int h,int r,int g,int b,int thick=1){
    SDL_SetRenderDrawColor(ren,r,g,b,255);
    for(int i=0;i<thick;i++){SDL_Rect rc={x+i,y+i,w-2*i,h-2*i};SDL_RenderDrawRect(ren,&rc);}
}

// ─── Scanlines effect ────────────────────────────────────────────────────────
static void drawScanlines(SDL_Renderer* ren){
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,0,0,0,40);
    for(int y=0;y<H;y+=2) SDL_RenderDrawLine(ren,0,y,W,y);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
}

// ─── Menu screen ─────────────────────────────────────────────────────────────
static void renderMenu(SDL_Renderer* ren,TTF_Font* big,TTF_Font* med,TTF_Font* sm,float t,int mx,int my){
    // Background
    fillRect(ren,0,0,W,H,5,5,15,255);
    // Animated grid
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    for(int y=0;y<H;y+=18){int a=12+(int)(8*sinf(y*0.08f+t*1.5f));SDL_SetRenderDrawColor(ren,0,180,255,a);SDL_RenderDrawLine(ren,0,y,W,y);}
    for(int x=0;x<W;x+=18){int a=12+(int)(8*sinf(x*0.08f-t*1.5f));SDL_SetRenderDrawColor(ren,0,180,255,a);SDL_RenderDrawLine(ren,x,0,x,H);}
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

    // Title glow
    int pulse=(int)(30+20*sinf(t*2.f));
    fillRect(ren,W/2-160,35,320,55,0,200,255,pulse);
    drawText(ren,big,"RAYCASTER",W/2,40,{0,240,255,255},true);
    drawText(ren,sm,"C++ EDITION",W/2,90,{100,200,200,220},true);

    // Divider
    SDL_SetRenderDrawColor(ren,0,200,255,180);
    SDL_RenderDrawLine(ren,W/2-120,112,W/2+120,112);

    // Mode buttons — big, full-width bands so clicking anywhere in them works
    int sc=(int)(180+50*sinf(t*3));
    int tc=(int)(180+50*sinf(t*3+1.f));

    // Solo band (click anywhere here)
    fillRect(ren,0,118,W,80,0,40,55,210);
    drawRect(ren,4,120,W-8,76,0,sc,sc,2);
    drawText(ren,med,"[ 1 ]  SOLO MODE  —  Click here",W/2,130,{0,(uint8_t)sc,(uint8_t)sc,255},true);
    drawText(ren,sm,"You vs all 7 enemies",W/2,158,{100,200,200,200},true);

    // Team band
    fillRect(ren,0,208,W,80,40,0,55,210);
    drawRect(ren,4,210,W-8,76,(uint8_t)tc,0,(uint8_t)tc,2);
    drawText(ren,med,"[ 2 ]  TEAM MODE  —  Click here",W/2,220,{(uint8_t)tc,80,(uint8_t)tc,255},true);
    drawText(ren,sm,"You + 4 allies vs 7 enemies",W/2,248,{180,100,200,200},true);

    // Controls hint
    drawText(ren,sm,"Press 1 or 2  |  WASD move  |  Mouse look  |  LMB shoot  |  ESC quit",W/2,308,{80,120,120,200},true);

    drawScanlines(ren);
}

// ─── HUD ─────────────────────────────────────────────────────────────────────
static void renderHUD(SDL_Renderer* ren,TTF_Font* med,TTF_Font* sm,int kills,int total,float elapsed){
    // Health bar
    int barW=120,barH=12,bx=10,by=H-26;
    fillRect(ren,bx,by,barW,barH,20,20,20,200);
    int hp=player.hp<0?0:player.hp;
    int hw=barW*hp/100;
    int hr=hp>60?0:hp>30?255:255,hg=hp>60?200:hp>30?140:30;
    fillRect(ren,bx,by,hw,barH,hr,hg,0,230);
    drawRect(ren,bx,by,barW,barH,100,100,100,1);
    drawText(ren,sm,"HP",bx+barW+6,by-2,{200,200,200,220});

    // Kill counter top-right
    std::string ks=std::to_string(kills)+"/"+std::to_string(total);
    drawText(ren,med,ks,W-60,8,{0,230,180,255});
    drawText(ren,sm,"KILLS",W-62,28,{100,180,140,200});

    // Mode tag
    drawText(ren,sm,gMode==TEAM?"TEAM 5v5":"SOLO",8,8,{0,180,255,200});

    // Allies alive (team mode)
    if(gMode==TEAM){
        int alive=0; for(auto&a:allies)if(a.alive)alive++;
        drawText(ren,sm,"ALLIES: "+std::to_string(alive),8,22,{80,220,255,200});
    }

    // Crosshair
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,255,255,255,160);
    SDL_RenderDrawLine(ren,W/2-8,H/2,W/2+8,H/2);
    SDL_RenderDrawLine(ren,W/2,H/2-8,W/2,H/2+8);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

    // Minimap
    const int S=5,OX=W-MW*S-8,OY=8+40;
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    fillRect(ren,OX-2,OY-2,MW*S+4,MH*S+4,0,0,0,140);
    for(int r=0;r<MH;r++) for(int c=0;c<MW;c++){
        SDL_Rect rc={OX+c*S,OY+r*S,S-1,S-1};
        if(MAP[r][c]){SDL_SetRenderDrawColor(ren,140,130,120,200);}else{SDL_SetRenderDrawColor(ren,25,25,25,150);}
        SDL_RenderFillRect(ren,&rc);
    }
    for(auto&e:enemies)if(e.alive){SDL_SetRenderDrawColor(ren,255,80,80,230);SDL_Rect r={OX+(int)(e.x*S)-1,OY+(int)(e.y*S)-1,3,3};SDL_RenderFillRect(ren,&r);}
    for(auto&a:allies) if(a.alive){SDL_SetRenderDrawColor(ren,80,220,255,230);SDL_Rect r={OX+(int)(a.x*S)-1,OY+(int)(a.y*S)-1,3,3};SDL_RenderFillRect(ren,&r);}
    int px2=OX+(int)(player.x*S),py2=OY+(int)(player.y*S);
    SDL_SetRenderDrawColor(ren,255,255,255,255);SDL_Rect pd={px2-2,py2-2,4,4};SDL_RenderFillRect(ren,&pd);
    SDL_SetRenderDrawColor(ren,255,220,0,255);
    SDL_RenderDrawLine(ren,px2,py2,px2+(int)(cos(player.angle)*10),py2+(int)(sin(player.angle)*10));
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

    // Hurt flash
    if(hp<30){
        int a=(int)(40+20*sinf(elapsed*8));
        fillRect(ren,0,0,W,H,200,0,0,a);
    }
}

// ─── End screen ──────────────────────────────────────────────────────────────
static void renderEndScreen(SDL_Renderer* ren,TTF_Font* big,TTF_Font* med,TTF_Font* sm,bool won,int kills,int total,float elapsed){
    fillRect(ren,0,0,W,H,0,0,0,200);
    // Panel
    int pw=340,ph=200,px2=W/2-pw/2,py=H/2-ph/2;
    fillRect(ren,px2,py,pw,ph,won?0:40,won?40:0,0,230);
    drawRect(ren,px2,py,pw,ph,won?0:200,won?200:0,0,2);

    // Glow
    int pulse=(int)(30+20*sinf(elapsed*3));
    fillRect(ren,px2,py,pw,ph,won?0:80,won?80:0,0,pulse);

    // Title
    SDL_Color titleCol=won?SDL_Color{0,255,120,255}:SDL_Color{255,60,60,255};
    drawText(ren,big,won?"MISSION COMPLETE":"GAME  OVER",W/2,py+20,titleCol,true);

    // Stats
    SDL_Color statCol={200,220,220,255};
    drawText(ren,med,"Kills:  "+std::to_string(kills)+"/"+std::to_string(total),W/2,py+80,statCol,true);
    int secs=(int)elapsed,mins=secs/60; secs%=60;
    char tbuf[32]; snprintf(tbuf,32,"Time:   %d:%02d",mins,secs);
    drawText(ren,med,std::string(tbuf),W/2,py+108,statCol,true);
    drawText(ren,med,"Health: "+std::to_string(player.hp<0?0:player.hp)+"%",W/2,py+136,statCol,true);

    // Prompt
    float blink=(sinf(elapsed*4)>0)?1.f:0.f;
    if(blink>0.5f) drawText(ren,sm,"Press ENTER to return to menu",W/2,py+170,{150,200,200,200},true);

    drawScanlines(ren);
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main(){
    srand((unsigned)time(nullptr));
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
    TTF_Init();
    Mix_OpenAudio(44100,AUDIO_S16SYS,1,512);
    Mix_AllocateChannels(8);

    SDL_Window* win=SDL_CreateWindow("RAYCASTER",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W*2,H*2,0);
    SDL_Renderer* ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    SDL_Texture* scr=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,W,H);
    SDL_RenderSetLogicalSize(ren,W,H);

    TTF_Font* fBig=TTF_OpenFont(FONT_PATH,28);
    TTF_Font* fMed=TTF_OpenFont(FONT_PATH,14);
    TTF_Font* fSm =TTF_OpenFont(FONT_PATH,10);

    std::vector<uint32_t> pixels(W*H);
    genTextures(); genAudio();

    SDL_SetRelativeMouseMode(SDL_FALSE); // menu uses absolute mouse

    bool    running=true;
    SDL_Event ev;
    uint32_t lastTick=SDL_GetTicks();
    float   menuTime=0.f;
    int     mousex=0,mousey=0;
    float   bobTime=0;

    while(running){
        uint32_t now=SDL_GetTicks();
        float dt=(now-lastTick)/1000.f; if(dt>0.05f)dt=0.05f;
        lastTick=now; menuTime+=dt; if(gState==PLAYING)gTime+=dt;

        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT)running=false;
            if(ev.type==SDL_KEYDOWN){
                if(ev.key.keysym.sym==SDLK_ESCAPE){
                    if(gState==PLAYING){gState=MENU;SDL_SetRelativeMouseMode(SDL_FALSE);}
                    else running=false;
                }
                if(gState==MENU){
                    if(ev.key.keysym.sym==SDLK_1||ev.key.keysym.sym==SDLK_KP_1){resetGame(SOLO);SDL_SetRelativeMouseMode(SDL_TRUE);}
                    if(ev.key.keysym.sym==SDLK_2||ev.key.keysym.sym==SDLK_KP_2){resetGame(TEAM);SDL_SetRelativeMouseMode(SDL_TRUE);}
                }
                if(ev.key.keysym.sym==SDLK_RETURN||(ev.key.keysym.sym==SDLK_SPACE)){
                    if(gState==WIN||gState==LOSE){gState=MENU;SDL_SetRelativeMouseMode(SDL_FALSE);}
                    else if(gState==MENU){resetGame(SOLO);SDL_SetRelativeMouseMode(SDL_TRUE);}
                }
            }
            if(ev.type==SDL_MOUSEMOTION&&gState==MENU){mousex=ev.motion.x;mousey=ev.motion.y;}
            if(ev.type==SDL_MOUSEMOTION&&gState==PLAYING){
                player.angle+=ev.motion.xrel*SENS;
                player.pitch-=ev.motion.yrel*1;
                if(player.pitch>200)player.pitch=200;
                if(player.pitch<-200)player.pitch=-200;
            }
            if(ev.type==SDL_MOUSEBUTTONDOWN){
                if(gState==MENU){
                    // Compare against physical window coords (window = W*2 x H*2)
                    int cx=ev.button.x, cy=ev.button.y;
                    int wW=W*2, wH=H*2;
                    // Solo button: upper area, Team: lower area — full width for reliability
                    if(cy>wH/4 && cy<wH*3/5){ resetGame(SOLO); SDL_SetRelativeMouseMode(SDL_TRUE); }
                    else if(cy>=wH*3/5 && cy<wH*4/5){ resetGame(TEAM); SDL_SetRelativeMouseMode(SDL_TRUE); }
                }
                if(gState==PLAYING&&ev.button.button==SDL_BUTTON_LEFT) playerShoot();
            }
        }

        if(gState==PLAYING){
            // Movement
            const uint8_t* k=SDL_GetKeyboardState(nullptr);
            double nx=player.x,ny=player.y;bool moving=false;
            if(k[SDL_SCANCODE_W]||k[SDL_SCANCODE_UP])  {nx+=cos(player.angle)*player.moveSpeed;ny+=sin(player.angle)*player.moveSpeed;moving=true;}
            if(k[SDL_SCANCODE_S]||k[SDL_SCANCODE_DOWN]){nx-=cos(player.angle)*player.moveSpeed;ny-=sin(player.angle)*player.moveSpeed;moving=true;}
            if(k[SDL_SCANCODE_A]){nx+=cos(player.angle-M_PI/2)*player.moveSpeed;ny+=sin(player.angle-M_PI/2)*player.moveSpeed;moving=true;}
            if(k[SDL_SCANCODE_D]){nx+=cos(player.angle+M_PI/2)*player.moveSpeed;ny+=sin(player.angle+M_PI/2)*player.moveSpeed;moving=true;}
            const double M=0.25;
            if(MAP[(int)ny][(int)player.x]==0&&MAP[(int)(ny+M)][(int)player.x]==0&&MAP[(int)(ny-M)][(int)player.x]==0)player.y=ny;
            if(MAP[(int)player.y][(int)nx]==0&&MAP[(int)(player.y+M)][(int)nx]==0&&MAP[(int)(player.y-M)][(int)nx]==0)player.x=nx;
            if(moving){bobTime+=dt*9;player.bobOffset=sinf(bobTime)*5.f;}else player.bobOffset*=0.85f;
            if(player.shootCD>0)player.shootCD-=dt;

            updateEnemies(dt);
            if(gMode==TEAM)updateAllies(dt);
            updateBullets(dt);

            // Win / lose check
            bool allDead=true; for(auto&e:enemies)if(e.alive){allDead=false;break;}
            if(allDead){gState=WIN;SDL_SetRelativeMouseMode(SDL_FALSE);}
            if(player.hp<=0){gState=LOSE;SDL_SetRelativeMouseMode(SDL_FALSE);}

            renderFrame(pixels.data(),player,player.bobOffset);
            SDL_UpdateTexture(scr,nullptr,pixels.data(),W*sizeof(uint32_t));
            SDL_RenderCopy(ren,scr,nullptr,nullptr);
            renderHUD(ren,fMed,fSm,gKills,(int)enemies.size(),gTime);
        }
        else if(gState==MENU){
            SDL_SetRenderDrawColor(ren,0,0,0,255);SDL_RenderClear(ren);
            renderMenu(ren,fBig,fMed,fSm,menuTime,mousex,mousey);
        }
        else{
            SDL_UpdateTexture(scr,nullptr,pixels.data(),W*sizeof(uint32_t));
            SDL_RenderCopy(ren,scr,nullptr,nullptr);
            renderEndScreen(ren,fBig,fMed,fSm,gState==WIN,gKills,(int)enemies.size(),gTime);
        }
        drawScanlines(ren);
        SDL_RenderPresent(ren);
    }

    TTF_CloseFont(fBig);TTF_CloseFont(fMed);TTF_CloseFont(fSm);
    Mix_CloseAudio();TTF_Quit();
    SDL_DestroyTexture(scr);SDL_DestroyRenderer(ren);SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
