#pragma once

#include <ostream>

namespace termcolor {
inline std::ostream&
reset(std::ostream& stream)
{
    return stream;
}

inline std::ostream&
bold(std::ostream& stream)
{
    return stream;
}

inline std::ostream&
bright_white(std::ostream& stream)
{
    return stream;
}

inline std::ostream&
bright_yellow(std::ostream& stream)
{
    return stream;
}

inline std::ostream&
bright_green(std::ostream& stream)
{
    return stream;
}

inline std::ostream&
bright_cyan(std::ostream& stream)
{
    return stream;
}

inline std::ostream&
bright_blue(std::ostream& stream)
{
    return stream;
}
}  // namespace termcolor
