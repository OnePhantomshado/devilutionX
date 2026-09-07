#pragma once

#ifdef _WIN32
#include <windows.h>

namespace d1hellforge {

void SetDiabloTheme(HWND window, bool enabled);
void InheritDiabloTheme(HWND window);
void AttachThemedChoiceChildren(HWND parent);
bool HandleThemedChoiceCustomDraw(LPARAM notification, LRESULT &result);

} // namespace d1hellforge
#endif
