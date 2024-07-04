struct VkDispatchIndirectCommand {
    uint x;
    uint y;
    uint z;
};
struct Tile {
    uint upperLeftXU16YU16;
    uint lowerRightXU16YU16;
    uint depthF24IDU8;
};
layout(set = 1, binding = 8) buffer TileBuffer {
    VkDispatchIndirectCommand command;
    Tile tiles[];
} tileBuffer;

#define LOCAL_SIZE 32
#define SUPERSAMPLE 2
