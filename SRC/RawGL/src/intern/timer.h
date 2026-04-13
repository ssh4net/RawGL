// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022 Erium Vladlen.

#pragma once

#include <chrono>
#include "log.h"

using std::chrono::high_resolution_clock;

class Timer {
public:
    Timer()
        : start_(high_resolution_clock::now()) {};
    ~Timer() {};

    template<typename T> T now(const bool reset = true)
    {
        high_resolution_clock::time_point end = high_resolution_clock::now();
        std::chrono::duration<T> dur          = std::chrono::duration_cast<std::chrono::duration<T>>(end - start_);

        if (reset)
            start_ = end;

        return dur.count();
    };

    //	void logNow(const std::string &msg)
    //	{
    //		LOG(info) << msg << std::fixed << std::setw(0) << std::setprecision(6) << now<float>() << " sec.";
    //	}

    const std::string nowText(int w = 0, int p = 6)
    {
        std::stringstream ss;
        ss << std::fixed << std::setw(w) << std::setprecision(p) << now<float>() << " sec";
        return ss.str();
    }

private:
    high_resolution_clock::time_point start_;
};
