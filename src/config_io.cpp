#include "config.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace zeropad {
namespace {
std::string io_error(std::string_view operation, const std::error_code& error) {
    return std::string(operation) + ": " + error.message();
}
#ifdef _WIN32
std::string windows_error(std::string_view operation, DWORD code = GetLastError()) {
    return io_error(operation, std::error_code(static_cast<int>(code), std::system_category()));
}
#endif
}
LoadStatus load_config(const std::filesystem::path& path, Config& output, std::string& error) {
    error.clear();
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) { error = io_error("Cannot inspect configuration", ec); return LoadStatus::Error; }
    if (!exists) return LoadStatus::Missing;
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "Cannot open configuration for reading"; return LoadStatus::Error; }
    std::array<char, max_config_bytes + 1> buffer{};
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = static_cast<std::size_t>(file.gcount());
    if (file.bad() || (!file.eof() && file.fail())) {
        error = "Cannot read complete configuration"; return LoadStatus::Error;
    }
    if (count > max_config_bytes) { error = "Configuration exceeds 64 KiB"; return LoadStatus::Error; }
    return parse_config(std::string_view(buffer.data(), count), output, error) ? LoadStatus::Loaded : LoadStatus::Error;
}
bool save_config(const std::filesystem::path& path, const Config& config, std::string& error) {
    error = validate_config(config);
    if (!error.empty()) return false;
    std::string text;
    try { text = serialize_config(config); }
    catch (const std::exception& exception) { error = exception.what(); return false; }
    std::error_code ec;
    auto directory = path.parent_path();
    if (directory.empty()) directory = ".";
    std::filesystem::create_directories(directory, ec);
    if (ec) { error = io_error("Cannot create configuration directory", ec); return false; }
    static std::atomic_uint sequence{0};
    std::filesystem::path temporary;
#ifdef _WIN32
    HANDLE file = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 64; ++attempt) {
        temporary = directory / (L".zeropad-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                 std::to_wstring(sequence.fetch_add(1)) + L".tmp");
        file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) {
            error = windows_error("Cannot create temporary configuration"); return false;
        }
    }
    if (file == INVALID_HANDLE_VALUE) { error = "Cannot allocate a unique temporary file"; return false; }
    DWORD written = 0;
    const bool wrote = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) != FALSE;
    if (!wrote) error = windows_error("Cannot write temporary configuration");
    else if (written != text.size()) error = "Incomplete temporary configuration write";
    if (error.empty() && !FlushFileBuffers(file)) error = windows_error("Cannot flush temporary configuration");
    if (!CloseHandle(file) && error.empty()) error = windows_error("Cannot close temporary configuration");
    if (error.empty()) {
        const auto attributes = GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            if (!ReplaceFileW(path.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr))
                error = windows_error("Cannot replace configuration");
        } else if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) {
            // No REPLACE_EXISTING: never overwrite a file that appeared meanwhile.
            if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH))
                error = windows_error("Cannot install new configuration");
        } else error = windows_error("Cannot inspect destination configuration");
    }
    if (!error.empty()) {
        if (!DeleteFileW(temporary.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
            error += "; " + windows_error("Cannot remove temporary configuration");
        return false;
    }
#else
    // Portable test support; the Windows application uses the APIs above.
    FILE* file = nullptr;
    for (int attempt = 0; attempt < 64 && !file; ++attempt) {
        temporary = directory / (".zeropad-" + std::to_string(sequence.fetch_add(1)) + ".tmp");
        file = std::fopen(temporary.c_str(), "wx");
        if (!file && errno != EEXIST) { error = "Cannot create temporary configuration"; return false; }
    }
    if (!file) { error = "Cannot allocate a unique temporary file"; return false; }
    const bool wrote = std::fwrite(text.data(), 1, text.size(), file) == text.size();
    const bool closed = std::fclose(file) == 0;
    if (!wrote || !closed) error = "Cannot write complete temporary configuration";
    if (error.empty()) {
        std::filesystem::rename(temporary, path, ec);
        if (ec) error = io_error("Cannot replace configuration", ec);
    }
    if (!error.empty()) {
        std::filesystem::remove(temporary, ec);
        return false;
    }
#endif
    return true;
}
std::optional<std::filesystem::path> default_config_path(std::string& error) {
    error.clear();
#ifdef _WIN32
    WCHAR path[MAX_PATH];
    DWORD result = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (result<=0) {
        error = std::format("Cannot get module path {}", result);
        return std::nullopt;
    }
    std::filesystem::path directory(path);
    directory = directory.parent_path();
    return directory / L"config.toml";
#else
    error = "Known Folder configuration location requires Windows";
    return std::nullopt;
#endif
}
Config startup_config(const std::filesystem::path& path, std::string& diagnostic) {
    Config config;
    const auto status = load_config(path, config, diagnostic);
    if (status == LoadStatus::Missing && !save_config(path, config, diagnostic))
        diagnostic = "Using defaults; could not persist configuration: " + diagnostic;
    else if (status == LoadStatus::Error)
        diagnostic = "Using defaults; configuration preserved: " + diagnostic;
    return config;
}
} // namespace zeropad
