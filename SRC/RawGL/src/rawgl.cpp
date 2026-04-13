/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022-2026 Erium Vladlen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by   //-V1042
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

#include "rawgl_core.h"

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
