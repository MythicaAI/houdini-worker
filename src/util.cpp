#include "util.h"

#include <unistd.h>

namespace util
{
    Logger::Logger()
    {
        std::cout << "Worker[" << getpid() << "]: ";
    }

    Logger log()
    {
        return Logger();
    }
}
