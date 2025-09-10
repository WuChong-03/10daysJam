#pragma region インクルード
#define NOMINMAX
#include <Novice.h>
#include <Windows.h>
#include <algorithm>
#include <math.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <random>
#include <ctime>
#include <cstdio>
#pragma endregion

#pragma region 基本設定と定数
const char kWindowTitle[] = "teamWork";

const int SCREEN_W = 1920;
const int SCREEN_H = 1080;

const float CAM_LERP = 0.15f;
const float CAM_SNAP = 0.5f;

const int TILE = 64;
const int MAP_COLS = 94;
const int MAP_ROWS = 94;
const int MAP_W_PX = MAP_COLS * TILE;
const int MAP_H_PX = MAP_ROWS * TILE;

const int   PLAYER_W = 64;
const int   PLAYER_H = 80;
const float HALF_W = PLAYER_W * 0.5f;
const float HALF_H = PLAYER_H * 0.5f;
// 仅用于碰撞
const int   PLY_COLL_W = 33;
const int   PLY_COLL_H = 52;
const float PLY_COLL_HALF_W = PLY_COLL_W * 0.5f;
const float PLY_COLL_HALF_H = PLY_COLL_H * 0.5f;

// 敌人碰撞盒（和玩家同规格，命中更公平）
const int   ENEMY_COLL_W = 33;
const int   ENEMY_COLL_H = 52;

constexpr float EPS = 0.001f;

const char* LIGHT_TEX_PATH = "./assets/play/env/img/light_mask.png";
const int   LIGHT_TEX_BASE = 1024;
int         gLightTex = -1;

// 灯光/电量
const float R_MIN = 120.0f;
const float R_MAX = 600.0f;
const float FUEL_MAX = 100.0f;
const float DRAIN_PER_FRAME = FUEL_MAX / (35.0f * 60.0f);
const float RADIUS_LERP = 0.20f;

// 三个灯光档位对应半径
const float LIGHT_LEVELS[3] = { 180.0f, 280.0f, 550.0f };
const float DRAIN_MULT[3] = { 0.6f, 1.0f, 1.6f };

constexpr unsigned int COL_WHITE = 0xFFFFFFFFu;
constexpr unsigned int COL_BLACK = 0x000000FFu;
constexpr unsigned int COL_BATT = 0x00FF00FFu;
constexpr unsigned int COL_ITEM = 0xFF00FFFFu;
constexpr unsigned int COL_PLY = 0x55AAFFFFu;
constexpr unsigned int COL_MM_BG = 0x000000A0u;
constexpr unsigned int COL_MM_P = 0x00FFFFFFu;
constexpr unsigned int COL_ENEMY = 0xFF0000FFu;

const float BATT_RESPAWN_SEC = 90.0f;
const float FRAME_SEC = 1.0f / 60.0f;
const float PICK_RADIUS = 48.0f;
const int   DOT_SZ = 20;

const int MINI_W = 288;
const int MINI_H = 288;
const int MINI_MARGIN = 12;

const int   ENEMY_W = 64;
const int   ENEMY_H = 80;
const float ENEMY_SPEED = 3.0f;
const int ENEMIES_PER_QUADRANT = 3;   // ★ 敌人数量入口
const float BACKTRACK_CHANCE_WHEN_ALT_EXISTS = 0.0f;
// 固定视野半径保留但不用于“发现玩家”
const float ENEMY_VIEW_RADIUS = 260.0f;
const float CHASE_REPATH_SEC = 0.25f;
const float CHASE_LOST_SECONDS = 10.0f;

const float ENEMY_ANIM_FPS = 30.0f;
const int ENEMY_FRAMES = 7;
const float ENEMY_ANIM_DT = 1.0f / ENEMY_ANIM_FPS;

const int   PLAYER_FRAMES = 9;
const float PLAYER_ANIM_FPS = 20.0f;
const float PLAYER_ANIM_DT = 1.0f / PLAYER_ANIM_FPS;

// 渲染缩放
const float RENDER_SCALE = 1.5f;
inline float ViewW() { return SCREEN_W / RENDER_SCALE; }
inline float ViewH() { return SCREEN_H / RENDER_SCALE; }

// BGM参数
const float DANGER_ENTER_DIST = 220.0f; // 保留
const float DANGER_EXIT_DIST = 270.0f; // 保留
const float DANGER_LINGER_SEC = 4.0f;
const float ALERT_LINGER_SEC = 2.5f;

const float MAIN_BGM_VOL = 0.50f;
const float ALERT_BGM_VOL = 0.80f;
const float DANGER_BGM_VOL = 0.85f;

const float FADE_UP_PER_SEC = 1.50f;
const float FADE_DOWN_PER_SEC = 1.30f;

const float ALERT_MAX_DIST = 500.0f;
const float DANGER_VOL_FAR_WHEN_CHASE = 650.0f;
const float DANGER_VOL_MIN_WHEN_CHASE = 0.12f;

const float DANGER_TAIL_IN_STAGE1 = 0.6f; // 保留

const int  MAX_BATT_ACTIVE = 6;   // 同时最多在地图上存在的电池数
const bool BATT_ENABLE_RESPAWN = true; // 关掉=不再刷新

#pragma endregion

#pragma region 小工具/数据结构
inline bool AABBOverlap(float cx1, float cy1, int w1, int h1,
    float cx2, float cy2, int w2, int h2) {
    float hw1 = w1 * 0.5f, hh1 = h1 * 0.5f;
    float hw2 = w2 * 0.5f, hh2 = h2 * 0.5f;
    return (fabsf(cx1 - cx2) <= (hw1 + hw2)) && (fabsf(cy1 - cy2) <= (hh1 + hh2));
}

struct BatterySpawn { float x, y; bool alive; float timer; };
struct Collectible { float x, y; bool alive; };

inline float Dist2(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2; return dx * dx + dy * dy;
}
inline void TileCenter(int r, int c, float& x, float& y) {
    x = c * (float)TILE + TILE * 0.5f;
    y = r * (float)TILE + TILE * 0.5f;
}
inline void WorldToRC(float x, float y, int& r, int& c) {
    c = (int)floorf(x / (float)TILE);
    r = (int)floorf(y / (float)TILE);
    if (r < 0) r = 0; if (r >= MAP_ROWS) r = MAP_ROWS - 1;
    if (c < 0) c = 0; if (c >= MAP_COLS) c = MAP_COLS - 1;
}
#pragma endregion

#pragma region マップ/CSV
enum TileType : int { T_WALL = 0, T_PATH = 1, T_SAFE = 2, T_BATT = 3, T_ITEM = 4 };
int tileGrid[MAP_ROWS][MAP_COLS];

