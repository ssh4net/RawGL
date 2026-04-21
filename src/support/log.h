// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022 Erium Vladlen.

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
