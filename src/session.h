#pragma once

#include "types.h"
#include "file_cache.h"

class MOT_Director;

struct HoudiniSession
{
    HoudiniSession();
    ~HoudiniSession();

    MOT_Director* m_director;
    std::vector<std::string> m_installed_libraries;
    CookRequest m_state;
};

struct ClientSession
{
    ClientSession(bool is_admin = false);

    bool m_is_admin;
    FileMap m_file_map;
};
