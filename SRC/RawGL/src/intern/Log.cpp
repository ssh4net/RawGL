#include "Common.h"
#include "Log.h"

#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>

// TODO: Make into a class with enhancements maybe?
void Log_Init()
{
	boost::log::add_console_log
	(
		std::cout,
		boost::log::keywords::format = "[%Severity%] %Message%"
		//boost::log::keywords::format = "[%TimeStamp%] [%Severity%] %File%(%Line%): %Message%"
	);
}

void Log_SetVerbosity(int l)
{
	boost::log::core::get()->set_filter(
		boost::log::trivial::severity >= (boost::log::trivial::debug + l)
	);

	//LOG(debug) << "DEBUGMSG";
	//LOG(info) << "INFOMSG";
}
