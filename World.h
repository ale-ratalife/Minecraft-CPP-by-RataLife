#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>

enum Block : uint8_t
{
    AIR = 0, GRASS, DIRT, STONE, SAND, WATER, WOOD, LEAVES, PLANKS, COBBLE, BRICKS,
    SNOW, GRAVEL, COAL, IRON, LAVA, OBSIDIAN, GLASS, BEDROCK,
    BLOCK_COUNT
};

enum Tile
{
    TL_GRASS_TOP = 0, TL_GRASS_SIDE, TL_DIRT, TL_STONE, TL_SAND, TL_WATER_STILL, TL_WATER_FLOW,
    TL_LOG_SIDE, TL_LOG_TOP, TL_LEAVES, TL_PLANKS, TL_COBBLE, TL_BRICKS, TL_SNOW, TL_GRAVEL,
    TL_COAL, TL_IRON, TL_LAVA_STILL, TL_LAVA_FLOW, TL_OBSIDIAN, TL_GLASS, TL_BEDROCK,
    TL_CRACK = 32
};

constexpr int ATLAS_N = 8;
constexpr int TILE_PX = 16;
constexpr int ATLAS_PX = ATLAS_N * TILE_PX;

constexpr int CHUNK_W = 16;
constexpr int CHUNK_H = 128;
constexpr int SEA_LEVEL = 40;
constexpr int LAVA_LEVEL = 11;
constexpr int FLUID_FALL = 8;

inline bool isFluid(uint8_t b) { return b == WATER || b == LAVA; }
inline bool isSolid(uint8_t b) { return b != AIR && b != WATER && b != LAVA; }
inline bool isOpaque(uint8_t b) { return b != AIR && b != WATER && b != LAVA && b != LEAVES && b != GLASS; }
inline bool isGravity(uint8_t b) { return b == SAND || b == GRAVEL; }

const char* blockName(uint8_t b);
int blockTile(uint8_t b, int face, uint8_t meta = 0);
float blockHardness(uint8_t b);
uint8_t blockDrop(uint8_t b);

extern const int FACE_N[6][3];
extern const int FACE_V[6][4][3];
extern const float FACE_SHADE[6];

struct Vertex
{
    float x, y, z;
    float u, v;
    uint8_t r, g, b, a;
};

constexpr int CHUNK_VOL = CHUNK_W * CHUNK_H * CHUNK_W;

struct Chunk
{
    int cx = 0, cz = 0;
    uint8_t blocks[CHUNK_VOL];
    uint8_t meta[CHUNK_VOL];
    int  maxY = 0;
    bool meshed = false;
    bool dirty = false;
    bool modified = false;
    std::vector<Vertex> meshOpaque, meshWater;
    unsigned int listOpaque = 0, listWater = 0;
};

inline int IDX(int x, int y, int z) { return x + z * CHUNK_W + y * CHUNK_W * CHUNK_W; }

class World
{
public:
    explicit World(uint32_t seed) : seed(seed) {}
    ~World();

    Chunk* getChunk(int cx, int cz) const;
    Chunk* generateChunk(int cx, int cz);
    void   removeChunk(int cx, int cz);

    uint8_t getBlock(int x, int y, int z) const;
    uint8_t getMeta(int x, int y, int z) const;
    bool    isLoaded(int x, int z) const { return getChunk(x >> 4, z >> 4) != nullptr; }
    void    setBlock(int x, int y, int z, uint8_t b, uint8_t m = 0);
    void    setMeta(int x, int y, int z, uint8_t m);

    int     heightAt(int x, int z) const;
    void    buildMesh(Chunk& c);
    void    tick(double dt);
    void    flowVector(int x, int y, int z, float& fx, float& fz) const;
    bool    riverCurrent(int x, int z, float& fx, float& fz) const;

    void (*onFall)(int, int, int, uint8_t) = nullptr;

    uint32_t seed;
    std::unordered_map<int64_t, Chunk*> chunks;

private:
    struct Task { int x, y, z; double due; };
    std::vector<Task> tasks;
    std::unordered_set<uint64_t> queued;
    double clock = 0.0;

    void touchChunks(Chunk* c, int lx, int lz);
    void schedule(int x, int y, int z);
    void scheduleAround(int x, int y, int z);
    void runTask(const Task& t);
    void updateFluid(int x, int y, int z);
};