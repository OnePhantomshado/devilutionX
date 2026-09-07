#ifdef _WIN32
#define NOMINMAX
#include "ui_theme_win32.hpp"

#include <commctrl.h>
#include <filesystem>
#include <gdiplus.h>
#include <memory>

namespace d1hellforge {
namespace {

constexpr wchar_t ThemeProperty[] = L"D1Hellforge.DiabloTheme";
constexpr wchar_t HoverProperty[] = L"D1Hellforge.ButtonHover";
std::unique_ptr<Gdiplus::Bitmap> CheckboxChecked;
std::unique_ptr<Gdiplus::Bitmap> CheckboxUnchecked;
std::unique_ptr<Gdiplus::Bitmap> RadioChecked;
std::unique_ptr<Gdiplus::Bitmap> RadioUnchecked;
std::unique_ptr<Gdiplus::Bitmap> ButtonNormal;
std::unique_ptr<Gdiplus::Bitmap> ButtonHover;
std::unique_ptr<Gdiplus::Bitmap> ButtonPressed;
bool AssetsAttempted = false;
bool DiabloThemeEnabled = false;

bool IsDiabloTheme(HWND window)
{
	for (HWND current = window; current != nullptr; current = GetParent(current))
		if (GetPropW(current, ThemeProperty) != nullptr)
			return true;
	return false;
}

bool IsChoice(HWND control)
{
	wchar_t className[32] {};
	if (GetClassNameW(control, className, static_cast<int>(std::size(className))) == 0
	    || _wcsicmp(className, L"Button") != 0)
		return false;
	const LONG_PTR type = GetWindowLongPtrW(control, GWL_STYLE) & BS_TYPEMASK;
	return type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON
	    || type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE;
}

bool IsCommandButton(HWND control)
{
	wchar_t className[32] {};
	if (GetClassNameW(control, className, static_cast<int>(std::size(className))) == 0
	    || _wcsicmp(className, L"Button") != 0)
		return false;
	const LONG_PTR type = GetWindowLongPtrW(control, GWL_STYLE) & BS_TYPEMASK;
	return type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON;
}

void LoadAssets()
{
	if (AssetsAttempted)
		return;
	AssetsAttempted = true;
	wchar_t executable[MAX_PATH] {};
	GetModuleFileNameW(nullptr, executable, MAX_PATH);
	const auto directory = std::filesystem::path(executable).parent_path() / "assets" / "ui" / "controls";
	auto load = [&directory](const wchar_t *name) {
		auto bitmap = std::make_unique<Gdiplus::Bitmap>((directory / name).c_str());
		return bitmap->GetLastStatus() == Gdiplus::Ok ? std::move(bitmap) : std::unique_ptr<Gdiplus::Bitmap> {};
	};
	CheckboxChecked = load(L"checkbox_checked.png");
	CheckboxUnchecked = load(L"checkbox_unchecked.png");
	RadioChecked = load(L"radio_checked.png");
	RadioUnchecked = load(L"radio_unchecked.png");
	const auto buttonDirectory = std::filesystem::path(executable).parent_path() / "assets" / "ui" / "buttons";
	auto loadButton = [&buttonDirectory](const wchar_t *name) {
		auto bitmap = std::make_unique<Gdiplus::Bitmap>((buttonDirectory / name).c_str());
		return bitmap->GetLastStatus() == Gdiplus::Ok ? std::move(bitmap) : std::unique_ptr<Gdiplus::Bitmap> {};
	};
	ButtonNormal = loadButton(L"horizontal_normal.png");
	ButtonHover = loadButton(L"horizontal_hover_focus.png");
	ButtonPressed = loadButton(L"horizontal_active_pressed.png");
}

void PaintChoice(HWND control, HDC dc)
{
	const LONG_PTR type = GetWindowLongPtrW(control, GWL_STYLE) & BS_TYPEMASK;
	const bool radio = type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
	const bool checked = SendMessageW(control, BM_GETCHECK, 0, 0) != BST_UNCHECKED;
	Gdiplus::Bitmap *art = radio ? (checked ? RadioChecked.get() : RadioUnchecked.get())
	    : (checked ? CheckboxChecked.get() : CheckboxUnchecked.get());
	RECT rect;
	GetClientRect(control, &rect);
	FillRect(dc, &rect, GetSysColorBrush(COLOR_BTNFACE));
	const int controlHeight = static_cast<int>(rect.bottom - rect.top);
	const int controlSize = std::max(16, std::min(20, controlHeight - 2));
	const int top = rect.top + (controlHeight - controlSize) / 2;
	if (art != nullptr && art->GetLastStatus() == Gdiplus::Ok) {
		Gdiplus::Graphics graphics(dc);
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		graphics.DrawImage(art, Gdiplus::Rect(rect.left, top, controlSize, controlSize));
	} else {
		RECT mark { rect.left + 1, top + 1, rect.left + controlSize - 1, top + controlSize - 1 };
		FrameRect(dc, &mark, GetSysColorBrush(COLOR_WINDOWFRAME));
	}
	wchar_t label[256] {};
	GetWindowTextW(control, label, static_cast<int>(std::size(label)));
	RECT textRect { rect.left + controlSize + 5, rect.top, rect.right, rect.bottom };
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, IsWindowEnabled(control) ? GetSysColor(COLOR_BTNTEXT) : GetSysColor(COLOR_GRAYTEXT));
	const bool multiline = (GetWindowLongPtrW(control, GWL_STYLE) & BS_MULTILINE) != 0;
	DrawTextW(dc, label, -1, &textRect, multiline ? (DT_LEFT | DT_VCENTER | DT_WORDBREAK) : (DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS));
	if (GetFocus() == control)
		DrawFocusRect(dc, &textRect);
}

void PaintCommandButton(HWND control, HDC dc)
{
	RECT rect {};
	GetClientRect(control, &rect);
	const bool enabled = IsWindowEnabled(control) != FALSE;
	const bool pressed = (SendMessageW(control, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;
	const bool highlighted = GetPropW(control, HoverProperty) != nullptr || GetFocus() == control;
	Gdiplus::Bitmap *art = pressed ? ButtonPressed.get() : highlighted ? ButtonHover.get() : ButtonNormal.get();
	if (art != nullptr && art->GetLastStatus() == Gdiplus::Ok) {
		Gdiplus::Graphics graphics(dc);
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		graphics.DrawImage(art, Gdiplus::Rect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top));
	} else {
		HBRUSH fill = CreateSolidBrush(pressed ? RGB(63, 40, 18) : RGB(24, 18, 12));
		FillRect(dc, &rect, fill);
		DeleteObject(fill);
		FrameRect(dc, &rect, GetSysColorBrush(COLOR_WINDOWFRAME));
	}
	wchar_t label[256] {};
	GetWindowTextW(control, label, static_cast<int>(std::size(label)));
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, enabled ? (highlighted ? RGB(255, 224, 135) : RGB(224, 211, 174)) : RGB(118, 105, 82));
	if (pressed)
		OffsetRect(&rect, 1, 1);
	DrawTextW(dc, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	if (GetFocus() == control) {
		InflateRect(&rect, -7, -6);
		DrawFocusRect(dc, &rect);
	}
}

LRESULT CALLBACK ChoiceProc(HWND control, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	if (message == WM_NCDESTROY) {
		RemovePropW(control, HoverProperty);
		RemoveWindowSubclass(control, ChoiceProc, 1);
		return DefSubclassProc(control, message, wParam, lParam);
	}
	if (!IsDiabloTheme(control))
		return DefSubclassProc(control, message, wParam, lParam);
	if (message == WM_PAINT) {
		LoadAssets();
		PAINTSTRUCT paint;
		HDC dc = BeginPaint(control, &paint);
		if (IsChoice(control))
			PaintChoice(control, dc);
		else
			PaintCommandButton(control, dc);
		EndPaint(control, &paint);
		return 0;
	}
	if (message == WM_ERASEBKGND)
		return 1;
	if (message == WM_MOUSEMOVE && IsCommandButton(control) && GetPropW(control, HoverProperty) == nullptr) {
		SetPropW(control, HoverProperty, reinterpret_cast<HANDLE>(1));
		TRACKMOUSEEVENT tracking { sizeof(tracking), TME_LEAVE, control, 0 };
		TrackMouseEvent(&tracking);
		InvalidateRect(control, nullptr, FALSE);
	}
	if (message == WM_MOUSELEAVE) {
		RemovePropW(control, HoverProperty);
		InvalidateRect(control, nullptr, FALSE);
	}
	const LRESULT result = DefSubclassProc(control, message, wParam, lParam);
	if (message == BM_SETCHECK || message == WM_ENABLE || message == WM_SETFOCUS || message == WM_KILLFOCUS
	    || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_KEYDOWN || message == WM_KEYUP)
		InvalidateRect(control, nullptr, TRUE);
	return result;
}

} // namespace

void SetDiabloTheme(HWND window, bool enabled)
{
	DiabloThemeEnabled = enabled;
	if (enabled)
		SetPropW(window, ThemeProperty, reinterpret_cast<HANDLE>(1));
	else
		RemovePropW(window, ThemeProperty);
}

void InheritDiabloTheme(HWND window)
{
	// Owned top-level windows do not reliably expose their owner through the
	// parent chain during WM_CREATE. Use the process presentation selected by
	// the main window, while still storing the property locally for children.
	SetDiabloTheme(window, DiabloThemeEnabled);
}

void AttachThemedChoiceChildren(HWND parent)
{
	EnumChildWindows(parent, [](HWND child, LPARAM) -> BOOL {
		if (IsChoice(child) || IsCommandButton(child))
			SetWindowSubclass(child, ChoiceProc, 1, 0);
		return TRUE;
	}, 0);
}

bool HandleThemedChoiceCustomDraw(LPARAM notification, LRESULT &result)
{
	auto *draw = reinterpret_cast<NMCUSTOMDRAW *>(notification);
	if (draw == nullptr || draw->hdr.code != NM_CUSTOMDRAW || !IsDiabloTheme(draw->hdr.hwndFrom))
		return false;
	const LONG_PTR type = GetWindowLongPtrW(draw->hdr.hwndFrom, GWL_STYLE) & BS_TYPEMASK;
	const bool radio = type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
	const bool checkbox = type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE;
	if (!radio && !checkbox)
		return false;
	if (draw->dwDrawStage != CDDS_PREPAINT) {
		result = CDRF_DODEFAULT;
		return true;
	}

	LoadAssets();
	PaintChoice(draw->hdr.hwndFrom, draw->hdc);
	result = CDRF_SKIPDEFAULT;
	return true;
}

} // namespace d1hellforge
#endif
