#pragma once

class MOT_Director;

struct HoudiniSession
{
    HoudiniSession();
    ~HoudiniSession();

    MOT_Director* m_director;
};
