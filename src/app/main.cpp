// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_cli.h"

//#include "shader.hpp"

int
main(int argc, const char* argv[])
{
// for renderdoc debugging
#if 0  //_DEBUG
	std::cout << "Debug mode" << std::endl;
	system("pause");
#endif
#if 0  // _DEBUG
	system("pause");
#endif
    return rawgl::Run(rawgl::CommandLineRequest { argc, argv }).exitCode;
}
