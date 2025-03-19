#pragma once

#include "types.h"
#include "file_cache.h"

class MOT_Director;

struct HoudiniSession
{
    HoudiniSession();
    ~HoudiniSession();

    MOT_Director* m_director;
    CookRequest m_state;
};

struct ClientSession
{
    FileMap m_file_map;
};

struct AdminSession
{
    FileMap m_file_map;
};