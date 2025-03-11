#pragma once

#include "frame.h"
#include "reader.h"
#include "decoder.h"

namespace scene_talk {


template<>
bool decode(const frame_payload &payload, reader::partial_header &header);
template<>
bool decode(const frame_payload &payload, reader::begin_context &out);
template<>
bool decode(const frame_payload &payload, reader::end_context &item);
template<>
bool decode(const frame_payload &payload, reader::attr_stream &item);

}