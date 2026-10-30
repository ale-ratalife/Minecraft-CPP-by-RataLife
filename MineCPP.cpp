#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include "World.h"
#include "Atlas.h"
#include "shaders_gl2.h"
#include "shaders_src.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

GL2 gl2;

static const double PI = 3.14159265358979323846;
static const double DEG = PI / 180.0;
static inline int ifl(double v) { return (int)std::floor(v); }
static inline float frand() { return (float)std::rand() / (float)RAND_MAX; }

static HWND    g_hwnd = nullptr;
static HDC     g_hdc = nullptr;
static HGLRC   g_rc = nullptr;
static HCURSOR g_arrow = nullptr;
static int     g_w = 1280, g_h = 720;
static bool    g_running = true, g_active = true, g_fullscreen = false;
static bool    g_captured = false, g_skipMouse = false, g_debug = false;

struct Events
{
    bool esc = false, inv = false, f3 = false, f11 = false;
    bool rdUp = false, rdDown = false;
    bool lclick = false, rclick = false, mclick = false, lostFocus = false;
    bool backspace = false, enter = false, paste = false, copy = false;
    int  hotkey = -1, wheel = 0;
    std::string chars;
} g_ev;

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_CLOSE:   g_running = false; return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_SIZE:    g_w = LOWORD(l); g_h = HIWORD(l); return 0;
    case WM_ACTIVATE:
        g_active = (LOWORD(w) != WA_INACTIVE);
        if (!g_active) g_ev.lostFocus = true;
        return 0;
    case WM_CHAR:
        if (w == 8) g_ev.backspace = true;
        else if (w == 13) g_ev.enter = true;
        else if (w == 22) g_ev.paste = true;
        else if (w == 3) g_ev.copy = true;
        else if (w >= 32 && w < 127) g_ev.chars += (char)w;
        return 0;
    case WM_KEYDOWN:
        if (l & (1 << 30)) return 0;
        if (w == VK_ESCAPE) g_ev.esc = true;
        else if (w == 'E') g_ev.inv = true;
        else if (w == VK_F3) g_ev.f3 = true;
        else if (w == VK_F11) g_ev.f11 = true;
        else if (w == VK_OEM_PLUS || w == VK_ADD) g_ev.rdUp = true;
        else if (w == VK_OEM_MINUS || w == VK_SUBTRACT) g_ev.rdDown = true;
        else if (w >= '1' && w <= '9') g_ev.hotkey = (int)(w - '1');
        return 0;
    case WM_LBUTTONDOWN: g_ev.lclick = true; return 0;
    case WM_RBUTTONDOWN: g_ev.rclick = true; return 0;
    case WM_MBUTTONDOWN: g_ev.mclick = true; return 0;
    case WM_MOUSEWHEEL:  g_ev.wheel += GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA; return 0;
    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT) { SetCursor(g_captured ? nullptr : g_arrow); return TRUE; }
        break;
    }
    return DefWindowProcA(h, m, w, l);
}

