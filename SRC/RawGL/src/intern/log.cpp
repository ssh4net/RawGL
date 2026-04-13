// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.


#include "common.h"
#include "log.h"

#include <algorithm>
#include <memory>
#include <string>

#define SPDLOG_HEADER_ONLY
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace {
spdlog::level::level_enum
rawgl_level_from_verbosity(const int verbosity)
{
    switch (std::clamp(verbosity, 0, 5)) {
    case 0: return spdlog::level::critical;
    case 1: return spdlog::level::err;
    case 2: return spdlog::level::warn;
    case 3: return spdlog::level::info;
    case 4: return spdlog::level::debug;
    default: return spdlog::level::trace;
    }
}

spdlog::level::level_enum
rawgl_level_from_stream(const rawgl::log::Level level)
{
    switch (level) {
    case rawgl::log::Level::trace: return spdlog::level::trace;
    case rawgl::log::Level::debug: return spdlog::level::debug;
    case rawgl::log::Level::info: return spdlog::level::info;
    case rawgl::log::Level::warning: return spdlog::level::warn;
    case rawgl::log::Level::error: return spdlog::level::err;
    case rawgl::log::Level::fatal: return spdlog::level::critical;
    }

    return spdlog::level::info;
}

std::shared_ptr<spdlog::logger>&
rawgl_logger()
{
    static std::shared_ptr<spdlog::logger> logger;
    return logger;
}

std::string
rawgl_finalize_message(std::string message)
{
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }

    return message;
}
}  // namespace

namespace rawgl::log {
Stream::Stream(const Level level)
    : m_level(level)
{
}

Stream::~Stream()
{
    std::shared_ptr<spdlog::logger>& logger = rawgl_logger();
    if (!logger) {
        return;
    }

    const spdlog::level::level_enum level = rawgl_level_from_stream(m_level);
    if (!logger->should_log(level)) {
        return;
    }

    const std::string message = rawgl_finalize_message(m_stream.str());
    if (message.empty()) {
        return;
    }

    logger->log(level, message);
}
}  // namespace rawgl::log

// TODO: Make into a class with enhancements maybe?
void
Log_Init()
{
    std::shared_ptr<spdlog::logger>& logger = rawgl_logger();
    if (!logger) {
        logger = spdlog::stdout_color_mt("rawgl");
        logger->set_pattern("[%L] %v");
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::err);
        spdlog::set_default_logger(logger);
    }
}

void
Log_SetVerbosity(int l)
{
    Log_Init();
    rawgl_logger()->set_level(rawgl_level_from_verbosity(l));
}
