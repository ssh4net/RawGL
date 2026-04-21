// SPDX-License-Identifier: Apache-2.0
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
