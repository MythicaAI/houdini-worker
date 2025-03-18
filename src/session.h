#pragma once

#include "types.h"

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
    ClientSession();
};

struct AdminSession
{
    AdminSession();
};