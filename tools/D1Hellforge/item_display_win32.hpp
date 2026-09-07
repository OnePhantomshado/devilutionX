#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <vector>

namespace d1hellforge {

HWND CreateItemDisplayControl(HWND parent, int x, int y, int width, int height);
void SetItemDisplayText(HWND control, const std::vector<std::wstring> &lines);

} // namespace d1hellforge
#endif