static bool LoadTileCSV(const char* path) {
    std::ifstream fin(path);
    if (!fin.is_open()) return false;
    std::string line; int r = 0;
    while (std::getline(fin, line) && r < MAP_ROWS) {
        std::stringstream ss(line);
        std::string cell; int c = 0;
        while (std::getline(ss, cell, ',') && c < MAP_COLS) {
            while (!cell.empty() && (cell.back() == '\r' || cell.back() == ' ')) cell.pop_back();
            while (!cell.empty() && cell.front() == ' ') cell.erase(cell.begin());
            int v = 1; if (!cell.empty()) { try { v = std::stoi(cell); } catch (...) { v = 1; } }
            if (v < 0 || v > 4) v = 1;
            tileGrid[r][c++] = v;
        }
        while (c < MAP_COLS) tileGrid[r][c++] = 1;
        ++r;
    }
    while (r < MAP_ROWS) { for (int c = 0; c < MAP_COLS; ++c) tileGrid[r][c] = 1; ++r; }
    return true;
}

inline int CollAtRC(int r, int c) {
    if (r < 0 || r >= MAP_ROWS || c < 0 || c >= MAP_COLS) return 1;
    return (tileGrid[r][c] == T_WALL) ? 1 : 0;
}
#pragma endregion

#pragma region 路线CSV（1~4 巡逻线）
int routeGrid[MAP_ROWS][MAP_COLS];

static bool LoadRouteCSV(const char* path) {
    std::ifstream fin(path);
    if (!fin.is_open()) return false;
    std::string line; int r = 0;
    while (std::getline(fin, line) && r < MAP_ROWS) {
        std::stringstream ss(line);
        std::string cell; int c = 0;
        while (std::getline(ss, cell, ',') && c < MAP_COLS) {
            while (!cell.empty() && (cell.back() == '\r' || cell.back() == ' ')) cell.pop_back();
            while (!cell.empty() && cell.front() == ' ') cell.erase(cell.begin());
            int v = 0; if (!cell.empty()) { try { v = std::stoi(cell); } catch (...) { v = 0; } }
            if (v < 0) v = 0; if (v > 4) v = 4;
            routeGrid[r][c++] = v;
        }
        while (c < MAP_COLS) routeGrid[r][c++] = 0;
        ++r;
    }
    while (r < MAP_ROWS) { for (int c = 0; c < MAP_COLS; ++c) routeGrid[r][c] = 0; ++r; }
    return true;
}
#pragma endregion

#pragma region 敌人AI（巡逻/追击/返航 + 动画）
inline bool EnemyWalkableTile(int r, int c) {
    if (r < 0 || r >= MAP_ROWS || c < 0 || c >= MAP_COLS) return false;
    int t = tileGrid[r][c];
    return (t != T_WALL && t != T_SAFE);
}
inline bool InQuadrant(int r, int c, int routeId) {
    int midR = MAP_ROWS / 2, midC = MAP_COLS / 2;
    switch (routeId) {
    case 1: return (r < midR && c < midC);
    case 2: return (r < midR && c >= midC);
    case 3: return (r >= midR && c < midC);
    case 4: return (r >= midR && c >= midC);
    default: return true;
    }
}
inline bool RoutePassableWithId(int r, int c, int routeId) {
    if (!EnemyWalkableTile(r, c)) return false;
    if (!InQuadrant(r, c, routeId)) return false;
    return (routeGrid[r][c] == routeId);
}
inline bool HasRouteNeighborWithId(int r, int c, int routeId) {
    const int dr[4] = { 1,-1,0,0 };
    const int dc[4] = { 0,0,1,-1 };
    for (int k = 0; k < 4; ++k) {
        int nr = r + dr[k], nc = c + dc[k];
        if (RoutePassableWithId(nr, nc, routeId)) return true;
    }
    return false;
}

static bool BFSPath(int sr, int sc, int gr, int gc, std::vector<std::pair<int, int>>& out) {
    out.clear();
    if (!EnemyWalkableTile(sr, sc) || !EnemyWalkableTile(gr, gc)) return false;

    static int vis[MAP_ROWS][MAP_COLS];
    static std::pair<int, int> pre[MAP_ROWS][MAP_COLS];
    for (int r = 0; r < MAP_ROWS; ++r)
        for (int c = 0; c < MAP_COLS; ++c) { vis[r][c] = 0; pre[r][c] = { -1,-1 }; }

    std::queue<std::pair<int, int>> q;
    q.push({ sr,sc }); vis[sr][sc] = 1;
    const int dr[4] = { 1,-1,0,0 }, dc[4] = { 0,0,1,-1 };
    bool found = false;

    while (!q.empty()) {
        auto u = q.front(); q.pop();
        if (u.first == gr && u.second == gc) { found = true; break; }
        for (int k = 0; k < 4; ++k) {
            int nr = u.first + dr[k], nc = u.second + dc[k];
            if (nr < 0 || nr >= MAP_ROWS || nc < 0 || nc >= MAP_COLS) continue;
            if (vis[nr][nc]) continue;
            if (!EnemyWalkableTile(nr, nc)) continue;
            vis[nr][nc] = 1; pre[nr][nc] = u; q.push({ nr,nc });
        }
    }
    if (!found) return false;

    std::vector<std::pair<int, int>> rev;
    int r = gr, c = gc;
    while (!(r == sr && c == sc)) { rev.push_back({ r,c }); auto p = pre[r][c]; r = p.first; c = p.second; }
    rev.push_back({ sr,sc });
    out.assign(rev.rbegin(), rev.rend());
    return true;
}

static bool BFSPath8(int sr, int sc, int gr, int gc, std::vector<std::pair<int, int>>& out) {
    out.clear();
    if (!EnemyWalkableTile(sr, sc) || !EnemyWalkableTile(gr, gc)) return false;

    static int vis[MAP_ROWS][MAP_COLS];
    static std::pair<int, int> pre[MAP_ROWS][MAP_COLS];
    for (int r = 0; r < MAP_ROWS; ++r)
        for (int c = 0; c < MAP_COLS; ++c) { vis[r][c] = 0; pre[r][c] = { -1,-1 }; }

    std::queue<std::pair<int, int>> q;
    q.push({ sr,sc }); vis[sr][sc] = 1;

    const int dr[8] = { 1,-1,0,0,  1,1,-1,-1 };
    const int dc[8] = { 0,0,1,-1, 1,-1, 1,-1 };

    auto canDiag = [&](int r0, int c0, int r1, int c1)->bool {
        int vr = r1 - r0, vc = c1 - c0;
        if (abs(vr) + abs(vc) != 2) return true;
        return EnemyWalkableTile(r0, c0 + vc) && EnemyWalkableTile(r0 + vr, c0);
        };

    bool found = false;
    while (!q.empty()) {
        auto u = q.front(); q.pop();
        if (u.first == gr && u.second == gc) { found = true; break; }
        for (int k = 0; k < 8; ++k) {
            int nr = u.first + dr[k], nc = u.second + dc[k];
            if (nr < 0 || nr >= MAP_ROWS || nc < 0 || nc >= MAP_COLS) continue;
            if (vis[nr][nc]) continue;
            if (!EnemyWalkableTile(nr, nc)) continue;
            if (!canDiag(u.first, u.second, nr, nc)) continue;
            vis[nr][nc] = 1; pre[nr][nc] = u; q.push({ nr,nc });
        }
    }
    if (!found) return false;

    std::vector<std::pair<int, int>> rev;
    int r = gr, c = gc;
    while (!(r == sr && c == sc)) { rev.push_back({ r,c }); auto p = pre[r][c]; r = p.first; c = p.second; }
    rev.push_back({ sr,sc });
    out.assign(rev.rbegin(), rev.rend());
    return true;
}

