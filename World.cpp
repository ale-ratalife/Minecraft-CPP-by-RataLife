#include "World.h"
#include "Noise.h"
#include <cmath>
#include <cstring>
#include <algorithm>

static inline int64_t keyOf(int cx, int cz) { return ((int64_t)cx << 32) | (uint32_t)cz; }
static inline uint64_t posKey(int x, int y, int z)
{
    return ((uint64_t)(uint32_t)(x + (1 << 24)) << 33) | ((uint64_t)(uint32_t)(z + (1 << 24)) << 8) | (uint64_t)(uint32_t)y;
}

inline int chunkCord(int x) { return x < 0 ? (x - 15) / 16 : x / 16; }

const int FACE_N[6][3] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };

const int FACE_V[6][4][3] = {
    { {1,0,1},{1,0,0},{1,1,0},{1,1,1} },
    { {0,0,0},{0,0,1},{0,1,1},{0,1,0} },
    { {0,1,1},{1,1,1},{1,1,0},{0,1,0} },
    { {0,0,0},{1,0,0},{1,0,1},{0,0,1} },
    { {0,0,1},{1,0,1},{1,1,1},{0,1,1} },
    { {1,0,0},{0,0,0},{0,1,0},{1,1,0} },
};

const float FACE_SHADE[6] = { 0.70f, 0.70f, 1.0f, 0.5f, 0.85f, 0.85f };
static const float AO_LEVEL[4] = { 0.50f, 0.68f, 0.84f, 1.0f };

const char* blockName(uint8_t b)
{
    static const char* n[] = { "Aire", "Cesped", "Tierra", "Piedra", "Arena", "Agua", "Madera", "Hojas",
                               "Tablones", "Adoquin", "Ladrillos", "Nieve", "Grava", "Carbon", "Hierro",
                               "Lava", "Obsidiana", "Vidrio", "Roca madre" };
    return b < BLOCK_COUNT ? n[b] : "?";
}

int blockTile(uint8_t b, int f, uint8_t meta)
{
    switch (b)
    {
    case GRASS:    return f == 2 ? TL_GRASS_TOP : (f == 3 ? TL_DIRT : TL_GRASS_SIDE);
    case DIRT:     return TL_DIRT;
    case STONE:    return TL_STONE;
    case SAND:     return TL_SAND;
    case WATER:    return (f == 2 || f == 3) ? (meta == 0 ? TL_WATER_STILL : TL_WATER_FLOW) : TL_WATER_FLOW;
    case WOOD:
    {
        bool end = meta == 1 ? (f == 0 || f == 1) : (meta == 2 ? (f == 4 || f == 5) : (f == 2 || f == 3));
        return end ? TL_LOG_TOP : TL_LOG_SIDE;
    }
    case LEAVES:   return TL_LEAVES;
    case PLANKS:   return TL_PLANKS;
    case COBBLE:   return TL_COBBLE;
    case BRICKS:   return TL_BRICKS;
    case SNOW:     return TL_SNOW;
    case GRAVEL:   return TL_GRAVEL;
    case COAL:     return TL_COAL;
    case IRON:     return TL_IRON;
    case LAVA:     return (f == 2 || f == 3) ? (meta == 0 ? TL_LAVA_STILL : TL_LAVA_FLOW) : TL_LAVA_FLOW;
    case OBSIDIAN: return TL_OBSIDIAN;
    case GLASS:    return TL_GLASS;
    case BEDROCK:  return TL_BEDROCK;
    }
    return TL_STONE;
}

float blockHardness(uint8_t b)
{
    switch (b)
    {
    case LEAVES: return 0.3f;
    case GLASS: return 0.45f;
    case SNOW: return 0.6f;
    case DIRT: return 0.75f;
    case SAND: return 0.75f;
    case GRASS: return 0.9f;
    case GRAVEL: return 0.9f;
    case WOOD: return 3.0f;
    case PLANKS: return 3.0f;
    case STONE: return 6.0f;
    case COBBLE: return 7.5f;
    case BRICKS: return 7.5f;
    case COAL: return 9.0f;
    case IRON: return 11.0f;
    case OBSIDIAN: return 40.0f;
    }
    return -1.0f;
}

uint8_t blockDrop(uint8_t b)
{
    switch (b)
    {
    case STONE: return COBBLE;
    case GRASS: return DIRT;
    case LEAVES: case GLASS: case WATER: case LAVA: case BEDROCK: case AIR: return AIR;
    }
    return b;
}

World::~World()
{
    for (auto& kv : chunks) delete kv.second;
}

Chunk* World::getChunk(int cx, int cz) const
{
    auto it = chunks.find(keyOf(cx, cz));
    return it == chunks.end() ? nullptr : it->second;
}

