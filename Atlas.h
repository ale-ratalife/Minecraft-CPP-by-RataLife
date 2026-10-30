#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "Noise.h"
#include "World.h"

namespace AtlasGen
{
    struct Col { float r, g, b, a; };

    constexpr bool MIRROR = true;

    inline int fx(int x) { return MIRROR ? std::min(x, 15 - x) : x; }
    inline float rn(int x, int y, uint32_t s) { return Noise::rnd(fx(x), y, s); }

    // Función nueva para simular los "clumps" de píxeles del arte original
    inline float chunky(int x, int y, uint32_t s) {
        return rn(x / 2, y / 2, s) * 0.75f + rn(x, y, s + 1) * 0.25f;
    }

    inline Col pal(const Col* p, int n, float v)
    {
        int i = (int)(v * n);
        if (i >= n) i = n - 1;
        if (i < 0) i = 0;
        return p[i];
    }

    inline Col mix(Col a, Col b, float t)
    {
        return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
    }

    inline Col shadeCol(Col c, float d) { return { c.r + d, c.g + d, c.b + d, c.a }; }

    inline uint8_t c8(float v) { return (uint8_t)std::min(255.0f, std::max(0.0f, v)); }

    inline void put(uint8_t* t, int x, int y, Col c)
    {
        uint8_t* p = t + (y * TILE_PX + x) * 4;
        p[0] = c8(c.r); p[1] = c8(c.g); p[2] = c8(c.b); p[3] = c8(c.a);
    }

    inline Col grassAt(int x, int y)
    {
        static const Col P[] = { {89,137,45,255}, {103,150,58,255}, {117,164,73,255}, {128,174,82,255} };
        return pal(P, 4, chunky(x, y, 11));
    }

    inline Col dirtAt(int x, int y)
    {
        static const Col P[] = { {104,73,46,255}, {119,85,58,255}, {134,96,67,255}, {150,111,79,255} };
        float v = chunky(x, y, 42);
        if (rn(x, y, 99) < 0.04f) return { 135, 135, 135, 255 };
        if (rn(x, y, 100) < 0.02f) return { 110, 110, 110, 255 };
        return pal(P, 4, v);
    }

    inline Col stoneAt(int x, int y)
    {
        static const Col P[] = { {100,100,100,255}, {115,115,115,255}, {130,130,130,255}, {145,145,145,255} };
        return pal(P, 4, chunky(x, y, 15));
    }

    inline bool inBlob(int x, int y)
    {
        static const int B[][4] = { {1,2,3,2}, {5,5,2,3}, {2,9,3,2}, {6,12,2,2}, {4,0,2,1}, {0,14,2,2} };
        int f = fx(x);
        for (const auto& b : B)
            if (f >= b[0] && f < b[0] + b[2] && y >= b[1] && y < b[1] + b[3]) return true;
        return false;
    }

