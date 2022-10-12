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