void World::removeChunk(int cx, int cz)
{
    auto it = chunks.find(keyOf(cx, cz));
    if (it == chunks.end()) return;
    delete it->second;
    chunks.erase(it);
}

uint8_t World::getBlock(int x, int y, int z) const
{
    if (y < 0) return STONE;
    if (y >= CHUNK_H) return AIR;
    Chunk* c = getChunk(chunkCord(x), chunkCord(z));
    if (!c) return AIR;
    return c->blocks[IDX(x & 15, y, z & 15)];
}

uint8_t World::getMeta(int x, int y, int z) const
{
    if (y < 0 || y >= CHUNK_H) return 0;
    Chunk* c = getChunk(chunkCord(x), chunkCord(z));
    if (!c) return 0;
    return c->meta[IDX(x & 15, y, z & 15)];
}

void World::touchChunks(Chunk* c, int lx, int lz)
{
    c->modified = true;
    c->dirty = true;
    auto mark = [&](int cx, int cz) { if (Chunk* n = getChunk(cx, cz)) n->dirty = true; };
    const bool w = lx == 0, e = lx == 15, s = lz == 0, n = lz == 15;
    if (w) mark(c->cx - 1, c->cz);
    if (e) mark(c->cx + 1, c->cz);
    if (s) mark(c->cx, c->cz - 1);
    if (n) mark(c->cx, c->cz + 1);
    if (w && s) mark(c->cx - 1, c->cz - 1);
    if (w && n) mark(c->cx - 1, c->cz + 1);
    if (e && s) mark(c->cx + 1, c->cz - 1);
    if (e && n) mark(c->cx + 1, c->cz + 1);
}

void World::setBlock(int x, int y, int z, uint8_t b, uint8_t m)
{
    if (y <= 0 || y >= CHUNK_H) return;
    Chunk* c = getChunk(chunkCord(x), chunkCord(z));
    if (!c) return;
    int lx = x & 15, lz = z & 15;
    c->blocks[IDX(lx, y, lz)] = b;
    c->meta[IDX(lx, y, lz)] = m;
    if (b != AIR && y > c->maxY) c->maxY = y;
    touchChunks(c, lx, lz);
    scheduleAround(x, y, z);
}

void World::setMeta(int x, int y, int z, uint8_t m)
{
    if (y <= 0 || y >= CHUNK_H) return;
    Chunk* c = getChunk(chunkCord(x), chunkCord(z));
    if (!c) return;
    int lx = x & 15, lz = z & 15;
    c->meta[IDX(lx, y, lz)] = m;
    touchChunks(c, lx, lz);
    scheduleAround(x, y, z);
}

void World::schedule(int x, int y, int z)
{
    if (y < 0 || y >= CHUNK_H) return;
    uint8_t b = getBlock(x, y, z);
    double delay;
    if (b == WATER) delay = 0.25;
    else if (b == LAVA) delay = 1.5;
    else if (isGravity(b)) delay = 0.08;
    else return;
    if (!queued.insert(posKey(x, y, z)).second) return;
    tasks.push_back({ x, y, z, clock + delay });
}

void World::scheduleAround(int x, int y, int z)
{
    schedule(x, y, z);
    schedule(x + 1, y, z); schedule(x - 1, y, z);
    schedule(x, y + 1, z); schedule(x, y - 1, z);
    schedule(x, y, z + 1); schedule(x, y, z - 1);
}

void World::tick(double dt)
{
    clock += dt;
    if (tasks.empty()) return;
    std::vector<Task> due;
    size_t w = 0;
    for (size_t i = 0; i < tasks.size(); i++)
    {
        if (tasks[i].due <= clock && due.size() < 512) due.push_back(tasks[i]);
        else tasks[w++] = tasks[i];
    }
    tasks.resize(w);
    for (const Task& t : due) queued.erase(posKey(t.x, t.y, t.z));
    for (const Task& t : due) runTask(t);
}

void World::runTask(const Task& t)
{
    uint8_t b = getBlock(t.x, t.y, t.z);
    if (isFluid(b)) { updateFluid(t.x, t.y, t.z); return; }
    if (isGravity(b) && t.y > 0)
    {
        uint8_t below = getBlock(t.x, t.y - 1, t.z);
        if (below == AIR || isFluid(below))
        {
            setBlock(t.x, t.y, t.z, AIR);
            if (onFall) onFall(t.x, t.y, t.z, b);
        }
    }
}

