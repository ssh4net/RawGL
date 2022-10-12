#include "Common.h"
#include "Sequence.h"
#include "OpenGLUtils.h"
#include "Timer.h"
#include "GLProgramManager.h"

const char* APP_NAME = "RawGL";
const int APP_VERSION[] = { 1, 5 };

//#include "shader.hpp"

int main(int argc, const char* argv[])
{
	//std::shared_ptr<GLSLProgram> program = g_glslProgramManager.loadTextFiles("");
	//return 0;
	Timer timer;
    OpenGLHandle glhandle;

	// init the logger
	Log_Init();

    // start the sequence
    Sequence sequence(argc, argv);
    sequence.run();

	LOG(info) << "------------ Completed -------------";
	LOG(info) << "Total processing time: " << timer.nowText();

    return 0;
}
