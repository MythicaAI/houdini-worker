#pragma once

#include <iostream>

namespace util
{
    struct Logger
    {
        Logger();

        template<typename T>
        Logger& operator<<(const T& value)
        {
            std::cout << value;
            return *this;
        }

        Logger& operator<<(std::ostream& (*manip)(std::ostream&))
        {
            manip(std::cout);
            return *this;
        }
    };

    Logger log();
}
