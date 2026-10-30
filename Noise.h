#pragma once
#include <cmath>
#include <cstdint>

namespace Noise
{
    inline uint32_t hash(int x, int y, uint32_t seed)
    {
        uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }

    inline uint32_t hash3(int x, int y, int z, uint32_t seed)
    {
        return hash((int)hash(x, y, seed), z, seed + 0x9E3779B9u);
    }

    inline float rnd(int x, int y, uint32_t seed)
    {
        return (hash(x, y, seed) & 0xFFFFFF) / 16777215.0f;
    }

    inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

    inline float grad2(uint32_t h, float x, float y)
    {
        switch (h & 7)
        {
        case 0: return  x + y;  case 1: return -x + y;
        case 2: return  x - y;  case 3: return -x - y;
        case 4: return  x;      case 5: return -x;
        case 6: return  y;      default: return -y;
        }
    }

    inline float grad3(uint32_t h, float x, float y, float z)
    {
        switch (h & 15)
        {
        case 0:  return  x + y;  case 1:  return -x + y;
        case 2:  return  x - y;  case 3:  return -x - y;
        case 4:  return  x + z;  case 5:  return -x + z;
        case 6:  return  x - z;  case 7:  return -x - z;
        case 8:  return  y + z;  case 9:  return -y + z;
        case 10: return  y - z;  case 11: return -y - z;
        case 12: return  x + y;  case 13: return -x + y;
        case 14: return -y + z;  default: return -y - z;
        }
    }

    inline float perlin(float x, float y, uint32_t seed)
    {
        int xi = (int)std::floor(x), yi = (int)std::floor(y);
        float xf = x - xi, yf = y - yi;
        float u = fade(xf), v = fade(yf);
        float n00 = grad2(hash(xi, yi, seed), xf, yf);
        float n10 = grad2(hash(xi + 1, yi, seed), xf - 1.0f, yf);
        float n01 = grad2(hash(xi, yi + 1, seed), xf, yf - 1.0f);
        float n11 = grad2(hash(xi + 1, yi + 1, seed), xf - 1.0f, yf - 1.0f);
        return lerp(lerp(n00, n10, u), lerp(n01, n11, u), v);
    }

    inline float perlin3(float x, float y, float z, uint32_t seed)
    {
        int xi = (int)std::floor(x), yi = (int)std::floor(y), zi = (int)std::floor(z);
        float xf = x - xi, yf = y - yi, zf = z - zi;
        float u = fade(xf), v = fade(yf), w = fade(zf);
        float c[2][2][2];
        for (int dz = 0; dz < 2; dz++)
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++)
                    c[dx][dy][dz] = grad3(hash3(xi + dx, yi + dy, zi + dz, seed), xf - dx, yf - dy, zf - dz);
        float x00 = lerp(c[0][0][0], c[1][0][0], u), x10 = lerp(c[0][1][0], c[1][1][0], u);
        float x01 = lerp(c[0][0][1], c[1][0][1], u), x11 = lerp(c[0][1][1], c[1][1][1], u);
        return lerp(lerp(x00, x10, v), lerp(x01, x11, v), w);
    }

    inline float fbm(float x, float y, int octaves, uint32_t seed)
    {
        float sum = 0, amp = 1, freq = 1, norm = 0;
        for (int i = 0; i < octaves; i++)
        {
            sum += perlin(x * freq, y * freq, seed + (uint32_t)i * 101u) * amp;
            norm += amp; amp *= 0.5f; freq *= 2.0f;
        }
        return sum / norm;
    }

    inline float fbm3(float x, float y, float z, int octaves, uint32_t seed)
    {
        float sum = 0, amp = 1, freq = 1, norm = 0;
        for (int i = 0; i < octaves; i++)
        {
            sum += perlin3(x * freq, y * freq, z * freq, seed + (uint32_t)i * 101u) * amp;
            norm += amp; amp *= 0.5f; freq *= 2.0f;
        }
        return sum / norm;
    }

    inline float ridged(float x, float y, int octaves, uint32_t seed)
    {
        float sum = 0, amp = 0.5f, freq = 1, weight = 1;
        for (int i = 0; i < octaves; i++)
        {
            float n = 1.0f - std::fabs(perlin(x * freq, y * freq, seed + (uint32_t)i * 131u));
            n *= n;
            n *= weight;
            weight = n * 2.0f; if (weight > 1.0f) weight = 1.0f;
            sum += n * amp;
            freq *= 2.05f; amp *= 0.5f;
        }
        return sum * 1.3f > 1.0f ? 1.0f : sum * 1.3f;
    }
}