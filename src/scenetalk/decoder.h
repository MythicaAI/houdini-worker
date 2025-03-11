#pragma once

#include "frame.h"
namespace scene_talk {
/**
 * Decoder method used to extract CBOR elements out of payloads
 * @tparam T
 * @param payload
 * @param out
 * @return
 */
template<typename T>
bool decode(const frame_payload &payload, T &out);
}