void World::updateFluid(int x, int y, int z)
{
    const uint8_t t = getBlock(x, y, z);
    if (!isFluid(t)) return;
    uint8_t m = getMeta(x, y, z);
    const int maxL = (t == WATER) ? 7 : 3;
    static const int DX[4] = { 1, -1, 0, 0 };
    static const int DZ[4] = { 0, 0, 1, -1 };

    if (t == LAVA)
    {
        bool touch = getBlock(x, y + 1, z) == WATER;
        for (int i = 0; i < 4 && !touch; i++)
            if (getBlock(x + DX[i], y, z + DZ[i]) == WATER) touch = true;
        if (touch) { setBlock(x, y, z, m == 0 ? (uint8_t)OBSIDIAN : (uint8_t)COBBLE); return; }
        if (m != 0 && getBlock(x, y - 1, z) == WATER) { setBlock(x, y, z, STONE); return; }
    }

    if (m != 0)
    {
        int newM = -1;
        if (getBlock(x, y + 1, z) == t) newM = FLUID_FALL;
        else
        {
            int best = 99;
            for (int i = 0; i < 4; i++)
            {
                int nx = x + DX[i], nz = z + DZ[i];
                if (getBlock(nx, y, nz) != t) continue;
                uint8_t nm = getMeta(nx, y, nz);
                int cand;
                if (nm == 0) cand = 1;
                else if (nm & FLUID_FALL) cand = (getBlock(nx, y - 1, nz) != AIR) ? 1 : 99;
                else cand = (nm & 7) + 1;
                if (cand < best) best = cand;
            }
            if (best <= maxL) newM = best;
        }
        if (t == WATER)
        {
            int src = 0;
            for (int i = 0; i < 4; i++)
                if (getBlock(x + DX[i], y, z + DZ[i]) == WATER && getMeta(x + DX[i], y, z + DZ[i]) == 0) src++;
            uint8_t bl = getBlock(x, y - 1, z);
            if (src >= 2 && (isSolid(bl) || (bl == WATER && getMeta(x, y - 1, z) == 0))) newM = 0;
        }
        if (newM < 0) { setBlock(x, y, z, AIR); return; }
        if (newM != (int)m) { setMeta(x, y, z, (uint8_t)newM); m = (uint8_t)newM; }
    }

    uint8_t bl = getBlock(x, y - 1, z);
    if (bl == AIR) setBlock(x, y - 1, z, t, FLUID_FALL);
    else if (bl == t)
    {
        uint8_t bm = getMeta(x, y - 1, z);
        if (bm != 0 && !(bm & FLUID_FALL)) setMeta(x, y - 1, z, FLUID_FALL);
    }

    if (m == 0 || bl != AIR)
    {
        int nl = (m & FLUID_FALL) ? 1 : (m & 7) + 1;
        if (nl <= maxL)
        {
            for (int i = 0; i < 4; i++)
            {
                int nx = x + DX[i], nz = z + DZ[i];
                uint8_t nb = getBlock(nx, y, nz);
                if (nb == AIR) setBlock(nx, y, nz, t, (uint8_t)nl);
                else if (nb == t)
                {
                    uint8_t om = getMeta(nx, y, nz);
                    if (om != 0 && !(om & FLUID_FALL) && (int)(om & 7) > nl) setMeta(nx, y, nz, (uint8_t)nl);
                }
            }
        }
    }
}

