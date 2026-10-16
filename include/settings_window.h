#pragma once
#include "engine_service.h"
namespace zeropad {
int run_settings_window(EngineService& engine, const std::optional<std::filesystem::path>& path,
                        const std::string& startup_diagnostic);
}
