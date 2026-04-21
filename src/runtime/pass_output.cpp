// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.


#include "sequence.h"

PassOutput::PassOutput()
    : internalFormatText("rgba32f")
    , channels(3)
    , alphaChannel(-1)
    , output(nullptr)
    , uniform(nullptr)
    , texture(nullptr)
{
}
