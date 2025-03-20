#include "session.h"

#include <MOT/MOT_Director.h>
#include <PI/PI_ResourceManager.h>

HoudiniSession::HoudiniSession()
{
    m_director = new MOT_Director("standalone");
    OPsetDirector(m_director);
    PIcreateResourceManager();
}

HoudiniSession::~HoudiniSession()
{
    OPsetDirector(nullptr);
    delete m_director;
}

ClientSession::ClientSession(bool is_admin)
    : m_is_admin(is_admin)
{

}
