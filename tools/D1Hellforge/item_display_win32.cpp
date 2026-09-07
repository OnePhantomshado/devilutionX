#ifdef _WIN32
#include "item_display_win32.hpp"

#include <richedit.h>

namespace d1hellforge {
namespace {

void SetFormat(HWND control, LONG first, LONG length, COLORREF color, bool bold = false)
{
	SendMessageW(control, EM_SETSEL, first, first + length);
	CHARFORMAT2W format {};
	format.cbSize = sizeof(format);
	format.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_BOLD;
	format.crTextColor = color;
	format.yHeight = 190;
	format.dwEffects = bold ? CFE_BOLD : 0;
	wcscpy_s(format.szFaceName, L"Segoe UI Emoji");
	SendMessageW(control, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
}

} // namespace

HWND CreateItemDisplayControl(HWND parent, int x, int y, int width, int height)
{
	static HMODULE richEdit = LoadLibraryW(L"Msftedit.dll");
	const wchar_t *windowClass = richEdit != nullptr ? MSFTEDIT_CLASS : L"EDIT";
	HWND control = CreateWindowExW(WS_EX_CLIENTEDGE, windowClass, L"",
	    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
	    x, y, width, height, parent, nullptr, nullptr, nullptr);
	SendMessageW(control, EM_SETBKGNDCOLOR, 0, RGB(8, 7, 4));
	CHARFORMAT2W format {};
	format.cbSize = sizeof(format);
	format.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
	format.crTextColor = RGB(224, 211, 174);
	format.yHeight = 190;
	wcscpy_s(format.szFaceName, L"Segoe UI Emoji");
	SendMessageW(control, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
	return control;
}

void SetItemDisplayText(HWND control, const std::vector<std::wstring> &lines)
{
	std::wstring text;
	for (std::size_t index = 0; index < lines.size(); ++index) {
		if (index != 0) text += L"\r\n";
		text += lines[index];
	}
	SetWindowTextW(control, text.c_str());
	for (std::size_t index = 0; index < lines.size(); ++index) {
		const auto &line = lines[index];
		const bool negative = line.starts_with(L"☠");
		const bool warning = line.starts_with(L"WARNING") || line.starts_with(L"Warning");
		const COLORREF color = negative ? RGB(232, 72, 72) : warning ? RGB(240, 170, 65) : RGB(224, 211, 174);
		const LONG lineStart = static_cast<LONG>(SendMessageW(control, EM_LINEINDEX, index, 0));
		const LONG lineLength = static_cast<LONG>(SendMessageW(control, EM_LINELENGTH, lineStart, 0));
		if (lineStart >= 0)
			SetFormat(control, lineStart, lineLength, color, index == 0);
	}
	SendMessageW(control, EM_SETSEL, 0, 0);
	SendMessageW(control, EM_SCROLLCARET, 0, 0);
}

} // namespace d1hellforge
#endif
