/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022 Erium Vladlen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <sstream>

namespace rawgl::log {
enum class Level {
    trace,
    debug,
    info,
    warning,
    error,
    fatal,
};

class Stream {
public:
    explicit Stream(Level level);
    ~Stream();

    template<typename T> Stream& operator<<(const T& value)
    {
        m_stream << value;
        return *this;
    }

    Stream& operator<<(std::ostream& (*manipulator)(std::ostream&))
    {
        manipulator(m_stream);
        return *this;
    }

private:
    Level m_level;
    std::ostringstream m_stream;
};
}  // namespace rawgl::log

#define LOG(x) ::rawgl::log::Stream(::rawgl::log::Level::x)

void
Log_Init();
void
Log_SetVerbosity(int l);
