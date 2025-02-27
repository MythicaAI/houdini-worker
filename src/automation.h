#pragma once

class MOT_Director;
class CookRequest;
class StreamWriter;

namespace util
{
    bool cook(MOT_Director* boss, const CookRequest& request, StreamWriter& writer);
    void cleanup_session(MOT_Director* boss);
}