void World::flowVector(int x, int y, int z, float& fx, float& fz) const
{
    fx = 0.0f; fz = 0.0f;
    const uint8_t t = getBlock(x, y, z);
    if (!isFluid(t)) return;
    static const int DX[4] = { 1, -1, 0, 0 };
    static const int DZ[4] = { 0, 0, 1, -1 };
    auto lv = [](uint8_t m) { return (m & FLUID_FALL) ? 0 : (int)(m & 7); };
    const int me = lv(getMeta(x, y, z));
    for (int i = 0; i < 4; i++)
    {
        const int nx = x + DX[i], nz = z + DZ[i];
        const uint8_t nb = getBlock(nx, y, nz);
        int d = 0;
        if (nb == t) d = lv(getMeta(nx, y, nz)) - me;
        else if (!isSolid(nb) && !isFluid(nb) && !isSolid(getBlock(nx, y - 1, nz))) d = 6;
        fx += (float)(DX[i] * d);
        fz += (float)(DZ[i] * d);
    }
    const float len = std::sqrt(fx * fx + fz * fz);
    if (len > 0.0f) { fx /= len; fz /= len; }
}

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float sstep(float a, float b, float x)
{
    float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static float spline(float x, const float p[][2], int n)
{
    if (x <= p[0][0]) return p[0][1];
    for (int i = 1; i < n; i++)
        if (x <= p[i][0])
        {
            float t = (x - p[i - 1][0]) / (p[i][0] - p[i - 1][0]);
            return p[i - 1][1] + (p[i][1] - p[i - 1][1]) * t;
        }
    return p[n - 1][1];
}

static const float CONT[][2] = {
    { -1.00f, 10.0f }, { -0.60f, 22.0f }, { -0.30f, SEA_LEVEL - 6.0f }, { -0.15f, SEA_LEVEL - 1.0f },
    { -0.05f, SEA_LEVEL + 2.0f }, { 0.20f, SEA_LEVEL + 8.0f }, { 0.60f, SEA_LEVEL + 16.0f }, { 1.00f, SEA_LEVEL + 24.0f }
};
static const int CONT_N = 8;

struct Sample { int h; float t, m; };

static Sample sampleColumn(uint32_t seed, int xi, int zi)
{
    const float x = (float)xi, z = (float)zi;

    float wx = x + Noise::fbm(x * 0.004f, z * 0.004f, 3, seed + 11) * 70.0f;
    float wz = z + Noise::fbm(x * 0.004f + 37.2f, z * 0.004f - 91.7f, 3, seed + 12) * 70.0f;

    float c = clampf(Noise::fbm(wx * 0.0015f, wz * 0.0015f, 5, seed) * 1.7f, -1.0f, 1.0f);
    float h = spline(c, CONT, CONT_N);

    float land = sstep(-0.15f, 0.10f, c);
    h += Noise::fbm(wx * 0.012f, wz * 0.012f, 4, seed + 3) * 9.0f * (0.35f + 0.65f * land);

    float peaks = clampf(Noise::fbm(wx * 0.0028f, wz * 0.0028f, 2, seed + 5) * 2.0f + 0.1f, 0.0f, 1.0f);
    float mount = sstep(0.28f, 0.80f, peaks) * sstep(-0.05f, 0.30f, c);
    float ridge = Noise::ridged(wx * 0.0055f, wz * 0.0055f, 5, seed + 9);
    h += mount * (ridge * 62.0f + 3.0f);

    float rn = std::fabs(Noise::fbm(wx * 0.0030f + 500.0f, wz * 0.0030f - 500.0f, 3, seed + 21));
    float w = 0.028f * sstep(-0.18f, 0.0f, c) * (1.0f - 0.85f * mount);
    if (rn < w)
    {
        float t = rn / w; t = t * t * (3.0f - 2.0f * t);
        float bed = (float)(SEA_LEVEL - 3) + 2.0f * Noise::fbm(wx * 0.05f, wz * 0.05f, 2, seed + 22);
        if (h > bed) h = bed + (h - bed) * t;
    }

    Sample s;
    float t = clampf(Noise::fbm(wx * 0.0011f - 300.0f, wz * 0.0011f + 300.0f, 3, seed + 41) * 2.0f, -1.0f, 1.0f);
    t -= std::max(0.0f, h - (SEA_LEVEL + 18)) * 0.011f;
    s.t = t;
    s.m = clampf(Noise::fbm(wx * 0.0013f + 800.0f, wz * 0.0013f + 100.0f, 3, seed + 43) * 2.0f, -1.0f, 1.0f);
    s.h = std::min(CHUNK_H - 16, std::max(3, (int)h));
    return s;
}

int World::heightAt(int x, int z) const { return sampleColumn(seed, x, z).h; }

static void warpXZ(uint32_t seed, float x, float z, float& wx, float& wz)
{
    wx = x + Noise::fbm(x * 0.004f, z * 0.004f, 3, seed + 11) * 70.0f;
    wz = z + Noise::fbm(x * 0.004f + 37.2f, z * 0.004f - 91.7f, 3, seed + 12) * 70.0f;
}

static float contAt(uint32_t seed, float x, float z)
{
    float wx, wz;
    warpXZ(seed, x, z, wx, wz);
    return clampf(Noise::fbm(wx * 0.0015f, wz * 0.0015f, 5, seed) * 1.7f, -1.0f, 1.0f);
}

static float riverAt(uint32_t seed, float x, float z)
{
    float wx, wz;
    warpXZ(seed, x, z, wx, wz);
    return Noise::fbm(wx * 0.0030f + 500.0f, wz * 0.0030f - 500.0f, 3, seed + 21);
}

bool World::riverCurrent(int xi, int zi, float& fx, float& fz) const
{
    fx = 0.0f; fz = 0.0f;
    const float x = (float)xi, z = (float)zi;
    const float c0 = contAt(seed, x, z);
    if (c0 < -0.12f) return false;
    const float r0 = riverAt(seed, x, z);
    const float w = 0.028f * sstep(-0.18f, 0.0f, c0);
    if (w <= 0.0f || std::fabs(r0) >= w) return false;
    const float e = 3.0f;
    const float gx = riverAt(seed, x + e, z) - riverAt(seed, x - e, z);
    const float gz = riverAt(seed, x, z + e) - riverAt(seed, x, z - e);
    const float gl = std::sqrt(gx * gx + gz * gz);
    if (gl < 1e-6f) return false;
    float tx = -gz / gl, tz = gx / gl;
    const float ce = 40.0f;
    const float cgx = contAt(seed, x + ce, z) - contAt(seed, x - ce, z);
    const float cgz = contAt(seed, x, z + ce) - contAt(seed, x, z - ce);
    if (tx * cgx + tz * cgz > 0.0f) { tx = -tx; tz = -tz; }
    const float s = 0.45f + 0.55f * (1.0f - std::fabs(r0) / w);
    fx = tx * s; fz = tz * s;
    return true;
}

struct Surf { uint8_t top, sub; int depth; bool pine; float density; };

static Surf surfaceFor(const Sample& s, int slope, int x, int z, uint32_t seed)
{
    Surf r = { GRASS, DIRT, 3, false, 0.006f };
    const int h = s.h;

    if (h <= SEA_LEVEL + 1)
    {
        bool patch = Noise::fbm(x * 0.04f, z * 0.04f, 2, seed + 61) > 0.12f;
        if (h < SEA_LEVEL - 2 && patch) { r.top = GRAVEL; r.sub = GRAVEL; }
        else { r.top = SAND;   r.sub = SAND; }
        r.depth = 3; r.density = 0.0f;
        return r;
    }
    if (s.t < -0.35f)
    {
        r.top = SNOW; r.sub = (h > 75) ? (uint8_t)STONE : (uint8_t)DIRT; r.pine = true;
        r.density = (s.m > -0.2f) ? 0.02f : 0.004f;
        if (h > 80) r.density = 0.0f;
    }
    else if (s.t > 0.30f && s.m < 0.0f)
    {
        r.top = SAND; r.sub = SAND; r.depth = 4; r.density = 0.0f;
    }
    else
    {
        r.density = (s.m > 0.10f) ? 0.045f : 0.006f;
        if (s.t > 0.2f) r.density *= 0.5f;
    }
    if (slope >= 3 && h > SEA_LEVEL + 6)
    {
        r.top = STONE; r.sub = STONE; r.depth = 0; r.density = 0.0f;
    }
    return r;
}

static uint8_t oreAt(uint32_t seed, int x, int y, int z)
{
    int cx = x >> 2, cy = y >> 2, cz = z >> 2;
    float lx = (float)(x & 3) + 0.5f, ly = (float)(y & 3) + 0.5f, lz = (float)(z & 3) + 0.5f;

    uint32_t h1 = Noise::hash(cx + cy * 131, cz, seed + 301);
    if ((h1 & 0xFF) < 18 && y < 100)
    {
        float dx = lx - (float)((h1 >> 8) & 3) - 0.5f, dy = ly - (float)((h1 >> 10) & 3) - 0.5f, dz = lz - (float)((h1 >> 12) & 3) - 0.5f;
        if (dx * dx + dy * dy + dz * dz <= 2.6f) return COAL;
    }
    uint32_t h2 = Noise::hash(cx + cy * 131, cz, seed + 302);
    if ((h2 & 0xFF) < 9 && y < 64)
    {
        float dx = lx - (float)((h2 >> 8) & 3) - 0.5f, dy = ly - (float)((h2 >> 10) & 3) - 0.5f, dz = lz - (float)((h2 >> 12) & 3) - 0.5f;
        if (dx * dx + dy * dy + dz * dz <= 2.2f) return IRON;
    }
    return STONE;
}

struct Grid
{
    float v[5][CHUNK_H / 4 + 1][5];
    float at(int lx, int y, int lz) const
    {
        int i = lx >> 2, j = y >> 2, k = lz >> 2;
        float fx = (lx & 3) * 0.25f, fy = (y & 3) * 0.25f, fz = (lz & 3) * 0.25f;
        auto L = [](float a, float b, float t) { return a + (b - a) * t; };
        float c00 = L(v[i][j][k], v[i + 1][j][k], fx), c10 = L(v[i][j][k + 1], v[i + 1][j][k + 1], fx);
        float c01 = L(v[i][j + 1][k], v[i + 1][j + 1][k], fx), c11 = L(v[i][j + 1][k + 1], v[i + 1][j + 1][k + 1], fx);
        return L(L(c00, c10, fz), L(c01, c11, fz), fy);
    }
};

Chunk* World::generateChunk(int cx, int cz)
{
    Chunk* c = new Chunk();
    std::memset(c->blocks, 0, sizeof(c->blocks));
    std::memset(c->meta, 0, sizeof(c->meta));
    c->cx = cx; c->cz = cz;

    const int B = 2;
    const int S = CHUNK_W + 2 * B;
    Sample smp[S][S];
    Surf   sf[S][S];

    for (int j = 0; j < S; j++)
        for (int i = 0; i < S; i++)
            smp[j][i] = sampleColumn(seed, cx * CHUNK_W - B + i, cz * CHUNK_W - B + j);

    for (int j = 0; j < S; j++)
        for (int i = 0; i < S; i++)
        {
            int h = smp[j][i].h, slope = 0;
            const int nj[4] = { j, j, std::max(0, j - 1), std::min(S - 1, j + 1) };
            const int ni[4] = { std::max(0, i - 1), std::min(S - 1, i + 1), i, i };
            for (int k = 0; k < 4; k++) slope = std::max(slope, std::abs(h - smp[nj[k]][ni[k]].h));
            sf[j][i] = surfaceFor(smp[j][i], slope, cx * CHUNK_W - B + i, cz * CHUNK_W - B + j, seed);
        }

    int maxY = 0, maxH = 0;

    for (int z = 0; z < CHUNK_W; z++)
        for (int x = 0; x < CHUNK_W; x++)
        {
            int h = smp[z + B][x + B].h;
            const Surf& s = sf[z + B][x + B];
            const int wx = cx * CHUNK_W + x, wz = cz * CHUNK_W + z;
            for (int y = 0; y <= h; y++)
            {
                uint8_t b = (y == h) ? s.top : (y > h - s.depth ? s.sub : (uint8_t)STONE);
                if (y == h && s.depth == 0) b = s.top;
                if (y == 0) b = BEDROCK;
                else if (y == 1 && Noise::rnd(wx, wz, seed + 7) < 0.6f) b = BEDROCK;
                else if (y == 2 && Noise::rnd(wx, wz, seed + 8) < 0.3f) b = BEDROCK;
                c->blocks[IDX(x, y, z)] = b;
            }
            for (int y = h + 1; y <= SEA_LEVEL; y++) c->blocks[IDX(x, y, z)] = WATER;
            maxH = std::max(maxH, h);
            maxY = std::max(maxY, std::max(h, h < SEA_LEVEL ? SEA_LEVEL : h));
        }

    {
        static thread_local Grid gA, gB, gC;
        const int ny = (maxH >> 2) + 2;
        for (int i = 0; i < 5; i++)
            for (int k = 0; k < 5; k++)
                for (int j = 0; j < ny && j < CHUNK_H / 4 + 1; j++)
                {
                    float x = (float)(cx * CHUNK_W + i * 4), y = (float)(j * 4), z = (float)(cz * CHUNK_W + k * 4);
                    gA.v[i][j][k] = Noise::perlin3(x * 0.018f, y * 0.024f, z * 0.018f, seed + 201);
                    gB.v[i][j][k] = Noise::perlin3(x * 0.018f + 91.0f, y * 0.024f, z * 0.018f - 47.0f, seed + 202);
                    gC.v[i][j][k] = Noise::fbm3(x * 0.011f, y * 0.020f, z * 0.011f, 2, seed + 203);
                }

        for (int z = 0; z < CHUNK_W; z++)
            for (int x = 0; x < CHUNK_W; x++)
            {
                const Sample& sm = smp[z + B][x + B];
                const Surf& s = sf[z + B][x + B];
                int h = sm.h;
                int limit = h - 4;
                if (s.top == STONE && h > SEA_LEVEL + 2) limit = h;
                for (int y = 3; y <= limit; y++)
                {
                    uint8_t& blk = c->blocks[IDX(x, y, z)];
                    if (blk == AIR || blk == WATER) continue;
                    float a = gA.at(x, y, z), b = gB.at(x, y, z), cc = gC.at(x, y, z);
                    float thr = 0.55f - 0.15f * clampf((50.0f - y) / 50.0f, 0.0f, 1.0f);
                    if (a * a + b * b < 0.0065f || cc > thr)
                    {
                        if (isGravity(c->blocks[IDX(x, y + 1, z)])) continue;
                        blk = (y <= LAVA_LEVEL) ? (uint8_t)LAVA : (uint8_t)AIR;
                        continue;
                    }
                    if (blk == STONE && y < h - 3) blk = oreAt(seed, cx * CHUNK_W + x, y, cz * CHUNK_W + z);
                }
            }
    }

    auto put = [&](int wx, int y, int wz, uint8_t b, bool overLeaves)
        {
            int lx = wx - cx * CHUNK_W, lz = wz - cz * CHUNK_W;
            if (lx < 0 || lx >= CHUNK_W || lz < 0 || lz >= CHUNK_W || y < 0 || y >= CHUNK_H) return;
            uint8_t& cur = c->blocks[IDX(lx, y, lz)];
            if (cur == AIR || (overLeaves && cur == LEAVES)) { cur = b; maxY = std::max(maxY, y); }
        };

    for (int j = 0; j < S; j++)
        for (int i = 0; i < S; i++)
        {
            const Surf& s = sf[j][i];
            if (s.density <= 0.0f) continue;
            int wx = cx * CHUNK_W - B + i, wz = cz * CHUNK_W - B + j;
            float clump = Noise::fbm(wx * 0.03f, wz * 0.03f, 2, seed + 55);
            float prob = s.density * (clump > 0.0f ? 2.0f : 0.35f);
            if (Noise::rnd(wx, wz, seed + 999) >= prob) continue;

            int h = smp[j][i].h;
            if (s.pine)
            {
                int topY = h + 6 + (int)(Noise::hash(wx, wz, seed + 5) % 4);
                if (topY + 3 >= CHUNK_H) continue;
                for (int ly = topY + 1; ly >= topY - 5; ly--)
                {
                    int k = topY + 1 - ly;
                    int r = k == 0 ? 0 : std::min(2, (k + 1) / 2);
                    for (int dz = -r; dz <= r; dz++)
                        for (int dx = -r; dx <= r; dx++)
                        {
                            if (r == 2 && std::abs(dx) == 2 && std::abs(dz) == 2) continue;
                            put(wx + dx, ly, wz + dz, LEAVES, false);
                        }
                }
                for (int y = h + 1; y <= topY; y++) put(wx, y, wz, WOOD, true);
            }
            else
            {
                int trunk = 4 + (int)(Noise::hash(wx, wz, seed + 5) % 3);
                int topY = h + trunk;
                if (topY + 2 >= CHUNK_H) continue;
                for (int ly = topY - 2; ly <= topY + 1; ly++)
                {
                    int r = (ly >= topY) ? 1 : 2;
                    for (int dz = -r; dz <= r; dz++)
                        for (int dx = -r; dx <= r; dx++)
                        {
                            if (std::abs(dx) == r && std::abs(dz) == r)
                            {
                                if (ly >= topY) continue;
                                if (Noise::rnd(wx + dx, wz + dz + ly * 7, seed + 77) < 0.5f) continue;
                            }
                            put(wx + dx, ly, wz + dz, LEAVES, false);
                        }
                }
                for (int y = h + 1; y <= topY; y++) put(wx, y, wz, WOOD, true);
            }
        }

    c->maxY = maxY;
    c->dirty = true;
    chunks[keyOf(cx, cz)] = c;
    return c;
}

void World::buildMesh(Chunk& c)
{
    c.meshOpaque.clear();
    c.meshWater.clear();
    const int topY = std::min(c.maxY + 1, CHUNK_H - 1);

    auto get = [&](int lx, int y, int lz) -> uint8_t
        {
            if (y < 0) return STONE;
            if (y >= CHUNK_H) return AIR;
            if (lx >= 0 && lx < CHUNK_W && lz >= 0 && lz < CHUNK_W) return c.blocks[IDX(lx, y, lz)];
            return getBlock(c.cx * CHUNK_W + lx, y, c.cz * CHUNK_W + lz);
        };

    auto getM = [&](int lx, int y, int lz) -> uint8_t
        {
            if (y < 0 || y >= CHUNK_H) return 0;
            if (lx >= 0 && lx < CHUNK_W && lz >= 0 && lz < CHUNK_W) return c.meta[IDX(lx, y, lz)];
            return getMeta(c.cx * CHUNK_W + lx, y, c.cz * CHUNK_W + lz);
        };

    auto levelH = [&](uint8_t t, uint8_t m) -> float
        {
            if (m & FLUID_FALL) return 1.0f;
            return 0.89f - (float)(m & 7) * (t == LAVA ? 0.22f : 0.11f);
        };

    auto cornerH = [&](int x, int y, int z, uint8_t t, int ox, int oz) -> float
        {
            const int sx = ox ? 1 : -1, sz = oz ? 1 : -1;
            float sum = 0.0f, wsum = 0.0f;
            for (int i = 0; i < 4; i++)
            {
                int nx = x + ((i & 1) ? sx : 0), nz = z + ((i & 2) ? sz : 0);
                uint8_t nb = get(nx, y, nz);
                if (nb == t)
                {
                    if (get(nx, y + 1, nz) == t) return 1.0f;
                    uint8_t m = getM(nx, y, nz);
                    float wt = (m == 0) ? 10.0f : 1.0f;
                    sum += levelH(t, m) * wt;
                    wsum += wt;
                }
                else if (!isSolid(nb)) wsum += 1.0f;
            }
            return wsum > 0.0f ? sum / wsum : 0.0f;
        };

    const float ts = 1.0f / ATLAS_N, e = 0.0004f;

    for (int y = 0; y <= topY; y++)
        for (int z = 0; z < CHUNK_W; z++)
            for (int x = 0; x < CHUNK_W; x++)
            {
                uint8_t b = c.blocks[IDX(x, y, z)];
                if (b == AIR) continue;
                const bool fl = isFluid(b);
                const uint8_t m = c.meta[IDX(x, y, z)];

                float ch[4] = { 1, 1, 1, 1 };
                if (fl)
                    for (int oz = 0; oz < 2; oz++)
                        for (int ox = 0; ox < 2; ox++)
                            ch[ox + oz * 2] = cornerH(x, y, z, b, ox, oz);

                for (int f = 0; f < 6; f++)
                {
                    const int* n = FACE_N[f];
                    uint8_t nb = get(x + n[0], y + n[1], z + n[2]);
                    bool vis;
                    if (fl) vis = !(nb == b || isOpaque(nb));
                    else if (b == GLASS) vis = !(nb == GLASS || isOpaque(nb));
                    else vis = !isOpaque(nb);
                    if (!vis) continue;

                    int tile = blockTile(b, f, m);
                    int rot = 0;
                    if (fl && f == 2)
                    {
                        float fvx, fvz;
                        flowVector(c.cx * CHUNK_W + x, y, c.cz * CHUNK_W + z, fvx, fvz);
                        if (std::fabs(fvx) < 0.01f && std::fabs(fvz) < 0.01f) tile = (b == WATER) ? TL_WATER_STILL : TL_LAVA_STILL;
                        else
                        {
                            tile = (b == WATER) ? TL_WATER_FLOW : TL_LAVA_FLOW;
                            if (std::fabs(fvz) >= std::fabs(fvx)) rot = fvz > 0.0f ? 0 : 2;
                            else rot = fvx > 0.0f ? 3 : 1;
                        }
                    }
                    float tu = (tile % ATLAS_N) * ts, tv = (tile / ATLAS_N) * ts;
                    const float uu0[4] = { tu + e, tu + ts - e, tu + ts - e, tu + e };
                    const float vv0[4] = { tv + ts - e, tv + ts - e, tv + e, tv + e };
                    if (b == WOOD && ((m == 1 && f >= 2) || (m == 2 && f < 2))) rot = 1;
                    float uu[4], vv[4];
                    for (int i = 0; i < 4; i++) { uu[i] = uu0[(i + rot) & 3]; vv[i] = vv0[(i + rot) & 3]; }

                    int ao[4] = { 3, 3, 3, 3 };
                    if (!fl)
                    {
                        for (int i = 0; i < 4; i++)
                        {
                            const int* o = FACE_V[f][i];
                            int t[3] = { n[0] ? 0 : (o[0] ? 1 : -1),
                                         n[1] ? 0 : (o[1] ? 1 : -1),
                                         n[2] ? 0 : (o[2] ? 1 : -1) };
                            int a = -1, bb = -1;
                            for (int k = 0; k < 3; k++) if (t[k] != 0) { if (a < 0) a = k; else bb = k; }
                            int base[3] = { x + n[0], y + n[1], z + n[2] };
                            int s1[3] = { base[0], base[1], base[2] }; s1[a] += t[a];
                            int s2[3] = { base[0], base[1], base[2] }; s2[bb] += t[bb];
                            int cc[3] = { base[0], base[1], base[2] }; cc[a] += t[a]; cc[bb] += t[bb];
                            int A = isOpaque(get(s1[0], s1[1], s1[2])) ? 1 : 0;
                            int B = isOpaque(get(s2[0], s2[1], s2[2])) ? 1 : 0;
                            int C = isOpaque(get(cc[0], cc[1], cc[2])) ? 1 : 0;
                            ao[i] = (A && B) ? 0 : 3 - (A + B + C);
                        }
                    }

                    Vertex q[4];
                    for (int i = 0; i < 4; i++)
                    {
                        const int* o = FACE_V[f][i];
                        float shade = fl ? (b == LAVA ? 1.0f : FACE_SHADE[f]) : FACE_SHADE[f] * AO_LEVEL[ao[i]];
                        uint8_t col = (uint8_t)(shade * 255.0f);
                        float py = o[1] ? (float)y + (fl ? ch[o[0] + o[2] * 2] : 1.0f) : (float)y;
                        q[i] = { (float)(x + o[0]), py, (float)(z + o[2]),
                                 uu[i], vv[i], col, col, col, (uint8_t)(b == WATER ? 190 : 255) };
                    }

                    std::vector<Vertex>& out = (b == WATER) ? c.meshWater : c.meshOpaque;
                    if (!fl && ao[0] + ao[2] < ao[1] + ao[3])
                    {
                        out.push_back(q[1]); out.push_back(q[2]); out.push_back(q[3]); out.push_back(q[0]);
                    }
                    else
                    {
                        out.push_back(q[0]); out.push_back(q[1]); out.push_back(q[2]); out.push_back(q[3]);
                    }
                }
            }
}