static bool FindNearestRouteTileById(int sr, int sc, int routeId, int& rr, int& rc) {
    static int vis[MAP_ROWS][MAP_COLS];
    for (int r = 0; r < MAP_ROWS; ++r) for (int c = 0; c < MAP_COLS; ++c) vis[r][c] = 0;

    std::queue<std::pair<int, int>> q;
    q.push({ sr,sc }); vis[sr][sc] = 1;

    const int dr[4] = { 1,-1,0,0 }, dc[4] = { 0,0,1,-1 };
    while (!q.empty()) {
        auto u = q.front(); q.pop();
        if (InQuadrant(u.first, u.second, routeId) &&
            routeGrid[u.first][u.second] == routeId &&
            EnemyWalkableTile(u.first, u.second)) {
            rr = u.first; rc = u.second; return true;
        }
        for (int k = 0; k < 4; ++k) {
            int nr = u.first + dr[k], nc = u.second + dc[k];
            if (nr < 0 || nr >= MAP_ROWS || nc < 0 || nc >= MAP_COLS) continue;
            if (vis[nr][nc]) continue;
            if (!EnemyWalkableTile(nr, nc)) continue;
            vis[nr][nc] = 1; q.push({ nr,nc });
        }
    }
    return false;
}

enum EnemyState { ES_Patrol, ES_Chase, ES_Return };
enum EnemyDir { ED_Front = 0, ED_Back = 1, ED_Left = 2, ED_Right = 3 };

struct Enemy {
    float x = 0.f, y = 0.f;
    int   curR = 0, curC = 0;
    int   tgtR = 0, tgtC = 0;
    int   prevR = -1, prevC = -1;
    int   routeId = 1;
    EnemyState state = ES_Patrol;

    std::vector<std::pair<int, int>> path;
    int   pathIdx = 0;
    float repathTimer = 0.0f;
    float lostTimer = 0.0f;

    int   dir = ED_Front;
    int   frame = 0;
    float animAccum = 0.0f;
    float lastVX = 0.0f, lastVY = 0.0f;
};

static void EnemyStepToTarget(Enemy& e) {
    float tx, ty; TileCenter(e.tgtR, e.tgtC, tx, ty);
    float dx = tx - e.x, dy = ty - e.y;

    e.lastVX = 0.0f; e.lastVY = 0.0f;
    if (fabsf(dx) >= fabsf(dy)) {
        float step = copysignf(std::min(fabsf(dx), ENEMY_SPEED), dx);
        e.x += step; e.lastVX = step;
    }
    else {
        float step = copysignf(std::min(fabsf(dy), ENEMY_SPEED), dy);
        e.y += step; e.lastVY = step;
    }

    if (e.x < HALF_W) e.x = HALF_W;
    if (e.y < HALF_H) e.y = HALF_H;
    if (e.x > MAP_W_PX - HALF_W) e.x = MAP_W_PX - HALF_W;
    if (e.y > MAP_H_PX - HALF_H) e.y = MAP_H_PX - HALF_H;
}

static void EnemyStepToTargetFree(Enemy& e) {
    float tx, ty; TileCenter(e.tgtR, e.tgtC, tx, ty);
    float dx = tx - e.x, dy = ty - e.y;
    float len = sqrtf(dx * dx + dy * dy);
    e.lastVX = 0.0f; e.lastVY = 0.0f;
    if (len > 0.0001f) {
        float vx = dx / len * ENEMY_SPEED;
        float vy = dy / len * ENEMY_SPEED;
        if (fabsf(vx) > fabsf(dx)) vx = dx;
        if (fabsf(vy) > fabsf(dy)) vy = dy;
        e.x += vx; e.y += vy;
        e.lastVX = vx; e.lastVY = vy;
    }
    if (e.x < HALF_W) e.x = HALF_W;
    if (e.y < HALF_H) e.y = HALF_H;
    if (e.x > MAP_W_PX - HALF_W) e.x = MAP_W_PX - HALF_W;
    if (e.y > MAP_H_PX - HALF_H) e.y = MAP_H_PX - HALF_H;
}

static void ChooseNextOnRoute(Enemy& e, std::mt19937& rng) {
    if (!RoutePassableWithId(e.curR, e.curC, e.routeId)) return;

    struct RC { int r, c; };
    RC cand[4]; int n = 0;
    const int dr[4] = { 1,-1,0,0 }, dc[4] = { 0,0,1,-1 };
    for (int k = 0; k < 4; ++k) {
        int nr = e.curR + dr[k], nc = e.curC + dc[k];
        if (RoutePassableWithId(nr, nc, e.routeId)) cand[n++] = { nr,nc };
    }
    if (n == 0) { e.tgtR = (e.prevR >= 0) ? e.prevR : e.curR; e.tgtC = (e.prevC >= 0) ? e.prevC : e.curC; return; }

    std::vector<RC> poolNoBack, poolAll;
    for (int i = 0; i < n; ++i) {
        poolAll.push_back(cand[i]);
        if (!(cand[i].r == e.prevR && cand[i].c == e.prevC)) poolNoBack.push_back(cand[i]);
    }
    std::uniform_int_distribution<int> pickAll(0, (int)poolAll.size() - 1);
    if (!poolNoBack.empty()) {
        std::uniform_real_distribution<float> uf(0.f, 1.f);
        if (uf(rng) < BACKTRACK_CHANCE_WHEN_ALT_EXISTS) {
            auto rc = poolAll[pickAll(rng)]; e.tgtR = rc.r; e.tgtC = rc.c;
        }
        else {
            std::uniform_int_distribution<int> pick(0, (int)poolNoBack.size() - 1);
            auto rc = poolNoBack[pick(rng)]; e.tgtR = rc.r; e.tgtC = rc.c;
        }
    }
    else {
        auto rc = poolAll[pickAll(rng)]; e.tgtR = rc.r; e.tgtC = rc.c;
    }
}
#pragma endregion

#pragma region 遮罩绘制（光圈PNG）
static inline void DrawDarkCircleHole(int cx, int cy, int radius, unsigned int blackARGB = COL_BLACK) {
    if (gLightTex < 0 || radius <= 0) {
        Novice::DrawBox(0, 0, SCREEN_W, SCREEN_H, 0.0f, blackARGB, kFillModeSolid);
        return;
    }
    float diameter = 2.0f * (float)radius;
    float s = diameter / (float)LIGHT_TEX_BASE;
    int   w = (int)(LIGHT_TEX_BASE * s);
    int   h = w;
    int   x = cx - w / 2;
    int   y = cy - h / 2;

    if (y > 0)                        Novice::DrawBox(0, 0, SCREEN_W, y, 0.0f, blackARGB, kFillModeSolid);
    if (y + h < SCREEN_H)             Novice::DrawBox(0, y + h, SCREEN_W, SCREEN_H - (y + h), 0.0f, blackARGB, kFillModeSolid);
    int midY = y; if (midY < 0) midY = 0;
    int midH = h; if (midY + midH > SCREEN_H) midH = SCREEN_H - midY; if (midH < 0) midH = 0;
    if (x > 0 && midH > 0)            Novice::DrawBox(0, midY, x, midH, 0.0f, blackARGB, kFillModeSolid);
    if (x + w < SCREEN_W && midH > 0) Novice::DrawBox(x + w, midY, SCREEN_W - (x + w), midH, 0.0f, blackARGB, kFillModeSolid);

    Novice::DrawSprite(x, y, gLightTex, s, s, 0.0f, COL_WHITE);
}
#pragma endregion

