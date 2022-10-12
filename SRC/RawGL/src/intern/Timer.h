// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
// 
// The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
// All rights reserved.

#pragma once

#include <chrono>
#include "Log.h"

using std::chrono::high_resolution_clock;

class Timer {
public:
    Timer() : start_(high_resolution_clock::now()) {};
    ~Timer() {};

    template <typename T>
    T now(const bool reset = true)
    {
        high_resolution_clock::time_point end = high_resolution_clock::now();
        std::chrono::duration<T> dur = std::chrono::duration_cast<std::chrono::duration<T>>(end - start_);
        
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