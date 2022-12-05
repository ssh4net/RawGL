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

#include "Common.h"
#include "Sequence.h"
#include "OpenGLUtils.h"
#include "Timer.h"
#include "GLProgramManager.h"
#include <iostream>

const char* APP_NAME = "RawGL";
const char* APP_AUTHOR = "Erium Vladlen";
const int APP_VERSION[] = { 1, 5, 3 };

//#include "shader.hpp"

int main(int argc, const char* argv[])
{
// for renderdoc debugging
#if 0 //_DEBUG
	std::cout << "Debug mode" << std::endl;
	system("pause");
#endif
	
	Timer timer;

	OpenGLHandle glhandle;

	// init the logger
	Log_Init();

    // start the sequence
    Sequence sequence(argc, argv);
    sequence.run();
	LOG(info) << std::endl;
	LOG(info) << "Total processing time : " << timer.nowText() << std::endl;

#if 0 // _DEBUG
	system("pause");
#endif
    return 0;
}
