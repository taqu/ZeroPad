#pragma once
#include "engine_service.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
namespace zeropad {
inline constexpr UINT settings_force_close = WM_APP + 43;
int run_settings_window(EngineService& engine, const std::optional<std::filesystem::path>& path,
                        const std::string& startup_diagnostic, HWND owner = nullptr);
}
