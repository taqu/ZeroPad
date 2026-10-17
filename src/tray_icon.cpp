#include "tray.h"
namespace zeropad {
bool TrayIcon::add(HWND owner, HICON icon, MappingMode mode) noexcept {
    if (registered_) return update(mode);
    data_ = {};
    data_.cbSize = sizeof(data_); data_.hWnd = owner; data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = tray_callback; data_.hIcon = icon;
    wcscpy_s(data_.szTip, tray_tooltip(mode).data());
    if (!notify_(NIM_ADD, &data_)) return false;
    data_.uVersion = NOTIFYICON_VERSION_4;
    if (!notify_(NIM_SETVERSION, &data_)) { notify_(NIM_DELETE, &data_); return false; }
    registered_ = true;
    return true;
}
bool TrayIcon::update(MappingMode mode) noexcept {
    if (!registered_) return false;
    wcscpy_s(data_.szTip, tray_tooltip(mode).data());
    return notify_(NIM_MODIFY, &data_) != FALSE;
}
bool TrayIcon::remove() noexcept {
    if (!registered_) return true;
    registered_ = false;
    return notify_(NIM_DELETE, &data_) != FALSE;
}
bool TrayIcon::restore(MappingMode mode) noexcept {
    const HWND owner = data_.hWnd; const HICON icon = data_.hIcon;
    remove();
    return add(owner, icon, mode);
}
}