static double nowSec()
{
    static LARGE_INTEGER f = {};
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

static bool key(int vk) { return g_active && (GetAsyncKeyState(vk) & 0x8000) != 0; }

static void toggleFullscreen()
{
    static WINDOWPLACEMENT prev = {};
    prev.length = sizeof(prev);
    LONG style = GetWindowLongA(g_hwnd, GWL_STYLE);
    if (!g_fullscreen)
    {
        MONITORINFO mi = {}; mi.cbSize = sizeof(mi);
        GetWindowPlacement(g_hwnd, &prev);
        GetMonitorInfoA(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
        SetWindowLongA(g_hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    else
    {
        SetWindowLongA(g_hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(g_hwnd, &prev);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    g_fullscreen = !g_fullscreen;
}

static std::string clipGet()
{
    std::string r;
    if (OpenClipboard(g_hwnd))
    {
        HANDLE h = GetClipboardData(CF_TEXT);
        if (h)
        {
            const char* p = (const char*)GlobalLock(h);
            if (p) { r = p; GlobalUnlock(h); }
        }
        CloseClipboard();
    }
    return r;
}

static void clipSet(const std::string& s)
{
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
    if (h)
    {
        char* p = (char*)GlobalLock(h);
        std::memcpy(p, s.c_str(), s.size() + 1);
        GlobalUnlock(h);
        SetClipboardData(CF_TEXT, h);
    }
    CloseClipboard();
}

static uint32_t randomSeed()
{
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    uint64_t v = (uint64_t)c.QuadPart * 6364136223846793005ULL + (uint64_t)std::time(nullptr) * 1442695040888963407ULL + (uint64_t)GetTickCount64();
    v ^= v >> 29; v *= 0xBF58476D1CE4E5B9ULL; v ^= v >> 32;
    return (uint32_t)v;
}

static uint32_t seedFromText(const std::string& raw)
{
    size_t a = 0, b = raw.size();
    while (a < b && raw[a] == ' ') a++;
    while (b > a && raw[b - 1] == ' ') b--;
    std::string t = raw.substr(a, b - a);
    if (t.empty()) return randomSeed();
    size_t i = (t[0] == '-' || t[0] == '+') ? 1 : 0;
    bool num = i < t.size() && (t.size() - i) <= 18;
    for (size_t k = i; k < t.size() && num; k++)
        if (t[k] < '0' || t[k] > '9') num = false;
    if (num)
    {
        long long v = std::strtoll(t.c_str(), nullptr, 10);
        uint64_t u = (uint64_t)v;
        return (uint32_t)(u ^ (u >> 32));
    }
    uint32_t h = 0;
    for (char ch : t) h = h * 31u + (uint8_t)ch;
    return h;
}

enum class State { MainMenu, CreateWorld, Loading, Playing, Paused, Inventory, Dead };
enum class Mode { Survival, Creative };

static State g_state = State::MainMenu;
static Mode  g_mode = Mode::Survival;
static World* g_world = nullptr;
static GLuint g_atlas = 0, g_fontBase = 0, g_fontBig = 0, g_minimapTex = 0;
static HFONT  g_font = nullptr, g_fontB = nullptr;
static int    g_rd = 8;
static float  g_s = 1.0f;
static std::string g_seedText;
static uint32_t g_seed = 0;
static double g_spawn[3] = { 0, 80, 0 };
static int    g_spawnX = 0, g_spawnZ = 0;
static float  g_loadFrac = 0.0f;
static double g_loadT = 0.0;
static bool   g_underWater = false, g_inLava = false;
static int    g_mouseX = 0, g_mouseY = 0;
static uint8_t* g_atlasPx = nullptr;
static const int MM = 64;
static uint8_t g_minimapPx[MM * MM * 4];
static double g_minimapT = 0.0;
static int g_underground = 0;
static double g_atlasT = 0.0;
static float  g_curX = 0.0f, g_curZ = 0.0f;
static double g_curT = 0.0;
static bool   g_wasInWater = false;
static float  g_bubT = 0.0f;

static ShaderProgram g_worldProg;
static ShaderProgram g_waterProg;
static bool          g_shadersOK = false;

static bool initShaders()
{
    bool ok = g_worldProg.build(WORLD_VS, WORLD_FS)
        && g_waterProg.build(WATER_VS, WATER_FS);
    if (!ok)
    {
        g_worldProg.destroy();
        g_waterProg.destroy();
    }
    g_shadersOK = ok;
    return ok;
}

struct Player
{
    double x = 0, y = 80, z = 0, vx = 0, vy = 0, vz = 0;
    float  yaw = 0, pitch = 0;
    bool   onGround = false, fly = false, sneak = false, sprint = false;
    float  health = 20, air = 15, hurt = 0, fall = 0, lavaT = 0.5f, drownT = 0, regenT = 0;
    float  walk = 0, bob = 0, eye = 1.62f, fov = 70.0f;
} P;

static const double PW = 0.3, PH = 1.8;
static bool   g_wasGround = false, g_spaceDown = false;
static double g_lastTap = 0;

struct Stack { uint8_t id = 0; int n = 0; };
static Stack g_inv[36];
static Stack g_cursor;
static int   g_sel = 0;
static float g_nameT = 0.0f, g_equip = 1.0f;

struct Part { float x, y, z, vx, vy, vz, life, size, u, v, us, shade, grav = 20.0f; };
struct Drop { double x, y, z, vx, vy, vz; uint8_t id; int n; double age; };
struct Fall { int x, z; double y, vy; uint8_t id; };
static std::vector<Part> g_parts;
static std::vector<Drop> g_drops;
static std::vector<Fall> g_falls;

struct Hit { bool hit = false; int x = 0, y = 0, z = 0, px = 0, py = 0, pz = 0; };
static Hit    g_hit;
static double g_cool = 0;
static float  g_swing = -1.0f;
static bool   g_mining = false;
static int    g_bx = 0, g_by = 0, g_bz = 0;
static float  g_prog = 0.0f, g_partT = 0.0f;
static int    g_drawn = 0;

static void setState(State s);
static void startWorld(uint32_t sd, Mode md);
static void goMenu();
static void respawn();
static void die();

static int addItem(uint8_t id, int n)
{
    if (!id) return 0;
    for (int pass = 0; pass < 2 && n > 0; pass++)
        for (int i = 0; i < 36 && n > 0; i++)
        {
            Stack& s = g_inv[i];
            if (pass == 0 && s.id == id && s.n < 64) { int a = std::min(n, 64 - s.n); s.n += a; n -= a; }
            else if (pass == 1 && s.id == 0) { int a = std::min(n, 64); s.id = id; s.n = a; n -= a; }
        }
    return n;
}

static int countItem(uint8_t id)
{
    int c = 0;
    for (int i = 0; i < 36; i++) if (g_inv[i].id == id) c += g_inv[i].n;
    return c;
}

static bool removeItem(uint8_t id, int n)
{
    if (countItem(id) < n) return false;
    for (int i = 35; i >= 0 && n > 0; i--)
    {
        Stack& s = g_inv[i];
        if (s.id != id) continue;
        int a = std::min(n, s.n);
        s.n -= a; n -= a;
        if (s.n == 0) s = Stack();
    }
    return true;
}

static void spawnDrop(double x, double y, double z, uint8_t id, int n, bool toss = false)
{
    if (!id || n <= 0) return;
    Drop d;
    d.x = x; d.y = y; d.z = z;
    double sp = toss ? 3.5 : 1.2;
    d.vx = (frand() - 0.5) * sp; d.vz = (frand() - 0.5) * sp; d.vy = toss ? 4.0 : 2.5;
    d.id = id; d.n = n; d.age = toss ? -0.8 : 0.0;
    g_drops.push_back(d);
}

static void spawnParticles(int bx, int by, int bz, uint8_t b, int count)
{
    if (g_parts.size() > 3000) return;
    const float ts = 1.0f / ATLAS_N, sub = ts * 0.25f;
    for (int i = 0; i < count; i++)
    {
        Part p;
        p.x = bx + frand(); p.y = by + frand(); p.z = bz + frand();
        p.vx = (frand() - 0.5f) * 4.0f; p.vy = 1.0f + frand() * 3.0f; p.vz = (frand() - 0.5f) * 4.0f;
        int tile = blockTile(b, (std::rand() % 3 == 0) ? 2 : 4);
        p.u = (tile % ATLAS_N) * ts + (std::rand() % 4) * sub;
        p.v = (tile / ATLAS_N) * ts + (std::rand() % 4) * sub;
        p.us = sub;
        p.life = 0.5f + frand() * 0.8f;
        p.size = 0.05f + frand() * 0.05f;
        p.shade = 0.6f + frand() * 0.4f;
        g_parts.push_back(p);
    }
}

static void spawnTilePart(float x, float y, float z, float vx, float vy, float vz, int tile, float life, float size, float shade, float grav)
{
    const float ts = 1.0f / ATLAS_N, sub = ts * 0.25f;
    Part p;
    p.x = x; p.y = y; p.z = z;
    p.vx = vx; p.vy = vy; p.vz = vz;
    p.u = (tile % ATLAS_N) * ts + (std::rand() % 4) * sub;
    p.v = (tile / ATLAS_N) * ts + (std::rand() % 4) * sub;
    p.us = sub;
    p.life = life; p.size = size; p.shade = shade; p.grav = grav;
    g_parts.push_back(p);
}

static void spawnFaceParticles(int bx, int by, int bz, int nx, int ny, int nz, uint8_t b, int count)
{
    if (g_parts.size() > 3000) return;
    const int f = nx > 0 ? 0 : (nx < 0 ? 1 : (ny > 0 ? 2 : (ny < 0 ? 3 : (nz > 0 ? 4 : 5))));
    const int tile = blockTile(b, f);
    for (int i = 0; i < count; i++)
    {
        float px = bx + 0.5f + nx * 0.52f + (nx ? 0.0f : (frand() - 0.5f) * 0.9f);
        float py = by + 0.5f + ny * 0.52f + (ny ? 0.0f : (frand() - 0.5f) * 0.9f);
        float pz = bz + 0.5f + nz * 0.52f + (nz ? 0.0f : (frand() - 0.5f) * 0.9f);
        spawnTilePart(px, py, pz, nx * 1.5f + (frand() - 0.5f) * 1.6f, 0.5f + frand() * 1.5f, nz * 1.5f + (frand() - 0.5f) * 1.6f,
            tile, 0.4f + frand() * 0.4f, 0.04f + frand() * 0.04f, 0.6f + frand() * 0.4f, 20.0f);
    }
}

static void spawnSplash(double x, double y, double z, int count)
{
    if (g_parts.size() > 3000) return;
    for (int i = 0; i < count; i++)
        spawnTilePart((float)x + (frand() - 0.5f) * 0.6f, (float)y, (float)z + (frand() - 0.5f) * 0.6f,
            (frand() - 0.5f) * 3.0f, 2.0f + frand() * 3.0f, (frand() - 0.5f) * 3.0f,
            TL_WATER_STILL, 0.5f + frand() * 0.4f, 0.04f + frand() * 0.04f, 0.9f, 20.0f);
}

static void spawnBubble(double x, double y, double z)
{
    if (g_parts.size() > 3000) return;
    spawnTilePart((float)x + (frand() - 0.5f) * 0.5f, (float)y, (float)z + (frand() - 0.5f) * 0.5f,
        (frand() - 0.5f) * 0.3f, 0.8f, (frand() - 0.5f) * 0.3f, TL_SNOW, 1.2f, 0.03f + frand() * 0.02f, 0.95f, -2.0f);
}

static void onFallCb(int x, int y, int z, uint8_t b)
{
    Fall f; f.x = x; f.z = z; f.y = (double)y; f.vy = 0.0; f.id = b;
    g_falls.push_back(f);
}

static void setCapture(bool c)
{
    g_captured = c;
    if (c)
    {
        RECT rc; GetClientRect(g_hwnd, &rc);
        POINT p = { rc.right / 2, rc.bottom / 2 }; ClientToScreen(g_hwnd, &p);
        SetCursorPos(p.x, p.y);
        g_skipMouse = true;
        SetCursor(nullptr);
    }
    else SetCursor(g_arrow);
}

static void setState(State s)
{
    if (g_state == State::Inventory && s != State::Inventory && g_cursor.id)
    {
        if (g_mode == Mode::Survival)
        {
            int left = addItem(g_cursor.id, g_cursor.n);
            if (left > 0) spawnDrop(P.x, P.y + 1.4, P.z, g_cursor.id, left, true);
        }
        g_cursor = Stack();
    }
    g_state = s;
    if (s == State::Playing) g_cool = 0.3;
    setCapture(s == State::Playing);
}

static void startWorld(uint32_t sd, Mode md)
{
    g_mode = md;
    g_seed = sd;
    if (g_world) delete g_world;
    g_world = new World(sd);
    g_world->onFall = onFallCb;
    g_parts.clear();
    g_drops.clear();
    g_falls.clear();
    g_underWater = false;
    g_inLava = false;
    g_wasInWater = false;
    g_cursor = Stack();
    for (int i = 0; i < 36; i++) g_inv[i] = Stack();
    if (md == Mode::Creative)
    {
        uint8_t cr[9] = { STONE, DIRT, GRASS, COBBLE, PLANKS, GLASS, WOOD, LEAVES, SAND };
        for (int i = 0; i < 9; i++) { g_inv[i].id = cr[i]; g_inv[i].n = 64; }
    }
    P = Player();
    int sx = 0, sz = 0;
    bool found = false;
    for (int r = 0; r < 96 && !found; r++)
        for (int dz = -r; dz <= r && !found; dz++)
            for (int dx = -r; dx <= r && !found; dx++)
            {
                if (dx * dx + dz * dz > r * r) continue;
                int h = g_world->heightAt(dx, dz);
                if (h > SEA_LEVEL + 2) { sx = dx; sz = dz; found = true; }
            }
    if (!found) { sx = 0; sz = 0; }
    int h = g_world->heightAt(sx, sz);
    P.x = sx + 0.5;
    P.z = sz + 0.5;
    P.y = h + 3;
    P.yaw = 0;
    P.pitch = 0;
    g_spawn[0] = P.x;
    g_spawn[1] = P.y;
    g_spawn[2] = P.z;
    g_spawnX = sx;
    g_spawnZ = sz;
    g_sel = 0;
    g_nameT = 0;
    g_loadFrac = 0;
    g_loadT = 0;
    g_minimapT = 0;
    setState(State::Loading);
}

static void goMenu()
{
    if (g_world) { delete g_world; g_world = nullptr; }
    g_underWater = false;
    g_inLava = false;
    g_parts.clear();
    g_drops.clear();
    g_falls.clear();
    g_cursor = Stack();
    setState(State::MainMenu);
}

static void respawn()
{
    P.x = g_spawn[0];
    P.y = g_spawn[1];
    P.z = g_spawn[2];
    P.vx = P.vy = P.vz = 0;
    P.health = 20;
    P.air = 15;
    P.hurt = 0;
    P.fall = 0;
    P.fly = false;
    setState(State::Playing);
}

static bool solidAt(int x, int y, int z)
{
    if (y < 0) return true;
    if (y >= CHUNK_H) return false;
    if (!g_world || !g_world->isLoaded(x, z)) return true;
    return isSolid(g_world->getBlock(x, y, z));
}

static bool collides(double x, double y, double z)
{
    int x0 = ifl(x - PW), x1 = ifl(x + PW), y0 = ifl(y), y1 = ifl(y + PH), z0 = ifl(z - PW), z1 = ifl(z + PW);
    for (int yy = y0; yy <= y1; yy++)
        for (int zz = z0; zz <= z1; zz++)
            for (int xx = x0; xx <= x1; xx++)
                if (solidAt(xx, yy, zz)) return true;
    return false;
}

static void moveAxis(int axis, double d)
{
    if (d == 0) return;
    int steps = (int)std::ceil(std::fabs(d) / 0.4);
    double sd = d / steps;
    double* p[3] = { &P.x, &P.y, &P.z };
    for (int i = 0; i < steps; i++)
    {
        *p[axis] += sd;
        if (axis != 1 && P.sneak && g_wasGround && !collides(P.x, P.y - 0.6, P.z))
        {
            *p[axis] -= sd;
            (axis == 0 ? P.vx : P.vz) = 0;
            return;
        }
        if (!collides(P.x, P.y, P.z)) continue;
        if (axis == 1)
        {
            if (sd < 0) { P.y = std::floor(P.y) + 1.0 + 1e-4; P.onGround = true; }
            else { P.y = std::floor(P.y + PH) - PH - 1e-4; }
            P.vy = 0;
        }
        else
        {
            double* c = p[axis];
            if (sd > 0) *c = std::floor(*c + PW) - PW - 1e-4;
            else        *c = std::floor(*c - PW) + 1.0 + PW + 1e-4;
            (axis == 0 ? P.vx : P.vz) = 0;
        }
        return;
    }
}

static bool fluidAtPoint(double x, double y, double z, uint8_t t)
{
    int bx = ifl(x), by = ifl(y), bz = ifl(z);
    if (!g_world) return false;
    if (g_world->getBlock(bx, by, bz) != t) return false;
    if (g_world->getBlock(bx, by + 1, bz) == t) return true;
    uint8_t m = g_world->getMeta(bx, by, bz);
    float h = (m & FLUID_FALL) ? 1.0f : 0.89f - (float)(m & 7) * (t == LAVA ? 0.22f : 0.11f);
    return (float)(y - by) < h;
}

static void damage(float a)
{
    if (g_mode != Mode::Survival || P.health <= 0) return;
    P.health -= a;
    P.hurt = 0.4f;
    if (P.health <= 0) { P.health = 0; die(); }
}

static void die()
{
    for (int i = 0; i < 36; i++)
        if (g_inv[i].id) { spawnDrop(P.x, P.y + 1.0, P.z, g_inv[i].id, g_inv[i].n, true); g_inv[i] = Stack(); }
    if (g_cursor.id) { spawnDrop(P.x, P.y + 1.0, P.z, g_cursor.id, g_cursor.n, true); g_cursor = Stack(); }
    setState(State::Dead);
}

static void flowPush(double dt, uint8_t t)
{
    if (!g_world) return;
    const int bx = ifl(P.x), by = ifl(P.y + 0.3), bz = ifl(P.z);
    if (g_world->getBlock(bx, by, bz) != t) return;
    float fx, fz;
    g_world->flowVector(bx, by, bz, fx, fz);
    const double a = (t == WATER ? 5.0 : 1.6) * dt;
    P.vx += fx * a;
    P.vz += fz * a;
}

static void riverPush(double dt)
{
    if (!g_world) return;
    const int bx = ifl(P.x), by = ifl(P.y + 0.3), bz = ifl(P.z);
    if (g_world->getBlock(bx, by, bz) != WATER || g_world->getMeta(bx, by, bz) != 0) return;
    const double now = nowSec();
    if (now - g_curT > 0.25)
    {
        g_curT = now;
        if (!g_world->riverCurrent(bx, bz, g_curX, g_curZ)) { g_curX = 0.0f; g_curZ = 0.0f; }
    }
    const double a = 1.6 * (P.onGround ? 3.0 : 1.0) * dt;
    P.vx += g_curX * a;
    P.vz += g_curZ * a;
}

static void updatePlayer(double dt, bool input)
{
    const double sy = std::sin(P.yaw * DEG), cy = std::cos(P.yaw * DEG);
    bool kw = input && key('W'), ks = input && key('S'), ka = input && key('A'), kd = input && key('D');
    bool space = input && key(VK_SPACE), shift = input && key(VK_SHIFT), ctrl = input && key(VK_CONTROL);

    double now = nowSec();
    if (space && !g_spaceDown)
    {
        if (g_mode == Mode::Creative && now - g_lastTap < 0.3) { P.fly = !P.fly; P.vy = 0; g_lastTap = 0; }
        else g_lastTap = now;
    }
    g_spaceDown = space;
    if (g_mode == Mode::Survival) P.fly = false;

    P.sneak = shift && !P.fly;
    double f = (kw ? 1 : 0) - (ks ? 1 : 0);
    double s = (kd ? 1 : 0) - (ka ? 1 : 0);
    double mx = sy * f + cy * s, mz = -cy * f + sy * s;
    double len = std::sqrt(mx * mx + mz * mz);
    if (len > 0) { mx /= len; mz /= len; }
    P.sprint = ctrl && kw && !P.sneak;

    bool feetW = fluidAtPoint(P.x, P.y + 0.2, P.z, WATER);
    bool headW = fluidAtPoint(P.x, P.y + P.eye, P.z, WATER);
    bool inLava = fluidAtPoint(P.x, P.y + 0.2, P.z, LAVA) || fluidAtPoint(P.x, P.y + 1.0, P.z, LAVA);
    bool inFl = feetW || inLava;

    if (feetW && !g_wasInWater && !P.fly && P.vy < -3.0) spawnSplash(P.x, P.y + 0.5, P.z, (int)std::min(24.0, -P.vy * 2.0));
    g_wasInWater = feetW;
    if (headW)
    {
        g_bubT -= (float)dt;
        if (g_bubT <= 0.0f)
        {
            g_bubT = 0.45f + frand() * 0.4f;
            spawnBubble(P.x, P.y + P.eye - 0.2, P.z);
            spawnBubble(P.x, P.y + P.eye - 0.2, P.z);
        }
    }
    else g_bubT = 0.0f;

    double speed;
    if (P.fly) speed = P.sprint ? 22.0 : 11.0;
    else if (P.sneak) speed = 1.4;
    else speed = P.sprint ? 5.8 : 4.3;
    if (inFl && !P.fly) speed *= inLava ? 0.35 : 0.6;
    double k = std::min(1.0, (P.fly ? 8.0 : (P.onGround ? 14.0 : 3.0)) * dt);
    P.vx += (mx * speed - P.vx) * k;
    P.vz += (mz * speed - P.vz) * k;

    if (P.fly)
    {
        double tv = (space ? speed : 0.0) - (shift ? speed : 0.0);
        P.vy += (tv - P.vy) * std::min(1.0, 10.0 * dt);
    }
    else if (inFl)
    {
        P.vy -= 9.0 * dt;
        P.vy *= (1.0 - std::min(1.0, (inLava ? 5.0 : 2.5) * dt));
        if (space) { if (headW) P.vy = std::min(P.vy + 30.0 * dt, 4.0); else P.vy = inLava ? 4.0 : 8.4; }
        if (P.vy < -4) P.vy = -4;
        if (feetW) { flowPush(dt, WATER); riverPush(dt); }
        else if (inLava) flowPush(dt, LAVA);
    }
    else
    {
        P.vy -= 28.0 * dt;
        if (P.vy < -50) P.vy = -50;
        if (space && P.onGround) P.vy = 8.6;
    }

    g_wasGround = P.onGround;
    double y0 = P.y;
    P.onGround = false;
    moveAxis(0, P.vx * dt);
    moveAxis(2, P.vz * dt);
    moveAxis(1, P.vy * dt);

    double dyMoved = y0 - P.y;
    if (P.onGround)
    {
        if (P.fall > 3.0f && !inFl) damage(std::ceil(P.fall - 3.0f));
        P.fall = 0;
        if (P.fly) P.fly = false;
    }
    else if (dyMoved > 0 && !inFl && !P.fly) P.fall += (float)dyMoved;
    if (inFl || P.fly) P.fall = 0;
    if (P.y < -20) damage(100.0f);

    float sp = (float)std::sqrt(P.vx * P.vx + P.vz * P.vz);
    if (P.onGround && !P.fly) P.walk += (float)(sp * dt * 0.6);
    float tb = (P.onGround && !P.fly) ? std::min(0.1f, sp * 0.0233f) : 0.0f;
    P.bob += (tb - P.bob) * std::min(1.0f, (float)(12.0 * dt));
    float te = P.sneak ? 1.27f : 1.62f;
    P.eye += (te - P.eye) * std::min(1.0f, (float)(14.0 * dt));
    float tf = 70.0f + ((P.sprint && sp > 2.0f) ? 9.0f : 0.0f);
    if (headW) tf *= 60.0f / 70.0f;
    P.fov += (tf - P.fov) * std::min(1.0f, (float)(10.0 * dt));

    g_underWater = headW;
    g_inLava = inLava;

    if (g_mode == Mode::Survival)
    {
        if (headW)
        {
            P.air = std::max(0.0f, P.air - (float)dt);
            if (P.air <= 0.0f)
            {
                P.drownT += (float)dt;
                if (P.drownT >= 1.0f) { P.drownT = 0; damage(2.0f); }
            }
        }
        else { P.air = std::min(15.0f, P.air + (float)(dt * 4.0)); P.drownT = 0; }

        if (inLava)
        {
            P.lavaT += (float)dt;
            if (P.lavaT >= 0.5f) { P.lavaT = 0; damage(4.0f); }
        }
        else P.lavaT = 0.5f;

        if (P.health > 0 && P.health < 20)
        {
            P.regenT += (float)dt;
            if (P.regenT >= 3.0f) { P.regenT = 0; P.health = std::min(20.0f, P.health + 1.0f); }
        }
        else P.regenT = 0;
    }
    else { P.health = 20; P.air = 15; P.hurt = 0; }
}

static void updateFalls(double dt)
{
    for (size_t i = 0; i < g_falls.size();)
    {
        Fall& f = g_falls[i];
        f.vy -= 26.0 * dt;
        if (f.vy < -40) f.vy = -40;
        double dy = f.vy * dt;
        int steps = std::max(1, (int)std::ceil(std::fabs(dy) / 0.4));
        double sd = dy / steps;
        bool landed = false;
        int hitCell = 0;
        for (int k = 0; k < steps && !landed; k++)
        {
            double ny = f.y + sd;
            int cell = ifl(ny);
            if (cell < 1 || isSolid(g_world->getBlock(f.x, cell, f.z))) { landed = true; hitCell = std::max(cell, 0); }
            else f.y = ny;
        }
        if (!landed) { i++; continue; }
        int ly = hitCell + 1;
        uint8_t cur = g_world->getBlock(f.x, ly, f.z);
        if (cur == AIR || isFluid(cur)) { g_world->setBlock(f.x, ly, f.z, f.id); spawnParticles(f.x, ly, f.z, f.id, 6); }
        else spawnDrop(f.x + 0.5, ly + 0.2, f.z + 0.5, f.id, 1);
        g_falls.erase(g_falls.begin() + i);
    }
}

static void updateDrops(double dt)
{
    for (size_t i = 0; i < g_drops.size();)
    {
        Drop& d = g_drops[i];
        d.age += dt;
        uint8_t inb = g_world->getBlock(ifl(d.x), ifl(d.y + 0.1), ifl(d.z));
        bool remove = (inb == LAVA) || d.age > 300.0;

        double px = P.x, py = P.y + 0.9, pz = P.z;
        double dx = px - d.x, dy = py - d.y, dz = pz - d.z;
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        bool pulling = !remove && d.age > 0.35 && dist < 3.0 && g_state != State::Dead;

        if (pulling)
        {
            if (dist < 0.9)
            {
                int left = addItem(d.id, d.n);
                if (left == 0) remove = true; else d.n = left;
            }
            if (!remove && dist > 0.001 && dist >= 0.9)
            {
                d.x += dx / dist * 7.0 * dt; d.y += dy / dist * 7.0 * dt; d.z += dz / dist * 7.0 * dt;
                d.vy = 0;
            }
        }

        if (!remove && !pulling)
        {
            if (inb == WATER)
            {
                const int dbx = ifl(d.x), dby = ifl(d.y + 0.1), dbz = ifl(d.z);
                const bool above = g_world->getBlock(dbx, dby + 1, dbz) == WATER;
                const double fy = (d.y + 0.1) - dby;
                d.vy += ((above || fy < 0.7) ? 4.0 : -2.0) * dt;
                d.vy *= (1.0 - std::min(1.0, 3.0 * dt));
                float ffx, ffz;
                g_world->flowVector(dbx, dby, dbz, ffx, ffz);
                d.vx += ffx * 3.0 * dt;
                d.vz += ffz * 3.0 * dt;
                d.vx *= (1.0 - std::min(1.0, 1.5 * dt));
                d.vz *= (1.0 - std::min(1.0, 1.5 * dt));
            }
            else d.vy -= 22.0 * dt;
            double nx = d.x + d.vx * dt;
            if (solidAt(ifl(nx), ifl(d.y + 0.1), ifl(d.z))) d.vx = -d.vx * 0.3; else d.x = nx;
            double nz = d.z + d.vz * dt;
            if (solidAt(ifl(d.x), ifl(d.y + 0.1), ifl(nz))) d.vz = -d.vz * 0.3; else d.z = nz;
            double ny = d.y + d.vy * dt;
            if (solidAt(ifl(d.x), ifl(ny), ifl(d.z)))
            {
                if (d.vy < 0) d.y = ifl(ny) + 1.0;
                d.vy = 0;
                double fr = std::max(0.0, 1.0 - 8.0 * dt);
                d.vx *= fr; d.vz *= fr;
            }
            else d.y = ny;
            if (solidAt(ifl(d.x), ifl(d.y + 0.05), ifl(d.z))) d.y += dt * 3.0;
        }

        if (remove) g_drops.erase(g_drops.begin() + i);
        else i++;
    }
}

static void updateParts(double dt)
{
    for (size_t i = 0; i < g_parts.size();)
    {
        Part& p = g_parts[i];
        p.life -= (float)dt;
        if (p.life <= 0) { g_parts[i] = g_parts.back(); g_parts.pop_back(); continue; }
        if (p.grav < 0.0f && g_world->getBlock(ifl(p.x), ifl(p.y), ifl(p.z)) != WATER) { g_parts[i] = g_parts.back(); g_parts.pop_back(); continue; }
        p.vy -= p.grav * (float)dt;
        float nx = p.x + p.vx * (float)dt, ny = p.y + p.vy * (float)dt, nz = p.z + p.vz * (float)dt;
        if (solidAt(ifl(nx), ifl(p.y), ifl(p.z))) p.vx = 0; else p.x = nx;
        if (solidAt(ifl(p.x), ifl(p.y), ifl(nz))) p.vz = 0; else p.z = nz;
        if (solidAt(ifl(p.x), ifl(ny), ifl(p.z)))
        {
            p.vy = 0;
            float fr = std::max(0.0f, 1.0f - 10.0f * (float)dt);
            p.vx *= fr; p.vz *= fr;
        }
        else p.y = ny;
        i++;
    }
}

static void updateGame(double dt)
{
    if (g_world) g_world->tick(dt);
    updateFalls(dt);
    updateDrops(dt);
    updateParts(dt);
    if (g_swing >= 0.0f) { g_swing += (float)(dt * 3.6); if (g_swing >= 1.0f) g_swing = g_mining ? 0.0f : -1.0f; }
    g_equip = std::min(1.0f, g_equip + (float)(dt * 5.0));
    P.hurt = std::max(0.0f, P.hurt - (float)dt);
    g_nameT = std::max(0.0f, g_nameT - (float)dt);
    g_cool = std::max(0.0, g_cool - dt);
}

static Hit raycast(double ox, double oy, double oz, double dx, double dy, double dz, double maxd)
{
    Hit r;
    int x = ifl(ox), y = ifl(oy), z = ifl(oz);
    int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1, sz = dz > 0 ? 1 : -1;
    const double INF = 1e30;
    double tdx = dx != 0 ? std::fabs(1.0 / dx) : INF, tdy = dy != 0 ? std::fabs(1.0 / dy) : INF, tdz = dz != 0 ? std::fabs(1.0 / dz) : INF;
    double tmx = dx > 0 ? (x + 1 - ox) / dx : (dx < 0 ? (ox - x) / -dx : INF);
    double tmy = dy > 0 ? (y + 1 - oy) / dy : (dy < 0 ? (oy - y) / -dy : INF);
    double tmz = dz > 0 ? (z + 1 - oz) / dz : (dz < 0 ? (oz - z) / -dz : INF);
    int px = x, py = y, pz = z;
    double t = 0;
    while (t <= maxd)
    {
        uint8_t b = g_world->getBlock(x, y, z);
        if (isSolid(b)) { r.hit = true; r.x = x; r.y = y; r.z = z; r.px = px; r.py = py; r.pz = pz; return r; }
        px = x; py = y; pz = z;
        if (tmx < tmy && tmx < tmz) { x += sx; t = tmx; tmx += tdx; }
        else if (tmy < tmz) { y += sy; t = tmy; tmy += tdy; }
        else { z += sz; t = tmz; tmz += tdz; }
    }
    return r;
}

static void updateInteraction(double dt)
{
    double dx = std::sin(P.yaw * DEG) * std::cos(P.pitch * DEG);
    double dy = -std::sin(P.pitch * DEG);
    double dz = -std::cos(P.yaw * DEG) * std::cos(P.pitch * DEG);
    g_hit = raycast(P.x, P.y + P.eye, P.z, dx, dy, dz, 5.0);

    bool left = g_captured && key(VK_LBUTTON);
    if (left && g_hit.hit)
    {
        if (!g_mining || g_hit.x != g_bx || g_hit.y != g_by || g_hit.z != g_bz)
        {
            g_mining = true;
            g_bx = g_hit.x; g_by = g_hit.y; g_bz = g_hit.z;
            g_prog = 0;
            g_swing = 0;
        }
        uint8_t b = g_world->getBlock(g_bx, g_by, g_bz);
        float hard = blockHardness(b);
        if (hard > 0)
        {
            float rate = (float)(dt / hard);
            if (g_mode == Mode::Creative) rate = (float)dt * 8.0f;
            else
            {
                if (!P.onGround && !P.fly) rate *= 0.2f;
                if (g_underWater) rate *= 0.2f;
            }
            g_prog += rate;
            g_partT -= (float)dt;
            if (g_partT <= 0.0f)
            {
                g_partT = 0.16f;
                spawnFaceParticles(g_bx, g_by, g_bz, g_hit.px - g_hit.x, g_hit.py - g_hit.y, g_hit.pz - g_hit.z, b, 2);
            }
            if (g_prog >= 1.0f)
            {
                uint8_t drop = blockDrop(b);
                g_world->setBlock(g_bx, g_by, g_bz, AIR);
                if (drop) spawnDrop(g_bx + 0.5, g_by + 0.5, g_bz + 0.5, drop, 1);
                spawnParticles(g_bx, g_by, g_bz, b, 18);
                g_prog = 0;
                g_mining = false;
            }
        }
    }
    else
    {
        g_mining = false;
        g_prog = 0;
    }

    if (g_ev.rclick && g_hit.hit)
    {
        uint8_t id = g_inv[g_sel].id;
        if (id)
        {
            int px = g_hit.px, py = g_hit.py, pz = g_hit.pz;
            uint8_t cur = g_world->getBlock(px, py, pz);
            bool can = (cur == AIR || isFluid(cur));
            bool inside =
                (px + 1 > P.x - PW && px < P.x + PW &&
                    py + 1 > P.y && py < P.y + PH &&
                    pz + 1 > P.z - PW && pz < P.z + PW);
            if (can && !inside)
            {
                uint8_t meta = 0;
                if (id == WOOD)
                {
                    int nx = px - g_hit.x;
                    int nz = pz - g_hit.z;
                    if (nx != 0) meta = 1;
                    else if (nz != 0) meta = 2;
                }
                g_world->setBlock(px, py, pz, id, meta);
                if (g_mode == Mode::Survival) removeItem(id, 1);
                g_swing = 0;
            }
        }
    }
}

static GLuint makeList(const std::vector<Vertex>& v)
{
    if (v.empty()) return 0;
    GLuint id = glGenLists(1);
    glNewList(id, GL_COMPILE);
    glBegin(GL_QUADS);
    for (size_t i = 0; i < v.size(); i++)
    {
        glColor4ub(v[i].r, v[i].g, v[i].b, v[i].a);
        glTexCoord2f(v[i].u, v[i].v);
        glVertex3f(v[i].x, v[i].y, v[i].z);
    }
    glEnd();
    glEndList();
    return id;
}

static void freeLists(Chunk& c)
{
    if (c.listOpaque) { glDeleteLists(c.listOpaque, 1); c.listOpaque = 0; }
    if (c.listWater) { glDeleteLists(c.listWater, 1);  c.listWater = 0; }
}

static void rebuild(Chunk* c)
{
    g_world->buildMesh(*c);
    freeLists(*c);
    c->listOpaque = makeList(c->meshOpaque);
    c->listWater = makeList(c->meshWater);
    std::vector<Vertex>().swap(c->meshOpaque);
    std::vector<Vertex>().swap(c->meshWater);
    c->meshed = true;
    c->dirty = false;
}

struct Off { int dx, dz, d2; };
static std::vector<Off> g_offs;

static void buildOffsets()
{
    g_offs.clear();
    int R = g_rd + 1;
    for (int dz = -R; dz <= R; dz++)
        for (int dx = -R; dx <= R; dx++)
            if (dx * dx + dz * dz <= R * R) g_offs.push_back({ dx, dz, dx * dx + dz * dz });
    std::sort(g_offs.begin(), g_offs.end(), [](const Off& a, const Off& b) { return a.d2 < b.d2; });
}

static bool hasNeighbors(int cx, int cz)
{
    return g_world->getChunk(cx + 1, cz) && g_world->getChunk(cx - 1, cz) &&
        g_world->getChunk(cx, cz + 1) && g_world->getChunk(cx, cz - 1);
}

static void streamChunks(double budgetSec)
{
    if (!g_world) return;
    int pcx = ifl(P.x / 16.0), pcz = ifl(P.z / 16.0);
    double t0 = nowSec();
    while (nowSec() - t0 < budgetSec)
    {
        bool did = false;
        for (const Off& o : g_offs)
            if (!g_world->getChunk(pcx + o.dx, pcz + o.dz)) { g_world->generateChunk(pcx + o.dx, pcz + o.dz); did = true; break; }
        for (const Off& o : g_offs)
        {
            if (o.d2 > g_rd * g_rd) break;
            Chunk* c = g_world->getChunk(pcx + o.dx, pcz + o.dz);
            if (c && (!c->meshed || c->dirty) && hasNeighbors(c->cx, c->cz)) { rebuild(c); did = true; break; }
        }
        if (!did) break;
    }
}

static void updateLoading(double dt)
{
    g_loadT += dt;
    streamChunks(0.025);
    int pcx = ifl(P.x / 16.0), pcz = ifl(P.z / 16.0);
    int total = 0, ready = 0;
    for (const Off& o : g_offs)
        if (o.d2 <= g_rd * g_rd)
        {
            total++;
            Chunk* c = g_world->getChunk(pcx + o.dx, pcz + o.dz);
            if (c && c->meshed) ready++;
        }
    g_loadFrac = total > 0 ? (float)ready / (float)total : 1.0f;
    if (g_loadFrac >= 0.995f)
    {
        for (int i = 0; i < 64 && solidAt(ifl(P.x), ifl(P.y), ifl(P.z)); i++) P.y += 1.0;
        setState(State::Playing);
    }
}

static float g_fr[6][4];
static void extractFrustum()
{
    float Pm[16], M[16], C[16];
    glGetFloatv(GL_PROJECTION_MATRIX, Pm);
    glGetFloatv(GL_MODELVIEW_MATRIX, M);
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
        {
            float s = 0;
            for (int k = 0; k < 4; k++) s += Pm[k * 4 + r] * M[c * 4 + k];
            C[c * 4 + r] = s;
        }
    auto row = [&](int i, int c) { return C[c * 4 + i]; };
    for (int c = 0; c < 4; c++)
    {
        g_fr[0][c] = row(3, c) + row(0, c);
        g_fr[1][c] = row(3, c) - row(0, c);
        g_fr[2][c] = row(3, c) + row(1, c);
        g_fr[3][c] = row(3, c) - row(1, c);
        g_fr[4][c] = row(3, c) + row(2, c);
        g_fr[5][c] = row(3, c) - row(2, c);
    }
}

static bool aabbVisible(float x0, float y0, float z0, float x1, float y1, float z1)
{
    for (int i = 0; i < 6; i++)
    {
        const float* p = g_fr[i];
        float px = p[0] >= 0 ? x1 : x0, py = p[1] >= 0 ? y1 : y0, pz = p[2] >= 0 ? z1 : z0;
        if (p[0] * px + p[1] * py + p[2] * pz + p[3] < 0) return false;
    }
    return true;
}

static void drawRect(float x, float y, float w, float h, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void drawText(float x, float y, const char* s, GLuint base, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(r, g, b, a);
    glRasterPos2f(x, y);
    glListBase(base);
    glCallLists((GLsizei)std::strlen(s), GL_UNSIGNED_BYTE, s);
}

static bool pointIn(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

static bool button(float x, float y, float w, float h, const char* label, bool big = false)
{
    bool hov = pointIn((float)g_mouseX, (float)g_mouseY, x, y, w, h);
    drawRect(x, y, w, h, hov ? 90 : 45, hov ? 125 : 62, hov ? 175 : 92, 235);
    drawRect(x, y, w, 2, 255, 255, 255, 95);
    float tw = (float)std::strlen(label) * (big ? 10.0f : 4.7f);
    drawText(x + w * 0.5f - tw, y + h * 0.5f + (big ? 12.0f : 6.0f), label, big ? g_fontBig : g_fontBase, 255, 255, 255, 255);
    if (hov && g_ev.lclick) { g_ev.lclick = false; return true; }
    return false;
}

static void drawTile(float x, float y, float s, int tile)
{
    if (tile < 0) return;
    float ts = 1.0f / ATLAS_N;
    float e = 0.0015f;
    float u0 = (tile % ATLAS_N) * ts + e;
    float v0 = (tile / ATLAS_N) * ts + e;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_atlas);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0 + ts - 2 * e); glVertex2f(x, y);
    glTexCoord2f(u0 + ts - 2 * e, v0 + ts - 2 * e); glVertex2f(x + s, y);
    glTexCoord2f(u0 + ts - 2 * e, v0); glVertex2f(x + s, y + s);
    glTexCoord2f(u0, v0); glVertex2f(x, y + s);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static void minimapColor(uint8_t b, bool under, unsigned char& r, unsigned char& g, unsigned char& bl, unsigned char& a)
{
    a = 255;
    switch (b)
    {
    case AIR: if (under) { r = 16; g = 18; bl = 22; }
            else { r = 8; g = 10; bl = 14; } break;
    case GRASS: r = 74; g = 150; bl = 54; break;
    case DIRT: r = 128; g = 91; bl = 61; break;
    case STONE: r = 122; g = 122; bl = 122; break;
    case SAND: r = 218; g = 208; bl = 156; break;
    case WATER: r = 48; g = 106; bl = 210; break;
    case WOOD: r = 103; g = 82; bl = 50; break;
    case LEAVES: r = 48; g = 112; bl = 32; break;
    case PLANKS: r = 174; g = 136; bl = 84; break;
    case COBBLE: r = 122; g = 122; bl = 122; break;
    case BRICKS: r = 148; g = 72; bl = 56; break;
    case SNOW: r = 242; g = 246; bl = 252; break;
    case GRAVEL: r = 132; g = 128; bl = 126; break;
    case COAL: r = 32; g = 32; bl = 36; break;
    case IRON: r = 204; g = 158; bl = 126; break;
    case LAVA: r = 236; g = 94; bl = 14; break;
    case OBSIDIAN: r = 28; g = 20; bl = 44; break;
    case GLASS: r = 190; g = 224; bl = 238; break;
    case BEDROCK: r = 72; g = 72; bl = 72; break;
    default: r = 100; g = 100; bl = 100; break;
    }
}

static void updateMinimap()
{
    if (!g_world) return;
    int cx = ifl(P.x), cz = ifl(P.z), cy = ifl(P.y);
    bool under = P.y < (double)g_world->heightAt(cx, cz) - 3.0;
    if (g_world->getBlock(cx, cy + 2, cz) != AIR) under = true;
    g_underground = under ? 1 : 0;
    int rad = MM / 2;
    for (int j = 0; j < MM; j++)
        for (int i = 0; i < MM; i++)
        {
            int wx = cx + i - rad;
            int wz = cz + j - rad;
            uint8_t b = AIR;
            if (under)
            {
                b = g_world->getBlock(wx, cy, wz);
                if (b == AIR)
                {
                    uint8_t below = g_world->getBlock(wx, cy - 1, wz);
                    if (below == AIR) b = AIR;
                }
            }
            else
            {
                int h = g_world->heightAt(wx, wz);
                b = g_world->getBlock(wx, h, wz);
                if (h < SEA_LEVEL) b = WATER;
                if (b == AIR && h > 0) b = g_world->getBlock(wx, h - 1, wz);
            }
            unsigned char r, g, bl, a;
            minimapColor(b, under, r, g, bl, a);
            int idx = (j * MM + i) * 4;
            g_minimapPx[idx + 0] = r;
            g_minimapPx[idx + 1] = g;
            g_minimapPx[idx + 2] = bl;
            g_minimapPx[idx + 3] = a;
        }
    glBindTexture(GL_TEXTURE_2D, g_minimapTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MM, MM, GL_RGBA, GL_UNSIGNED_BYTE, g_minimapPx);
}

static void drawMinimap()
{
    if (!g_world) return;
    float size = 150.0f;
    float x = (float)g_w - size - 12.0f;
    float y = 12.0f;
    drawRect(x - 4, y - 4, size + 8, size + 8, 0, 0, 0, 170);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_minimapTex);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + size, y);
    glTexCoord2f(1, 1); glVertex2f(x + size, y + size);
    glTexCoord2f(0, 1); glVertex2f(x, y + size);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    drawRect(x + size * 0.5f - 2, y + size * 0.5f - 2, 4, 4, 255, 52, 52, 255);
    if (g_underground) drawText(x + 6, y + 18, "Cueva", g_fontBase, 255, 255, 255, 255);
}

static void drawArm(float s)
{
    glDisable(GL_TEXTURE_2D);
    glColor4ub(214, 164, 122, 255);
    glBegin(GL_QUADS);
    glVertex2f(-s * 0.34f, -s * 0.08f);
    glVertex2f(s * 0.34f, -s * 0.08f);
    glVertex2f(s * 0.22f, s * 1.24f);
    glVertex2f(-s * 0.22f, s * 1.24f);
    glEnd();
    glColor4ub(76, 124, 190, 255);
    glBegin(GL_QUADS);
    glVertex2f(-s * 0.36f, s * 0.95f);
    glVertex2f(s * 0.36f, s * 0.95f);
    glVertex2f(s * 0.22f, s * 1.24f);
    glVertex2f(-s * 0.22f, s * 1.24f);
    glEnd();
}

static void drawHeldBlock(uint8_t id, float s)
{
    int top = blockTile(id, 2);
    int side = blockTile(id, 4);
    int front = blockTile(id, 0);
    float ts = 1.0f / ATLAS_N;
    float e = 0.0015f;
    auto UV = [&](int tile, float& u0, float& v0)
        {
            u0 = (tile % ATLAS_N) * ts + e;
            v0 = (tile / ATLAS_N) * ts + e;
        };
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_atlas);
    float u0, v0;
    UV(top, u0, v0);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0 + ts - 2 * e); glVertex2f(0, -s * 1.05f);
    glTexCoord2f(u0 + ts - 2 * e, v0 + ts - 2 * e); glVertex2f(s * 0.55f, -s * 0.72f);
    glTexCoord2f(u0 + ts - 2 * e, v0); glVertex2f(0, -s * 0.38f);
    glTexCoord2f(u0, v0); glVertex2f(-s * 0.55f, -s * 0.72f);
    glEnd();
    UV(side, u0, v0);
    glColor4ub(170, 170, 170, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0 + ts - 2 * e); glVertex2f(-s * 0.55f, -s * 0.72f);
    glTexCoord2f(u0 + ts - 2 * e, v0 + ts - 2 * e); glVertex2f(0, -s * 0.38f);
    glTexCoord2f(u0 + ts - 2 * e, v0); glVertex2f(0, s * 0.14f);
    glTexCoord2f(u0, v0); glVertex2f(-s * 0.55f, -s * 0.20f);
    glEnd();
    UV(front, u0, v0);
    glColor4ub(220, 220, 220, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0 + ts - 2 * e); glVertex2f(0, -s * 0.38f);
    glTexCoord2f(u0 + ts - 2 * e, v0 + ts - 2 * e); glVertex2f(s * 0.55f, -s * 0.72f);
    glTexCoord2f(u0 + ts - 2 * e, v0); glVertex2f(s * 0.55f, -s * 0.20f);
    glTexCoord2f(u0, v0); glVertex2f(0, s * 0.14f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static void drawHand()
{
    if (g_state != State::Playing) return;
    float s = (float)std::min(g_w, g_h) * 0.24f;
    float sw = (g_swing >= 0.0f) ? std::sin(g_swing * (float)PI) : 0.0f;
    float bob = std::sin(P.walk * 2.0f) * P.bob * 38.0f;
    float x = (float)g_w * 0.78f + std::sin(P.walk) * P.bob * 24.0f;
    float y = (float)g_h * 0.94f + bob + std::fabs(sw) * 22.0f;
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(-14.0f - sw * 58.0f, 0, 0, 1);
    drawArm(s);
    uint8_t id = g_inv[g_sel].id;
    if (id) drawHeldBlock(id, s);
    glPopMatrix();
}

static void drawCrosshair()
{
    float cx = (float)g_w * 0.5f;
    float cy = (float)g_h * 0.5f;
    drawRect(cx - 10, cy - 1, 20, 2, 255, 255, 255, 190);
    drawRect(cx - 1, cy - 10, 2, 20, 255, 255, 255, 190);
}

struct HRect { int x, y, w, h; };
static const HRect HEART[] = { {1,0,3,1}, {5,0,3,1}, {0,1,9,3}, {1,4,7,1}, {2,5,5,1}, {3,6,3,1}, {4,7,1,1} };
static const HRect BUB_OUT[] = { {2,0,5,9}, {0,2,9,5}, {1,1,7,7} };
static const HRect BUB_IN[] = { {3,1,3,7}, {1,3,7,3}, {2,2,5,5} };

static void drawHeart(float x, float y, float u, int state)
{
    for (const HRect& r : HEART)
        drawRect(x + (r.x - 1) * u, y + (r.y - 1) * u, (r.w + 2) * u, (r.h + 2) * u, 20, 8, 8, 255);
    for (const HRect& r : HEART)
        drawRect(x + r.x * u, y + r.y * u, r.w * u, r.h * u, 62, 32, 32, 255);
    if (state <= 0) return;
    for (const HRect& r : HEART)
    {
        float x1 = (float)(r.x + r.w);
        if (state == 1) x1 = std::min(x1, 4.5f);
        float w = x1 - (float)r.x;
        if (w <= 0.0f) continue;
        drawRect(x + r.x * u, y + r.y * u, w * u, r.h * u, 222, 36, 36, 255);
    }
    drawRect(x + 1 * u, y + 1 * u, 2 * u, 1 * u, 255, 150, 150, 255);
}

static void drawBubble(float x, float y, float u)
{
    for (const HRect& r : BUB_OUT) drawRect(x + r.x * u, y + r.y * u, r.w * u, r.h * u, 190, 228, 255, 255);
    for (const HRect& r : BUB_IN) drawRect(x + r.x * u, y + r.y * u, r.w * u, r.h * u, 60, 118, 224, 255);
    drawRect(x + 3 * u, y + 2 * u, u, u, 235, 248, 255, 255);
}

static void drawHUD()
{
    drawCrosshair();
    drawMinimap();

    float slot = 52.0f;
    float gap = 4.0f;
    float total = 9 * slot + 8 * gap;
    float x0 = ((float)g_w - total) * 0.5f;
    float y0 = (float)g_h - slot - 12.0f;
    for (int i = 0; i < 9; i++)
    {
        float x = x0 + i * (slot + gap);
        drawRect(x, y0, slot, slot, 22, 24, 30, 190);
        drawRect(x + 1, y0 + 1, slot - 2, slot - 2, 56, 60, 68, 160);
        if (i == g_sel) drawRect(x - 2, y0 - 2, slot + 4, 3, 255, 255, 255, 230);
        Stack& st = g_inv[i];
        if (st.id)
        {
            drawTile(x + 8, y0 + 8, slot - 16, blockTile(st.id, 2));
            if (st.n > 1)
            {
                char b[16];
                std::snprintf(b, sizeof(b), "%d", st.n);
                drawText(x + slot - 18, y0 + slot - 5, b, g_fontBase, 255, 255, 255, 255);
            }
        }
    }

    if (g_mode == Mode::Survival)
    {
        const float hu = 2.0f;
        const float hy = y0 - 28.0f;
        for (int i = 0; i < 10; i++)
        {
            float hp = P.health - (float)(i * 2);
            int st = hp >= 2.0f ? 2 : (hp >= 1.0f ? 1 : 0);
            drawHeart(x0 + 4.0f + i * 10.0f * hu, hy, hu, st);
        }
        if (P.air < 15.0f)
        {
            int nb = (int)std::ceil(P.air / 1.5f);
            float xr = x0 + total - 4.0f;
            for (int i = 0; i < nb; i++)
                drawBubble(xr - (i + 1) * 10.0f * hu, hy, hu);
        }
    }

    if (g_nameT > 0.0f)
    {
        uint8_t id = g_inv[g_sel].id;
        if (id)
        {
            const char* n = blockName(id);
            float tw = (float)std::strlen(n) * 4.7f;
            drawText((float)g_w * 0.5f - tw, y0 - 46, n, g_fontBase, 255, 255, 255, 255);
        }
    }
}

static void drawMainMenu()
{
    drawRect(0, 0, (float)g_w, (float)g_h, 32, 58, 92, 120);
    const char* title = "MineCPP";
    drawText((float)g_w * 0.5f - (float)std::strlen(title) * 10.0f, 110, title, g_fontBig, 255, 255, 255, 255);
    drawText((float)g_w * 0.5f - 44, 145, "By RataLife", g_fontBase, 220, 235, 255, 255);
    float bw = 320, bh = 50, bx = ((float)g_w - bw) * 0.5f;
    if (button(bx, 240, bw, bh, "Jugar", true)) { startWorld(randomSeed(), g_mode); return; }
    if (button(bx, 305, bw, bh, "Crear mundo", true)) { setState(State::CreateWorld); return; }
    const char* m = (g_mode == Mode::Survival) ? "Modo: Supervivencia" : "Modo: Creativo";
    if (button(bx, 370, bw, bh, m, true)) g_mode = (g_mode == Mode::Survival) ? Mode::Creative : Mode::Survival;
    if (button(bx, 435, bw, bh, "Salir", true)) { g_running = false; return; }
}

static void drawCreateWorld()
{
    drawRect(0, 0, (float)g_w, (float)g_h, 22, 30, 44, 180);
    drawText((float)g_w * 0.5f - 105, 100, "Create World", g_fontBig, 255, 255, 255, 255);
    float bw = 420, bx = ((float)g_w - bw) * 0.5f;
    drawRect(bx, 180, bw, 54, 18, 20, 26, 230);
    drawRect(bx + 2, 182, bw - 4, 50, 48, 54, 64, 220);
    const char* txt = g_seedText.empty() ? "Seed" : g_seedText.c_str();
    drawText(bx + 14, 216, txt, g_fontBase, g_seedText.empty() ? 170 : 255, g_seedText.empty() ? 180 : 255, g_seedText.empty() ? 190 : 255, 255);
    if (button(bx, 260, bw, 50, "Create", true)) { startWorld(seedFromText(g_seedText), g_mode); return; }
    const char* m = (g_mode == Mode::Survival) ? "Supervivence" : "Creative";
    if (button(bx, 325, bw, 50, m, true)) g_mode = (g_mode == Mode::Survival) ? Mode::Creative : Mode::Survival;
    if (button(bx, 390, bw, 50, "Back", true)) { setState(State::MainMenu); return; }
}

static void drawLoading()
{
    drawRect(0, 0, (float)g_w, (float)g_h, 18, 28, 48, 255);
    drawText((float)g_w * 0.5f - 120, (float)g_h * 0.42f, "Creating World...", g_fontBig, 255, 255, 255, 255);
    float bw = 420, bx = ((float)g_w - bw) * 0.5f, by = (float)g_h * 0.55f;
    drawRect(bx, by, bw, 22, 24, 26, 34, 230);
    drawRect(bx + 2, by + 2, (bw - 4) * g_loadFrac, 18, 80, 165, 255, 255);
    char b[64];
    std::snprintf(b, sizeof(b), "%d%%", (int)(g_loadFrac * 100.0f));
    drawText((float)g_w * 0.5f - 14, by + 48, b, g_fontBase, 255, 255, 255, 255);
}

static void drawPaused()
{
    drawRect(0, 0, (float)g_w, (float)g_h, 0, 0, 0, 150);
    drawText((float)g_w * 0.5f - 112, 160, "Game Paused", g_fontBig, 255, 255, 255, 255);
    float bw = 320, bh = 50, bx = ((float)g_w - bw) * 0.5f;
    if (button(bx, 280, bw, bh, "Continue", true)) { setState(State::Playing); return; }
    if (button(bx, 345, bw, bh, "Exit to menu", true)) { goMenu(); return; }
}

static void drawDeath()
{
    drawRect(0, 0, (float)g_w, (float)g_h, 90, 0, 0, 150);
    drawText((float)g_w * 0.5f - 150, 160, "You Died", g_fontBig, 255, 220, 220, 255);
    float bw = 320, bh = 50, bx = ((float)g_w - bw) * 0.5f;
    if (button(bx, 280, bw, bh, "Restart", true)) { respawn(); return; }
    if (button(bx, 345, bw, bh, "Exit to menu", true)) { goMenu(); return; }
}

static void drawSlot(int idx, float x, float y, float s)
{
    bool hov = pointIn((float)g_mouseX, (float)g_mouseY, x, y, s, s);
    drawRect(x, y, s, s, 20, 22, 28, 220);
    drawRect(x + 1, y + 1, s - 2, s - 2, hov ? 82 : 52, hov ? 90 : 58, hov ? 104 : 68, 220);
    Stack& st = g_inv[idx];
    if (st.id)
    {
        drawTile(x + 7, y + 7, s - 14, blockTile(st.id, 2));
        if (st.n > 1)
        {
            char b[16];
            std::snprintf(b, sizeof(b), "%d", st.n);
            drawText(x + s - 17, y + s - 5, b, g_fontBase, 255, 255, 255, 255);
        }
    }
    if (hov && g_ev.lclick)
    {
        g_ev.lclick = false;
        if (g_cursor.id == 0 && st.id)
        {
            g_cursor = st;
            st = Stack();
        }
        else if (g_cursor.id && st.id == 0)
        {
            st = g_cursor;
            g_cursor = Stack();
        }
        else if (g_cursor.id && st.id == g_cursor.id)
        {
            int a = std::min(64 - st.n, g_cursor.n);
            st.n += a;
            g_cursor.n -= a;
            if (g_cursor.n <= 0) g_cursor = Stack();
        }
        else if (g_cursor.id || st.id)
        {
            Stack t = st;
            st = g_cursor;
            g_cursor = t;
        }
    }
}

static void drawInventoryScreen()
{
    drawRect(0, 0, (float)g_w, (float)g_h, 0, 0, 0, 150);
    float s = 48.0f;
    float gap = 4.0f;
    float gw = 9 * s + 8 * gap;
    float x0 = ((float)g_w - gw) * 0.5f;
    float y0 = ((float)g_h - (4 * s + 3 * gap + 40)) * 0.5f;
    drawRect(x0 - 18, y0 - 60, gw + 36, 4 * s + 3 * gap + 94, 18, 20, 26, 240);
    drawText(x0, y0 - 28, "Inventario", g_fontBig, 255, 255, 255, 255);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 9; c++)
        {
            int idx = 9 + r * 9 + c;
            drawSlot(idx, x0 + c * (s + gap), y0 + r * (s + gap), s);
        }
    float hy = y0 + 3 * (s + gap) + 18;
    for (int c = 0; c < 9; c++)
        drawSlot(c, x0 + c * (s + gap), hy, s);
    if (g_mode == Mode::Survival)
    {
        struct Recipe { uint8_t in; int inN; uint8_t out; int outN; const char* label; };
        static const Recipe RC[] = {
            { WOOD, 1, PLANKS, 4, "1 Madera -> 4 Tablones" },
            { SAND, 4, GLASS, 1, "4 Arena -> 1 Vidrio" },
            { COBBLE, 1, STONE, 1, "1 Adoquin -> 1 Piedra" },
        };
        const float px = x0 + gw + 36.0f;
        drawRect(px - 12, y0 - 60, 274, 4 * s + 3 * gap + 94, 18, 20, 26, 240);
        drawText(px, y0 - 28, "Crafteo", g_fontBig, 255, 255, 255, 255);
        for (int i = 0; i < 3; i++)
        {
            const Recipe& r = RC[i];
            const float bx = px, by = y0 + 6.0f + i * 44.0f, bw = 250.0f, bh = 34.0f;
            if (countItem(r.in) >= r.inN)
            {
                if (button(bx, by, bw, bh, r.label))
                {
                    removeItem(r.in, r.inN);
                    int left = addItem(r.out, r.outN);
                    if (left > 0) spawnDrop(P.x, P.y + 1.4, P.z, r.out, left, true);
                }
            }
            else
            {
                drawRect(bx, by, bw, bh, 30, 34, 42, 235);
                drawText(bx + bw * 0.5f - (float)std::strlen(r.label) * 4.7f, by + bh * 0.5f + 6.0f, r.label, g_fontBase, 120, 124, 132, 255);
            }
        }
    }
    if (g_cursor.id)
    {
        drawTile((float)g_mouseX - 20, (float)g_mouseY - 20, 40, blockTile(g_cursor.id, 2));
        if (g_cursor.n > 1)
        {
            char b[16];
            std::snprintf(b, sizeof(b), "%d", g_cursor.n);
            drawText((float)g_mouseX + 8, (float)g_mouseY + 18, b, g_fontBase, 255, 255, 255, 255);
        }
    }
}

static void updateAtlasAnimation(double dt)
{
    if (!g_atlasPx) return;
    if (g_shadersOK) return;
    g_atlasT += dt;
    if (g_atlasT < 0.08) return;
    g_atlasT = 0.0;
    double t = nowSec();
    uint8_t tile[TILE_PX * TILE_PX * 4];
    int anim[4] = { TL_WATER_STILL, TL_WATER_FLOW, TL_LAVA_STILL, TL_LAVA_FLOW };
    for (int i = 0; i < 4; i++)
    {
        AtlasGen::animateTile(tile, anim[i], t);
        AtlasGen::blit(g_atlasPx, tile, anim[i]);
    }
    glBindTexture(GL_TEXTURE_2D, g_atlas);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ATLAS_PX, ATLAS_PX, GL_RGBA, GL_UNSIGNED_BYTE, g_atlasPx);
}

static void processEvents()
{
    if (g_ev.f11) toggleFullscreen();
    if (g_ev.lostFocus && g_state == State::Playing) setState(State::Paused);

    if (g_state == State::CreateWorld)
    {
        for (char c : g_ev.chars) if (g_seedText.size() < 32) g_seedText += c;
        if (g_ev.backspace && !g_seedText.empty()) g_seedText.pop_back();
        if (g_ev.enter) startWorld(seedFromText(g_seedText), g_mode);
        else if (g_ev.esc) setState(State::MainMenu);
        return;
    }

    if (g_state == State::Playing)
    {
        if (g_ev.inv) { setState(State::Inventory); return; }
        if (g_ev.esc) { setState(State::Paused); return; }
        if (g_ev.hotkey >= 0) { g_sel = g_ev.hotkey; g_nameT = 2.0f; }
        if (g_ev.wheel != 0) { g_sel = (g_sel + (g_ev.wheel > 0 ? -1 : 1) + 9) % 9; g_nameT = 2.0f; }
        if (g_ev.rdUp && g_rd < 16) { g_rd++; buildOffsets(); }
        if (g_ev.rdDown && g_rd > 3) { g_rd--; buildOffsets(); }
    }
    else if (g_state == State::Inventory)
    {
        if (g_ev.inv || g_ev.esc) setState(State::Playing);
    }
    else if (g_state == State::Paused)
    {
        if (g_ev.esc) setState(State::Playing);
    }
    else if (g_state == State::Dead)
    {
        if (g_ev.esc) goMenu();
    }
}

static void environment(float* col, float& fs, float& fe)
{
    const float dist = (float)(g_rd * 16);
    if (g_inLava) { col[0] = 0.55f; col[1] = 0.12f; col[2] = 0.02f; fs = 0.0f; fe = 2.5f; }
    else if (g_underWater) { col[0] = 0.08f; col[1] = 0.20f; col[2] = 0.50f; fs = 0.5f; fe = 20.0f; }
    else { col[0] = 0.53f; col[1] = 0.78f; col[2] = 0.98f; fs = dist * 0.55f; fe = dist * 0.95f; }
    col[3] = 1.0f;
}

static void drawCube(uint8_t id)
{
    const float ts = 1.0f / ATLAS_N, e = 0.0004f;
    glBegin(GL_QUADS);
    for (int f = 0; f < 6; f++)
    {
        const int tile = blockTile(id, f, 0);
        const float tu = (tile % ATLAS_N) * ts, tv = (tile / ATLAS_N) * ts;
        const float uu[4] = { tu + e, tu + ts - e, tu + ts - e, tu + e };
        const float vv[4] = { tv + ts - e, tv + ts - e, tv + e, tv + e };
        const unsigned char c = (unsigned char)(FACE_SHADE[f] * 255.0f);
        glColor4ub(c, c, c, 255);
        for (int i = 0; i < 4; i++)
        {
            glTexCoord2f(uu[i], vv[i]);
            glVertex3f(FACE_V[f][i][0] - 0.5f, FACE_V[f][i][1] - 0.5f, FACE_V[f][i][2] - 0.5f);
        }
    }
    glEnd();
}

static void drawEntities()
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_atlas);
    glEnable(GL_CULL_FACE);

    for (const Fall& f : g_falls)
    {
        glPushMatrix();
        glTranslatef((float)f.x + 0.5f, (float)f.y + 0.5f, (float)f.z + 0.5f);
        glScalef(0.999f, 0.999f, 0.999f);
        drawCube(f.id);
        glPopMatrix();
    }

    for (const Drop& d : g_drops)
    {
        const float bob = std::sin((float)(d.age * 2.5 + d.x)) * 0.05f + 0.2f;
        const float spin = (float)(d.age * 70.0 + d.z * 40.0);
        glPushMatrix();
        glTranslatef((float)d.x, (float)d.y + bob, (float)d.z);
        glRotatef(spin, 0.0f, 1.0f, 0.0f);
        glScalef(0.28f, 0.28f, 0.28f);
        drawCube(d.id);
        if (d.n > 1)
        {
            glTranslatef(0.55f, 0.15f, 0.4f);
            drawCube(d.id);
        }
        if (d.n > 16)
        {
            glTranslatef(-1.0f, 0.1f, -0.7f);
            drawCube(d.id);
        }
        glPopMatrix();
    }

    if (!g_parts.empty())
    {
        float mv[16];
        glGetFloatv(GL_MODELVIEW_MATRIX, mv);
        const float rx = mv[0], ry = mv[4], rz = mv[8];
        const float ux = mv[1], uy = mv[5], uz = mv[9];
        glDisable(GL_CULL_FACE);
        glBegin(GL_QUADS);
        for (const Part& p : g_parts)
        {
            const unsigned char c = (unsigned char)std::min(255.0f, p.shade * 255.0f);
            glColor4ub(c, c, c, 255);
            const float s = p.size;
            glTexCoord2f(p.u, p.v + p.us); glVertex3f(p.x - (rx + ux) * s, p.y - (ry + uy) * s, p.z - (rz + uz) * s);
            glTexCoord2f(p.u + p.us, p.v + p.us); glVertex3f(p.x + (rx - ux) * s, p.y + (ry - uy) * s, p.z + (rz - uz) * s);
            glTexCoord2f(p.u + p.us, p.v); glVertex3f(p.x + (rx + ux) * s, p.y + (ry + uy) * s, p.z + (rz + uz) * s);
            glTexCoord2f(p.u, p.v); glVertex3f(p.x - (rx - ux) * s, p.y - (ry - uy) * s, p.z - (rz - uz) * s);
        }
        glEnd();
        glEnable(GL_CULL_FACE);
    }
    glColor4ub(255, 255, 255, 255);
}

static void drawWorld()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)g_w / (float)(g_h ? g_h : 1);
    float fH = (float)std::tan(P.fov * PI / 360.0) * 0.1f;
    float fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, 0.1, 1000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    {
        const float ph = P.walk * (float)PI;
        const float bb = P.bob;
        glTranslatef(std::sin(ph) * bb * 0.5f, -std::fabs(std::cos(ph)) * bb, 0.0f);
        glRotatef(std::sin(ph) * bb * 3.0f, 0.0f, 0.0f, 1.0f);
        glRotatef(std::fabs(std::cos(ph - 0.2f)) * bb * 5.0f, 1.0f, 0.0f, 0.0f);
        if (P.hurt > 0.0f)
        {
            const float hr = std::min(1.0f, P.hurt / 0.4f);
            glRotatef(-std::sin(hr * hr * hr * hr * (float)PI) * 14.0f, 0.0f, 0.0f, 1.0f);
        }
    }
    glRotatef(P.pitch, 1.0f, 0.0f, 0.0f);
    glRotatef(P.yaw, 0.0f, 1.0f, 0.0f);
    glTranslatef(-(float)P.x, -(float)(P.y + P.eye), -(float)P.z);

    extractFrustum();

    float fogC[4], fogS, fogE;
    environment(fogC, fogS, fogE);

    if (g_shadersOK)
    {
        glDisable(GL_FOG);
        glDisable(GL_ALPHA_TEST);
        gl2.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_atlas);
        g_worldProg.use();
        g_worldProg.seti("uAtlas", 0);
        g_worldProg.set3("uFogColor", fogC[0], fogC[1], fogC[2]);
        g_worldProg.setf("uFogStart", fogS);
        g_worldProg.setf("uFogEnd", fogE);
    }
    else
    {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.1f);
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogfv(GL_FOG_COLOR, fogC);
        glFogf(GL_FOG_START, fogS);
        glFogf(GL_FOG_END, fogE);
        glBindTexture(GL_TEXTURE_2D, g_atlas);
    }

    const float cullD = fogE + 24.0f;
    auto inRange = [&](float cx0, float cz0)
        {
            const float ddx = cx0 + 8.0f - (float)P.x, ddz = cz0 + 8.0f - (float)P.z;
            return ddx * ddx + ddz * ddz <= cullD * cullD;
        };

    for (auto& kv : g_world->chunks)
    {
        Chunk* c = kv.second;
        if (!c->meshed || !c->listOpaque) continue;
        float cx0 = c->cx * 16.0f, cz0 = c->cz * 16.0f;
        if (!inRange(cx0, cz0)) continue;
        if (!aabbVisible(cx0, 0, cz0, cx0 + 16.0f, 128.0f, cz0 + 16.0f)) continue;

        glPushMatrix();
        glTranslatef(cx0, 0.0f, cz0);
        glCallList(c->listOpaque);
        glPopMatrix();
    }

    drawEntities();
    glBindTexture(GL_TEXTURE_2D, g_atlas);

    if (g_shadersOK)
    {
        g_waterProg.use();
        g_waterProg.seti("uAtlas", 0);
        g_waterProg.set3("uFogColor", fogC[0], fogC[1], fogC[2]);
        g_waterProg.setf("uFogStart", fogS);
        g_waterProg.setf("uFogEnd", fogE);
        g_waterProg.setf("uTime", (float)nowSec());
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (g_underWater) glDisable(GL_CULL_FACE);
    for (auto& kv : g_world->chunks)
    {
        Chunk* c = kv.second;
        if (!c->meshed || !c->listWater) continue;
        float cx0 = c->cx * 16.0f, cz0 = c->cz * 16.0f;
        if (!inRange(cx0, cz0)) continue;
        if (!aabbVisible(cx0, 0, cz0, cx0 + 16.0f, 128.0f, cz0 + 16.0f)) continue;
        if (g_shadersOK)
        {
            g_waterProg.set3("uChunkOffset", cx0, 0.0f, cz0);
            glPushMatrix();
            glTranslatef(cx0, 0.0f, cz0);
            glCallList(c->listWater);
            glPopMatrix();
        }
        else
        {
            glPushMatrix();
            glTranslatef(cx0, 0.0f, cz0);
            glCallList(c->listWater);
            glPopMatrix();
        }
    }
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    if (g_shadersOK) gl2.UseProgram(0);

    if (g_state == State::Playing && g_hit.hit)
    {
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);
        glColor4ub(0, 0, 0, 180);
        glLineWidth(2.0f);
        float x0 = (float)g_hit.x, y0 = (float)g_hit.y, z0 = (float)g_hit.z;
        float x1 = x0 + 1.0f, y1 = y0 + 1.0f, z1 = z0 + 1.0f;
        glBegin(GL_LINES);
        glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0);
        glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1);
        glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
        glVertex3f(x0, y0, z1); glVertex3f(x0, y0, z0);
        glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);
        glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
        glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
        glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
        glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0);
        glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0);
        glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1);
        glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1);
        glEnd();
        glLineWidth(1.0f);
        glEnable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);
    }
    if (g_mining && g_prog > 0.0f)
    {
        int stage = (int)(g_prog * 10.0f);
        if (stage > 9) stage = 9;
        int tile = TL_CRACK + stage;
        float ts = 1.0f / ATLAS_N;
        float tu = (tile % ATLAS_N) * ts, tv = (tile / ATLAS_N) * ts;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        glColor4ub(255, 255, 255, 255);
        float x0 = (float)g_bx - 0.005f, y0 = (float)g_by - 0.005f, z0 = (float)g_bz - 0.005f;
        float x1 = x0 + 1.01f, y1 = y0 + 1.01f, z1 = z0 + 1.01f;
        glBegin(GL_QUADS);
        glTexCoord2f(tu, tv + ts); glVertex3f(x0, y1, z1);
        glTexCoord2f(tu + ts, tv + ts); glVertex3f(x1, y1, z1);
        glTexCoord2f(tu + ts, tv); glVertex3f(x1, y1, z0);
        glTexCoord2f(tu, tv); glVertex3f(x0, y1, z0);
        glTexCoord2f(tu, tv + ts); glVertex3f(x0, y0, z0);
        glTexCoord2f(tu + ts, tv + ts); glVertex3f(x1, y0, z0);
        glTexCoord2f(tu + ts, tv); glVertex3f(x1, y0, z1);
        glTexCoord2f(tu, tv); glVertex3f(x0, y0, z1);
        glTexCoord2f(tu, tv + ts); glVertex3f(x0, y1, z1);
        glTexCoord2f(tu + ts, tv + ts); glVertex3f(x0, y0, z1);
        glTexCoord2f(tu + ts, tv); glVertex3f(x1, y0, z1);
        glTexCoord2f(tu, tv); glVertex3f(x1, y1, z1);
        glTexCoord2f(tu, tv + ts); glVertex3f(x1, y1, z0);
        glTexCoord2f(tu + ts, tv + ts); glVertex3f(x1, y0, z0);
        glTexCoord2f(tu + ts, tv); glVertex3f(x0, y0, z0);
        glTexCoord2f(tu, tv); glVertex3f(x0, y1, z0);
        glTexCoord2f(tu, tv + ts); glVertex3f(x0, y1, z0);
        glTexCoord2f(tu + ts, tv + ts); glVertex3f(x0, y0, z0);
        glTexCoord2f(tu + ts, tv); glVertex3f(x0, y0, z1);
        glTexCoord2f(tu, tv); glVertex3f(x0, y1, z1);
        glTexCoord2f(tu, tv + ts); glVertex3f(x1, y1, z1);
        glTexCoord2f(tu + ts, tv + ts); glVertex3f(x1, y0, z1);
        glTexCoord2f(tu + ts, tv); glVertex3f(x1, y0, z0);
        glTexCoord2f(tu, tv); glVertex3f(x1, y1, z0);
        glEnd();
        glDisable(GL_BLEND);
    }
}