    inline void paintTile(uint8_t* t, int tile)
    {
        for (int y = 0; y < TILE_PX; y++)
            for (int x = 0; x < TILE_PX; x++)
            {
                Col c = { 0, 0, 0, 0 };
                switch (tile)
                {
                case TL_GRASS_TOP:
                    c = grassAt(x, y);
                    break;
                case TL_GRASS_SIDE:
                {
                    int edge = 3 + (int)(rn(x, 0, 21) * 1.5f);
                    if (y < edge) c = grassAt(x, y);
                    else if (y == edge && rn(x, y, 22) < 0.4f) c = shadeCol(grassAt(x, y), -15.0f);
                    else c = dirtAt(x, y);
                    break;
                }
                case TL_DIRT:
                    c = dirtAt(x, y);
                    break;
                case TL_STONE:
                    c = stoneAt(x, y);
                    break;
                case TL_SAND:
                {
                    static const Col P[] = { {205,196,146,255}, {212,203,150,255}, {219,211,160,255}, {226,218,170,255} };
                    c = pal(P, 4, chunky(x, y, 4));
                    break;
                }
                case TL_LOG_SIDE:
                {
                    float v = rn(x, y / 4, 8) * 0.7f + rn(x, 0, 9) * 0.3f;
                    static const Col P[] = { {60,45,28,255}, {85,65,40,255}, {100,75,45,255}, {115,85,50,255} };
                    c = pal(P, 4, v);
                    break;
                }
                case TL_LOG_TOP:
                {
                    int dx = (x < 8) ? 7 - x : x - 8;
                    int dy = (y < 8) ? 7 - y : y - 8;
                    int ring = std::max(dx, dy);

                    static const Col BARK = { 60, 45, 28, 255 };
                    static const Col LIGHT = { 175, 143, 85, 255 };
                    static const Col DARK = { 150, 116, 65, 255 };
                    static const Col MID = { 163, 130, 75, 255 };

                    if (ring >= 7) c = BARK;
                    else if (ring == 6) c = DARK;
                    else if (ring % 2 == 1) c = LIGHT;
                    else c = MID;

                    if (ring < 7 && rn(x, y, 9) < 0.3f) c = shadeCol(c, -10.0f);
                    break;
                }
                case TL_LEAVES:
                {
                    static const Col P[] = { {35,105,15,255}, {50,135,25,255}, {65,155,35,255}, {80,170,45,255} };
                    float trans = chunky(x, y, 77);
                    if (trans < 0.35f) c = { 0, 0, 0, 0 };
                    else c = pal(P, 4, chunky(x, y, 2));
                    break;
                }
                case TL_PLANKS:
                {
                    int row = y / 4;
                    bool seamH = (y % 4 == 3);
                    bool seamV = (row % 2 == 0) ? (x == 7 || x == 8) : (x == 3 || x == 12);
                    Col base = (row % 2) ? Col{ 168, 131, 80, 255 } : Col{ 176, 139, 87, 255 };
                    if (seamH || seamV) c = { 118, 88, 48, 255 };
                    else if (rn(x, y, 19) < 0.12f) c = shadeCol(base, -16.0f);
                    else c = base;
                    break;
                }
                case TL_COBBLE:
                {
                    int row = y / 4, off = (row % 2) * 2, f = fx(x);
                    int lx = (f + off) % 4, ly = y % 4;
                    static const Col P[] = { {128,128,128,255}, {112,112,112,255}, {140,140,140,255}, {120,118,116,255} };
                    if (lx == 0 || ly == 0) c = { 72, 72, 72, 255 };
                    else
                    {
                        c = pal(P, 4, Noise::rnd((f + off) / 4, row, 5));
                        if (lx == 1 || ly == 1) c = shadeCol(c, 14.0f);
                        if (lx == 3 || ly == 3) c = shadeCol(c, -12.0f);
                    }
                    break;
                }
                case TL_BRICKS:
                {
                    int row = y / 4;
                    bool seamH = (y % 4 == 3);
                    bool seamV = (row % 2 == 0) ? (x == 7 || x == 8) : (x == 3 || x == 12);
                    static const Col P[] = { {150,74,58,255}, {139,66,50,255}, {160,84,66,255}, {128,58,44,255} };
                    if (seamH || seamV) c = { 188, 182, 170, 255 };
                    else c = pal(P, 4, rn(x, y, 1));
                    break;
                }
                case TL_SNOW:
                {
                    static const Col P[] = { {250,250,250,255}, {240,244,250,255}, {232,238,246,255} };
                    c = pal(P, 3, chunky(x, y, 6));
                    break;
                }
                case TL_GRAVEL:
                {
                    static const Col P[] = { {131,127,126,255}, {150,146,145,255}, {112,108,107,255}, {95,92,92,255}, {170,165,160,255} };
                    c = pal(P, 5, chunky(x, y, 14));
                    break;
                }
                case TL_COAL:
                case TL_IRON:
                {
                    static const Col CoalP[] = { {25,25,27,255}, {40,40,44,255} };
                    static const Col IronP[] = { {216,173,142,255}, {190,142,112,255} };
                    c = inBlob(x, y) ? pal(tile == TL_COAL ? CoalP : IronP, 2, rn(x, y, 77)) : stoneAt(x, y);
                    break;
                }
                case TL_OBSIDIAN:
                {
                    static const Col P[] = { {21,18,30,255}, {30,22,46,255}, {14,11,22,255}, {44,30,70,255} };
                    c = pal(P, 4, chunky(x, y, 32));
                    break;
                }
                case TL_GLASS:
                {
                    bool border = (x == 0 || x == 15 || y == 0 || y == 15);
                    bool hi = (fx(x) == y && y >= 2 && y <= 4);
                    if (border) c = { 210, 235, 245, 255 };
                    else if (hi) c = { 255, 255, 255, 200 };
                    break;
                }
                case TL_BEDROCK:
                {
                    static const Col P[] = { {84,84,84,255}, {60,60,60,255}, {104,104,104,255}, {40,40,40,255} };
                    c = pal(P, 4, chunky(x, y, 52));
                    break;
                }
                default:
                    if (tile >= TL_CRACK && tile < TL_CRACK + 10)
                    {
                        int k = tile - TL_CRACK;
                        float v = Noise::perlin(x * 0.42f + 3.1f, y * 0.42f + 7.7f, 909);
                        if (std::fabs(v) < 0.045f + k * 0.02f) c = { 0, 0, 0, 190 };
                    }
                    break;
                }
                put(t, x, y, c);
            }
    }

