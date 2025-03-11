#include <span>

namespace scene_talk {

template<typename T>
struct vert3 { T x, y, z; };

template<typename T, size_t C>
struct matrix {
   T values[C];
};

using vertices_3f = std::span<vert3<float>>;
using indicies_u32 = std::span<uint32_t>;

}