static void drawUI()
{
    if (g_shadersOK) gl2.UseProgram(0);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1f);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g_w, g_h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (g_world && g_underWater) drawRect(0, 0, (float)g_w, (float)g_h, 20, 50, 140, 90);
    else if (g_world && g_inLava) drawRect(0, 0, (float)g_w, (float)g_h, 226, 82, 8, 140);
    if (g_world && P.hurt > 0.0f) drawRect(0, 0, (float)g_w, (float)g_h, 210, 20, 20, (unsigned char)(std::min(1.0f, P.hurt / 0.4f) * 95.0f));

    if (g_state == State::Playing)
    {
        drawHand();
        drawHUD();
    }
    else if (g_state == State::Inventory)
    {
        drawInventoryScreen();
    }
    else if (g_state == State::Paused)
    {
        drawPaused();
    }
    else if (g_state == State::Dead)
    {
        drawDeath();
    }
    else if (g_state == State::MainMenu)
    {
        drawMainMenu();
    }
    else if (g_state == State::CreateWorld)
    {
        drawCreateWorld();
    }
    else if (g_state == State::Loading)
    {
        drawLoading();
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_arrow = LoadCursor(nullptr, IDC_ARROW);

    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = g_arrow;
    wc.lpszClassName = "MineCPP_Class";

    if (!RegisterClassExA(&wc)) return -1;

    g_hwnd = CreateWindowExA(0, "MineCPP_Class", "MineCPP By RataLife",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, g_w, g_h,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hwnd) return -1;

    g_hdc = GetDC(g_hwnd);

    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int pf = ChoosePixelFormat(g_hdc, &pfd);
    SetPixelFormat(g_hdc, pf, &pfd);
    g_rc = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, g_rc);

    if (GL2_Init())
    {
        initShaders();
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1f);

    g_font = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Consolas");

    g_fontB = CreateFontA(44, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Consolas");

    SelectObject(g_hdc, g_font);
    g_fontBase = glGenLists(256);
    wglUseFontBitmapsA(g_hdc, 0, 256, g_fontBase);
    SelectObject(g_hdc, g_fontB);
    g_fontBig = glGenLists(256);
    wglUseFontBitmapsA(g_hdc, 0, 256, g_fontBig);

    buildOffsets();

    g_atlasPx = new uint8_t[ATLAS_PX * ATLAS_PX * 4];
    generateAtlas(g_atlasPx);
    glGenTextures(1, &g_atlas);
    glBindTexture(GL_TEXTURE_2D, g_atlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_PX, ATLAS_PX, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_atlasPx);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    std::memset(g_minimapPx, 0, sizeof(g_minimapPx));
    glGenTextures(1, &g_minimapTex);
    glBindTexture(GL_TEXTURE_2D, g_minimapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MM, MM, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_minimapPx);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    setState(State::MainMenu);

    double lastTime = nowSec();
    MSG msg = { 0 };

    while (g_running)
    {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { g_running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!g_running) break;

        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(g_hwnd, &pt);
        g_mouseX = pt.x;
        g_mouseY = pt.y;

        double currentTime = nowSec();
        double dt = currentTime - lastTime;
        if (dt > 0.1) dt = 0.1;
        lastTime = currentTime;

        if (g_captured && g_active && g_state == State::Playing)
        {
            RECT rc;
            GetClientRect(g_hwnd, &rc);
            POINT center = { rc.right / 2, rc.bottom / 2 };
            int dx = pt.x - center.x;
            int dy = pt.y - center.y;
            if (dx != 0 || dy != 0)
            {
                P.yaw += (float)dx * 0.15f;
                P.pitch += (float)dy * 0.15f;
                if (P.pitch < -89.9f) P.pitch = -89.9f;
                if (P.pitch > 89.9f) P.pitch = 89.9f;
                POINT sp = center;
                ClientToScreen(g_hwnd, &sp);
                SetCursorPos(sp.x, sp.y);
            }
        }

        processEvents();

        if (g_state == State::Loading && g_world)
        {
            updateLoading(dt);
        }
        else if (g_state == State::Playing && g_world)
        {
            updatePlayer(dt, true);
            updateInteraction(dt);
            streamChunks(0.005);
            updateGame(dt);
            if (currentTime - g_minimapT > 0.35)
            {
                updateMinimap();
                g_minimapT = currentTime;
            }
        }

        updateAtlasAnimation(dt);

        glViewport(0, 0, g_w, g_h);
        float skyC[4], skyS, skyE;
        environment(skyC, skyS, skyE);
        glClearColor(skyC[0], skyC[1], skyC[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (g_world && (g_state == State::Playing || g_state == State::Paused || g_state == State::Inventory || g_state == State::Dead))
        {
            drawWorld();
        }

        drawUI();

        SwapBuffers(g_hdc);
        g_ev = Events();
        Sleep(1);
    }

    if (g_world) delete g_world;
    if (g_atlasPx) delete[] g_atlasPx;
    if (g_shadersOK) { g_worldProg.destroy(); g_waterProg.destroy(); }
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(g_rc);
    ReleaseDC(g_hwnd, g_hdc);
    DestroyWindow(g_hwnd);

    return 0;
}