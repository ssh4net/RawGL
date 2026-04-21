// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <cctype>
#include <string>

inline std::string
get_file_ext(const std::string& filepath)
{
    std::string::size_type idx = filepath.rfind('.');
    std::string ext            = (idx == std::string::npos) ? "" : filepath.substr(idx + 1);
    for (char& c : ext) {
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}
