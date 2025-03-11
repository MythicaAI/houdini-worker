
#include "attributes.h"
#include "frame.h"
#include "decoder.h"

namespace scene_talk {

template <>
bool decode(const frame_payload &payload, vertices_3f &out);

}