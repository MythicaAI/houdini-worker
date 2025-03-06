#pragma once

class HoudiniSession;
class CookRequest;
class StreamWriter;

namespace util
{
    bool cook(HoudiniSession& session, const CookRequest& request, StreamWriter& writer);
}