    inline float tn(float u, float v, int pu, int pv, uint32_t seed)
    {
        int iu = (int)std::floor(u), iv = (int)std::floor(v);
        float fu = u - iu, fv = v - iv;
        fu = fu * fu * (3.0f - 2.0f * fu);
        fv = fv * fv * (3.0f - 2.0f * fv);
        auto g = [&](int a, int b) { return Noise::rnd(((a % pu) + pu) % pu, ((b % pv) + pv) % pv, seed); };
        float top = g(iu, iv) + (g(iu + 1, iv) - g(iu, iv)) * fu;
        float bot = g(iu, iv + 1) + (g(iu + 1, iv + 1) - g(iu, iv + 1)) * fu;
        return top + (bot - top) * fv;
    }

    inline void animateTile(uint8_t* t, int tile, double time)
    {
        const float tm = (float)time;
        const Col wLo = { 34, 80, 186, 200 };
        const Col wHi = { 64, 120, 242, 200 };
        const Col lDark = { 150, 28, 4, 255 }, lMid = { 226, 82, 8, 255 }, lHi = { 255, 196, 52, 255 };

        for (int y = 0; y < TILE_PX; y++)
            for (int x = 0; x < TILE_PX; x++)
            {
                Col c = { 0, 0, 0, 255 };
                switch (tile)
                {
                case TL_WATER_STILL:
                case TL_WATER_FLOW:
                {
                    float streak = tn(x * 0.5f, y * 1.5f - tm * 2.0f, 8, 8, 31);
                    c = streak > 0.65f ? wHi : wLo;
                    break;
                }
                case TL_LAVA_STILL:
                case TL_LAVA_FLOW:
                {
                    float v = tn(x * 0.25f, (y - tm * 2.5f) * 0.125f, 4, 2, 63) * 0.6f
                        + tn(x * 0.5f + 5.0f, (y - tm * 4.0f) * 0.25f, 8, 4, 64) * 0.4f;
                    c = v < 0.45f ? mix(lDark, lMid, v / 0.45f) : mix(lMid, lHi, (v - 0.45f) / 0.55f);
                    break;
                }
                }
                put(t, x, y, c);
            }
    }

    inline void blit(uint8_t* atlas, const uint8_t* tile, int index)
    {
        int ox = (index % ATLAS_N) * TILE_PX, oy = (index / ATLAS_N) * TILE_PX;
        for (int y = 0; y < TILE_PX; y++)
            std::memcpy(atlas + ((oy + y) * ATLAS_PX + ox) * 4, tile + y * TILE_PX * 4, TILE_PX * 4);
    }
}

inline void animateAtlasTile(uint8_t* tile, int index, double time)
{
    AtlasGen::animateTile(tile, index, time);
}

inline void generateAtlas(uint8_t* px)
{
    std::memset(px, 0, ATLAS_PX * ATLAS_PX * 4);
    uint8_t tile[TILE_PX * TILE_PX * 4];
    for (int i = 0; i <= TL_BEDROCK; i++)
    {
        if (i == TL_WATER_STILL || i == TL_WATER_FLOW || i == TL_LAVA_STILL || i == TL_LAVA_FLOW) AtlasGen::animateTile(tile, i, 0.0);
        else AtlasGen::paintTile(tile, i);
        AtlasGen::blit(px, tile, i);
    }
    for (int k = 0; k < 10; k++)
    {
        AtlasGen::paintTile(tile, TL_CRACK + k);
        AtlasGen::blit(px, tile, TL_CRACK + k);
    }
}