#pragma region エントリポイント
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    Novice::Initialize(kWindowTitle, SCREEN_W, SCREEN_H);

    // 资源
    int mapTex = Novice::LoadTexture("./assets/play/env/img/map.png");
    gLightTex = Novice::LoadTexture(LIGHT_TEX_PATH);

    // 双路径回退加载音频
    auto LoadAudioTry = [](const char* pAssets, const char* pAsetts)->int {
        int id = Novice::LoadAudio(pAssets);
        if (id >= 0) return id;
        return Novice::LoadAudio(pAsetts);
        };

    // 安全设置音量
    auto SetVoiceVol = [](int voice, float vol) {
        if (voice >= 0) {
            if (vol < 0.0f) vol = 0.0f;
            if (vol > 1.0f) vol = 1.0f;
            Novice::SetAudioVolume(voice, vol);
        }
        };

    // 线性靠近
    auto ApproachLinear = [](float cur, float tgt, float ratePerSec)->float {
        float step = ratePerSec * FRAME_SEC;
        if (cur < tgt) { cur += step; if (cur > tgt) cur = tgt; }
        else if (cur > tgt) { cur -= step; if (cur < tgt) cur = tgt; }
        return cur;
        };

    // Audio: BGM + Running
    int sfxBgm = Novice::LoadAudio("./assets/play/bgm/play.mp3");
    int sfxRun = Novice::LoadAudio("./assets/play/player/sfx/running.mp3");
    int voiceBgm = -1;
    int voiceRun = -1;
    if (sfxBgm >= 0) {
        voiceBgm = Novice::PlayAudio(sfxBgm, /*loop=*/true, /*volume=*/MAIN_BGM_VOL);
    }

    // 一阶段/二阶段 BGM
    int sfxAlert = LoadAudioTry("./assets/play/enemy/sfx/1.mp3", "./asetts/play/enemy/sfx/1.mp3");
    int sfxDanger = LoadAudioTry("./assets/play/enemy/sfx/2.mp3", "./asetts/play/enemy/sfx/2.mp3");

    int voiceAlert = -1;
    int voiceDanger = -1;
    if (sfxAlert >= 0) voiceAlert = Novice::PlayAudio(sfxAlert,  /*loop=*/true, /*volume=*/0.0f);
    if (sfxDanger >= 0) voiceDanger = Novice::PlayAudio(sfxDanger, /*loop=*/true, /*volume=*/0.0f);

    // 当前音量缓存
    float volMain = MAIN_BGM_VOL;
    float volAlert = 0.0f;
    float volDanger = 0.0f;

    // 玩家贴图
    int playerTex[4][PLAYER_FRAMES];
    for (int d = 0; d < 4; ++d)
        for (int f = 0; f < PLAYER_FRAMES; ++f)
            playerTex[d][f] = -1;

    auto loadDir4 = [&](const char* dirName, int dIdx) {
        for (int i = 0; i < PLAYER_FRAMES; ++i) {
            char path[256];
            std::snprintf(path, sizeof(path), "./assets/play/player/img/%s_%02d.png", dirName, i);
            playerTex[dIdx][i] = Novice::LoadTexture(path);
        }
        };
    loadDir4("front", 0);
    loadDir4("back", 1);
    loadDir4("left", 2);
    loadDir4("right", 3);

    // 敌人贴图
    int enemyTex[4][ENEMY_FRAMES];
    for (int d = 0; d < 4; ++d)
        for (int f = 0; f < ENEMY_FRAMES; ++f)
            enemyTex[d][f] = -1;

    auto loadEnemyDirN = [&](const char* dirName, int dIdx) {
        for (int i = 0; i < ENEMY_FRAMES; ++i) {
            char path[256];
            std::snprintf(path, sizeof(path), "./assets/play/enemy/img/%s_%02d.png", dirName, i);
            enemyTex[dIdx][i] = Novice::LoadTexture(path);
        }
        };
    loadEnemyDirN("front", 0);
    loadEnemyDirN("back", 1);
    loadEnemyDirN("left", 2);
    loadEnemyDirN("right", 3);

    // 玩家动画状态
    enum PlayerDir { PD_Front = 0, PD_Back = 1, PD_Left = 2, PD_Right = 3 };
    int   playerDir = PD_Front, playerFrame = 0;
    float playerAnimAccum = 0.0f;

    (void)LoadTileCSV("./assets/play/env/collision_94x94.csv");
    (void)LoadRouteCSV("./assets/play/env/route_combined_94x94.csv");

    char keys[256] = { 0 }, preKeys[256] = { 0 };

    // 初始玩家/相机
    float playerX = 256.0f, playerY = 256.0f;
    float speed = 5.0f;
    float camX = std::clamp(playerX - SCREEN_W * 0.5f, 0.0f, (float)(MAP_W_PX - SCREEN_W));
    float camY = std::clamp(playerY - SCREEN_H * 0.5f, 0.0f, (float)(MAP_H_PX - SCREEN_H));

    // 灯光/电量
    float fuel = FUEL_MAX;
    int   batteryInv = 4, collectedCnt = 0; // 收集物计数
    bool  showMiniMap = true;

    // 灯光三档：默认中档
    int   lightLevel = 1; // 0/1/2 => 180/280/550
    float targetRadius = LIGHT_LEVELS[lightLevel];
    float radius = targetRadius;

    // 刷新点
    std::vector<BatterySpawn> batteries;
    std::vector<Collectible>  collectibles;

    auto RebuildPickups = [&]() {
        batteries.clear();
        collectibles.clear();

        std::vector<int> battIndices;

        for (int r = 0; r < MAP_ROWS; ++r) {
            for (int c = 0; c < MAP_COLS; ++c) {
                int t = tileGrid[r][c];
                float x, y; TileCenter(r, c, x, y);

                if (t == T_BATT) {
                    batteries.push_back({ x, y, false, 0.0f }); // 先全设为未激活
                    battIndices.push_back((int)batteries.size() - 1);
                }
                else if (t == T_ITEM) {
                    collectibles.push_back({ x, y, true });
                }
            }
        }

        // 随机挑选一部分激活（不超过 MAX_BATT_ACTIVE）
        std::mt19937 shufRng((unsigned)time(nullptr));
        std::shuffle(battIndices.begin(), battIndices.end(), shufRng);

        int aliveN = std::min((int)battIndices.size(), MAX_BATT_ACTIVE);
        for (int i = 0; i < aliveN; ++i) {
            batteries[battIndices[i]].alive = true;
        }
        // 其余的给一个初始计时，避免同时刷一堆
        for (int i = aliveN; i < (int)battIndices.size(); ++i) {
            batteries[battIndices[i]].timer = (float)(i - aliveN) * 3.0f; // 可随意
        }
        };

    RebuildPickups();

    // 小地图
    const float miniSX = (float)MINI_W / (float)MAP_W_PX;
    const float miniSY = (float)MINI_H / (float)MAP_H_PX;
    const int   miniX = SCREEN_W - MINI_W - MINI_MARGIN;
    const int   miniY = MINI_MARGIN;

    // RNG
    std::mt19937 rng(static_cast<unsigned>(time(nullptr)));

    // 敌人容器
    std::vector<Enemy> enemies;
    enemies.reserve(4);

    // 敌人颜色
    auto enemyColor = [](int id) -> unsigned int {
        switch (id) {
        case 1: return 0xFF66CCFFu; // 粉
        case 2: return 0xFFA500FFu; // 橙
        case 3: return 0xFF0000FFu; // 红
        case 4: return 0x1E90FFFFu; // 蓝
        default: return COL_ENEMY;
        }
        };

    // 生成指定巡逻线上的敌人
    auto spawnEnemyOnRoute = [&](int routeId) {
        std::vector<std::pair<int, int>> pool;
        for (int r = 0; r < MAP_ROWS; ++r) {
            for (int c = 0; c < MAP_COLS; ++c) {
                if (routeGrid[r][c] == routeId &&
                    RoutePassableWithId(r, c, routeId) &&
                    HasRouteNeighborWithId(r, c, routeId)) {
                    pool.push_back({ r, c });
                }
            }
        }

        if (!pool.empty()) {
            std::uniform_int_distribution<int> pick(0, static_cast<int>(pool.size()) - 1);
            auto rc = pool[pick(rng)];

            float sx, sy;
            TileCenter(rc.first, rc.second, sx, sy);

            Enemy e{};
            e.x = sx;  e.y = sy;
            e.curR = rc.first;  e.curC = rc.second;
            e.tgtR = e.curR;    e.tgtC = e.curC;
            e.prevR = -1;       e.prevC = -1;
            e.routeId = routeId;
            e.state = ES_Patrol;

            e.dir = ED_Front;
            e.frame = 0;
            e.animAccum = 0.0f;
            e.lastVX = 0.0f;
            e.lastVY = 0.0f;

            ChooseNextOnRoute(e, rng);
            enemies.push_back(e);
        }
        };

    auto RebuildEnemies = [&]() {
        enemies.clear();
        for (int rid = 1; rid <= 4; ++rid) {
            for (int i = 0; i < ENEMIES_PER_QUADRANT; ++i) {
                spawnEnemyOnRoute(rid);
            }
        }
        };
    RebuildEnemies();

    // 重置游戏（胜利/失败均调用）
    auto ResetGame = [&]() {
        // 停跑步声
        if (voiceRun >= 0) { Novice::StopAudio(voiceRun); voiceRun = -1; }

        // 玩家
        playerX = 256.0f; playerY = 256.0f;
        playerDir = PD_Front; playerFrame = 0; playerAnimAccum = 0.0f;
        speed = 5.0f;

        // 相机
        camX = std::clamp(playerX - SCREEN_W * 0.5f, 0.0f, (float)(MAP_W_PX - SCREEN_W));
        camY = std::clamp(playerY - SCREEN_H * 0.5f, 0.0f, (float)(MAP_H_PX - SCREEN_H));

        // 灯光/电量
        fuel = FUEL_MAX;
        batteryInv = 4;
        collectedCnt = 0;
        lightLevel = 1;
        targetRadius = LIGHT_LEVELS[lightLevel];
        radius = targetRadius;

        // 拾取和敌人
        RebuildPickups();
        RebuildEnemies();

        // BGM音量回到主
        volMain = MAIN_BGM_VOL;
        volAlert = 0.0f;
        volDanger = 0.0f;
        SetVoiceVol(voiceBgm, volMain);
        SetVoiceVol(voiceAlert, volAlert);
        SetVoiceVol(voiceDanger, volDanger);
        };

    // ===== 主循环 =====
    while (Novice::ProcessMessage() == 0) {
        Novice::BeginFrame();
        memcpy(preKeys, keys, 256);
        Novice::GetHitKeyStateAll(keys);

        // E：没电充电，否则切换档位
        if (!preKeys[DIK_SPACE] && keys[DIK_SPACE]) {
            if (fuel <= 0.0f && batteryInv > 0) {
                batteryInv--;
                fuel = FUEL_MAX;
            }
            else {
                lightLevel = (lightLevel + 1) % 3;
            }
        }

        // 移动输入（归一化）
        int ix = 0, iy = 0;
        if (keys[DIK_A] || keys[DIK_LEFT])  ix -= 1;
        if (keys[DIK_D] || keys[DIK_RIGHT]) ix += 1;
        if (keys[DIK_W] || keys[DIK_UP])    iy -= 1;
        if (keys[DIK_S] || keys[DIK_DOWN])  iy += 1;

        float dx = 0.0f, dy = 0.0f;
        if (ix != 0 || iy != 0) {
            float len = sqrtf((float)(ix * ix + iy * iy));
            float inv = 1.0f / len;
            dx = ix * speed * inv;
            dy = iy * speed * inv;
        }

        // 跑步声
        bool isMoving = (ix != 0 || iy != 0);
        if (isMoving) {
            if (voiceRun < 0 && sfxRun >= 0) {
                voiceRun = Novice::PlayAudio(sfxRun, /*loop=*/true, /*volume=*/0.8f);
            }
        }
        else {
            if (voiceRun >= 0) { Novice::StopAudio(voiceRun); voiceRun = -1; }
        }

        // 面向与动画
        if (isMoving) {
            if (abs(ix) >= abs(iy))  playerDir = (ix > 0) ? PD_Right : PD_Left;
            else                     playerDir = (iy > 0) ? PD_Front : PD_Back;

            playerAnimAccum += FRAME_SEC;
            while (playerAnimAccum >= PLAYER_ANIM_DT) {
                playerAnimAccum -= PLAYER_ANIM_DT;
                playerFrame = (playerFrame + 1) % PLAYER_FRAMES;
            }
        }
        else {
            playerFrame = 0;
            playerAnimAccum = 0.0f;
        }

        // 玩家碰撞（X）
        if (dx != 0.0f) {
            float newX = playerX + dx;
            float top = playerY - PLY_COLL_HALF_H, bottom = playerY + PLY_COLL_HALF_H;
            int r0 = (int)floorf(top / (float)TILE);
            int r1 = (int)floorf((bottom - EPS) / (float)TILE);
            r0 = std::clamp(r0, 0, MAP_ROWS - 1);
            r1 = std::clamp(r1, 0, MAP_ROWS - 1);
            if (dx > 0.0f) {
                float right = newX + PLY_COLL_HALF_W;
                int c = (int)floorf((right - EPS) / (float)TILE);
                c = std::min(c, MAP_COLS - 1);
                bool hit = false; for (int r = r0; r <= r1; ++r) if (CollAtRC(r, c)) { hit = true; break; }
                if (hit) newX = (float)(c * TILE) - PLY_COLL_HALF_W;
            }
            else {
                float left = newX - PLY_COLL_HALF_W;
                int c = (int)floorf(left / (float)TILE);
                c = std::max(c, 0);
                bool hit = false; for (int r = r0; r <= r1; ++r) if (CollAtRC(r, c)) { hit = true; break; }
                if (hit) newX = (float)((c + 1) * TILE) + PLY_COLL_HALF_W;
            }
            playerX = newX;
        }

        // 玩家碰撞（Y）
        if (dy != 0.0f) {
            float newY = playerY + dy;
            float left = playerX - PLY_COLL_HALF_W, right = playerX + PLY_COLL_HALF_W;
            int c0 = (int)floorf(left / (float)TILE);
            int c1 = (int)floorf((right - EPS) / (float)TILE);
            c0 = std::clamp(c0, 0, MAP_COLS - 1);
            c1 = std::clamp(c1, 0, MAP_COLS - 1);
            if (dy > 0.0f) {
                float bottom = newY + PLY_COLL_HALF_H;
                int r = (int)floorf((bottom - EPS) / (float)TILE);
                r = std::min(r, MAP_ROWS - 1);
                bool hit = false; for (int c = c0; c <= c1; ++c) if (CollAtRC(r, c)) { hit = true; break; }
                if (hit) newY = (float)(r * TILE) - PLY_COLL_HALF_H;
            }
            else {
                float top = newY - PLY_COLL_HALF_H;
                int r = (int)floorf(top / (float)TILE);
                r = std::max(r, 0);
                bool hit = false; for (int c = c0; c <= c1; ++c) if (CollAtRC(r, c)) { hit = true; break; }
                if (hit) newY = (float)((r + 1) * TILE) + PLY_COLL_HALF_H;
            }
            playerY = newY;
        }

        // 边界与相机
        playerX = std::clamp(playerX, HALF_W, (float)MAP_W_PX - HALF_W);
        playerY = std::clamp(playerY, HALF_H, (float)MAP_H_PX - HALF_H);

        float targetCamX = std::clamp(playerX - ViewW() * 0.5f, 0.0f, (float)(MAP_W_PX - ViewW()));
        float targetCamY = std::clamp(playerY - ViewH() * 0.5f, 0.0f, (float)(MAP_H_PX - ViewH()));

        camX += (targetCamX - camX) * CAM_LERP;
        camY += (targetCamY - camY) * CAM_LERP;
        if (fabsf(targetCamX - camX) < CAM_SNAP) camX = targetCamX;
        if (fabsf(targetCamY - camY) < CAM_SNAP) camY = targetCamY;

        // 电量/灯光
        fuel -= DRAIN_PER_FRAME * DRAIN_MULT[lightLevel];
        if (fuel < 0.0f) fuel = 0.0f;
        targetRadius = (fuel <= 0.0f) ? R_MIN : LIGHT_LEVELS[lightLevel];
        radius += (targetRadius - radius) * RADIUS_LERP;

        // === 安全区判定 ===
        int pR, pC; WorldToRC(playerX, playerY, pR, pC);
        bool playerInSafe = (tileGrid[pR][pC] == T_SAFE);

        // 拾取
        const float pickR2 = PICK_RADIUS * PICK_RADIUS;
        // 先统计当前激活数量
        int activeBatt = 0;
        for (auto& b : batteries) if (b.alive) ++activeBatt;

        for (auto& b : batteries) {
            if (!b.alive) {
                if (BATT_ENABLE_RESPAWN) {
                    b.timer -= FRAME_SEC;
                    if (b.timer <= 0.0f && activeBatt < MAX_BATT_ACTIVE) {
                        b.alive = true;
                        b.timer = 0.0f;
                        ++activeBatt; // 记得递增
                    }
                }
            }
            else {
                if (Dist2(playerX, playerY, b.x, b.y) <= pickR2) {
                    b.alive = false;
                    if (BATT_ENABLE_RESPAWN) b.timer = BATT_RESPAWN_SEC; else b.timer = 0.0f;
                    batteryInv++;
                }
            }
        }

        for (auto& it : collectibles) {
            if (it.alive && Dist2(playerX, playerY, it.x, it.y) <= pickR2) { it.alive = false; collectedCnt++; }
        }

        // 敌人AI & 动画
        const float lightR2 = radius * radius;

        // 聚合量
        float minEnemyDist2 = 1e30f;
        bool  anyChasing = false;
        bool  anyInL2 = false;
        bool  anyInL3 = false;

        for (auto& enemy : enemies) {
            float d2 = Dist2(enemy.x, enemy.y, playerX, playerY);
            if (d2 < minEnemyDist2) minEnemyDist2 = d2;

            // 进入光圈才算“看到玩家”，但玩家在安全区时，强制不可见
            bool litFound = (!playerInSafe) && (d2 <= lightR2);

            switch (enemy.state) {
            case ES_Patrol: {
                if (litFound) {
                    int er, ec, pr, pc; WorldToRC(enemy.x, enemy.y, er, ec); WorldToRC(playerX, playerY, pr, pc);
                    std::vector<std::pair<int, int>> path;
                    if (BFSPath(er, ec, pr, pc, path)) {
                        enemy.state = ES_Chase; enemy.path = std::move(path);
                        enemy.pathIdx = (enemy.path.size() >= 2) ? 1 : 0;
                        auto tgt = enemy.path[enemy.pathIdx]; enemy.tgtR = tgt.first; enemy.tgtC = tgt.second;
                        enemy.repathTimer = CHASE_REPATH_SEC; enemy.lostTimer = 0.0f;
                    }
                }
                else {
                    float tx, ty; TileCenter(enemy.tgtR, enemy.tgtC, tx, ty);
                    if (fabsf(enemy.x - tx) <= 2.0f && fabsf(enemy.y - ty) <= 2.0f) {
                        enemy.x = tx; enemy.y = ty;
                        enemy.prevR = enemy.curR; enemy.prevC = enemy.curC;
                        enemy.curR = enemy.tgtR;  enemy.curC = enemy.tgtC;
                        ChooseNextOnRoute(enemy, rng);
                    }
                    EnemyStepToTarget(enemy);
                }
            } break;

            case ES_Chase: {
                // ★ 玩家在安全区：立即脱战返航，避免“堵门”
                if (playerInSafe) {
                    int er, ec; WorldToRC(enemy.x, enemy.y, er, ec);
                    int rr, rc;
                    if (FindNearestRouteTileById(er, ec, enemy.routeId, rr, rc)) {
                        std::vector<std::pair<int, int>> back;
                        if (BFSPath(er, ec, rr, rc, back)) {
                            enemy.state = ES_Return;
                            enemy.path = std::move(back);
                            enemy.pathIdx = (enemy.path.size() >= 2) ? 1 : 0;
                            auto tgt = enemy.path[enemy.pathIdx];
                            enemy.tgtR = tgt.first; enemy.tgtC = tgt.second;
                        }
                        else {
                            enemy.state = ES_Patrol;
                            ChooseNextOnRoute(enemy, rng);
                        }
                    }
                    else {
                        enemy.state = ES_Patrol;
                        ChooseNextOnRoute(enemy, rng);
                    }
                    enemy.lostTimer = 0.0f;
                    EnemyStepToTarget(enemy);
                    break; // 本帧不再继续追击逻辑
                }

                // 追击正常逻辑
                if (litFound) enemy.lostTimer = 0.0f; else enemy.lostTimer += FRAME_SEC;

                enemy.repathTimer -= FRAME_SEC;
                if (enemy.repathTimer <= 0.0f) {
                    int er, ec, pr, pc; WorldToRC(enemy.x, enemy.y, er, ec); WorldToRC(playerX, playerY, pr, pc);
                    std::vector<std::pair<int, int>> newPath;
                    if (BFSPath8(er, ec, pr, pc, newPath)) {
                        enemy.path = std::move(newPath);
                        enemy.pathIdx = (enemy.path.size() >= 2) ? 1 : 0;
                        auto tgt = enemy.path[enemy.pathIdx]; enemy.tgtR = tgt.first; enemy.tgtC = tgt.second;
                    }
                    enemy.repathTimer = CHASE_REPATH_SEC;
                }

                float tx, ty; TileCenter(enemy.tgtR, enemy.tgtC, tx, ty);
                if (fabsf(enemy.x - tx) <= 2.5f && fabsf(enemy.y - ty) <= 2.5f) {
                    enemy.x = tx; enemy.y = ty;
                    int cr, cc; WorldToRC(enemy.x, enemy.y, cr, cc);
                    enemy.curR = cr; enemy.curC = cc;
                    if (enemy.pathIdx + 1 < (int)enemy.path.size()) {
                        ++enemy.pathIdx; auto nxt = enemy.path[enemy.pathIdx];
                        enemy.tgtR = nxt.first; enemy.tgtC = nxt.second;
                    }
                }
                EnemyStepToTargetFree(enemy);

                if (enemy.lostTimer >= CHASE_LOST_SECONDS) {
                    int er, ec; WorldToRC(enemy.x, enemy.y, er, ec);
                    int rr, rc;
                    if (FindNearestRouteTileById(er, ec, enemy.routeId, rr, rc)) {
                        std::vector<std::pair<int, int>> back;
                        if (BFSPath(er, ec, rr, rc, back)) {
                            enemy.state = ES_Return; enemy.path = std::move(back);
                            enemy.pathIdx = (enemy.path.size() >= 2) ? 1 : 0;
                            auto tgt = enemy.path[enemy.pathIdx]; enemy.tgtR = tgt.first; enemy.tgtC = tgt.second;
                        }
                        else { enemy.state = ES_Patrol; ChooseNextOnRoute(enemy, rng); }
                    }
                    else { enemy.state = ES_Patrol; ChooseNextOnRoute(enemy, rng); }
                }
            } break;

            case ES_Return: {
                float tx, ty; TileCenter(enemy.tgtR, enemy.tgtC, tx, ty);
                if (fabsf(enemy.x - tx) <= 2.0f && fabsf(enemy.y - ty) <= 2.0f) {
                    enemy.x = tx; enemy.y = ty;
                    int cr, cc; WorldToRC(enemy.x, enemy.y, cr, cc);
                    enemy.curR = cr; enemy.curC = cc;
                    if (enemy.pathIdx + 1 < (int)enemy.path.size()) {
                        ++enemy.pathIdx; auto nxt = enemy.path[enemy.pathIdx];
                        enemy.tgtR = nxt.first; enemy.tgtC = nxt.second;
                    }
                    else {
                        enemy.prevR = -1; enemy.prevC = -1;
                        enemy.state = ES_Patrol; ChooseNextOnRoute(enemy, rng);
                    }
                }
                EnemyStepToTarget(enemy);

                // 返航途中，如再次被光照到且玩家不在安全区，立刻转追击
                if (litFound) {
                    int er, ec, pr, pc; WorldToRC(enemy.x, enemy.y, er, ec); WorldToRC(playerX, playerY, pr, pc);
                    std::vector<std::pair<int, int>> path;
                    if (BFSPath(er, ec, pr, pc, path)) {
                        enemy.state = ES_Chase; enemy.path = std::move(path);
                        enemy.pathIdx = (enemy.path.size() >= 2) ? 1 : 0;
                        auto tgt = enemy.path[enemy.pathIdx]; enemy.tgtR = tgt.first; enemy.tgtC = tgt.second;
                        enemy.repathTimer = CHASE_REPATH_SEC; enemy.lostTimer = 0.0f;
                    }
                }
            } break;
            }

            // 敌人动画
            const float MV_EPS = 0.01f;
            bool enemyMoving = (fabsf(enemy.lastVX) > MV_EPS || fabsf(enemy.lastVY) > MV_EPS);
            if (enemyMoving) {
                if (fabsf(enemy.lastVX) > fabsf(enemy.lastVY)) enemy.dir = (enemy.lastVX > 0.0f) ? ED_Right : ED_Left;
                else enemy.dir = (enemy.lastVY > 0.0f) ? ED_Front : ED_Back;
                enemy.animAccum += FRAME_SEC;
                while (enemy.animAccum >= ENEMY_ANIM_DT) {
                    enemy.animAccum -= ENEMY_ANIM_DT;
                    enemy.frame = (enemy.frame + 1) % ENEMY_FRAMES;
                }
            }
            else { enemy.frame = 0; enemy.animAccum = 0.0f; }

            // BGM聚合：安全区内不计警戒
            if (!playerInSafe) {
                if (enemy.state == ES_Chase) anyChasing = true;
                if (d2 <= LIGHT_LEVELS[1] * LIGHT_LEVELS[1]) anyInL2 = true;
                if (d2 <= LIGHT_LEVELS[2] * LIGHT_LEVELS[2]) anyInL3 = true;
            }
        }

        // === 胜负判定（抓到/收集10个）===
        bool caught = false;
        if (!playerInSafe) {
            for (auto& enemy : enemies) {
                if (AABBOverlap(playerX, playerY, PLY_COLL_W, PLY_COLL_H,
                    enemy.x, enemy.y, ENEMY_COLL_W, ENEMY_COLL_H)) {
                    caught = true; break;
                }
            }
        }
        bool win = (collectedCnt >= 10);

        if (caught || win) {
            // 直接重置（胜利/失败均如此）
            ResetGame();
        }

        // === BGM状态机 与 音量 ===
        const float rL2 = LIGHT_LEVELS[1];
        const float rL3 = LIGHT_LEVELS[2];

        static int musicStage = 0; // 0=Main, 1=Alert, 2=Danger
        bool want2 = anyInL2 || anyChasing;
        bool want1 = (!want2) && anyInL3;

        if (playerInSafe) {
            musicStage = 0; // 安全区强制主BGM
        }
        else {
            if (want2) {
                musicStage = 2;
            }
            else {
                if (musicStage == 2) {
                    if (!anyChasing && !anyInL3) musicStage = 0;
                }
                else if (musicStage == 1) {
                    if (!anyInL3) musicStage = 0;
                }
                else {
                    if (want1) musicStage = 1;
                }
            }
        }

        auto clamp01 = [](float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };

        float d = sqrtf(minEnemyDist2);
        float tgtMain = 0.f, tgtAlert = 0.f, tgtDanger = 0.f;

        if (musicStage == 2) {
            float rNear = rL2;
            float rFar = anyChasing ? DANGER_VOL_FAR_WHEN_CHASE : rL3;
            if (rFar <= rNear) rFar = rNear + 1.0f;

            float g = (rFar - d) / (rFar - rNear);
            g = clamp01(g);
            if (anyChasing && g < DANGER_VOL_MIN_WHEN_CHASE) g = DANGER_VOL_MIN_WHEN_CHASE;

            tgtDanger = DANGER_BGM_VOL * g;
        }
        else if (musicStage == 1) {
            float rNear = rL3;
            float rFar = ALERT_MAX_DIST;
            if (rFar <= rNear) rFar = rNear + 1.0f;

            float g = (rFar - d) / (rFar - rNear);
            g = clamp01(g);
            tgtAlert = ALERT_BGM_VOL * g;
        }
        else {
            tgtMain = MAIN_BGM_VOL;
        }

        volMain = ApproachLinear(volMain, tgtMain, (tgtMain > volMain) ? FADE_UP_PER_SEC : FADE_DOWN_PER_SEC);
        volAlert = ApproachLinear(volAlert, tgtAlert, (tgtAlert > volAlert) ? FADE_UP_PER_SEC : FADE_DOWN_PER_SEC);
        volDanger = ApproachLinear(volDanger, tgtDanger, (tgtDanger > volDanger) ? FADE_UP_PER_SEC : FADE_DOWN_PER_SEC);

        SetVoiceVol(voiceBgm, volMain);
        SetVoiceVol(voiceAlert, volAlert);
        SetVoiceVol(voiceDanger, volDanger);

        // 调试显示
        Novice::ScreenPrintf(20, 120, "stage=%d anyChase=%d L2=%d L3=%d d=%.1f safe=%d",
            musicStage, anyChasing ? 1 : 0, anyInL2 ? 1 : 0, anyInL3 ? 1 : 0, d, playerInSafe ? 1 : 0);
        Novice::ScreenPrintf(20, 140, "Main=%.2f Alert=%.2f Danger=%.2f", volMain, volAlert, volDanger);
        Novice::ScreenPrintf(20, 160, "Collected=%d  (Win at 10)", collectedCnt);

        // ===== 绘制 =====
        Novice::DrawSprite(
            (int)(-camX * RENDER_SCALE),
            (int)(-camY * RENDER_SCALE),
            mapTex, RENDER_SCALE, RENDER_SCALE, 0.0f, COL_WHITE);

        int pScreenX = (int)((playerX - camX) * RENDER_SCALE);
        int pScreenY = (int)((playerY - camY) * RENDER_SCALE);

        int tex = playerTex[playerDir][playerFrame];
        if (tex >= 0) {
            Novice::DrawSprite(
                pScreenX - (int)(HALF_W * RENDER_SCALE),
                pScreenY - (int)(HALF_H * RENDER_SCALE),
                tex, RENDER_SCALE, RENDER_SCALE, 0.0f, COL_WHITE);
        }
        else {
            Novice::DrawBox(
                pScreenX - (int)(HALF_W * RENDER_SCALE),
                pScreenY - (int)(HALF_H * RENDER_SCALE),
                (int)(PLAYER_W * RENDER_SCALE), (int)(PLAYER_H * RENDER_SCALE),
                0.0f, COL_PLY, kFillModeSolid);
        }

        for (auto& enemy : enemies) {
            int ex = (int)((enemy.x - camX) * RENDER_SCALE);
            int ey = (int)((enemy.y - camY) * RENDER_SCALE);
            int et = enemyTex[enemy.dir][enemy.frame];
            if (et >= 0) {
                Novice::DrawSprite(
                    ex - (int)(HALF_W * RENDER_SCALE),
                    ey - (int)(HALF_H * RENDER_SCALE),
                    et, RENDER_SCALE, RENDER_SCALE, 0.0f, COL_WHITE);
            }
            else {
                Novice::DrawBox(
                    ex - (int)(HALF_W * RENDER_SCALE),
                    ey - (int)(HALF_H * RENDER_SCALE),
                    (int)(ENEMY_W * RENDER_SCALE), (int)(ENEMY_H * RENDER_SCALE),
                    0.0f, enemyColor(enemy.routeId), kFillModeSolid);
            }
        }

        auto drawDot = [&](float wx, float wy, unsigned int col) {
            int sx = (int)((wx - camX) * RENDER_SCALE);
            int sy = (int)((wy - camY) * RENDER_SCALE);
            Novice::DrawBox(
                sx - (int)(DOT_SZ * 0.5f * RENDER_SCALE),
                sy - (int)(DOT_SZ * 0.5f * RENDER_SCALE),
                (int)(DOT_SZ * RENDER_SCALE), (int)(DOT_SZ * RENDER_SCALE),
                0.0f, col, kFillModeSolid);
            };

        for (auto& b : batteries)    if (b.alive)  drawDot(b.x, b.y, COL_BATT);
        for (auto& it : collectibles) if (it.alive) drawDot(it.x, it.y, COL_ITEM);

        DrawDarkCircleHole(
            pScreenX, pScreenY,
            (int)(radius * RENDER_SCALE + 0.5f),
            COL_BLACK);

        if (showMiniMap) {
            Novice::DrawBox(miniX - 4, miniY - 4, MINI_W + 8, MINI_H + 8, 0.0f, COL_MM_BG, kFillModeSolid);
            Novice::DrawSprite(miniX, miniY, mapTex, miniSX, miniSY, 0.0f, 0xFFFFFFB0);
            int mpX = (int)(miniX + playerX * miniSX);
            int mpY = (int)(miniY + playerY * miniSY);
            Novice::DrawBox(mpX - 3, mpY - 3, 6, 6, 0.0f, COL_MM_P, kFillModeSolid);
        }

        Novice::ScreenPrintf(20, 20, "Fuel: %3d / 100", (int)(fuel + 0.5f));
        Novice::ScreenPrintf(20, 40, "Batteries: %d", batteryInv);
        Novice::ScreenPrintf(20, 60, "Collected: %d", collectedCnt);
        Novice::ScreenPrintf(20, 80, "[Space] Light/Recharge");
        Novice::ScreenPrintf(20, 100, "Light: L%d  (radius %.0f)", (fuel <= 0.0f ? 1 : lightLevel + 1), radius);

        Novice::EndFrame();
        if (!preKeys[DIK_ESCAPE] && keys[DIK_ESCAPE]) break;
    }

    Novice::Finalize();
    return 0;
}
#pragma endregion
