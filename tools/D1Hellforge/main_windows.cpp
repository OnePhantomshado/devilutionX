#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <commdlg.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <format>
#include <optional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "inventory_renderer.hpp"
#include "item_transfer.hpp"
#include "item_workshop.hpp"
#include "item_advanced_editor.hpp"
#include "item_display_win32.hpp"
#include "save_document.hpp"
#include "stash_document.hpp"
#include "ui_theme_win32.hpp"

namespace {

constexpr int IdSaveList = 1001;
constexpr int IdBrowse = 1002;
constexpr int IdRefresh = 1003;
constexpr int IdDetails = 1004;
constexpr int IdStatFirst = 1100;
constexpr int IdSaveStats = 1200;
constexpr int IdAutoCapStats = 1201;
constexpr int IdCharacterView = 1300;
constexpr int IdInventoryView = 1301;
constexpr int IdInventoryImage = 1302;
constexpr int IdInventoryInfo = 1303;
constexpr int IdImportItem = 1304;
constexpr int IdExportItem = 1305;
constexpr int IdEditItem = 1306;
constexpr int IdMakeItem = 1307;
constexpr int IdCopyItem = 1308;
constexpr int IdDeleteItem = 1309;
constexpr UINT WmReforgeProgress = WM_APP + 41;
constexpr UINT WmReforgeComplete = WM_APP + 42;
constexpr int IdSaveInventory = 1310;
constexpr int IdAttemptItemFix = 1311;
constexpr int IdReforgeItem = 1312;
constexpr int IdStashView = 1313;
constexpr int IdStashPrevious = 1314;
constexpr int IdStashNext = 1315;
constexpr int IdSaveStash = 1316;
constexpr int IdCopyStashItem = 1317;
constexpr int IdEditStashItem = 1318;
constexpr int IdDeleteStashItem = 1319;
constexpr int IdReforgeStashItem = 1320;
constexpr int IdAppearanceNative = 1400;
constexpr int IdAppearanceDiablo = 1401;

enum class AppearanceTheme {
	Native,
	Diablo,
};

HWND BrowseButton;
HWND RefreshButton;
HWND SaveList;
HWND Details;
HWND StatLabels[5];
HWND StatEdits[5];
HWND SaveButton;
HWND AutoCapCheckbox;
HWND CharacterViewButton;
HWND InventoryViewButton;
HWND StashViewButton;
HWND StashPreviousButton;
HWND StashNextButton;
HWND StashPageLabel;
HWND InventoryImage;
HWND InventoryInfo;
HBITMAP InventoryHBitmap;
std::optional<d1hellforge::RenderedInventory> InventoryRenderSource;
std::optional<d1hellforge::RenderedItemPreview> StashBackground;
int InventoryBitmapWidth;
int InventoryBitmapHeight;
int SelectedInventoryItem = -1;
uint16_t SelectedStashItem = 0;
bool DraggingStashItem = false;
unsigned StashDragGrabColumn = 0;
unsigned StashDragGrabRow = 0;
int StashDragTargetColumn = -1;
int StashDragTargetRow = -1;
bool StashDragTargetValid = false;
bool StashDirty = false;
bool DraggingInventoryItem;
int DragItemSlot = -1;
d1hellforge::InventoryArea DragSourceArea = d1hellforge::InventoryArea::Inventory;
d1hellforge::InventoryArea DragTargetArea = d1hellforge::InventoryArea::Inventory;
int DragTargetBeltSlot = -1;
int DragTargetEquipmentSlot = -1;
int DragGrabColumn;
int DragGrabRow;
int DragTargetColumn;
int DragTargetRow;
int DragOriginalColumn;
int DragOriginalRow;
int DragItemWidth;
int DragItemHeight;
bool DragTargetValid;
bool DragMoved;
bool ShowingInventory;
bool ShowingStash;
std::optional<d1hellforge::StashDocument> CurrentStash;
std::wstring StashLoadStatus;
std::wstring InventoryDefaultInfo = L"Open a save to view character and item information.";
std::filesystem::path SaveDirectory;
std::filesystem::path ItemDirectory;
std::filesystem::path OriginalGameSaveDirectory;
std::vector<std::filesystem::path> SavePaths;
std::optional<d1hellforge::CharacterSummary> CurrentCharacter;
bool EnforceVanillaItemRules = true;
d1hellforge::ReforgeEngine PreferredReforgeEngine = d1hellforge::ReforgeEngine::Fast;
int MainWindowWidth = 820;
int MainWindowHeight = 540;
int WorkshopWindowWidth = 980;
int WorkshopWindowHeight = 840;
int ReforgeWindowWidth = 980;
int ReforgeWindowHeight = 840;
int AdvancedEditorWidth = 1190;
int AdvancedEditorHeight = 655;
AppearanceTheme CurrentAppearance = AppearanceTheme::Diablo;
bool AllowNativeAppearanceFallback = true;
HBRUSH DiabloBackgroundBrush;
ULONG_PTR GdiPlusToken;
std::unique_ptr<Gdiplus::Bitmap> DiabloButtonNormal;
std::unique_ptr<Gdiplus::Bitmap> DiabloButtonHover;
std::unique_ptr<Gdiplus::Bitmap> DiabloButtonActive;

void OpenItemWorkshop(HWND owner, d1hellforge::WorkshopMode mode);
std::wstring ToWide(std::string_view text);

void AppendSelectedItemDetails(std::wstring &text, const d1hellforge::CharacterSummary::PackedItemSummary &item,
	std::wstring_view heading, std::wstring_view location = {})
{
	text += heading;
	text += L"\r\n" + ToWide(item.displayName.empty() ? item.baseName : item.displayName);
	if (!location.empty()) text += L"\r\n" + std::wstring(location);
	for (const auto &line : item.detailLines)
		text += L"\r\n" + ToWide(line);
	if (item.activeGameIdentityDiffers)
		text += L"\r\n\r\nWARNING: The compact item differs from its active-game identity. Saving is blocked until it is repaired.";
}

std::filesystem::path FolderSettingsPath()
{
	wchar_t executable[MAX_PATH] {};
	GetModuleFileNameW(nullptr, executable, MAX_PATH);
	return std::filesystem::path(executable).parent_path() / "D1Hellforge.ini";
}

std::filesystem::path DefaultItemLibraryDirectory()
{
	wchar_t executable[MAX_PATH] {};
	GetModuleFileNameW(nullptr, executable, MAX_PATH);
	const auto buildDirectory = std::filesystem::path(executable).parent_path().parent_path().parent_path();
	const auto bundledLibrary = buildDirectory / "Im-Ex Items";
	return std::filesystem::is_directory(bundledLibrary) ? bundledLibrary : std::filesystem::path(executable).parent_path();
}

void SaveRememberedFolders()
{
	const auto path = FolderSettingsPath();
	WritePrivateProfileStringW(L"General", L"DefaultSaveType", L"DevilutionX", path.c_str());
	WritePrivateProfileStringW(L"Folders", L"DevilutionXSavePath", SaveDirectory.c_str(), path.c_str());
	WritePrivateProfileStringW(L"Folders", L"OriginalGameSavePath", OriginalGameSaveDirectory.c_str(), path.c_str());
	WritePrivateProfileStringW(L"Folders", L"ItemLibraryPath", ItemDirectory.c_str(), path.c_str());
	WritePrivateProfileStringW(L"Workshop", L"EnforceVanillaItemRules", EnforceVanillaItemRules ? L"true" : L"false", path.c_str());
	WritePrivateProfileStringW(L"Workshop", L"ReforgeEngine", PreferredReforgeEngine == d1hellforge::ReforgeEngine::Fast ? L"Fast" : L"Compatible", path.c_str());
	WritePrivateProfileStringW(L"Appearance", L"Theme", CurrentAppearance == AppearanceTheme::Diablo ? L"Diablo" : L"Native", path.c_str());
	WritePrivateProfileStringW(L"Appearance", L"AllowNativeFallback", AllowNativeAppearanceFallback ? L"true" : L"false", path.c_str());
	auto writeSize = [&path](const wchar_t *key, int value) { WritePrivateProfileStringW(L"Windows", key, std::to_wstring(value).c_str(), path.c_str()); };
	writeSize(L"MainWidth", MainWindowWidth); writeSize(L"MainHeight", MainWindowHeight);
	writeSize(L"WorkshopWidth", WorkshopWindowWidth); writeSize(L"WorkshopHeight", WorkshopWindowHeight);
	writeSize(L"ReforgeWidth", ReforgeWindowWidth); writeSize(L"ReforgeHeight", ReforgeWindowHeight);
	writeSize(L"ItemWorkshopWidth", WorkshopWindowWidth); writeSize(L"ItemWorkshopHeight", WorkshopWindowHeight);
	writeSize(L"AdvancedEditorWidth", AdvancedEditorWidth); writeSize(L"AdvancedEditorHeight", AdvancedEditorHeight);
}

void LoadRememberedFolders()
{
	const auto path = FolderSettingsPath();
	auto readDirectory = [&path](const wchar_t *key) {
		std::wstring value(32768, L'\0');
		const DWORD length = GetPrivateProfileStringW(L"Folders", key, L"", value.data(), static_cast<DWORD>(value.size()), path.c_str());
		value.resize(length);
		const std::filesystem::path directory(value);
		return std::filesystem::is_directory(directory) ? directory : std::filesystem::path {};
	};
	const auto devilutionX = readDirectory(L"DevilutionXSavePath");
	if (!devilutionX.empty()) SaveDirectory = devilutionX;
	OriginalGameSaveDirectory = readDirectory(L"OriginalGameSavePath");
	ItemDirectory = readDirectory(L"ItemLibraryPath");
	wchar_t vanillaRules[16] {};
	GetPrivateProfileStringW(L"Workshop", L"EnforceVanillaItemRules", L"true", vanillaRules, static_cast<DWORD>(std::size(vanillaRules)), path.c_str());
	EnforceVanillaItemRules = _wcsicmp(vanillaRules, L"false") != 0 && wcscmp(vanillaRules, L"0") != 0;
	wchar_t reforgeEngine[16] {};
	GetPrivateProfileStringW(L"Workshop", L"ReforgeEngine", L"Fast", reforgeEngine, static_cast<DWORD>(std::size(reforgeEngine)), path.c_str());
	PreferredReforgeEngine = _wcsicmp(reforgeEngine, L"Compatible") == 0 ? d1hellforge::ReforgeEngine::Compatible : d1hellforge::ReforgeEngine::Fast;
	wchar_t appearance[16] {};
	GetPrivateProfileStringW(L"Appearance", L"Theme", L"Diablo", appearance, static_cast<DWORD>(std::size(appearance)), path.c_str());
	CurrentAppearance = _wcsicmp(appearance, L"Native") == 0 ? AppearanceTheme::Native : AppearanceTheme::Diablo;
	wchar_t allowFallback[16] {};
	GetPrivateProfileStringW(L"Appearance", L"AllowNativeFallback", L"true", allowFallback, static_cast<DWORD>(std::size(allowFallback)), path.c_str());
	AllowNativeAppearanceFallback = _wcsicmp(allowFallback, L"false") != 0 && wcscmp(allowFallback, L"0") != 0;
	auto readSize = [&path](const wchar_t *key, int fallback, int minimum) { return std::max(minimum, static_cast<int>(GetPrivateProfileIntW(L"Windows", key, fallback, path.c_str()))); };
	MainWindowWidth = readSize(L"MainWidth", MainWindowWidth, 820); MainWindowHeight = readSize(L"MainHeight", MainWindowHeight, 540);
	WorkshopWindowWidth = readSize(L"WorkshopWidth", WorkshopWindowWidth, 660); WorkshopWindowHeight = readSize(L"WorkshopHeight", WorkshopWindowHeight, 660);
	ReforgeWindowWidth = readSize(L"ReforgeWidth", ReforgeWindowWidth, 980); ReforgeWindowHeight = readSize(L"ReforgeHeight", ReforgeWindowHeight, 840);
	const int sharedWorkshopWidth = static_cast<int>(GetPrivateProfileIntW(L"Windows", L"ItemWorkshopWidth", 0, path.c_str()));
	const int sharedWorkshopHeight = static_cast<int>(GetPrivateProfileIntW(L"Windows", L"ItemWorkshopHeight", 0, path.c_str()));
	WorkshopWindowWidth = sharedWorkshopWidth > 0 ? std::max(980, sharedWorkshopWidth) : std::max(WorkshopWindowWidth, ReforgeWindowWidth);
	WorkshopWindowHeight = sharedWorkshopHeight > 0 ? std::max(840, sharedWorkshopHeight) : std::max(WorkshopWindowHeight, ReforgeWindowHeight);
	ReforgeWindowWidth = WorkshopWindowWidth;
	ReforgeWindowHeight = WorkshopWindowHeight;
	AdvancedEditorWidth = readSize(L"AdvancedEditorWidth", AdvancedEditorWidth, 1190); AdvancedEditorHeight = readSize(L"AdvancedEditorHeight", AdvancedEditorHeight, 655);
	SaveRememberedFolders(); // Creates a readable INI with defaults on first launch.
}

void Layout(HWND window);

std::array<unsigned, 4> MaximumStats(uint8_t characterClass)
{
	constexpr std::array<std::array<unsigned, 4>, 6> Limits {{
	    { 250, 50, 60, 100 }, { 55, 70, 250, 80 }, { 45, 250, 85, 80 },
	    { 150, 80, 150, 80 }, { 120, 120, 120, 100 }, { 255, 0, 55, 150 }
	}};
	return characterClass < Limits.size() ? Limits[characterClass] : std::array<unsigned, 4> { 255, 255, 255, 255 };
}

std::wstring ToWide(std::string_view text)
{
	if (text.empty())
		return {};
	const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	std::wstring result(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
	return result;
}

std::string ToUtf8(std::wstring_view text)
{
	if (text.empty())
		return {};
	const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
	std::string result(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
	return result;
}

void SetStyledDisplayText(HWND control, std::wstring_view text)
{
	std::vector<std::wstring> lines;
	for (std::size_t start = 0; start <= text.size();) {
		const std::size_t end = text.find(L"\r\n", start);
		lines.emplace_back(text.substr(start, end == std::wstring_view::npos ? text.size() - start : end - start));
		if (end == std::wstring_view::npos) break;
		start = end + 2;
	}
	d1hellforge::SetItemDisplayText(control, lines);
}

void SetDetails(std::string_view text)
{
	const std::wstring wide = ToWide(text);
	SetWindowTextW(Details, wide.c_str());
}

void RestoreInventoryInfo()
{
	SetStyledDisplayText(InventoryInfo, InventoryDefaultInfo);
	InvalidateRect(InventoryInfo, nullptr, TRUE);
}

void ShowSelectedView()
{
	const bool showGraphic = (ShowingInventory && !ShowingStash && InventoryHBitmap != nullptr) || (ShowingStash && CurrentStash.has_value());
	ShowWindow(InventoryImage, showGraphic ? SW_SHOW : SW_HIDE);
	ShowWindow(InventoryInfo, showGraphic ? SW_SHOW : SW_HIDE);
	ShowWindow(Details, showGraphic ? SW_HIDE : SW_SHOW);
	ShowWindow(StashPreviousButton, ShowingStash ? SW_SHOW : SW_HIDE);
	ShowWindow(StashNextButton, ShowingStash ? SW_SHOW : SW_HIDE);
	ShowWindow(StashPageLabel, ShowingStash ? SW_SHOW : SW_HIDE);
	for (int i = 0; i < 5; ++i) {
		ShowWindow(StatLabels[i], ShowingInventory || ShowingStash ? SW_HIDE : SW_SHOW);
		ShowWindow(StatEdits[i], ShowingInventory || ShowingStash ? SW_HIDE : SW_SHOW);
	}
	ShowWindow(SaveButton, ShowingInventory || ShowingStash ? SW_HIDE : SW_SHOW);
	ShowWindow(AutoCapCheckbox, ShowingInventory || ShowingStash ? SW_HIDE : SW_SHOW);
	InvalidateRect(CharacterViewButton, nullptr, TRUE);
	InvalidateRect(InventoryViewButton, nullptr, TRUE);
	InvalidateRect(StashViewButton, nullptr, TRUE);
}

void RefreshStashDetails()
{
	if (!CurrentStash.has_value()) {
		SetWindowTextW(Details, StashLoadStatus.empty() ? L"No shared DevilutionX stash was found for this save environment." : StashLoadStatus.c_str());
		SetWindowTextW(StashPageLabel, L"Page —");
		EnableWindow(StashPreviousButton, FALSE); EnableWindow(StashNextButton, FALSE);
		return;
	}
	auto &stash = *CurrentStash;
	const uint32_t page = stash.selectedPage;
	SetWindowTextW(StashPageLabel, std::format(L"Shared Stash — Page {} of 100", page + 1).c_str());
	EnableWindow(StashPreviousButton, page > 0); EnableWindow(StashNextButton, page + 1 < d1hellforge::StashPageCount);
	std::wstring text = std::format(L"SHARED STASH\r\n\r\nContent: {}\r\nGold: {}\r\nPage: {} of 100\r\nItems: {}\r\n\r\n",
	    ToWide(stash.contentProfile.displayLabel), stash.gold, page + 1, stash.items.size());
	const auto found = stash.pages.find(page);
	if (found == stash.pages.end()) {
		text += L"This page is empty.\r\n";
	} else {
		std::vector<uint16_t> shown;
		bool selectionShown = false;
		for (const uint16_t cell : found->second) {
			if (cell == 0 || std::find(shown.begin(), shown.end(), cell) != shown.end()) continue;
			shown.push_back(cell);
			const auto &record = stash.items[cell - 1];
			d1hellforge::CharacterSummary::PackedItemSummary item;
			item.fullItemRecord = record.fullItemRecord;
			if (const auto refreshed = d1hellforge::RefreshSummaryFromFullItemRecord(item, stash.contentProfile.baseMode); refreshed.has_value()) {
				if (cell == SelectedStashItem) {
					selectionShown = true;
					AppendSelectedItemDetails(text, item, L"SELECTED ITEM");
					text += L"\r\n";
				}
			}
			else
				text += std::format(L"Item {} is unresolved and preserved.\r\n", cell);
		}
		if (SelectedStashItem == 0)
			text += L"Click an item in the grid to view its details.\r\n";
		else if (!selectionShown)
			text += L"The selected stash item could not be resolved; its stored record remains untouched.\r\n";
	}
	text += StashDirty
	    ? L"\r\nUNSAVED PREVIEW\r\nRight-click the stash to save with backup and verification."
	    : L"\r\nPREVIEW MODE\r\nDrag within this page to stage a move. Right-click to save staged stash changes.";
	SetStyledDisplayText(InventoryInfo, text);
	InvalidateRect(InventoryInfo, nullptr, TRUE);
	InvalidateRect(InventoryImage, nullptr, TRUE);
}

void SaveStashPreview(HWND owner)
{
	if (!CurrentStash.has_value() || !StashDirty) {
		MessageBoxW(owner, L"There are no staged stash changes to save.", L"Save Stash", MB_OK | MB_ICONINFORMATION);
		return;
	}
	if (MessageBoxW(owner, L"Save the staged stash changes now?\n\nD1Hellforge will create a backup and verify a temporary archive before replacing the stash.",
	        L"Save Stash with Backup", MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;
	const auto saved = d1hellforge::SaveStashDocument(*CurrentStash);
	if (!saved.has_value()) {
		MessageBoxW(owner, ToWide(saved.error()).c_str(), L"Stash save failed", MB_OK | MB_ICONERROR);
		return;
	}
	const auto reopened = d1hellforge::ReadStashDocument(CurrentStash->path, CurrentStash->contentProfile);
	if (reopened.has_value()) CurrentStash = *reopened;
	StashDirty = false;
	SelectedStashItem = 0;
	RefreshStashDetails();
	MessageBoxW(owner, std::format(L"Stash saved and verified.\n\nBackup: {}", saved->backupPath.wstring()).c_str(), L"Stash Saved", MB_OK | MB_ICONINFORMATION);
}

std::optional<d1hellforge::CharacterSummary::PackedItemSummary> SelectedStashSummary()
{
	if (!CurrentStash.has_value() || SelectedStashItem == 0 || SelectedStashItem > CurrentStash->items.size())
		return std::nullopt;
	d1hellforge::CharacterSummary::PackedItemSummary summary;
	summary.fullItemRecord = CurrentStash->items[SelectedStashItem - 1].fullItemRecord;
	if (!d1hellforge::RefreshSummaryFromFullItemRecord(summary, CurrentStash->contentProfile.baseMode).has_value())
		return std::nullopt;
	return summary;
}

void CopySelectedStashItem(HWND owner)
{
	if (!CurrentStash.has_value() || SelectedStashItem == 0) return;
	const auto copied = d1hellforge::CopyStashItem(*CurrentStash, SelectedStashItem, CurrentStash->selectedPage);
	if (!copied.has_value()) {
		MessageBoxW(owner, ToWide(copied.error()).c_str(), L"Copy Stash Item", MB_OK | MB_ICONWARNING);
		return;
	}
	SelectedStashItem = *copied;
	StashDirty = true;
	RefreshStashDetails();
}

void DeleteSelectedStashItem(HWND owner)
{
	if (!CurrentStash.has_value() || SelectedStashItem == 0) return;
	const auto summary = SelectedStashSummary();
	const std::wstring name = summary.has_value() ? ToWide(summary->displayName.empty() ? summary->baseName : summary->displayName) : L"selected stash item";
	const std::wstring question = L"Delete " + name + L"?\r\n\r\nThis removes it from the stash preview. The stash changes only when you choose Save Stash Changes.";
	if (MessageBoxW(owner, question.c_str(), L"Confirm Delete Stash Item", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
		return;
	const auto deleted = d1hellforge::DeleteStashItem(*CurrentStash, SelectedStashItem);
	if (!deleted.has_value()) {
		MessageBoxW(owner, ToWide(deleted.error()).c_str(), L"Delete Stash Item", MB_OK | MB_ICONWARNING);
		return;
	}
	SelectedStashItem = 0;
	StashDirty = true;
	RefreshStashDetails();
}

void ShowStashContextMenu(HWND control, int x, int y)
{
	HMENU menu = CreatePopupMenu();
	if (menu == nullptr) return;
	const UINT selectedState = SelectedStashItem != 0 ? MF_ENABLED : MF_GRAYED;
	AppendMenuW(menu, MF_STRING | selectedState, IdCopyStashItem, L"Copy Item");
	AppendMenuW(menu, MF_STRING | selectedState, IdEditStashItem, L"Workshop...");
	AppendMenuW(menu, MF_STRING | selectedState, IdReforgeStashItem, L"Reforge...");
	AppendMenuW(menu, MF_STRING | selectedState, IdDeleteStashItem, L"Delete Item...");
	AppendMenuW(menu, MF_STRING, IdMakeItem, L"Make Item from Catalog...");
	AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(menu, StashDirty ? MF_STRING : MF_STRING | MF_GRAYED, IdSaveStash, L"Save Stash Changes (with Backup)");
	POINT point { x, y }; ClientToScreen(control, &point);
	const int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, point.x, point.y, 0, GetParent(control), nullptr);
	DestroyMenu(menu);
	HWND owner = GetParent(control);
	if (command == IdCopyStashItem) CopySelectedStashItem(owner);
	else if (command == IdEditStashItem) OpenItemWorkshop(owner, d1hellforge::WorkshopMode::Edit);
	else if (command == IdReforgeStashItem) OpenItemWorkshop(owner, d1hellforge::WorkshopMode::Reforge);
	else if (command == IdDeleteStashItem) DeleteSelectedStashItem(owner);
	else if (command == IdMakeItem) OpenItemWorkshop(owner, d1hellforge::WorkshopMode::Create);
	else if (command == IdSaveStash) SaveStashPreview(owner);
}

void ClearInventoryBitmap()
{
	if (InventoryHBitmap != nullptr)
		DeleteObject(InventoryHBitmap);
	InventoryHBitmap = nullptr;
	InventoryBitmapWidth = 0;
	InventoryBitmapHeight = 0;
	if (InventoryImage != nullptr)
		InvalidateRect(InventoryImage, nullptr, TRUE);
}

bool UpdateScaledInventoryBitmap(int availableWidth, int availableHeight)
{
	if (!InventoryRenderSource.has_value() || availableWidth <= 0 || availableHeight <= 0)
		return false;
	const auto &source = *InventoryRenderSource;
	const double scale = std::min(static_cast<double>(availableWidth) / source.width, static_cast<double>(availableHeight) / source.height);
	const int targetWidth = std::max(1, static_cast<int>(source.width * scale + 0.5));
	const int targetHeight = std::max(1, static_cast<int>(source.height * scale + 0.5));
	if (InventoryHBitmap != nullptr && targetWidth == InventoryBitmapWidth && targetHeight == InventoryBitmapHeight)
		return true;

	BITMAPINFO bitmapInfo {};
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = targetWidth;
	bitmapInfo.bmiHeader.biHeight = -targetHeight;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	void *pixels = nullptr;
	HBITMAP bitmap = CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
	if (bitmap == nullptr || pixels == nullptr)
		return false;
	auto *destination = static_cast<uint32_t *>(pixels);
	for (int y = 0; y < targetHeight; ++y) {
		const int sourceY = y * source.height / targetHeight;
		for (int x = 0; x < targetWidth; ++x) {
			const int sourceX = x * source.width / targetWidth;
			destination[y * targetWidth + x] = source.pixels[sourceY * source.width + sourceX];
		}
	}
	if (SelectedInventoryItem >= 0 && static_cast<std::size_t>(SelectedInventoryItem) < source.items.size()) {
		const auto &region = source.items[SelectedInventoryItem];
		const int left = std::clamp(region.x * targetWidth / source.width, 0, targetWidth - 1);
		const int top = std::clamp(region.y * targetHeight / source.height, 0, targetHeight - 1);
		const int right = std::clamp((region.x + region.width) * targetWidth / source.width, left + 1, targetWidth);
		const int bottom = std::clamp((region.y + region.height) * targetHeight / source.height, top + 1, targetHeight);
		constexpr uint32_t SelectionColor = 0x00E0B84E;
		for (int thickness = 0; thickness < 2; ++thickness) {
			for (int x = left; x < right; ++x) {
				destination[(top + thickness) * targetWidth + x] = SelectionColor;
				destination[(bottom - 1 - thickness) * targetWidth + x] = SelectionColor;
			}
			for (int y = top; y < bottom; ++y) {
				destination[y * targetWidth + left + thickness] = SelectionColor;
				destination[y * targetWidth + right - 1 - thickness] = SelectionColor;
			}
		}
	}
	if (DraggingInventoryItem) {
		constexpr std::array<RECT, 7> EquipmentRects {{
		    { 132, 4, 190, 62 }, { 47, 176, 76, 205 }, { 248, 176, 277, 205 },
		    { 204, 32, 233, 61 }, { 16, 75, 74, 162 }, { 247, 75, 305, 162 }, { 132, 75, 190, 162 }
		}};
		int logicalLeft;
		int logicalTop;
		int logicalRight;
		int logicalBottom;
		if (DragTargetArea == d1hellforge::InventoryArea::Belt) {
			logicalLeft = 15 + DragTargetBeltSlot * 37;
			logicalTop = 357;
			logicalRight = logicalLeft + 31;
			logicalBottom = logicalTop + 31;
		} else if (DragTargetArea == d1hellforge::InventoryArea::Equipment && DragTargetEquipmentSlot >= 0) {
			const RECT &rect = EquipmentRects[DragTargetEquipmentSlot];
			logicalLeft = rect.left;
			logicalTop = rect.top;
			logicalRight = rect.right;
			logicalBottom = rect.bottom;
		} else {
			logicalLeft = 16 + DragTargetColumn * 29;
			logicalTop = 222 + DragTargetRow * 29;
			logicalRight = logicalLeft + DragItemWidth * 29;
			logicalBottom = logicalTop + DragItemHeight * 29;
		}
		const int left = std::clamp(logicalLeft * targetWidth / source.width, 0, targetWidth - 1);
		const int top = std::clamp(logicalTop * targetHeight / source.height, 0, targetHeight - 1);
		const int right = std::clamp(logicalRight * targetWidth / source.width, left + 1, targetWidth);
		const int bottom = std::clamp(logicalBottom * targetHeight / source.height, top + 1, targetHeight);
		const uint32_t previewColor = DragTargetValid ? 0x0038D060 : 0x00D04038;
		for (int thickness = 0; thickness < 3 && top + thickness < bottom && left + thickness < right; ++thickness) {
			for (int x = left; x < right; ++x) {
				destination[(top + thickness) * targetWidth + x] = previewColor;
				destination[(bottom - 1 - thickness) * targetWidth + x] = previewColor;
			}
			for (int y = top; y < bottom; ++y) {
				destination[y * targetWidth + left + thickness] = previewColor;
				destination[y * targetWidth + right - 1 - thickness] = previewColor;
			}
		}
	}
	if (InventoryHBitmap != nullptr)
		DeleteObject(InventoryHBitmap);
	InventoryHBitmap = bitmap;
	InventoryBitmapWidth = targetWidth;
	InventoryBitmapHeight = targetHeight;
	return true;
}

bool UpdateInventoryGraphic(const d1hellforge::CharacterSummary &hero)
{
	const auto rendered = d1hellforge::RenderInventory(hero);
	if (!rendered.has_value()) {
		InventoryRenderSource.reset();
		ClearInventoryBitmap();
		return false;
	}
	InventoryRenderSource = *rendered;
	SelectedInventoryItem = -1;
	ClearInventoryBitmap();
	return true;
}

void LoadSave(const std::filesystem::path &path)
{
	const auto summary = d1hellforge::ReadCharacterSummary(path);
	if (!summary.has_value()) {
		InventoryRenderSource.reset();
		ClearInventoryBitmap();
		ShowingInventory = false;
		ShowingStash = false;
		CurrentStash.reset();
		StashDirty = false;
		CurrentCharacter.reset();
		InventoryDefaultInfo = L"Open a valid save to view character and item information.";
		RestoreInventoryInfo();
		EnableWindow(SaveButton, FALSE);
		SetDetails(std::format("Could not read {}\r\n\r\n{}", path.filename().string(), summary.error()));
		return;
	}

	const auto &hero = *summary;
	CurrentCharacter = hero;
	const auto stashPath = d1hellforge::StashPathForCharacterSave(hero.path);
	const auto stash = d1hellforge::ReadStashDocument(stashPath, hero.contentProfile);
	if (stash.has_value()) { CurrentStash = *stash; StashLoadStatus.clear(); }
	else { CurrentStash.reset(); StashLoadStatus = ToWide(stash.error()); }
	StashDirty = false;
	const auto stashBackground = d1hellforge::RenderStashBackground(hero);
	if (stashBackground.has_value()) StashBackground = *stashBackground;
	else StashBackground.reset();
	InventoryDefaultInfo = ToWide(std::format(
	    "CHARACTER\r\n{}\r\n{} — {}\r\nLevel {}\r\n\r\nSAVE\r\n{}\r\n\r\nITEMS\r\nEquipment: {}\r\nInventory: {}\r\nBelt: {}\r\n\r\nLeft-click an item for details. Right-click the inventory for item actions.",
	    hero.name,
	    hero.contentProfile.displayLabel,
	    d1hellforge::ClassName(hero.characterClass, hero.game), hero.level,
	    hero.path.filename().string(), hero.equipment.size(), hero.inventory.size(), hero.belt.size()));
	RestoreInventoryInfo();
	UpdateInventoryGraphic(hero);
	ShowingInventory = false;
	ShowingStash = false;
	const auto limits = MaximumStats(hero.characterClass);
	constexpr const wchar_t *Names[] { L"Strength", L"Magic", L"Dexterity", L"Vitality" };
	for (int i = 0; i < 4; ++i) {
		const std::wstring label = std::format(L"{} (safe max {})", Names[i], limits[i]);
		SetWindowTextW(StatLabels[i], label.c_str());
	}
	const unsigned values[] { hero.strength, hero.magic, hero.dexterity, hero.vitality, hero.unspentStatPoints };
	for (int i = 0; i < 5; ++i)
		SetDlgItemInt(GetParent(StatEdits[i]), IdStatFirst + i, values[i], FALSE);
	EnableWindow(SaveButton, TRUE);
	std::string details = std::format(
	    "Character: {}\r\n"
	    "Content: {}\r\n"
	    "Class: {}\r\n"
	    "Level: {}\r\n"
	    "Experience: {}\r\n"
	    "Gold: {}\r\n\r\n"
	    "Strength: {}\r\n"
	    "Magic: {}\r\n"
	    "Dexterity: {}\r\n"
	    "Vitality: {}\r\n"
	    "Unspent stat points: {}\r\n\r\n"
	    "Save: {}\r\n\r\n"
	    "Game data: {}\r\n"
	    "Graphics: {}\r\n\r\n"
	    "Only the five fields below are editable in this milestone.",
	    hero.name,
	    hero.contentProfile.displayLabel,
	    d1hellforge::ClassName(hero.characterClass, hero.game),
	    hero.level, hero.experience, hero.gold,
	    hero.strength, hero.magic, hero.dexterity, hero.vitality,
	    hero.unspentStatPoints, hero.path.string(),
	    hero.gameDataDirectory.empty() ? "Not selected" : hero.gameDataDirectory.string(),
	    hero.gameGraphicsStatus);

	constexpr const char *EquipmentSlots[] { "Head", "Left Ring", "Right Ring", "Amulet", "Left Hand", "Right Hand", "Chest" };
	auto appendItem = [&details](std::string_view location, const d1hellforge::CharacterSummary::PackedItemSummary &item) {
		const std::string itemName = item.baseName.empty() ? std::format("Base item #{}", item.baseItemId) : item.baseName;
		details += std::format("{}: {} (#{}) — seed {:08X}, durability {}/{}, charges {}/{}, identified {}, quality {}\r\n",
		    location, itemName, item.baseItemId, item.seed, item.durability, item.maxDurability,
		    item.charges, item.maxCharges, item.identified ? "yes" : "no", item.magicalQuality);
	};
	details += "\r\nEquipment (read-only)\r\n";
	if (hero.equipment.empty())
		details += "(empty)\r\n";
	for (const auto &item : hero.equipment)
		appendItem(EquipmentSlots[item.slot], item);
	details += std::format("\r\nInventory (read-only, {} item records)\r\n", hero.inventory.size());
	if (hero.inventory.empty())
		details += "(empty)\r\n";
	for (const auto &item : hero.inventory)
		appendItem(std::format("Item {}", item.slot + 1), item);
	details += std::format("\r\nBelt (read-only, {} occupied slots)\r\n", hero.belt.size());
	if (hero.belt.empty())
		details += "(empty)\r\n";
	for (const auto &item : hero.belt)
		appendItem(std::format("Slot {}", item.slot + 1), item);
	SetDetails(details);
	ShowSelectedView();
}

bool IsDevilutionXRunning()
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return false;
	PROCESSENTRY32W entry {};
	entry.dwSize = sizeof(entry);
	bool found = false;
	if (Process32FirstW(snapshot, &entry)) {
		do {
			if (_wcsicmp(entry.szExeFile, L"devilutionx.exe") == 0) {
				found = true;
				break;
			}
		} while (Process32NextW(snapshot, &entry));
	}
	CloseHandle(snapshot);
	return found;
}

void SaveStats(HWND owner)
{
	if (!CurrentCharacter.has_value())
		return;
	if (IsDevilutionXRunning()) {
		MessageBoxW(owner, L"Close DevilutionX before changing a save.", L"D1Hellforge", MB_OK | MB_ICONWARNING);
		return;
	}

	unsigned values[5] {};
	for (int i = 0; i < 5; ++i) {
		BOOL valid = FALSE;
		values[i] = GetDlgItemInt(owner, IdStatFirst + i, &valid, FALSE);
		if (!valid) {
			MessageBoxW(owner, L"Each editable value must be a non-negative whole number.", L"D1Hellforge", MB_OK | MB_ICONWARNING);
			return;
		}
	}
	const auto limits = MaximumStats(CurrentCharacter->characterClass);
	const bool autoCap = SendMessageW(AutoCapCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
	bool capped = false;
	for (int i = 0; i < 4; ++i) {
		if (values[i] > limits[i]) {
			if (autoCap) {
				values[i] = limits[i];
				SetDlgItemInt(owner, IdStatFirst + i, values[i], FALSE);
				capped = true;
				continue;
			}
			const std::wstring warning = std::format(L"That class has a maximum {} value of {}. DevilutionX would clamp a larger value while loading.", i == 0 ? L"Strength" : i == 1 ? L"Magic" : i == 2 ? L"Dexterity" : L"Vitality", limits[i]);
			MessageBoxW(owner, warning.c_str(), L"D1Hellforge", MB_OK | MB_ICONWARNING);
			return;
		}
	}
	if (values[4] > 255) {
		MessageBoxW(owner, L"Unspent stat points must be between 0 and 255.", L"D1Hellforge", MB_OK | MB_ICONWARNING);
		return;
	}

	d1hellforge::EditableStats stats {
	    static_cast<uint8_t>(values[0]), static_cast<uint8_t>(values[1]),
	    static_cast<uint8_t>(values[2]), static_cast<uint8_t>(values[3]),
	    static_cast<uint8_t>(values[4])
	};
	const auto result = d1hellforge::SaveCharacterStats(CurrentCharacter->path, stats);
	if (!result.has_value()) {
		const std::wstring error = ToWide(result.error());
		MessageBoxW(owner, error.c_str(), L"Save failed", MB_OK | MB_ICONERROR);
		return;
	}
	LoadSave(CurrentCharacter->path);
	const std::wstring message = (capped ? L"Character saved and verified. Values above the safe class maximum were capped.\n\nBackup:\n" : L"Character saved and verified.\n\nBackup:\n") + result->backupPath.wstring();
	MessageBoxW(owner, message.c_str(), L"D1Hellforge", MB_OK | MB_ICONINFORMATION);
}

void RefreshSaves()
{
	SendMessageW(SaveList, LB_RESETCONTENT, 0, 0);
	SavePaths = d1hellforge::FindSaves(SaveDirectory);
	for (const auto &path : SavePaths) {
		const std::wstring filename = path.filename().wstring();
		SendMessageW(SaveList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(filename.c_str()));
	}
	if (SavePaths.empty()) {
		SetDetails(std::format("No DevilutionX saves were found in:\r\n{}\r\n\r\nUse Open Save to select a .sv or .hsv file.", SaveDirectory.string()));
		return;
	}
	SendMessageW(SaveList, LB_SETCURSEL, 0, 0);
	LoadSave(SavePaths.front());
}

void BrowseForSave(HWND owner)
{
	wchar_t filename[MAX_PATH] = L"";
	OPENFILENAMEW dialog {};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = owner;
	dialog.lpstrFilter = L"Diablo save files (*.sv;*.hsv)\0*.sv;*.hsv\0All files (*.*)\0*.*\0";
	const std::wstring initialDirectory = SaveDirectory.wstring();
	dialog.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
	dialog.lpstrFile = filename;
	dialog.nMaxFile = MAX_PATH;
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	if (!GetOpenFileNameW(&dialog))
		return;
	const std::filesystem::path path(filename);
	SaveDirectory = path.parent_path();
	SaveRememberedFolders();
	RefreshSaves();
	for (std::size_t i = 0; i < SavePaths.size(); ++i) {
		if (std::filesystem::equivalent(SavePaths[i], path)) {
			SendMessageW(SaveList, LB_SETCURSEL, i, 0);
			LoadSave(path);
			break;
		}
	}
}

const d1hellforge::CharacterSummary::PackedItemSummary *SelectedItem()
{
	if (!CurrentCharacter.has_value() || !InventoryRenderSource.has_value() || SelectedInventoryItem < 0
	    || static_cast<std::size_t>(SelectedInventoryItem) >= InventoryRenderSource->items.size())
		return nullptr;
	const auto &region = InventoryRenderSource->items[SelectedInventoryItem];
	const auto &items = region.area == d1hellforge::InventoryArea::Equipment ? CurrentCharacter->equipment
	    : region.area == d1hellforge::InventoryArea::Belt                         ? CurrentCharacter->belt
	                                                                            : CurrentCharacter->inventory;
	const auto found = std::find_if(items.begin(), items.end(), [&region](const auto &item) { return item.slot == region.slot; });
	return found == items.end() ? nullptr : &*found;
}

void ExportSelectedItem(HWND owner)
{
	const auto *item = SelectedItem();
	if (item == nullptr) {
		MessageBoxW(owner, L"Select an item before exporting it.", L"Export Item", MB_OK | MB_ICONINFORMATION);
		return;
	}
	std::wstring filename = ToWide(item->baseName.empty() ? "item" : item->baseName);
	for (wchar_t &character : filename) {
		if (wcschr(L"\\/:*?\"<>|", character) != nullptr)
			character = L'_';
	}
	filename += L".dxitem";
	wchar_t path[MAX_PATH] {};
	wcsncpy_s(path, filename.c_str(), _TRUNCATE);
	OPENFILENAMEW dialog {};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = owner;
	dialog.lpstrFilter = L"D1Hellforge item (*.dxitem)\0*.dxitem\0Raw ItemPack (*.rawitem)\0*.rawitem\0Legacy Diablo item (*.ITM)\0*.ITM\0Legacy Hellfire item (*.HIF)\0*.HIF\0";
		const std::wstring initialDirectory = ItemDirectory.wstring();
	dialog.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
	dialog.lpstrFile = path;
	dialog.nMaxFile = MAX_PATH;
	dialog.nFilterIndex = 1;
	dialog.lpstrDefExt = L"dxitem";
	dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	if (!GetSaveFileNameW(&dialog))
		return;
	std::filesystem::path exportPath(path);
	const wchar_t *extensions[] { L"", L".dxitem", L".rawitem", L".ITM", L".HIF" };
	if (dialog.nFilterIndex >= 1 && dialog.nFilterIndex <= 4)
		exportPath.replace_extension(extensions[dialog.nFilterIndex]);
	ItemDirectory = exportPath.parent_path();
	SaveRememberedFolders();
	const bool legacy = dialog.nFilterIndex >= 3;
	const auto selectedGame = dialog.nFilterIndex == 4 ? d1hellforge::Game::Hellfire : d1hellforge::Game::Diablo;
	std::array<std::byte, 20> exportedBytes = item->packedBytes;
	if (legacy && selectedGame != CurrentCharacter->game) {
		const auto converted = d1hellforge::ConvertPackedItem(exportedBytes, CurrentCharacter->game, selectedGame);
		if (!converted.has_value()) {
			MessageBoxW(owner, ToWide(converted.error()).c_str(), L"Export failed", MB_OK | MB_ICONERROR);
			return;
		}
		exportedBytes = *converted;
	}
	const auto game = selectedGame == d1hellforge::Game::Hellfire ? devilution::ItemGame::Hellfire : devilution::ItemGame::Diablo;
	std::expected<void, std::string> result;
	if (legacy) {
		if (selectedGame == CurrentCharacter->game && exportedBytes == item->packedBytes) {
			result = d1hellforge::ExportLegacyItem(exportPath, *item, selectedGame);
		} else {
			auto exportedSummary = d1hellforge::SummarizeImportedItem(exportedBytes, selectedGame);
			if (!exportedSummary.has_value()) {
				MessageBoxW(owner, ToWide(exportedSummary.error()).c_str(), L"Export failed", MB_OK | MB_ICONERROR);
				return;
			}
			result = d1hellforge::ExportLegacyItem(exportPath, *exportedSummary, selectedGame);
		}
	} else {
		const auto nativeGame = CurrentCharacter->game == d1hellforge::Game::Hellfire ? devilution::ItemGame::Hellfire : devilution::ItemGame::Diablo;
		if (exportedBytes == item->packedBytes)
			result = d1hellforge::ExportItemFile(exportPath, *item, nativeGame, dialog.nFilterIndex == 2);
		else
			result = d1hellforge::ExportItemFile(exportPath, exportedBytes, nativeGame, dialog.nFilterIndex == 2);
	}
	if (!result.has_value()) {
		MessageBoxW(owner, ToWide(result.error()).c_str(), L"Export failed", MB_OK | MB_ICONERROR);
		return;
	}
	MessageBoxW(owner, L"Selected item exported successfully.", L"Export Item", MB_OK | MB_ICONINFORMATION);
}

bool PlaceItemInBackpackPreview(HWND owner, const d1hellforge::CharacterSummary::PackedItemSummary &source, std::wstring_view action)
{
	const auto footprint = d1hellforge::GetItemFootprint(*CurrentCharacter, source.cursorGraphic);
	if (!footprint.has_value()) {
		MessageBoxW(owner, ToWide(footprint.error()).c_str(), L"Item placement failed", MB_OK | MB_ICONWARNING);
		return false;
	}
	std::array<bool, 40> usedSlots {};
	for (const auto &item : CurrentCharacter->inventory)
		usedSlots[item.slot] = true;
	const auto freeSlot = std::find(usedSlots.begin(), usedSlots.end(), false);
	if (freeSlot == usedSlots.end()) {
		MessageBoxW(owner, L"The character already has the maximum number of backpack item records.", L"Item placement blocked", MB_OK | MB_ICONWARNING);
		return false;
	}
	int destination = -1;
	for (int row = 0; row + footprint->second <= 4 && destination < 0; ++row) {
		for (int column = 0; column + footprint->first <= 10; ++column) {
			bool fits = true;
			for (int y = 0; y < footprint->second && fits; ++y)
				for (int x = 0; x < footprint->first; ++x)
					fits = fits && CurrentCharacter->inventoryGrid[(row + y) * 10 + column + x] == 0;
			if (fits) {
				destination = row * 10 + column;
				break;
			}
		}
	}
	if (destination < 0) {
		MessageBoxW(owner, L"There is no open backpack area large enough for this item.", L"Item placement blocked", MB_OK | MB_ICONWARNING);
		return false;
	}
	auto item = source;
	item.slot = static_cast<uint8_t>(std::distance(usedSlots.begin(), freeSlot));
	const int8_t itemIndex = static_cast<int8_t>(item.slot + 1);
	for (int y = 0; y < footprint->second; ++y) {
		for (int x = 0; x < footprint->first; ++x) {
			const bool anchor = x == 0 && y == footprint->second - 1;
			CurrentCharacter->inventoryGrid[destination + y * 10 + x] = anchor ? itemIndex : -itemIndex;
		}
	}
	CurrentCharacter->inventory.push_back(std::move(item));
	UpdateInventoryGraphic(*CurrentCharacter);
	ShowingInventory = true;
	Layout(owner);
	const std::wstring message = std::wstring(action) + L" ITEM PREVIEW\r\n\r\nThe item was placed in the first available backpack space. The save file has not been changed.\r\n\r\nDrag it as needed, then Refresh to discard the preview.";
	SetWindowTextW(InventoryInfo, message.c_str());
	InvalidateRect(InventoryInfo, nullptr, TRUE);
	return true;
}

void CopySelectedItem(HWND owner)
{
	const auto *selected = SelectedItem();
	if (selected == nullptr)
		return;
	const auto copy = *selected;
	PlaceItemInBackpackPreview(owner, copy, L"COPIED");
}

void DeleteSelectedItem(HWND owner)
{
	const auto *selected = SelectedItem();
	if (selected == nullptr)
		return;
	const std::wstring name = ToWide(selected->baseName.empty() ? "Unknown item" : selected->baseName);
	const std::wstring question = L"Delete " + name + L"?\r\n\r\nThis removes it from the inventory preview. The save changes only when you choose Save Inventory Changes.";
	if (MessageBoxW(owner, question.c_str(), L"Confirm Delete Item", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
		return;
	const auto &region = InventoryRenderSource->items[SelectedInventoryItem];
	if (region.area == d1hellforge::InventoryArea::Inventory) {
		const int8_t index = static_cast<int8_t>(region.slot + 1);
		for (int8_t &cell : CurrentCharacter->inventoryGrid)
			if (std::abs(static_cast<int>(cell)) == index) cell = 0;
		std::erase_if(CurrentCharacter->inventory, [&region](const auto &item) { return item.slot == region.slot; });
	} else if (region.area == d1hellforge::InventoryArea::Belt) {
		std::erase_if(CurrentCharacter->belt, [&region](const auto &item) { return item.slot == region.slot; });
	} else {
		std::erase_if(CurrentCharacter->equipment, [&region](const auto &item) { return item.slot == region.slot; });
	}
	SelectedInventoryItem = -1;
	UpdateInventoryGraphic(*CurrentCharacter);
	Layout(owner);
	const std::wstring message = L"DELETE PREVIEW\r\n\r\n" + name + L" was removed in memory.\r\n\r\nChoose Save Inventory Changes from the right-click menu to commit, or Refresh to discard.";
	SetWindowTextW(InventoryInfo, message.c_str());
}

void AttemptFixSelectedItem(HWND owner)
{
	auto *item = const_cast<d1hellforge::CharacterSummary::PackedItemSummary *>(SelectedItem());
	if (item == nullptr || !item->activeGameIdentityDiffers) return;
	d1hellforge::ItemGenerationOptions options;
	options.seed = item->seed;
	options.level = static_cast<uint8_t>(std::clamp<int>(CurrentCharacter->level, 1, 63));
	options.onlyGood = false;
	if (item->magicalQuality == 2) options.quality = d1hellforge::ItemQualityChoice::Unique;
	else if (item->magicalQuality == 1) options.quality = d1hellforge::ItemQualityChoice::Magic;
	const auto repaired = d1hellforge::CreateCatalogItem(item->baseItemId, CurrentCharacter->game, options);
	if (!repaired.has_value()) {
		MessageBoxW(owner, ToWide(repaired.error()).c_str(), L"Attempt Fix", MB_OK | MB_ICONWARNING);
		return;
	}
	const std::wstring oldName = ToWide(item->displayName.empty() ? item->baseName : item->displayName);
	const std::wstring newName = ToWide(repaired->displayName.empty() ? repaired->baseName : repaired->displayName);
	std::wstring question = L"D1Hellforge found a game-native replacement for the split item.\r\n\r\nCurrent active-game identity:\r\n" + oldName
	    + L"\r\n\r\nProposed canonical replacement:\r\n" + newName
	    + L"\r\nSeed: " + std::to_wstring(repaired->seed)
	    + L"\r\n\r\nThe base item and quality are preserved where possible, but incompatible legacy names or exact effects may change. Apply this repair to the in-memory preview?";
	if (MessageBoxW(owner, question.c_str(), L"Attempt Item Repair", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
		return;
	const uint8_t slot = item->slot;
	*item = *repaired;
	item->slot = slot;
	UpdateInventoryGraphic(*CurrentCharacter);
	Layout(owner);
	SetWindowTextW(InventoryInfo, L"ITEM REPAIR PREVIEW\r\n\r\nThe selected item now has one game-native compact and active-game identity.\r\n\r\nChoose Save Inventory Changes to commit, or Refresh to discard.");
}

struct ItemEditDialogState {
	d1hellforge::CharacterSummary::PackedItemSummary *item;
	bool accepted = false;
	HWND controls[7] {};
};

LRESULT CALLBACK ItemEditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto *state = reinterpret_cast<ItemEditDialogState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if (message == WM_CREATE) {
		d1hellforge::InheritDiabloTheme(window);
		state = static_cast<ItemEditDialogState *>(reinterpret_cast<CREATESTRUCTW *>(lParam)->lpCreateParams);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
		const wchar_t *labels[] { L"Seed", L"Durability", L"Maximum durability", L"Charges", L"Maximum charges", L"Gold value" };
		const uint32_t values[] { state->item->seed, state->item->durability, state->item->maxDurability, state->item->charges, state->item->maxCharges, state->item->value };
		CreateWindowW(L"STATIC", ToWide(state->item->baseName).c_str(), WS_CHILD | WS_VISIBLE, 18, 14, 310, 24, window, nullptr, nullptr, nullptr);
		state->controls[0] = CreateWindowW(L"BUTTON", L"Identified", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 44, 160, 24, window, nullptr, nullptr, nullptr);
		SendMessageW(state->controls[0], BM_SETCHECK, state->item->identified ? BST_CHECKED : BST_UNCHECKED, 0);
		for (int i = 0; i < 6; ++i) {
			CreateWindowW(L"STATIC", labels[i], WS_CHILD | WS_VISIBLE, 18, 76 + i * 30, 160, 22, window, nullptr, nullptr, nullptr);
			state->controls[i + 1] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(values[i]).c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_RIGHT, 185, 73 + i * 30, 110, 24, window, nullptr, nullptr, nullptr);
		}
		CreateWindowW(L"BUTTON", L"Randomize", WS_CHILD | WS_VISIBLE, 305, 73, 88, 24, window, reinterpret_cast<HMENU>(2101), nullptr, nullptr);
		if (state->item->baseItemId != 0)
			EnableWindow(state->controls[6], FALSE);
		CreateWindowW(L"STATIC", L"Changing the seed regenerates the item's game-derived properties.", WS_CHILD | WS_VISIBLE, 18, 258, 375, 22, window, nullptr, nullptr, nullptr);
		CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 205, 288, 90, 30, window, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
		CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 305, 288, 90, 30, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
		d1hellforge::AttachThemedChoiceChildren(window);
		return 0;
	}
	if (message == WM_NOTIFY) {
		LRESULT result = 0;
		if (d1hellforge::HandleThemedChoiceCustomDraw(lParam, result))
			return result;
	}
	if (message == WM_COMMAND && state != nullptr) {
		if (LOWORD(wParam) == IDCANCEL) { DestroyWindow(window); return 0; }
		if (LOWORD(wParam) == 2101) {
			const uint32_t seed = static_cast<uint32_t>(GetTickCount64() ^ reinterpret_cast<uintptr_t>(window));
			SetWindowTextW(state->controls[1], std::to_wstring(seed).c_str());
			return 0;
		}
		if (LOWORD(wParam) == IDOK) {
			uint32_t values[6] {};
			for (int i = 0; i < 6; ++i) {
				wchar_t text[16] {};
				GetWindowTextW(state->controls[i + 1], text, static_cast<int>(std::size(text)));
				wchar_t *end = nullptr;
				const unsigned long parsed = wcstoul(text, &end, 10);
				values[i] = static_cast<uint32_t>(parsed);
				if (end == text || *end != L'\0' || (i >= 1 && values[i] > (i == 5 ? 65535U : 255U))) {
					MessageBoxW(window, L"Enter a 32-bit seed, durability and charges from 0 to 255, and gold from 0 to 65535.", L"Invalid Item Value", MB_OK | MB_ICONWARNING);
					return 0;
				}
			}
			if (values[1] > values[2] || values[3] > values[4]) {
				MessageBoxW(window, L"Current durability or charges cannot exceed its maximum.", L"Invalid Item Value", MB_OK | MB_ICONWARNING);
				return 0;
			}
			if (values[0] != state->item->seed) {
				const auto regenerated = d1hellforge::RegenerateItemWithSeed(*state->item, CurrentCharacter->game, values[0]);
				if (!regenerated.has_value()) { MessageBoxW(window, ToWide(regenerated.error()).c_str(), L"Seed regeneration failed", MB_OK | MB_ICONWARNING); return 0; }
				*state->item = *regenerated;
			}
			state->item->identified = SendMessageW(state->controls[0], BM_GETCHECK, 0, 0) == BST_CHECKED;
			state->item->durability = static_cast<uint8_t>(values[1]);
			state->item->maxDurability = static_cast<uint8_t>(values[2]);
			state->item->charges = static_cast<uint8_t>(values[3]);
			state->item->maxCharges = static_cast<uint8_t>(values[4]);
			if (state->item->baseItemId == 0) state->item->value = static_cast<uint16_t>(values[5]);
			auto &bytes = state->item->packedBytes;
			bytes[8] = static_cast<std::byte>((std::to_integer<uint8_t>(bytes[8]) & ~1U) | (state->item->identified ? 1U : 0U));
			bytes[9] = static_cast<std::byte>(state->item->durability);
			bytes[10] = static_cast<std::byte>(state->item->maxDurability);
			bytes[11] = static_cast<std::byte>(state->item->charges);
			bytes[12] = static_cast<std::byte>(state->item->maxCharges);
			bytes[13] = static_cast<std::byte>(state->item->value);
			bytes[14] = static_cast<std::byte>(state->item->value >> 8);
			state->accepted = true;
			DestroyWindow(window);
			return 0;
		}
	}
	if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
	return DefWindowProcW(window, message, wParam, lParam);
}

void EditSelectedItem(HWND owner)
{
	OpenItemWorkshop(owner, d1hellforge::WorkshopMode::Edit);
}

struct ItemCatalogDialogState {
	d1hellforge::WorkshopItemModel model;
	std::vector<d1hellforge::CatalogItem> items;
	std::vector<std::size_t> visible;
	std::vector<std::wstring> categories;
	std::optional<d1hellforge::CharacterSummary::PackedItemSummary> created;
	HWND category = nullptr;
	HWND search = nullptr;
	HWND list = nullptr;
	HWND seed = nullptr;
	HWND sprite = nullptr;
	HWND preview = nullptr;
	HBITMAP spriteBitmap = nullptr;
	HWND quality[3] {};
	HWND level = nullptr;
	HWND onlyGood = nullptr;
	HWND effect = nullptr;
	HWND prefix = nullptr;
	HWND suffix = nullptr;
	HWND unique = nullptr;
	std::optional<uint16_t> initialBaseItemId;
	d1hellforge::ReforgeRequest reforgeRequest;
	std::optional<d1hellforge::ReforgeResult> reforgeResult;
	HWND reforgeResults = nullptr;
	HWND reforgeSkill = nullptr;
	HWND reforgePreference = nullptr;
	HWND reforgeEngine = nullptr;
	HWND reforgeDetails = nullptr;
	HWND reforgeStatus = nullptr;
	HWND reforgeSearch = nullptr;
	HWND reforgeSearchAgain = nullptr;
	HWND reforgeCancel = nullptr;
	HWND reforgeLocks[5] {};
	std::jthread reforgeWorker;
	std::shared_ptr<std::atomic_bool> reforgeCancelRequested;
	bool reforgeSearching = false;
	HWND enforceRules = nullptr;
	HWND grayInvalid = nullptr;
	HWND identified = nullptr;
	std::vector<std::string> prefixValues;
	std::vector<std::string> suffixValues;
	std::vector<std::string> uniqueValues;
};

struct AsyncReforgeResult {
	std::expected<d1hellforge::ReforgeResult, std::string> result;
	d1hellforge::ReforgeEngine engine;
};

void SetReforgeSearching(HWND window, ItemCatalogDialogState &state, bool searching)
{
	state.reforgeSearching = searching;
	EnumChildWindows(window, [](HWND child, LPARAM enabled) -> BOOL {
		EnableWindow(child, static_cast<BOOL>(enabled));
		return TRUE;
	}, searching ? FALSE : TRUE);
	if (searching) {
		EnableWindow(state.reforgeCancel, TRUE);
		SetWindowTextW(state.reforgeCancel, L"Cancel Search");
	} else {
		SetWindowTextW(state.reforgeCancel, L"Cancel");
	}
}

void UpdateReforgeResultPreview(ItemCatalogDialogState &state)
{
	if (!state.reforgeResult.has_value()) return;
	const int selected = static_cast<int>(SendMessageW(state.reforgeResults, LB_GETCURSEL, 0, 0));
	if (selected < 0 || static_cast<std::size_t>(selected) >= state.reforgeResult->topCandidates.size()) return;
	const auto &candidate = state.reforgeResult->topCandidates[selected];
	std::vector<std::wstring> details { ToWide(candidate.item.displayName), L"Base: " + ToWide(candidate.item.baseName),
		std::format(L"Seed: 0x{:08X}   Score: {:.1f}%", candidate.seed, candidate.overallScore) };
	for (const auto &line : candidate.item.detailLines) details.push_back(ToWide(line));
	d1hellforge::SetItemDisplayText(state.reforgeDetails, details);
}

std::wstring Lowercase(std::wstring value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) { return std::towlower(ch); });
	return value;
}

std::optional<uint32_t> CatalogSeed(const ItemCatalogDialogState &state)
{
	wchar_t text[32] {};
	GetWindowTextW(state.seed, text, static_cast<int>(std::size(text)));
	wchar_t *end = nullptr;
	const unsigned long value = wcstoul(text, &end, 10);
	if (end == text || *end != L'\0')
		return std::nullopt;
	return static_cast<uint32_t>(value);
}

std::optional<d1hellforge::ItemGenerationOptions> CatalogOptions(const ItemCatalogDialogState &state)
{
	const auto seed = CatalogSeed(state);
	if (!seed.has_value()) return std::nullopt;
	wchar_t levelText[8] {};
	GetWindowTextW(state.level, levelText, static_cast<int>(std::size(levelText)));
	wchar_t *end = nullptr;
	const unsigned long level = wcstoul(levelText, &end, 10);
	if (end == levelText || *end != L'\0' || level < 1 || level > 63) return std::nullopt;
	d1hellforge::ItemGenerationOptions options;
	options.seed = *seed;
	options.level = static_cast<uint8_t>(level);
	options.onlyGood = SendMessageW(state.onlyGood, BM_GETCHECK, 0, 0) == BST_CHECKED;
	const int effect = static_cast<int>(SendMessageW(state.effect, CB_GETCURSEL, 0, 0));
	if (effect >= 0 && effect <= static_cast<int>(d1hellforge::ItemEffectFilter::AttackSpeed))
		options.effectFilter = static_cast<d1hellforge::ItemEffectFilter>(effect);
	if (SendMessageW(state.quality[2], BM_GETCHECK, 0, 0) == BST_CHECKED) options.quality = d1hellforge::ItemQualityChoice::Unique;
	else if (SendMessageW(state.quality[1], BM_GETCHECK, 0, 0) == BST_CHECKED) options.quality = d1hellforge::ItemQualityChoice::Magic;
	auto selectedChoice = [&state](HWND combo) {
		const int selected = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
		if (selected <= 0) return std::string {};
		const auto *values = combo == state.prefix ? &state.prefixValues : combo == state.suffix ? &state.suffixValues : &state.uniqueValues;
		if (static_cast<std::size_t>(selected) < values->size()) return (*values)[selected];
		wchar_t text[128] {};
		SendMessageW(combo, CB_GETLBTEXT, selected, reinterpret_cast<LPARAM>(text));
		return ToUtf8(text);
	};
	options.prefixName = selectedChoice(state.prefix);
	options.suffixName = selectedChoice(state.suffix);
	options.uniqueName = selectedChoice(state.unique);
	return options;
}

std::expected<d1hellforge::CharacterSummary::PackedItemSummary, std::string> WithIdentifiedState(
	d1hellforge::CharacterSummary::PackedItemSummary item, bool identified)
{
	item.packedBytes[8] = static_cast<std::byte>((std::to_integer<uint8_t>(item.packedBytes[8]) & ~1U) | (identified ? 1U : 0U));
	auto updated = d1hellforge::SummarizeImportedItem(item.packedBytes, CurrentCharacter->game);
	if (!updated.has_value()) return std::unexpected(updated.error());
	updated->slot = item.slot;
	return updated;
}

void RefreshGenerationChoices(ItemCatalogDialogState &state)
{
	const int selected = static_cast<int>(SendMessageW(state.list, LB_GETCURSEL, 0, 0));
	if (selected == LB_ERR || static_cast<std::size_t>(selected) >= state.visible.size())
		return;
	wchar_t levelText[8] {};
	GetWindowTextW(state.level, levelText, static_cast<int>(std::size(levelText)));
	const int level = std::clamp(_wtoi(levelText), 1, 63);
	const bool onlyGood = SendMessageW(state.onlyGood, BM_GETCHECK, 0, 0) == BST_CHECKED;
	const auto &choice = state.items[state.visible[selected]];
	const auto choices = d1hellforge::GetItemGenerationChoices(choice.baseItemId, CurrentCharacter->game, static_cast<uint8_t>(level), onlyGood);
	auto fill = [level](HWND combo, const wchar_t *automatic, const auto &values, const std::string &wanted, std::vector<std::string> &storedValues) {
		SendMessageW(combo, CB_RESETCONTENT, 0, 0);
		storedValues.clear();
		storedValues.emplace_back();
		SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(automatic));
		int selection = 0;
		for (const auto &value : values) {
			SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ToWide(value).c_str()));
			storedValues.push_back(value);
			if (value == wanted) selection = static_cast<int>(SendMessageW(combo, CB_GETCOUNT, 0, 0)) - 1;
		}
		if (selection == 0 && !wanted.empty()) {
			const std::wstring label = ToWide(wanted) + L"  [current; invalid at level " + std::to_wstring(level) + L"]";
			SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
			storedValues.push_back(wanted);
			selection = static_cast<int>(SendMessageW(combo, CB_GETCOUNT, 0, 0)) - 1;
		}
		SendMessageW(combo, CB_SETCURSEL, selection, 0);
	};
	fill(state.prefix, L"Any compatible prefix", choices.prefixes, state.model.generation.prefixName, state.prefixValues);
	fill(state.suffix, L"Any compatible suffix", choices.suffixes, state.model.generation.suffixName, state.suffixValues);
	fill(state.unique, L"Any compatible unique", choices.uniques, state.model.generation.uniqueName, state.uniqueValues);
	EnableWindow(state.prefix, !choices.prefixes.empty());
	EnableWindow(state.suffix, !choices.suffixes.empty());
	EnableWindow(state.unique, !choices.uniques.empty());
}

void UpdateCatalogPreview(ItemCatalogDialogState &state)
{
	const int selected = static_cast<int>(SendMessageW(state.list, LB_GETCURSEL, 0, 0));
	const auto options = CatalogOptions(state);
	if (selected == LB_ERR || static_cast<std::size_t>(selected) >= state.visible.size() || !options.has_value()) {
		d1hellforge::SetItemDisplayText(state.preview, { L"Choose an item, a seed, and a generation level from 1 to 63." });
		return;
	}
	const auto &choice = state.items[state.visible[selected]];
	state.model.workingItem.baseItemId = choice.baseItemId;
	state.model.generation = *options;
	const bool showOriginal = state.model.originalItem.has_value()
	    && (state.model.mode == d1hellforge::WorkshopMode::Reforge || (state.model.mode == d1hellforge::WorkshopMode::Edit && !state.model.dirty));
	const auto generated = showOriginal
	    ? std::expected<d1hellforge::CharacterSummary::PackedItemSummary, std::string> { *state.model.originalItem }
	    : d1hellforge::VanillaItemRules::Generate(state.model);
	if (!generated.has_value()) {
		d1hellforge::SetItemDisplayText(state.preview, { ToWide(generated.error()) });
		return;
	}
	const auto &item = *generated;
	if (state.spriteBitmap != nullptr) {
		DeleteObject(state.spriteBitmap);
		state.spriteBitmap = nullptr;
	}
	if (CurrentCharacter->gameGraphicsAvailable) {
		const auto rendered = d1hellforge::RenderItemPreview(*CurrentCharacter, item.cursorGraphic);
		if (rendered.has_value()) {
			BITMAPINFO info {};
			info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			info.bmiHeader.biWidth = rendered->width;
			info.bmiHeader.biHeight = -rendered->height;
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = 32;
			info.bmiHeader.biCompression = BI_RGB;
			void *pixels = nullptr;
			state.spriteBitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
			if (state.spriteBitmap != nullptr && pixels != nullptr) {
				std::memcpy(pixels, rendered->pixels.data(), rendered->pixels.size() * sizeof(uint32_t));
				SendMessageW(state.sprite, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(state.spriteBitmap));
			}
		}
	}
	std::vector<std::wstring> detailLines { ToWide(item.displayName.empty() ? item.baseName : item.displayName),
		std::format(L"Seed: {} (0x{:08X})", item.seed, item.seed) };
	for (const auto &line : item.detailLines) detailLines.push_back(ToWide(line));
	if (item.detailLines.empty()) detailLines.push_back(L"Normal game-native item");
	const auto validation = d1hellforge::VanillaItemRules::Validate(state.model);
	if (!validation.errors.empty()) detailLines.push_back(L"WARNING: " + ToWide(validation.errors.front()));
	if (item.activeGameIdentityDiffers) {
		detailLines.push_back(L"Stored expanded name: " + ToWide(item.storedActiveGameName.empty() ? item.displayName : item.storedActiveGameName));
		detailLines.push_back(L"DevilutionX reconstruction: " + ToWide(item.reconstructedDisplayName));
		detailLines.push_back(L"WARNING: Expanded and compact identities differ");
	}
	d1hellforge::SetItemDisplayText(state.preview, detailLines);
}

void RefreshCatalogList(ItemCatalogDialogState &state)
{
	SendMessageW(state.list, LB_RESETCONTENT, 0, 0);
	state.visible.clear();
	const int categoryIndex = static_cast<int>(SendMessageW(state.category, CB_GETCURSEL, 0, 0));
	const std::wstring selected = categoryIndex >= 0 ? state.categories[categoryIndex] : L"All items";
	wchar_t searchText[256] {};
	GetWindowTextW(state.search, searchText, static_cast<int>(std::size(searchText)));
	const std::wstring query = Lowercase(searchText);
	for (std::size_t index = 0; index < state.items.size(); ++index) {
		if (selected != L"All items" && ToWide(state.items[index].category) != selected)
			continue;
		if (!query.empty() && Lowercase(ToWide(state.items[index].name)).find(query) == std::wstring::npos)
			continue;
		state.visible.push_back(index);
		SendMessageW(state.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ToWide(state.items[index].name).c_str()));
	}
	int selection = 0;
	if (state.initialBaseItemId.has_value()) {
		const auto found = std::find_if(state.visible.begin(), state.visible.end(), [&state](std::size_t index) {
			return state.items[index].baseItemId == *state.initialBaseItemId;
		});
		if (found != state.visible.end()) selection = static_cast<int>(std::distance(state.visible.begin(), found));
		state.initialBaseItemId.reset();
	}
	if (!state.visible.empty()) SendMessageW(state.list, LB_SETCURSEL, selection, 0);
	RefreshGenerationChoices(state);
	UpdateCatalogPreview(state);
}

LRESULT CALLBACK ItemCatalogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto *state = reinterpret_cast<ItemCatalogDialogState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if (message == WM_CREATE) {
		d1hellforge::InheritDiabloTheme(window);
		state = static_cast<ItemCatalogDialogState *>(reinterpret_cast<CREATESTRUCTW *>(lParam)->lpCreateParams);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
		CreateWindowW(L"STATIC", L"Category", WS_CHILD | WS_VISIBLE, 16, 15, 70, 22, window, nullptr, nullptr, nullptr);
		state->category = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 88, 12, 220, 300, window, reinterpret_cast<HMENU>(2001), nullptr, nullptr);
		CreateWindowW(L"STATIC", L"Search", WS_CHILD | WS_VISIBLE, 330, 15, 55, 22, window, nullptr, nullptr, nullptr);
		state->search = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 387, 12, 255, 24, window, reinterpret_cast<HMENU>(2003), nullptr, nullptr);
		state->list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 16, 48, 350, 455, window, reinterpret_cast<HMENU>(2002), nullptr, nullptr);
		CreateWindowW(L"STATIC", L"Seed", WS_CHILD | WS_VISIBLE, 382, 50, 50, 22, window, nullptr, nullptr, nullptr);
		const uint32_t initialSeed = state->model.generation.seed;
		state->seed = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(initialSeed).c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, 432, 47, 210, 24, window, reinterpret_cast<HMENU>(2004), nullptr, nullptr);
		CreateWindowW(L"BUTTON", L"Previous", WS_CHILD | WS_VISIBLE, 382, 80, 76, 26, window, reinterpret_cast<HMENU>(2005), nullptr, nullptr);
		CreateWindowW(L"BUTTON", L"Next", WS_CHILD | WS_VISIBLE, 464, 80, 70, 26, window, reinterpret_cast<HMENU>(2006), nullptr, nullptr);
		CreateWindowW(L"BUTTON", L"Randomize", WS_CHILD | WS_VISIBLE, 540, 80, 102, 26, window, reinterpret_cast<HMENU>(2007), nullptr, nullptr);
		state->sprite = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE, 456, 118, 112, 112, window, nullptr, nullptr, nullptr);
		state->preview = d1hellforge::CreateItemDisplayControl(window, 382, 240, 260, 263);
		CreateWindowW(L"STATIC", L"Quality", WS_CHILD | WS_VISIBLE, 660, 121, 55, 22, window, nullptr, nullptr, nullptr);
		state->quality[0] = CreateWindowW(L"BUTTON", L"Normal", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 718, 118, 70, 24, window, reinterpret_cast<HMENU>(2008), nullptr, nullptr);
		state->quality[1] = CreateWindowW(L"BUTTON", L"Magic", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 790, 118, 65, 24, window, reinterpret_cast<HMENU>(2009), nullptr, nullptr);
		state->quality[2] = CreateWindowW(L"BUTTON", L"Unique", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 857, 118, 70, 24, window, reinterpret_cast<HMENU>(2010), nullptr, nullptr);
		const int qualityIndex = static_cast<int>(state->model.generation.quality);
		for (int index = 0; index < 3; ++index)
			SendMessageW(state->quality[index], BM_SETCHECK, index == qualityIndex ? BST_CHECKED : BST_UNCHECKED, 0);
		CreateWindowW(L"STATIC", L"Level", WS_CHILD | WS_VISIBLE, 660, 155, 45, 22, window, nullptr, nullptr, nullptr);
		state->level = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(state->model.generation.level).c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_RIGHT, 718, 152, 55, 24, window, reinterpret_cast<HMENU>(2011), nullptr, nullptr);
		state->onlyGood = CreateWindowW(L"BUTTON", L"Exclude cursed affixes", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 785, 152, 160, 24, window, reinterpret_cast<HMENU>(2012), nullptr, nullptr);
		SendMessageW(state->onlyGood, BM_SETCHECK, state->model.generation.onlyGood ? BST_CHECKED : BST_UNCHECKED, 0);
		CreateWindowW(L"STATIC", L"Effect focus", WS_CHILD | WS_VISIBLE, 660, 187, 82, 22, window, nullptr, nullptr, nullptr);
		state->effect = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 748, 184, 197, 220, window, reinterpret_cast<HMENU>(2013), nullptr, nullptr);
		for (const wchar_t *label : { L"Any effect", L"Damage", L"Chance to hit", L"Attributes", L"Resistances", L"Life or mana", L"Attack speed" })
			SendMessageW(state->effect, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
		SendMessageW(state->effect, CB_SETCURSEL, 0, 0);
		CreateWindowW(L"STATIC", L"Prefix", WS_CHILD | WS_VISIBLE, 660, 217, 82, 22, window, nullptr, nullptr, nullptr);
		state->prefix = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 748, 214, 197, 260, window, reinterpret_cast<HMENU>(2014), nullptr, nullptr);
		CreateWindowW(L"STATIC", L"Suffix", WS_CHILD | WS_VISIBLE, 660, 247, 82, 22, window, nullptr, nullptr, nullptr);
		state->suffix = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 748, 244, 197, 260, window, reinterpret_cast<HMENU>(2015), nullptr, nullptr);
		CreateWindowW(L"STATIC", L"Named unique", WS_CHILD | WS_VISIBLE, 660, 277, 82, 22, window, nullptr, nullptr, nullptr);
		state->unique = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 748, 274, 197, 260, window, reinterpret_cast<HMENU>(2016), nullptr, nullptr);
		state->categories.push_back(L"All items");
		for (const auto &item : state->items) {
			const auto category = ToWide(item.category);
			if (std::find(state->categories.begin(), state->categories.end(), category) == state->categories.end()) state->categories.push_back(category);
		}
		std::sort(state->categories.begin() + 1, state->categories.end());
		for (const auto &category : state->categories) SendMessageW(state->category, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(category.c_str()));
		SendMessageW(state->category, CB_SETCURSEL, 0, 0);
		RefreshCatalogList(*state);
		state->identified = CreateWindowW(L"BUTTON", L"Identified", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 205, 520, 105, 24, window, reinterpret_cast<HMENU>(2033), nullptr, nullptr);
		SendMessageW(state->identified, BM_SETCHECK, state->model.workingItem.identified ? BST_CHECKED : BST_UNCHECKED, 0);
		CreateWindowW(L"BUTTON", L"Create Item", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 680, 553, 112, 30, window, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
		SetWindowTextW(GetDlgItem(window, IDOK), state->model.mode == d1hellforge::WorkshopMode::Create ? L"Create Item" : state->model.mode == d1hellforge::WorkshopMode::Edit ? L"Apply Changes" : L"Apply Selected");
		CreateWindowW(L"BUTTON", L"Reset to Original", WS_CHILD | WS_VISIBLE, 552, 553, 120, 30, window, reinterpret_cast<HMENU>(2017), nullptr, nullptr);
		ShowWindow(GetDlgItem(window, 2017), state->model.originalItem.has_value() ? SW_SHOW : SW_HIDE);
		CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 800, 553, 120, 30, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
		if (state->model.mode == d1hellforge::WorkshopMode::Edit)
			CreateWindowW(L"BUTTON", L"I'm Feeling Lucky...", WS_CHILD | WS_VISIBLE, 382, 553, 160, 30, window, reinterpret_cast<HMENU>(2034), nullptr, nullptr);
		if (state->model.mode == d1hellforge::WorkshopMode::Reforge) {
			ShowWindow(GetDlgItem(window, IDOK), SW_HIDE);
			CreateWindowW(L"BUTTON", L"REFORGE", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 16, 580, 928, 172, window, nullptr, nullptr, nullptr);
			const wchar_t *labels[] { L"Base", L"Quality", L"Prefix", L"Suffix", L"Unique" };
			for (int index = 0; index < 5; ++index) {
				state->reforgeLocks[index] = CreateWindowW(L"BUTTON", labels[index], WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 28 + index * 82, 598, 80, 22, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(2020 + index)), nullptr, nullptr);
				SendMessageW(state->reforgeLocks[index], BM_SETCHECK, index < 2 ? BST_CHECKED : BST_UNCHECKED, 0);
			}
			CreateWindowW(L"STATIC", L"Skill", WS_CHILD | WS_VISIBLE, 438, 600, 38, 20, window, nullptr, nullptr, nullptr);
			state->reforgeSkill = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 478, 596, 132, 100, window, reinterpret_cast<HMENU>(2025), nullptr, nullptr);
			for (const wchar_t *label : { L"Novice", L"Competent", L"Master" }) SendMessageW(state->reforgeSkill, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
			SendMessageW(state->reforgeSkill, CB_SETCURSEL, 1, 0);
			CreateWindowW(L"STATIC", L"Preference", WS_CHILD | WS_VISIBLE, 28, 628, 72, 20, window, nullptr, nullptr, nullptr);
			state->reforgePreference = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 102, 624, 200, 210, window, reinterpret_cast<HMENU>(2026), nullptr, nullptr);
			for (const wchar_t *label : { L"Balanced", L"Stronger Magic", L"Stronger Base Roll", L"Maximum Damage", L"Maximum Defense", L"Attributes", L"Resistances", L"Life / Mana", L"Closest to Original" }) SendMessageW(state->reforgePreference, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
			SendMessageW(state->reforgePreference, CB_SETCURSEL, 0, 0);
			CreateWindowW(L"STATIC", L"Engine", WS_CHILD | WS_VISIBLE, 320, 628, 52, 20, window, nullptr, nullptr, nullptr);
			state->reforgeEngine = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 374, 624, 120, 90, window, reinterpret_cast<HMENU>(2035), nullptr, nullptr);
			for (const wchar_t *label : { L"Fast", L"Compatible" }) SendMessageW(state->reforgeEngine, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
			SendMessageW(state->reforgeEngine, CB_SETCURSEL, PreferredReforgeEngine == d1hellforge::ReforgeEngine::Fast ? 0 : 1, 0);
			state->reforgeResults = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY, 28, 654, 470, 68, window, reinterpret_cast<HMENU>(2027), nullptr, nullptr);
			state->reforgeDetails = d1hellforge::CreateItemDisplayControl(window, 510, 628, 420, 94);
			d1hellforge::SetItemDisplayText(state->reforgeDetails, { L"Select a result to view its complete statistics." });
			state->reforgeStatus = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 28, 726, 700, 20, window, nullptr, nullptr, nullptr);
			state->reforgeSearch = CreateWindowW(L"BUTTON", L"Search / Reforge", WS_CHILD | WS_VISIBLE, 640, 596, 135, 27, window, reinterpret_cast<HMENU>(2028), nullptr, nullptr);
			state->reforgeSearchAgain = CreateWindowW(L"BUTTON", L"Search Again", WS_CHILD | WS_VISIBLE, 785, 596, 135, 27, window, reinterpret_cast<HMENU>(2029), nullptr, nullptr);
			CreateWindowW(L"BUTTON", L"Apply Selected", WS_CHILD | WS_VISIBLE, 760, 724, 170, 28, window, reinterpret_cast<HMENU>(2030), nullptr, nullptr);
			MoveWindow(GetDlgItem(window, 2017), 680, 762, 120, 30, TRUE);
			MoveWindow(GetDlgItem(window, IDCANCEL), 810, 762, 120, 30, TRUE);
			state->reforgeCancel = GetDlgItem(window, IDCANCEL);
		}
		state->enforceRules = CreateWindowW(L"BUTTON", L"Enforce Vanilla Rules", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 16, 520, 180, 24, window, reinterpret_cast<HMENU>(2031), nullptr, nullptr);
		state->grayInvalid = CreateWindowW(L"BUTTON", L"Gray Invalid Options", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 16, 548, 180, 24, window, reinterpret_cast<HMENU>(2032), nullptr, nullptr);
		SendMessageW(state->enforceRules, BM_SETCHECK, state->model.enforceVanillaRules ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(state->grayInvalid, BM_SETCHECK, state->model.grayInvalidOptions ? BST_CHECKED : BST_UNCHECKED, 0);
		UpdateCatalogPreview(*state);
		d1hellforge::AttachThemedChoiceChildren(window);
		return 0;
	}
	if (message == WM_NOTIFY) {
		LRESULT result = 0;
		if (d1hellforge::HandleThemedChoiceCustomDraw(lParam, result))
			return result;
	}
	if (message == WmReforgeProgress && state != nullptr) {
		std::unique_ptr<d1hellforge::ReforgeProgress> progress(reinterpret_cast<d1hellforge::ReforgeProgress *>(lParam));
		const double seconds = progress->elapsedMilliseconds / 1000.0;
		const double rate = seconds > 0 ? progress->candidatesExamined / seconds : 0;
		SetWindowTextW(state->reforgeStatus, std::format(L"Searching... {} / {} seeds — {} native generations — {:.0f} seeds/sec",
		    progress->candidatesExamined, progress->budget, progress->generationCalls, rate).c_str());
		return 0;
	}
	if (message == WmReforgeComplete && state != nullptr) {
		std::unique_ptr<AsyncReforgeResult> completed(reinterpret_cast<AsyncReforgeResult *>(lParam));
		SetReforgeSearching(window, *state, false);
		if (!completed->result.has_value()) {
			if (completed->result.error() == "Reforge search cancelled")
				SetWindowTextW(state->reforgeStatus, L"Search cancelled — original item unchanged.");
			else
				MessageBoxW(window, ToWide(completed->result.error()).c_str(), L"Reforge Search", MB_OK | MB_ICONWARNING);
			return 0;
		}
		const auto &result = *completed->result;
		state->reforgeResult = result;
		state->reforgeRequest.searchOffset = result.nextSearchOffset;
		SendMessageW(state->reforgeResults, LB_RESETCONTENT, 0, 0);
		for (std::size_t index = 0; index < result.topCandidates.size(); ++index) {
			const auto &candidate = result.topCandidates[index];
			const std::wstring row = std::format(L"#{}  {:.1f}%  0x{:08X}  {}", index + 1, candidate.overallScore, candidate.seed, ToWide(candidate.item.displayName));
			SendMessageW(state->reforgeResults, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
		}
		SendMessageW(state->reforgeResults, LB_SETCURSEL, 0, 0);
		UpdateReforgeResultPreview(*state);
		const double seconds = result.elapsedMilliseconds / 1000.0;
		const double rate = seconds > 0 ? result.candidatesExamined / seconds : 0;
		const std::wstring status = completed->engine == d1hellforge::ReforgeEngine::Fast
		    ? std::format(L"Fast — {} attempts — {} native generations — {:.0f} attempts/sec — {:.0f} ms", result.candidatesExamined, result.generationCalls, rate, result.elapsedMilliseconds)
		    : std::format(L"Compatible — {} attempts — {:.0f} attempts/sec — {:.0f} ms", result.candidatesExamined, rate, result.elapsedMilliseconds);
		SetWindowTextW(state->reforgeStatus, status.c_str());
		return 0;
	}
	if (message == WM_COMMAND && state != nullptr) {
		if (LOWORD(wParam) == 2034 && state->model.mode == d1hellforge::WorkshopMode::Edit) {
			const auto edited = d1hellforge::ShowAdvancedItemEditor(window, state->model.workingItem, state->model.game, state->model.enforceVanillaRules);
			if (edited.has_value()) { state->model.workingItem = *edited; state->model.dirty = true; state->created = *edited; DestroyWindow(window); }
			return 0;
		}
		if (LOWORD(wParam) == 2031 || LOWORD(wParam) == 2032) {
			state->model.enforceVanillaRules = SendMessageW(state->enforceRules, BM_GETCHECK, 0, 0) == BST_CHECKED;
			state->model.grayInvalidOptions = SendMessageW(state->grayInvalid, BM_GETCHECK, 0, 0) == BST_CHECKED;
			EnforceVanillaItemRules = state->model.enforceVanillaRules;
			SaveRememberedFolders();
			RefreshCatalogList(*state);
			return 0;
		}
		if ((LOWORD(wParam) == 2028 || LOWORD(wParam) == 2029) && state->model.mode == d1hellforge::WorkshopMode::Reforge) {
			const int selectedBase = static_cast<int>(SendMessageW(state->list, LB_GETCURSEL, 0, 0));
			const auto options = CatalogOptions(*state);
			if (!options.has_value() || selectedBase == LB_ERR || static_cast<std::size_t>(selectedBase) >= state->visible.size()) {
				MessageBoxW(window, L"Choose valid Workshop options before searching.", L"Reforge", MB_OK | MB_ICONWARNING);
				return 0;
			}
			state->model.workingItem.baseItemId = state->items[state->visible[selectedBase]].baseItemId;
			state->model.generation = *options;
			state->reforgeRequest.desired = state->model;
			state->reforgeRequest.generationLevel = options->level;
			state->reforgeRequest.skill = static_cast<d1hellforge::ReforgeSkill>(std::max<LRESULT>(0, SendMessageW(state->reforgeSkill, CB_GETCURSEL, 0, 0)));
			state->reforgeRequest.preference = static_cast<d1hellforge::ReforgePreference>(std::max<LRESULT>(0, SendMessageW(state->reforgePreference, CB_GETCURSEL, 0, 0)));
			state->reforgeRequest.engine = SendMessageW(state->reforgeEngine, CB_GETCURSEL, 0, 0) == 1 ? d1hellforge::ReforgeEngine::Compatible : d1hellforge::ReforgeEngine::Fast;
			PreferredReforgeEngine = state->reforgeRequest.engine;
			SaveRememberedFolders();
			bool *locks[] { &state->reforgeRequest.lockBase, &state->reforgeRequest.lockQuality, &state->reforgeRequest.lockPrefix, &state->reforgeRequest.lockSuffix, &state->reforgeRequest.lockUnique };
			for (int index = 0; index < 5; ++index) *locks[index] = SendMessageW(state->reforgeLocks[index], BM_GETCHECK, 0, 0) == BST_CHECKED;
			if (LOWORD(wParam) == 2028) state->reforgeRequest.searchOffset = 0;
			const d1hellforge::ReforgeRequest request = state->reforgeRequest;
			state->reforgeCancelRequested = std::make_shared<std::atomic_bool>(false);
			const auto cancel = state->reforgeCancelRequested;
			SetReforgeSearching(window, *state, true);
			SetWindowTextW(state->reforgeStatus, L"Starting Reforge search...");
			state->reforgeWorker = std::jthread([window, request, cancel] {
				auto result = d1hellforge::SearchReforgeCandidates(request, [window, cancel](const d1hellforge::ReforgeProgress &progress) {
					if (cancel->load(std::memory_order_relaxed) || !IsWindow(window)) return false;
					auto *update = new d1hellforge::ReforgeProgress(progress);
					if (!PostMessageW(window, WmReforgeProgress, 0, reinterpret_cast<LPARAM>(update))) delete update;
					return !cancel->load(std::memory_order_relaxed);
				});
				auto *completed = new AsyncReforgeResult { std::move(result), request.engine };
				if (!PostMessageW(window, WmReforgeComplete, 0, reinterpret_cast<LPARAM>(completed))) delete completed;
			});
			return 0;
		}
		if (LOWORD(wParam) == 2027 && HIWORD(wParam) == LBN_SELCHANGE) { UpdateReforgeResultPreview(*state); return 0; }
		if (LOWORD(wParam) == 2035 && HIWORD(wParam) == CBN_SELCHANGE) {
			PreferredReforgeEngine = SendMessageW(state->reforgeEngine, CB_GETCURSEL, 0, 0) == 1 ? d1hellforge::ReforgeEngine::Compatible : d1hellforge::ReforgeEngine::Fast;
			SaveRememberedFolders(); return 0;
		}
		if (LOWORD(wParam) == 2030 && state->reforgeResult.has_value()) {
			const int selected = static_cast<int>(SendMessageW(state->reforgeResults, LB_GETCURSEL, 0, 0));
			if (selected >= 0 && static_cast<std::size_t>(selected) < state->reforgeResult->topCandidates.size()) {
				const auto &candidate = state->reforgeResult->topCandidates[selected];
				const std::wstring originalName = ToWide(state->model.originalItem->displayName);
				const std::wstring proposedName = ToWide(candidate.item.displayName);
				const std::wstring comparison = L"Original Item\r\n" + originalName + L"\r\nSeed: " + std::to_wstring(state->model.originalItem->seed)
				    + L"\r\n\r\nReforged Item\r\n" + proposedName + L"\r\nSeed: " + std::to_wstring(candidate.seed)
				    + std::format(L"\r\nScore: {:.1f}%\r\nStatus: Vanilla Reproducible\r\n\r\nApply this selected candidate?", candidate.overallScore);
				if (MessageBoxW(window, comparison.c_str(), L"Apply Reforge Candidate", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return 0;
				const bool identified = SendMessageW(state->identified, BM_GETCHECK, 0, 0) == BST_CHECKED;
				const auto updated = WithIdentifiedState(candidate.item, identified);
				if (!updated.has_value()) { MessageBoxW(window, ToWide(updated.error()).c_str(), L"Identified State", MB_OK | MB_ICONWARNING); return 0; }
				state->created = *updated;
				DestroyWindow(window);
			}
			return 0;
		}
		if (LOWORD(wParam) == 2017) {
			d1hellforge::ResetWorkshopModel(state->model);
			state->initialBaseItemId = state->model.workingItem.baseItemId;
			SetWindowTextW(state->seed, std::to_wstring(state->model.generation.seed).c_str());
			RefreshCatalogList(*state);
			return 0;
		}
		if (LOWORD(wParam) == 2001 && HIWORD(wParam) == CBN_SELCHANGE) { RefreshCatalogList(*state); return 0; }
		if (LOWORD(wParam) == 2003 && HIWORD(wParam) == EN_CHANGE) { RefreshCatalogList(*state); return 0; }
		if (LOWORD(wParam) == 2002 && HIWORD(wParam) == LBN_SELCHANGE) { state->model.dirty = true; RefreshGenerationChoices(*state); UpdateCatalogPreview(*state); return 0; }
		if (LOWORD(wParam) == 2004 && HIWORD(wParam) == EN_CHANGE) { UpdateCatalogPreview(*state); return 0; }
		if ((LOWORD(wParam) >= 2008 && LOWORD(wParam) <= 2010 && HIWORD(wParam) == BN_CLICKED)
		    || (LOWORD(wParam) == 2011 && HIWORD(wParam) == EN_CHANGE)
		    || (LOWORD(wParam) == 2012 && HIWORD(wParam) == BN_CLICKED)) { RefreshGenerationChoices(*state); UpdateCatalogPreview(*state); return 0; }
		if ((LOWORD(wParam) >= 2013 && LOWORD(wParam) <= 2016) && HIWORD(wParam) == CBN_SELCHANGE) { UpdateCatalogPreview(*state); return 0; }
		if (LOWORD(wParam) >= 2005 && LOWORD(wParam) <= 2007) {
			uint32_t seed = CatalogSeed(*state).value_or(0);
			if (LOWORD(wParam) == 2005) --seed;
			else if (LOWORD(wParam) == 2006) ++seed;
			else seed = static_cast<uint32_t>(GetTickCount64() ^ reinterpret_cast<uintptr_t>(window));
			SetWindowTextW(state->seed, std::to_wstring(seed).c_str());
			return 0;
		}
		if (LOWORD(wParam) == IDCANCEL) {
			if (state->reforgeSearching && state->reforgeCancelRequested != nullptr) {
				state->reforgeCancelRequested->store(true, std::memory_order_relaxed);
				SetWindowTextW(state->reforgeStatus, L"Cancelling search...");
				EnableWindow(state->reforgeCancel, FALSE);
				return 0;
			}
			DestroyWindow(window); return 0;
		}
		if (LOWORD(wParam) == IDOK || (LOWORD(wParam) == 2002 && HIWORD(wParam) == LBN_DBLCLK)) {
			if (state->model.mode == d1hellforge::WorkshopMode::Reforge) return 0;
			const int selected = static_cast<int>(SendMessageW(state->list, LB_GETCURSEL, 0, 0));
			if (selected == LB_ERR || static_cast<std::size_t>(selected) >= state->visible.size()) return 0;
			const auto &choice = state->items[state->visible[selected]];
			const auto options = CatalogOptions(*state);
			if (!options.has_value()) { MessageBoxW(window, L"Enter a seed from 0 to 4294967295 and a level from 1 to 63.", L"Invalid Generation Options", MB_OK | MB_ICONWARNING); return 0; }
			state->model.workingItem.baseItemId = choice.baseItemId;
			state->model.generation = *options;
			const auto created = state->model.mode != d1hellforge::WorkshopMode::Create && !state->model.dirty && state->model.originalItem.has_value()
			    ? std::expected<d1hellforge::CharacterSummary::PackedItemSummary, std::string> { *state->model.originalItem }
			    : d1hellforge::VanillaItemRules::Generate(state->model);
			if (!created.has_value()) { MessageBoxW(window, ToWide(created.error()).c_str(), L"Create Item failed", MB_OK | MB_ICONWARNING); return 0; }
			const bool identified = SendMessageW(state->identified, BM_GETCHECK, 0, 0) == BST_CHECKED;
			const auto updated = WithIdentifiedState(*created, identified);
			if (!updated.has_value()) { MessageBoxW(window, ToWide(updated.error()).c_str(), L"Identified State", MB_OK | MB_ICONWARNING); return 0; }
			state->created = *updated;
			DestroyWindow(window);
			return 0;
		}
	}
	if (message == WM_DESTROY && state != nullptr) {
		if (state->reforgeCancelRequested != nullptr) state->reforgeCancelRequested->store(true, std::memory_order_relaxed);
		if (state->reforgeWorker.joinable()) state->reforgeWorker.join();
		if (state->spriteBitmap != nullptr) { DeleteObject(state->spriteBitmap); state->spriteBitmap = nullptr; }
		return 0;
	}
	if (message == WM_CLOSE) {
		if (state != nullptr && state->reforgeSearching && state->reforgeCancelRequested != nullptr) {
			state->reforgeCancelRequested->store(true, std::memory_order_relaxed);
			SetWindowTextW(state->reforgeStatus, L"Cancelling search...");
			return 0;
		}
		DestroyWindow(window); return 0;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

void OpenItemWorkshop(HWND owner, d1hellforge::WorkshopMode mode)
{
	const bool stashTarget = ShowingStash && CurrentStash.has_value();
	std::optional<d1hellforge::CharacterSummary::PackedItemSummary> selectedStorage;
	if (mode != d1hellforge::WorkshopMode::Create) {
		if (stashTarget)
			selectedStorage = SelectedStashSummary();
		else if (const auto *selected = SelectedItem(); selected != nullptr)
			selectedStorage = *selected;
		if (!selectedStorage.has_value()) return;
	}
	const auto &profile = stashTarget ? CurrentStash->contentProfile : CurrentCharacter->contentProfile;
	ItemCatalogDialogState state;
	state.model = mode == d1hellforge::WorkshopMode::Create
	    ? d1hellforge::MakeCreateWorkshopModel(profile, CurrentCharacter->level, EnforceVanillaItemRules)
	    : d1hellforge::MakeInventoryWorkshopModel(mode, profile, CurrentCharacter->level, *selectedStorage, EnforceVanillaItemRules);
	state.items = d1hellforge::GetItemCatalog(profile);
	if (mode != d1hellforge::WorkshopMode::Create) {
		state.initialBaseItemId = selectedStorage->baseItemId;
	}
	if (state.items.empty()) { MessageBoxW(owner, L"The DevilutionX item catalog could not be loaded.", L"Make Item", MB_OK | MB_ICONWARNING); return; }
	static bool registered = false;
	if (!registered) {
		WNDCLASSW cls {};
		cls.lpfnWndProc = ItemCatalogProc;
		cls.hInstance = GetModuleHandleW(nullptr);
		cls.lpszClassName = L"D1HellforgeItemCatalog";
		cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
		cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
		registered = RegisterClassW(&cls) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}
	EnableWindow(owner, FALSE);
	const wchar_t *title = mode == d1hellforge::WorkshopMode::Create ? L"Item Workshop — Create" : mode == d1hellforge::WorkshopMode::Edit ? L"Item Workshop — Edit" : L"Item Workshop — Reforge";
	HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"D1HellforgeItemCatalog", title, WS_CAPTION | WS_SYSMENU,
	    CW_USEDEFAULT, CW_USEDEFAULT, WorkshopWindowWidth, WorkshopWindowHeight, owner, nullptr, GetModuleHandleW(nullptr), &state);
	ShowWindow(dialog, SW_SHOW);
	MSG message;
	while (IsWindow(dialog) && GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
	EnableWindow(owner, TRUE);
	SetForegroundWindow(owner);
	if (!state.created.has_value()) return;
	if (stashTarget) {
		auto replacement = *state.created;
		if (replacement.fullItemRecord.empty()) {
			auto model = d1hellforge::MakeAdvancedItemEditorModel(replacement, profile.baseMode, EnforceVanillaItemRules);
			if (!model.has_value()) {
				MessageBoxW(owner, ToWide(model.error()).c_str(), L"Workshop Item", MB_OK | MB_ICONWARNING);
				return;
			}
			auto materialized = d1hellforge::ApplyAdvancedItemEditorModel(*model, replacement);
			if (!materialized.has_value()) {
				MessageBoxW(owner, ToWide(materialized.error()).c_str(), L"Workshop Item", MB_OK | MB_ICONWARNING);
				return;
			}
			replacement = std::move(*materialized);
		}
		if (mode == d1hellforge::WorkshopMode::Create) {
			const auto footprint = d1hellforge::GetItemFootprint(*CurrentCharacter, replacement.cursorGraphic);
			if (!footprint.has_value()) {
				MessageBoxW(owner, ToWide(footprint.error()).c_str(), L"Make Item", MB_OK | MB_ICONWARNING);
				return;
			}
			const auto added = d1hellforge::AddStashItem(*CurrentStash, std::move(replacement.fullItemRecord),
			    static_cast<unsigned>(footprint->first), static_cast<unsigned>(footprint->second), CurrentStash->selectedPage);
			if (!added.has_value()) {
				MessageBoxW(owner, ToWide(added.error()).c_str(), L"Make Item", MB_OK | MB_ICONWARNING);
				return;
			}
			SelectedStashItem = *added;
			StashDirty = true;
			RefreshStashDetails();
			return;
		}
		auto &stored = CurrentStash->items[SelectedStashItem - 1].fullItemRecord;
		if (replacement.fullItemRecord.size() != stored.size()) {
			MessageBoxW(owner, L"The Workshop result does not match this stash's item-record format.", L"Workshop Item", MB_OK | MB_ICONWARNING);
			return;
		}
		// Placement coordinates belong to the storage adapter, not item generation.
		std::copy(stored.begin() + 12, stored.begin() + 20, replacement.fullItemRecord.begin() + 12);
		stored = std::move(replacement.fullItemRecord);
		StashDirty = true;
		RefreshStashDetails();
		return;
	}
	if (mode == d1hellforge::WorkshopMode::Create) {
		PlaceItemInBackpackPreview(owner, *state.created, L"CREATED");
		return;
	}
	auto *selected = const_cast<d1hellforge::CharacterSummary::PackedItemSummary *>(SelectedItem());
	if (selected == nullptr) return;
	const uint8_t slot = selected->slot;
	*selected = *state.created;
	selected->slot = slot;
	UpdateInventoryGraphic(*CurrentCharacter);
	Layout(owner);
	SetWindowTextW(InventoryInfo, mode == d1hellforge::WorkshopMode::Edit
	    ? L"WORKSHOP EDIT PREVIEW\r\n\r\nThe selected item was changed only after Apply Changes.\r\n\r\nChoose Save Inventory Changes to commit, or Refresh to discard."
	    : L"REFORGE PREVIEW\r\n\r\nThe selected item was rebuilt with game-native Workshop rules.\r\n\r\nChoose Save Inventory Changes to commit, or Refresh to discard.");
}

void SaveInventoryChanges(HWND owner)
{
	if (!CurrentCharacter.has_value()) return;
	const std::filesystem::path savePath = CurrentCharacter->path;
	const auto result = d1hellforge::SaveCharacterInventory(savePath, *CurrentCharacter);
	if (!result.has_value()) {
		MessageBoxW(owner, ToWide(result.error()).c_str(), L"Inventory save failed", MB_OK | MB_ICONERROR);
		return;
	}
	const std::wstring message = L"Inventory saved and verified.\r\n\r\nBackup:\r\n" + result->backupPath.wstring();
	MessageBoxW(owner, message.c_str(), L"D1Hellforge", MB_OK | MB_ICONINFORMATION);
	LoadSave(savePath);
}

void ImportItem(HWND owner)
{
	if (!CurrentCharacter.has_value()) {
		MessageBoxW(owner, L"Load a character save before importing an item.", L"Import Item", MB_OK | MB_ICONINFORMATION);
		return;
	}
	wchar_t path[MAX_PATH] {};
	OPENFILENAMEW dialog {};
	if (ItemDirectory.empty()) ItemDirectory = DefaultItemLibraryDirectory();
	const std::wstring initialDirectory = std::filesystem::is_directory(ItemDirectory) ? ItemDirectory.wstring() : std::wstring {};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = owner;
	dialog.lpstrFilter = L"Supported item files (*.dxitem;*.itm;*.hif)\0*.dxitem;*.itm;*.hif\0All files (*.*)\0*.*\0";
	dialog.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
	dialog.lpstrFile = path;
	dialog.nMaxFile = MAX_PATH;
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	if (!GetOpenFileNameW(&dialog))
		return;
	ItemDirectory = std::filesystem::path(path).parent_path();
	SaveRememberedFolders();
	auto imported = d1hellforge::ImportItemFile(path);
	if (!imported.has_value()) {
		MessageBoxW(owner, ToWide(imported.error()).c_str(), L"Import failed", MB_OK | MB_ICONWARNING);
		return;
	}
	const auto expectedGame = CurrentCharacter->game == d1hellforge::Game::Hellfire ? devilution::ItemGame::Hellfire : devilution::ItemGame::Diablo;
	if (imported->game != devilution::ItemGame::Unknown && imported->game != expectedGame) {
		const auto sourceGame = imported->game == devilution::ItemGame::Hellfire ? d1hellforge::Game::Hellfire : d1hellforge::Game::Diablo;
		const auto sourceSummary = d1hellforge::SummarizeImportedItem(imported->packedItem, sourceGame);
		const std::wstring name = sourceSummary.has_value() ? ToWide(sourceSummary->displayName.empty() ? sourceSummary->baseName : sourceSummary->displayName) : L"this item";
		const auto converted = d1hellforge::ConvertPackedItem(imported->packedItem, sourceGame, CurrentCharacter->game);
		if (!converted.has_value()) {
			MessageBoxW(owner, ToWide(converted.error()).c_str(), L"Conversion failed", MB_OK | MB_ICONWARNING);
			return;
		}
		const auto destinationSummary = d1hellforge::SummarizeImportedItem(*converted, CurrentCharacter->game);
		const std::wstring destinationName = destinationSummary.has_value() ? ToWide(destinationSummary->displayName) : L"unknown item";
		std::wstring question = L"Convert " + name + L" from Diablo to Hellfire?\r\n\r\nHellfire reconstruction: " + destinationName;
		if (!imported->legacyStoredName.empty())
			question += L"\r\nLegacy stored name: " + ToWide(imported->legacyStoredName);
		if (destinationName != name || (!imported->legacyStoredName.empty() && ToWide(imported->legacyStoredName) != destinationName))
			question += L"\r\n\r\nThe legacy name/effects and compact generation data disagree. DevilutionX will use the Hellfire reconstruction shown above.";
		question += L"\r\n\r\nContinue with this reconstructed item?";
		if (CurrentCharacter->game != d1hellforge::Game::Hellfire || MessageBoxW(owner, question.c_str(), L"Legacy Item Reconstruction", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
			return;
		imported->packedItem = *converted;
		imported->game = expectedGame;
	}
	auto summary = d1hellforge::SummarizeImportedItem(imported->packedItem, CurrentCharacter->game);
	if (!summary.has_value()) {
		MessageBoxW(owner, ToWide(summary.error()).c_str(), L"Import failed", MB_OK | MB_ICONWARNING);
		return;
	}
	if (!imported->fullItemRecord.empty() && imported->game == expectedGame) {
		summary->fullItemRecord = imported->fullItemRecord;
		const auto refreshed = d1hellforge::RefreshSummaryFromFullItemRecord(*summary, CurrentCharacter->game);
		if (!refreshed.has_value()) {
			MessageBoxW(owner, ToWide(refreshed.error()).c_str(), L"Import failed", MB_OK | MB_ICONWARNING);
			return;
		}
	}
	PlaceItemInBackpackPreview(owner, *summary, L"IMPORTED");
}

void Layout(HWND window)
{
	RECT area;
	GetClientRect(window, &area);
	constexpr int Margin = 12;
	constexpr int ButtonHeight = 38;
	constexpr int ListWidth = 245;
	constexpr int RightX = Margin + ListWidth + 12;
	const int rightWidth = area.right - (Margin * 2 + ListWidth + 12);
	constexpr int ToolbarButtonWidth = 122;
	constexpr int ToolbarGap = 8;
	const int toolbarWidth = ToolbarButtonWidth * 5 + ToolbarGap * 4;
	const int toolbarX = std::max(Margin, (static_cast<int>(area.right) - toolbarWidth) / 2);
	const int toolbarY = std::max(Margin, static_cast<int>(area.bottom) - Margin - ButtonHeight);
	MoveWindow(GetDlgItem(window, IdBrowse), toolbarX, toolbarY, ToolbarButtonWidth, ButtonHeight, TRUE);
	MoveWindow(GetDlgItem(window, IdRefresh), toolbarX + (ToolbarButtonWidth + ToolbarGap), toolbarY, ToolbarButtonWidth, ButtonHeight, TRUE);
	MoveWindow(CharacterViewButton, toolbarX + 2 * (ToolbarButtonWidth + ToolbarGap), toolbarY, ToolbarButtonWidth, ButtonHeight, TRUE);
	MoveWindow(InventoryViewButton, toolbarX + 3 * (ToolbarButtonWidth + ToolbarGap), toolbarY, ToolbarButtonWidth, ButtonHeight, TRUE);
	MoveWindow(StashViewButton, toolbarX + 4 * (ToolbarButtonWidth + ToolbarGap), toolbarY, ToolbarButtonWidth, ButtonHeight, TRUE);
	const int contentBottom = toolbarY - 8;
	const bool inventorySidebar = (ShowingInventory && CurrentCharacter.has_value() && InventoryRenderSource.has_value())
	    || (ShowingStash && CurrentStash.has_value());
	const int contentHeight = std::max(1, contentBottom - Margin);
	const int saveListHeight = inventorySidebar ? std::clamp(contentHeight / 5, 90, 150) : contentHeight;
	MoveWindow(SaveList, Margin, Margin, ListWidth, saveListHeight, TRUE);
	if (inventorySidebar)
		MoveWindow(InventoryInfo, Margin, Margin + saveListHeight + 8, ListWidth, std::max(1, contentHeight - saveListHeight - 8), TRUE);
	const int detailsHeight = std::max(Margin + 120, contentBottom - 170);
	const int detailsTop = ShowingStash ? Margin + 38 : Margin;
	MoveWindow(Details, RightX, detailsTop, rightWidth, std::max(1, detailsHeight - detailsTop), TRUE);
	MoveWindow(StashPreviousButton, RightX, Margin, 100, 28, TRUE);
	MoveWindow(StashPageLabel, RightX + 110, Margin + 4, std::max(100, rightWidth - 220), 22, TRUE);
	MoveWindow(StashNextButton, std::max(RightX + 220, static_cast<int>(area.right) - Margin - 100), Margin, 100, 28, TRUE);
	const int imageTop = ShowingStash ? Margin + 38 : Margin;
	const int imageAreaHeight = std::max(1, contentBottom - imageTop);
	if (ShowingStash) {
		constexpr int StashWidth = 320;
		constexpr int StashHeight = 352;
		const double scale = std::min(static_cast<double>(rightWidth) / StashWidth, static_cast<double>(imageAreaHeight) / StashHeight);
		InventoryBitmapWidth = std::max(1, static_cast<int>(StashWidth * scale));
		InventoryBitmapHeight = std::max(1, static_cast<int>(StashHeight * scale));
	} else {
		UpdateScaledInventoryBitmap(std::max(1, rightWidth), imageAreaHeight);
	}
	MoveWindow(InventoryImage,
	    RightX + std::max(0, (rightWidth - InventoryBitmapWidth) / 2),
	    imageTop + std::max(0, (imageAreaHeight - InventoryBitmapHeight) / 2),
	    std::max(1, InventoryBitmapWidth), std::max(1, InventoryBitmapHeight), TRUE);
	for (int i = 0; i < 5; ++i) {
		MoveWindow(StatLabels[i], RightX, detailsHeight + 8 + i * 28, 190, 22, TRUE);
		MoveWindow(StatEdits[i], RightX + 195, detailsHeight + 5 + i * 28, 70, 24, TRUE);
	}
	MoveWindow(SaveButton, RightX + 280, detailsHeight + 5, 180, 32, TRUE);
	MoveWindow(AutoCapCheckbox, RightX + 280, detailsHeight + 43, std::max(180, rightWidth - 280), 42, TRUE);
	ShowSelectedView();
}

int HitTestInventoryItem(int x, int y)
{
	if (!InventoryRenderSource.has_value() || InventoryBitmapWidth <= 0 || InventoryBitmapHeight <= 0)
		return -1;
	const int sourceX = x * InventoryRenderSource->width / InventoryBitmapWidth;
	const int sourceY = y * InventoryRenderSource->height / InventoryBitmapHeight;
	for (int index = static_cast<int>(InventoryRenderSource->items.size()) - 1; index >= 0; --index) {
		const auto &region = InventoryRenderSource->items[index];
		if (sourceX >= region.x && sourceX < region.x + region.width
		    && sourceY >= region.y && sourceY < region.y + region.height)
			return index;
	}
	return -1;
}

void SelectInventoryItemAt(HWND control, int x, int y)
{
	SelectedInventoryItem = HitTestInventoryItem(x, y);
	if (SelectedInventoryItem >= 0) {
		const auto &region = InventoryRenderSource->items[SelectedInventoryItem];
		const auto *item = SelectedItem();
		const char *area = region.area == d1hellforge::InventoryArea::Equipment ? "Equipment" : region.area == d1hellforge::InventoryArea::Belt ? "Belt" : "Backpack";
		std::wstring itemText;
		if (item != nullptr) {
			AppendSelectedItemDetails(itemText, *item, L"ITEM DETAILS", ToWide(std::format("Location: {} slot {}", area, region.slot + 1)));
		} else {
			itemText = L"ITEM DETAILS\r\n" + ToWide(region.description);
		}
		itemText += L"\r\n\r\nDrag to move. Right-click for item actions.";
		SetStyledDisplayText(InventoryInfo, itemText);
		InvalidateRect(InventoryInfo, nullptr, TRUE);
	} else {
		RestoreInventoryInfo();
	}
	ClearInventoryBitmap();
	RECT rect;
	GetClientRect(control, &rect);
	UpdateScaledInventoryBitmap(rect.right, rect.bottom);
	InvalidateRect(control, nullptr, TRUE);
}

void ShowInventoryContextMenu(HWND control, int x, int y)
{
	const int item = HitTestInventoryItem(x, y);
	SelectInventoryItemAt(control, x, y);

	HMENU menu = CreatePopupMenu();
	if (menu == nullptr)
		return;
	AppendMenuW(menu, MF_STRING, IdImportItem, L"Import Item...");
	AppendMenuW(menu, MF_STRING | (item >= 0 ? MF_ENABLED : MF_GRAYED), IdExportItem, L"Export Item...");
	AppendMenuW(menu, MF_STRING | (item >= 0 ? MF_ENABLED : MF_GRAYED), IdCopyItem, L"Copy Item");
	AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(menu, MF_STRING | (item >= 0 ? MF_ENABLED : MF_GRAYED), IdEditItem, L"Workshop...");
	AppendMenuW(menu, MF_STRING | (item >= 0 ? MF_ENABLED : MF_GRAYED), IdReforgeItem, L"Reforge...");
	const auto *selectedItem = SelectedItem();
	AppendMenuW(menu, MF_STRING | (selectedItem != nullptr && selectedItem->activeGameIdentityDiffers ? MF_ENABLED : MF_GRAYED), IdAttemptItemFix, L"Attempt Fix...");
	AppendMenuW(menu, MF_STRING | (item >= 0 ? MF_ENABLED : MF_GRAYED), IdDeleteItem, L"Delete Item...");
	AppendMenuW(menu, MF_STRING, IdMakeItem, L"Make Item from Catalog...");
	AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(menu, MF_STRING, IdSaveInventory, L"Save Inventory Changes");

	POINT screenPoint { x, y };
	ClientToScreen(control, &screenPoint);
	const int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
	    screenPoint.x, screenPoint.y, 0, GetParent(control), nullptr);
	DestroyMenu(menu);

	HWND owner = GetParent(control);
	if (command == IdImportItem)
		ImportItem(owner);
	else if (command == IdExportItem)
		ExportSelectedItem(owner);
	else if (command == IdCopyItem)
		CopySelectedItem(owner);
	else if (command == IdEditItem)
		EditSelectedItem(owner);
	else if (command == IdReforgeItem)
		OpenItemWorkshop(owner, d1hellforge::WorkshopMode::Reforge);
	else if (command == IdAttemptItemFix)
		OpenItemWorkshop(owner, d1hellforge::WorkshopMode::Reforge);
	else if (command == IdDeleteItem)
		DeleteSelectedItem(owner);
	else if (command == IdSaveInventory)
		SaveInventoryChanges(owner);
	else if (command == IdMakeItem)
		OpenItemWorkshop(owner, d1hellforge::WorkshopMode::Create);
}

void RebuildInventoryBitmap(HWND control)
{
	ClearInventoryBitmap();
	RECT rect;
	GetClientRect(control, &rect);
	UpdateScaledInventoryBitmap(rect.right, rect.bottom);
	InvalidateRect(control, nullptr, TRUE);
}

const d1hellforge::CharacterSummary::PackedItemSummary *DraggedItem()
{
	if (!CurrentCharacter.has_value())
		return nullptr;
	const auto &items = DragSourceArea == d1hellforge::InventoryArea::Equipment ? CurrentCharacter->equipment
	    : DragSourceArea == d1hellforge::InventoryArea::Belt                         ? CurrentCharacter->belt
	                                                                               : CurrentCharacter->inventory;
	const auto found = std::find_if(items.begin(), items.end(), [](const auto &item) { return item.slot == DragItemSlot; });
	return found == items.end() ? nullptr : &*found;
}

int EquipmentSlotAt(int x, int y)
{
	constexpr std::array<RECT, 7> Rects {{
	    { 132, 4, 190, 62 }, { 47, 176, 76, 205 }, { 248, 176, 277, 205 },
	    { 203, 31, 235, 63 }, { 16, 75, 74, 162 }, { 247, 75, 305, 162 }, { 132, 75, 190, 162 }
	}};
	for (int slot = 0; slot < static_cast<int>(Rects.size()); ++slot) {
		if (x >= Rects[slot].left && x < Rects[slot].right && y >= Rects[slot].top && y < Rects[slot].bottom)
			return slot;
	}
	return -1;
}

bool CanPreviewEquip(const d1hellforge::CharacterSummary::PackedItemSummary &item, int slot)
{
	if (item.minimumStrength > CurrentCharacter->strength || item.minimumMagic > CurrentCharacter->magic || item.minimumDexterity > CurrentCharacter->dexterity)
		return false;
	const bool matchingType = d1hellforge::IsItemCompatibleWithEquipmentSlot(item, CurrentCharacter->contentProfile, static_cast<uint8_t>(slot));
	if (!matchingType)
		return false;
	for (const auto &equipped : CurrentCharacter->equipment) {
		if (equipped.slot == slot && !(DragSourceArea == d1hellforge::InventoryArea::Equipment && equipped.slot == DragItemSlot))
			return false;
	}
	if (slot == 4 || slot == 5) {
		const int otherHand = slot == 4 ? 5 : 4;
		for (const auto &equipped : CurrentCharacter->equipment) {
			if (equipped.slot != otherHand || (DragSourceArea == d1hellforge::InventoryArea::Equipment && equipped.slot == DragItemSlot))
				continue;
			if (item.equipType == "Two-handed" || equipped.equipType == "Two-handed")
				return false;
		}
	}
	return true;
}

bool UpdateDragTarget(int clientX, int clientY)
{
	if (!CurrentCharacter.has_value() || !InventoryRenderSource.has_value())
		return false;
	const int sourceX = clientX * InventoryRenderSource->width / InventoryBitmapWidth;
	const int sourceY = clientY * InventoryRenderSource->height / InventoryBitmapHeight;
	const int equipmentSlot = EquipmentSlotAt(sourceX, sourceY);
	if (equipmentSlot >= 0) {
		const auto *item = DraggedItem();
		const bool valid = item != nullptr && CanPreviewEquip(*item, equipmentSlot);
		const bool changed = DragTargetArea != d1hellforge::InventoryArea::Equipment || DragTargetEquipmentSlot != equipmentSlot || valid != DragTargetValid;
		DragTargetArea = d1hellforge::InventoryArea::Equipment;
		DragTargetEquipmentSlot = equipmentSlot;
		DragTargetValid = valid;
		if (DragSourceArea != d1hellforge::InventoryArea::Equipment || DragItemSlot != equipmentSlot)
			DragMoved = true;
		return changed;
	}
	if (sourceY >= 352 && sourceY < 394 && sourceX >= 15 && sourceX < 311) {
		const int beltSlot = (sourceX - 15) / 37;
		bool beltEligible = DragSourceArea == d1hellforge::InventoryArea::Belt;
		if (DragSourceArea == d1hellforge::InventoryArea::Inventory) {
			const auto dragged = std::find_if(CurrentCharacter->inventory.begin(), CurrentCharacter->inventory.end(), [](const auto &item) { return item.slot == DragItemSlot; });
			beltEligible = dragged != CurrentCharacter->inventory.end() && dragged->canBePlacedOnBelt;
		}
		bool valid = beltSlot >= 0 && beltSlot < 8 && (sourceX - 15) % 37 < 31
		    && DragItemWidth == 1 && DragItemHeight == 1 && beltEligible;
		if (valid) {
			for (const auto &item : CurrentCharacter->belt) {
				if (item.slot == beltSlot && !(DragSourceArea == d1hellforge::InventoryArea::Belt && item.slot == DragItemSlot)) {
					valid = false;
					break;
				}
			}
		}
		const bool changed = DragTargetArea != d1hellforge::InventoryArea::Belt || DragTargetBeltSlot != beltSlot || valid != DragTargetValid;
		DragTargetArea = d1hellforge::InventoryArea::Belt;
		DragTargetBeltSlot = beltSlot;
		DragTargetValid = valid;
		if (DragSourceArea != d1hellforge::InventoryArea::Belt || beltSlot != DragItemSlot)
			DragMoved = true;
		return changed;
	}
	const int hoveredColumn = sourceX < 16 ? -1 : (sourceX - 16) / 29;
	const int hoveredRow = sourceY < 222 ? -1 : (sourceY - 222) / 29;
	const int targetColumn = hoveredColumn - DragGrabColumn;
	const int targetRow = hoveredRow - DragGrabRow;
	bool valid = targetColumn >= 0 && targetRow >= 0
	    && targetColumn + DragItemWidth <= 10 && targetRow + DragItemHeight <= 4;
	if (valid) {
		for (int row = 0; row < DragItemHeight && valid; ++row) {
			for (int column = 0; column < DragItemWidth; ++column) {
				const int8_t occupant = CurrentCharacter->inventoryGrid[(targetRow + row) * 10 + targetColumn + column];
				const bool occupiedByDraggedInventoryItem = DragSourceArea == d1hellforge::InventoryArea::Inventory
				    && std::abs(static_cast<int>(occupant)) == DragItemSlot + 1;
				if (occupant != 0 && !occupiedByDraggedInventoryItem) {
					valid = false;
					break;
				}
			}
		}
	}
	const bool changed = DragTargetArea != d1hellforge::InventoryArea::Inventory || targetColumn != DragTargetColumn || targetRow != DragTargetRow || valid != DragTargetValid;
	DragTargetArea = d1hellforge::InventoryArea::Inventory;
	DragTargetColumn = targetColumn;
	DragTargetRow = targetRow;
	DragTargetValid = valid;
	if (DragSourceArea != d1hellforge::InventoryArea::Inventory || targetColumn != DragOriginalColumn || targetRow != DragOriginalRow)
		DragMoved = true;
	return changed;
}

void ApplyInventoryMovePreview(HWND control)
{
	if (!CurrentCharacter.has_value() || !DragTargetValid || DragItemSlot < 0)
		return;
	auto &hero = *CurrentCharacter;
	d1hellforge::CharacterSummary::PackedItemSummary movedItem;
	if (DragSourceArea == d1hellforge::InventoryArea::Inventory) {
		const auto found = std::find_if(hero.inventory.begin(), hero.inventory.end(), [](const auto &item) { return item.slot == DragItemSlot; });
		if (found == hero.inventory.end())
			return;
		movedItem = *found;
		const int8_t itemIndex = static_cast<int8_t>(DragItemSlot + 1);
		for (int8_t &cell : hero.inventoryGrid) {
			if (std::abs(static_cast<int>(cell)) == itemIndex)
				cell = 0;
		}
		if (DragTargetArea != d1hellforge::InventoryArea::Inventory)
			hero.inventory.erase(found);
	} else if (DragSourceArea == d1hellforge::InventoryArea::Belt) {
		const auto found = std::find_if(hero.belt.begin(), hero.belt.end(), [](const auto &item) { return item.slot == DragItemSlot; });
		if (found == hero.belt.end())
			return;
		movedItem = *found;
		hero.belt.erase(found);
	} else {
		const auto found = std::find_if(hero.equipment.begin(), hero.equipment.end(), [](const auto &item) { return item.slot == DragItemSlot; });
		if (found == hero.equipment.end())
			return;
		movedItem = *found;
		hero.equipment.erase(found);
	}
	int movedSlot = DragItemSlot;
	if (DragTargetArea == d1hellforge::InventoryArea::Equipment) {
		movedItem.slot = static_cast<uint8_t>(DragTargetEquipmentSlot);
		movedSlot = movedItem.slot;
		hero.equipment.push_back(movedItem);
	} else if (DragTargetArea == d1hellforge::InventoryArea::Belt) {
		movedItem.slot = static_cast<uint8_t>(DragTargetBeltSlot);
		movedSlot = movedItem.slot;
		hero.belt.push_back(movedItem);
	} else {
		if (DragSourceArea != d1hellforge::InventoryArea::Inventory) {
			std::array<bool, 40> usedSlots {};
			for (const auto &item : hero.inventory)
				usedSlots[item.slot] = true;
			const auto freeSlot = std::find(usedSlots.begin(), usedSlots.end(), false);
			if (freeSlot == usedSlots.end())
				return;
			movedItem.slot = static_cast<uint8_t>(std::distance(usedSlots.begin(), freeSlot));
			movedSlot = movedItem.slot;
			hero.inventory.push_back(movedItem);
		}
		const int8_t itemIndex = static_cast<int8_t>(movedItem.slot + 1);
		for (int row = 0; row < DragItemHeight; ++row) {
			for (int column = 0; column < DragItemWidth; ++column) {
				const bool anchor = column == 0 && row == DragItemHeight - 1;
				hero.inventoryGrid[(DragTargetRow + row) * 10 + DragTargetColumn + column] = anchor ? itemIndex : -itemIndex;
			}
		}
	}
	const auto movedArea = DragTargetArea;
	DraggingInventoryItem = false;
	UpdateInventoryGraphic(*CurrentCharacter);
	for (std::size_t index = 0; index < InventoryRenderSource->items.size(); ++index) {
		const auto &region = InventoryRenderSource->items[index];
		if (region.area == movedArea && region.slot == movedSlot) {
			SelectedInventoryItem = static_cast<int>(index);
			break;
		}
	}
	RebuildInventoryBitmap(control);
	SetWindowTextW(InventoryInfo, L"MOVE PREVIEW\r\n\r\nItem repositioned in memory only. The save file has not been changed.\r\n\r\nBackpack and belt previews are enabled. Reload or Refresh to discard.");
	InvalidateRect(InventoryInfo, nullptr, TRUE);
}

LRESULT CALLBACK InventoryImageProc(HWND control, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
	if (ShowingStash) {
		auto cellAt = [control](LPARAM position) {
			RECT area {}; GetClientRect(control, &area);
			const int logicalX = GET_X_LPARAM(position) * 320 / std::max(1L, area.right);
			const int logicalY = GET_Y_LPARAM(position) * 352 / std::max(1L, area.bottom);
			if (logicalX < 17 || logicalY < 48) return std::pair { -1, -1 };
			return std::pair { (logicalX - 17) / 29, (logicalY - 48) / 29 };
		};
		if (message == WM_LBUTTONDOWN && CurrentStash.has_value()) {
			const auto [column, row] = cellAt(lParam);
			const auto page = CurrentStash->pages.find(CurrentStash->selectedPage);
			SelectedStashItem = column >= 0 && column < 10 && row >= 0 && row < 10 && page != CurrentStash->pages.end() ? page->second[column * 10 + row] : 0;
			if (SelectedStashItem != 0) {
				unsigned minimumColumn = 10, minimumRow = 10;
				for (unsigned x = 0; x < 10; ++x) for (unsigned y = 0; y < 10; ++y) if (page->second[x * 10 + y] == SelectedStashItem) {
					minimumColumn = std::min(minimumColumn, x); minimumRow = std::min(minimumRow, y);
				}
				StashDragGrabColumn = static_cast<unsigned>(column) - minimumColumn;
				StashDragGrabRow = static_cast<unsigned>(row) - minimumRow;
				DraggingStashItem = true; StashDragTargetValid = true;
				StashDragTargetColumn = minimumColumn; StashDragTargetRow = minimumRow;
				SetCapture(control);
			}
			RefreshStashDetails();
			return 0;
		}
		if (message == WM_MOUSEMOVE && DraggingStashItem && CurrentStash.has_value()) {
			const auto [column, row] = cellAt(lParam);
			StashDragTargetColumn = column - static_cast<int>(StashDragGrabColumn);
			StashDragTargetRow = row - static_cast<int>(StashDragGrabRow);
			StashDragTargetValid = false;
			if (StashDragTargetColumn >= 0 && StashDragTargetRow >= 0) {
				auto preview = *CurrentStash;
				StashDragTargetValid = d1hellforge::MoveStashItem(preview, SelectedStashItem, CurrentStash->selectedPage,
				    static_cast<unsigned>(StashDragTargetColumn), static_cast<unsigned>(StashDragTargetRow)).has_value();
			}
			InvalidateRect(control, nullptr, FALSE);
			return 0;
		}
		if (message == WM_LBUTTONUP) {
			if (DraggingStashItem) {
				ReleaseCapture(); DraggingStashItem = false;
				if (StashDragTargetValid && CurrentStash.has_value()) {
					(void)d1hellforge::MoveStashItem(*CurrentStash, SelectedStashItem, CurrentStash->selectedPage,
					    static_cast<unsigned>(StashDragTargetColumn), static_cast<unsigned>(StashDragTargetRow));
					StashDirty = true;
					RefreshStashDetails();
				}
				StashDragTargetColumn = -1; StashDragTargetRow = -1; StashDragTargetValid = false;
				InvalidateRect(control, nullptr, TRUE);
				return 0;
			}
			return 0;
		}
		if (message == WM_RBUTTONUP) {
			if (CurrentStash.has_value()) {
				const auto [column, row] = cellAt(lParam);
				const auto page = CurrentStash->pages.find(CurrentStash->selectedPage);
				SelectedStashItem = column >= 0 && column < 10 && row >= 0 && row < 10 && page != CurrentStash->pages.end()
				    ? page->second[column * 10 + row]
				    : 0;
				RefreshStashDetails();
			}
			ShowStashContextMenu(control, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		}
		return DefSubclassProc(control, message, wParam, lParam);
	}
	if (message == WM_LBUTTONDOWN) {
		SelectInventoryItemAt(control, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		if (SelectedInventoryItem >= 0) {
			const auto &region = InventoryRenderSource->items[SelectedInventoryItem];
			if (region.area == d1hellforge::InventoryArea::Inventory || region.area == d1hellforge::InventoryArea::Belt || region.area == d1hellforge::InventoryArea::Equipment) {
				const int sourceX = GET_X_LPARAM(lParam) * InventoryRenderSource->width / InventoryBitmapWidth;
				const int sourceY = GET_Y_LPARAM(lParam) * InventoryRenderSource->height / InventoryBitmapHeight;
				DraggingInventoryItem = true;
				DragItemSlot = region.slot;
				DragSourceArea = region.area;
				DragItemWidth = region.footprintWidth;
				DragItemHeight = region.footprintHeight;
				DragGrabColumn = std::clamp((sourceX - region.x) / 29, 0, DragItemWidth - 1);
				DragGrabRow = std::clamp((sourceY - region.y) / 29, 0, DragItemHeight - 1);
				DragTargetColumn = (region.x - 16) / 29;
				DragTargetRow = (region.y - 222) / 29;
				DragTargetArea = region.area;
				DragTargetBeltSlot = region.area == d1hellforge::InventoryArea::Belt ? region.slot : -1;
				DragTargetEquipmentSlot = region.area == d1hellforge::InventoryArea::Equipment ? region.slot : -1;
				DragOriginalColumn = DragTargetColumn;
				DragOriginalRow = DragTargetRow;
				DragTargetValid = true;
				DragMoved = false;
				SetCapture(control);
			}
		}
		return 0;
	} else if (message == WM_MOUSEMOVE) {
		if (DraggingInventoryItem) {
			if (UpdateDragTarget(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
				RebuildInventoryBitmap(control);
			return 0;
		}
		const int item = HitTestInventoryItem(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		if (item >= 0)
			SetCursor(LoadCursor(nullptr, IDC_HAND));
	} else if (message == WM_LBUTTONUP) {
		if (DraggingInventoryItem) {
			UpdateDragTarget(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			ReleaseCapture();
			if (!DragMoved) {
				DraggingInventoryItem = false;
				RebuildInventoryBitmap(control);
			} else if (DragTargetValid) {
				ApplyInventoryMovePreview(control);
			} else {
				DraggingInventoryItem = false;
				RebuildInventoryBitmap(control);
				SetWindowTextW(InventoryInfo, L"MOVE BLOCKED\r\n\r\nThat destination is occupied, out of bounds, or incompatible with this item. No change was made.");
				InvalidateRect(InventoryInfo, nullptr, TRUE);
			}
			return 0;
		}
		SelectInventoryItemAt(control, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	} else if (message == WM_RBUTTONUP) {
		ShowInventoryContextMenu(control, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	}
	return DefSubclassProc(control, message, wParam, lParam);
}

void DrawInventoryCanvas(const DRAWITEMSTRUCT &item)
{
	FillRect(item.hDC, &item.rcItem, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
	if (ShowingStash && CurrentStash.has_value()) {
		constexpr int LogicalWidth = 320;
		constexpr int LogicalHeight = 352;
		const int canvasWidth = item.rcItem.right - item.rcItem.left;
		const int canvasHeight = item.rcItem.bottom - item.rcItem.top;
		if (StashBackground.has_value()) {
			BITMAPINFO info {};
			info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			info.bmiHeader.biWidth = StashBackground->width;
			info.bmiHeader.biHeight = -StashBackground->height;
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = 32;
			info.bmiHeader.biCompression = BI_RGB;
			StretchDIBits(item.hDC, 0, 0, canvasWidth, canvasHeight,
			    0, 0, StashBackground->width, StashBackground->height,
			    StashBackground->pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
		}
		auto scaledX = [canvasWidth](int value) { return value * canvasWidth / LogicalWidth; };
		auto scaledY = [canvasHeight](int value) { return value * canvasHeight / LogicalHeight; };
		const auto page = CurrentStash->pages.find(CurrentStash->selectedPage);
		HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(112, 102, 79));
		HPEN oldPen = static_cast<HPEN>(SelectObject(item.hDC, gridPen));
		SetBkMode(item.hDC, TRANSPARENT);
		SetTextColor(item.hDC, RGB(238, 218, 160));
		for (int row = 0; row < 10; ++row) {
			for (int column = 0; column < 10; ++column) {
				RECT cell { scaledX(17 + column * 29), scaledY(48 + row * 29), scaledX(17 + (column + 1) * 29), scaledY(48 + (row + 1) * 29) };
				// DevilutionX serializes the stash grid column-first, matching its in-game layout.
				const uint16_t reference = page == CurrentStash->pages.end() ? 0 : page->second[column * 10 + row];
				if (!StashBackground.has_value()) {
					HBRUSH fill = CreateSolidBrush(reference == 0 ? RGB(12, 17, 23) : RGB(37, 30, 24));
					FillRect(item.hDC, &cell, fill);
					DeleteObject(fill);
					Rectangle(item.hDC, cell.left, cell.top, cell.right, cell.bottom);
				}
			}
		}
		SelectObject(item.hDC, oldPen);
		DeleteObject(gridPen);

		if (page != CurrentStash->pages.end() && CurrentCharacter.has_value()) {
			std::vector<uint16_t> drawn;
			for (uint16_t reference : page->second) {
				if (reference == 0 || reference > CurrentStash->items.size()
				    || std::find(drawn.begin(), drawn.end(), reference) != drawn.end())
					continue;
				drawn.push_back(reference);
				int minimumColumn = 10;
				int maximumColumn = -1;
				int minimumRow = 10;
				int maximumRow = -1;
				for (int column = 0; column < 10; ++column) {
					for (int row = 0; row < 10; ++row) {
						if (page->second[column * 10 + row] != reference) continue;
						minimumColumn = std::min(minimumColumn, column);
						maximumColumn = std::max(maximumColumn, column);
						minimumRow = std::min(minimumRow, row);
						maximumRow = std::max(maximumRow, row);
					}
				}
				if (maximumColumn < minimumColumn || maximumRow < minimumRow) continue;
				d1hellforge::CharacterSummary::PackedItemSummary summary;
				summary.fullItemRecord = CurrentStash->items[reference - 1].fullItemRecord;
				if (!d1hellforge::RefreshSummaryFromFullItemRecord(summary, CurrentStash->contentProfile.baseMode).has_value()) continue;
				const auto preview = d1hellforge::RenderItemPreview(*CurrentCharacter, summary.cursorGraphic);
				if (!preview.has_value()) continue;

				int sourceLeft = preview->width;
				int sourceTop = preview->height;
				int sourceRight = -1;
				int sourceBottom = -1;
				for (int y = 0; y < preview->height; ++y) {
					for (int x = 0; x < preview->width; ++x) {
						if ((preview->pixels[y * preview->width + x] & 0x00FFFFFF) == 0) continue;
						sourceLeft = std::min(sourceLeft, x); sourceRight = std::max(sourceRight, x);
						sourceTop = std::min(sourceTop, y); sourceBottom = std::max(sourceBottom, y);
					}
				}
				if (sourceRight < sourceLeft || sourceBottom < sourceTop) continue;
				const int boxLeft = scaledX(18 + minimumColumn * 29);
				const int boxTop = scaledY(49 + minimumRow * 29);
				const int boxWidth = scaledX(17 + (maximumColumn + 1) * 29) - boxLeft;
				const int boxHeight = scaledY(48 + (maximumRow + 1) * 29) - boxTop;
				const int sourceWidth = sourceRight - sourceLeft + 1;
				const int sourceHeight = sourceBottom - sourceTop + 1;
				const double scale = std::min(static_cast<double>(boxWidth) / sourceWidth, static_cast<double>(boxHeight) / sourceHeight);
				const int drawWidth = std::max(1, static_cast<int>(sourceWidth * scale));
				const int drawHeight = std::max(1, static_cast<int>(sourceHeight * scale));
				const int drawLeft = boxLeft + (boxWidth - drawWidth) / 2;
				const int drawTop = boxTop + (boxHeight - drawHeight) / 2;
				for (int y = 0; y < drawHeight; ++y) {
					const int sourceY = sourceTop + y * sourceHeight / drawHeight;
					for (int x = 0; x < drawWidth; ++x) {
						const int sourceX = sourceLeft + x * sourceWidth / drawWidth;
						const uint32_t color = preview->pixels[sourceY * preview->width + sourceX] & 0x00FFFFFF;
						if (color != 0) SetPixelV(item.hDC, drawLeft + x, drawTop + y,
						    RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
					}
				}
				if (reference == SelectedStashItem) {
					HPEN selectionPen = CreatePen(PS_SOLID, 2, RGB(224, 184, 78));
					oldPen = static_cast<HPEN>(SelectObject(item.hDC, selectionPen));
					HBRUSH oldSelectionBrush = static_cast<HBRUSH>(SelectObject(item.hDC, GetStockObject(NULL_BRUSH)));
					Rectangle(item.hDC, boxLeft, boxTop, boxLeft + boxWidth, boxTop + boxHeight);
					SelectObject(item.hDC, oldSelectionBrush); SelectObject(item.hDC, oldPen); DeleteObject(selectionPen);
					if (DraggingStashItem && StashDragTargetColumn >= 0 && StashDragTargetRow >= 0) {
						const int width = maximumColumn - minimumColumn + 1;
						const int height = maximumRow - minimumRow + 1;
						const int targetLeft = scaledX(17 + StashDragTargetColumn * 29);
						const int targetTop = scaledY(48 + StashDragTargetRow * 29);
						const int targetRight = scaledX(17 + (StashDragTargetColumn + width) * 29);
						const int targetBottom = scaledY(48 + (StashDragTargetRow + height) * 29);
						HPEN previewPen = CreatePen(PS_SOLID, 3, StashDragTargetValid ? RGB(56, 208, 96) : RGB(208, 64, 56));
						oldPen = static_cast<HPEN>(SelectObject(item.hDC, previewPen));
						oldSelectionBrush = static_cast<HBRUSH>(SelectObject(item.hDC, GetStockObject(NULL_BRUSH)));
						Rectangle(item.hDC, targetLeft, targetTop, targetRight, targetBottom);
						SelectObject(item.hDC, oldSelectionBrush); SelectObject(item.hDC, oldPen); DeleteObject(previewPen);
					}
				}
			}
		}
		return;
	}
	if (InventoryHBitmap == nullptr)
		return;
	HDC bitmapDc = CreateCompatibleDC(item.hDC);
	HGDIOBJ oldBitmap = SelectObject(bitmapDc, InventoryHBitmap);
	BitBlt(item.hDC, 0, 0, InventoryBitmapWidth, InventoryBitmapHeight, bitmapDc, 0, 0, SRCCOPY);
	SelectObject(bitmapDc, oldBitmap);
	DeleteDC(bitmapDc);
}

void DrawDiabloButton(const DRAWITEMSTRUCT &item)
{
	const bool selected = (item.CtlID == IdCharacterView && !ShowingInventory && !ShowingStash)
	    || (item.CtlID == IdInventoryView && ShowingInventory && !ShowingStash)
	    || (item.CtlID == IdStashView && ShowingStash);
	const bool pressed = (item.itemState & ODS_SELECTED) != 0;
	RECT rect = item.rcItem;
	Gdiplus::Bitmap *art = pressed || selected ? DiabloButtonActive.get()
	    : (item.itemState & ODS_FOCUS) != 0 ? DiabloButtonHover.get() : DiabloButtonNormal.get();
	if (art != nullptr && art->GetLastStatus() == Gdiplus::Ok) {
		Gdiplus::Graphics graphics(item.hDC);
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		graphics.DrawImage(art, Gdiplus::Rect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top));
	} else {
	HBRUSH fill = CreateSolidBrush(pressed || selected ? RGB(63, 40, 18) : RGB(24, 18, 12));
	FillRect(item.hDC, &rect, fill);
	DeleteObject(fill);
	HPEN outer = CreatePen(PS_SOLID, 2, selected ? RGB(224, 184, 78) : RGB(158, 126, 56));
	HPEN oldPen = static_cast<HPEN>(SelectObject(item.hDC, outer));
	HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(item.hDC, GetStockObject(NULL_BRUSH)));
	Rectangle(item.hDC, rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1);
	SelectObject(item.hDC, oldBrush);
	SelectObject(item.hDC, oldPen);
	DeleteObject(outer);
	InflateRect(&rect, -5, -5);
	HPEN inner = CreatePen(PS_SOLID, 1, RGB(91, 68, 32));
	oldPen = static_cast<HPEN>(SelectObject(item.hDC, inner));
	oldBrush = static_cast<HBRUSH>(SelectObject(item.hDC, GetStockObject(NULL_BRUSH)));
	Rectangle(item.hDC, rect.left, rect.top, rect.right, rect.bottom);
	SelectObject(item.hDC, oldBrush);
	SelectObject(item.hDC, oldPen);
	DeleteObject(inner);
	}
	wchar_t text[64] {};
	GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
	SetBkMode(item.hDC, TRANSPARENT);
	SetTextColor(item.hDC, selected ? RGB(255, 224, 135) : RGB(224, 211, 174));
	RECT textRect = item.rcItem;
	DrawTextW(item.hDC, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	if ((item.itemState & ODS_FOCUS) != 0) {
		RECT focus = item.rcItem;
		InflateRect(&focus, -8, -8);
		DrawFocusRect(item.hDC, &focus);
	}
}

void LoadDiabloButtonArt()
{
	wchar_t executable[MAX_PATH] {};
	GetModuleFileNameW(nullptr, executable, MAX_PATH);
	const auto directory = std::filesystem::path(executable).parent_path() / "assets" / "ui" / "buttons";
	auto load = [&directory](const wchar_t *name) {
		auto bitmap = std::make_unique<Gdiplus::Bitmap>((directory / name).c_str());
		return bitmap->GetLastStatus() == Gdiplus::Ok ? std::move(bitmap) : std::unique_ptr<Gdiplus::Bitmap> {};
	};
	DiabloButtonNormal = load(L"horizontal_normal.png");
	DiabloButtonHover = load(L"horizontal_hover_focus.png");
	DiabloButtonActive = load(L"horizontal_active_pressed.png");
}

void DrawNativeButton(const DRAWITEMSTRUCT &item)
{
	RECT rect = item.rcItem;
	UINT state = DFCS_BUTTONPUSH;
	if ((item.itemState & ODS_SELECTED) != 0)
		state |= DFCS_PUSHED;
	if ((item.itemState & ODS_DISABLED) != 0)
		state |= DFCS_INACTIVE;
	DrawFrameControl(item.hDC, &rect, DFC_BUTTON, state);
	wchar_t label[64] {};
	GetWindowTextW(item.hwndItem, label, static_cast<int>(std::size(label)));
	SetBkMode(item.hDC, TRANSPARENT);
	SetTextColor(item.hDC, GetSysColor((item.itemState & ODS_DISABLED) != 0 ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
	if ((item.itemState & ODS_SELECTED) != 0)
		OffsetRect(&rect, 1, 1);
	DrawTextW(item.hDC, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	if ((item.itemState & ODS_FOCUS) != 0) {
		InflateRect(&rect, -4, -4);
		DrawFocusRect(item.hDC, &rect);
	}
}

void ApplyAppearance(HWND window)
{
	const bool diablo = CurrentAppearance == AppearanceTheme::Diablo;
	d1hellforge::SetDiabloTheme(window, diablo);
	CheckMenuRadioItem(GetMenu(window), IdAppearanceNative, IdAppearanceDiablo,
	    diablo ? IdAppearanceDiablo : IdAppearanceNative, MF_BYCOMMAND);
	SetWindowTextW(window, diablo ? L"D1Hellforge v0.3.0 - Diablo Save Editor [Diablo UI]" : L"D1Hellforge v0.3.0 - Diablo Save Editor [Native UI]");
	RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void DrawInventoryInfo(const DRAWITEMSTRUCT &item)
{
	HBRUSH fill = CreateSolidBrush(RGB(10, 8, 6));
	FillRect(item.hDC, &item.rcItem, fill);
	DeleteObject(fill);
	HPEN border = CreatePen(PS_SOLID, 1, RGB(158, 126, 56));
	HPEN oldPen = static_cast<HPEN>(SelectObject(item.hDC, border));
	HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(item.hDC, GetStockObject(NULL_BRUSH)));
	Rectangle(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom);
	SelectObject(item.hDC, oldBrush);
	SelectObject(item.hDC, oldPen);
	DeleteObject(border);
	wchar_t text[512] {};
	GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
	RECT textRect = item.rcItem;
	InflateRect(&textRect, -12, -12);
	SetBkMode(item.hDC, TRANSPARENT);
	SetTextColor(item.hDC, RGB(224, 211, 174));
	DrawTextW(item.hDC, text, -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_CREATE:
		BrowseButton = CreateWindowW(L"BUTTON", L"OPEN SAVE", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdBrowse)), nullptr, nullptr);
		RefreshButton = CreateWindowW(L"BUTTON", L"REFRESH", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdRefresh)), nullptr, nullptr);
		SaveList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
		    WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdSaveList)), nullptr, nullptr);
		Details = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
		    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdDetails)), nullptr, nullptr);
		CharacterViewButton = CreateWindowW(L"BUTTON", L"CHARACTER", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdCharacterView)), nullptr, nullptr);
		InventoryViewButton = CreateWindowW(L"BUTTON", L"INVENTORY", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdInventoryView)), nullptr, nullptr);
		StashViewButton = CreateWindowW(L"BUTTON", L"STASH", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdStashView)), nullptr, nullptr);
		StashPreviousButton = CreateWindowW(L"BUTTON", L"Previous", WS_CHILD,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdStashPrevious)), nullptr, nullptr);
		StashNextButton = CreateWindowW(L"BUTTON", L"Next", WS_CHILD,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdStashNext)), nullptr, nullptr);
		StashPageLabel = CreateWindowW(L"STATIC", L"Page —", WS_CHILD | SS_CENTER,
		    0, 0, 0, 0, window, nullptr, nullptr, nullptr);
		InventoryImage = CreateWindowW(L"BUTTON", nullptr, WS_CHILD | BS_OWNERDRAW,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdInventoryImage)), nullptr, nullptr);
		SetWindowSubclass(InventoryImage, InventoryImageProc, 1, 0);
		InventoryInfo = d1hellforge::CreateItemDisplayControl(window, 0, 0, 1, 1);
		SetWindowLongPtrW(InventoryInfo, GWLP_ID, IdInventoryInfo);
		d1hellforge::SetItemDisplayText(InventoryInfo, { L"Open a save to view character and item information." });
		{
			constexpr const wchar_t *Labels[] { L"Strength", L"Magic", L"Dexterity", L"Vitality", L"Unspent points" };
			for (int i = 0; i < 5; ++i) {
				StatLabels[i] = CreateWindowW(L"STATIC", Labels[i], WS_CHILD | WS_VISIBLE,
				    0, 0, 0, 0, window, nullptr, nullptr, nullptr);
				StatEdits[i] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0",
				    WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_RIGHT,
				    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdStatFirst + i)), nullptr, nullptr);
			}
		}
		SaveButton = CreateWindowW(L"BUTTON", L"Save Stats (with Backup)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdSaveStats)), nullptr, nullptr);
		AutoCapCheckbox = CreateWindowW(L"BUTTON", L"Auto-cap stats to safe class maximum when saving",
		    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_MULTILINE,
		    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdAutoCapStats)), nullptr, nullptr);
		SendMessageW(AutoCapCheckbox, BM_SETCHECK, BST_CHECKED, 0);
		EnableWindow(SaveButton, FALSE);
		SaveDirectory = d1hellforge::DefaultSaveDirectory();
		ItemDirectory = DefaultItemLibraryDirectory();
		LoadRememberedFolders();
		ApplyAppearance(window);
		d1hellforge::AttachThemedChoiceChildren(window);
		RefreshSaves();
		return 0;
	case WM_SIZE:
		Layout(window);
		RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
		return 0;
	case WM_NOTIFY: {
		LRESULT result = 0;
		if (d1hellforge::HandleThemedChoiceCustomDraw(lParam, result))
			return result;
		break;
	}
	case WM_DRAWITEM:
		if (wParam == IdInventoryImage) {
			DrawInventoryCanvas(*reinterpret_cast<DRAWITEMSTRUCT *>(lParam));
			return TRUE;
		}
		if (wParam == IdBrowse || wParam == IdRefresh || wParam == IdCharacterView || wParam == IdInventoryView || wParam == IdStashView) {
			if (CurrentAppearance == AppearanceTheme::Diablo)
				DrawDiabloButton(*reinterpret_cast<DRAWITEMSTRUCT *>(lParam));
			else
				DrawNativeButton(*reinterpret_cast<DRAWITEMSTRUCT *>(lParam));
			return TRUE;
		}
		if (wParam == IdInventoryInfo) {
			DrawInventoryInfo(*reinterpret_cast<DRAWITEMSTRUCT *>(lParam));
			return TRUE;
		}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IdAppearanceNative || LOWORD(wParam) == IdAppearanceDiablo) {
			CurrentAppearance = LOWORD(wParam) == IdAppearanceDiablo ? AppearanceTheme::Diablo : AppearanceTheme::Native;
			SaveRememberedFolders();
			ApplyAppearance(window);
			return 0;
		}
		if (LOWORD(wParam) == IdInventoryImage && HIWORD(wParam) == BN_CLICKED && !ShowingStash) {
			POINT cursor;
			GetCursorPos(&cursor);
			ScreenToClient(InventoryImage, &cursor);
			SelectInventoryItemAt(InventoryImage, cursor.x, cursor.y);
			return 0;
		}
		if (LOWORD(wParam) == IdBrowse) {
			BrowseForSave(window);
			return 0;
		}
		if (LOWORD(wParam) == IdRefresh) {
			RefreshSaves();
			return 0;
		}
		if (LOWORD(wParam) == IdImportItem) {
			ImportItem(window);
			return 0;
		}
		if (LOWORD(wParam) == IdExportItem) {
			ExportSelectedItem(window);
			return 0;
		}
		if (LOWORD(wParam) == IdSaveStats) {
			SaveStats(window);
			return 0;
		}
		if (LOWORD(wParam) == IdSaveStash) {
			SaveStashPreview(window);
			return 0;
		}
		if (LOWORD(wParam) == IdCharacterView) {
			ShowingInventory = false;
			ShowingStash = false;
			Layout(window);
			return 0;
		}
		if (LOWORD(wParam) == IdInventoryView) {
			ShowingInventory = true;
			ShowingStash = false;
			Layout(window);
			return 0;
		}
		if (LOWORD(wParam) == IdStashView) {
			ShowingInventory = false;
			ShowingStash = true;
			SelectedStashItem = 0;
			RefreshStashDetails();
			Layout(window);
			return 0;
		}
		if ((LOWORD(wParam) == IdStashPrevious || LOWORD(wParam) == IdStashNext) && CurrentStash.has_value()) {
			if (LOWORD(wParam) == IdStashPrevious && CurrentStash->selectedPage > 0) --CurrentStash->selectedPage;
			if (LOWORD(wParam) == IdStashNext && CurrentStash->selectedPage + 1 < d1hellforge::StashPageCount) ++CurrentStash->selectedPage;
			SelectedStashItem = 0;
			RefreshStashDetails();
			return 0;
		}
		if (LOWORD(wParam) == IdSaveList && HIWORD(wParam) == LBN_SELCHANGE) {
			const LRESULT selected = SendMessageW(SaveList, LB_GETCURSEL, 0, 0);
			if (selected != LB_ERR && static_cast<std::size_t>(selected) < SavePaths.size())
				LoadSave(SavePaths[selected]);
			return 0;
		}
		break;
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLOREDIT:
		if (CurrentAppearance == AppearanceTheme::Diablo && reinterpret_cast<HWND>(lParam) == Details) {
			SetBkColor(reinterpret_cast<HDC>(wParam), RGB(8, 7, 4));
			SetTextColor(reinterpret_cast<HDC>(wParam), RGB(224, 211, 174));
			return reinterpret_cast<LRESULT>(DiabloBackgroundBrush);
		}
		break;
	case WM_DESTROY:
		SaveRememberedFolders();
		ClearInventoryBitmap();
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
	Gdiplus::GdiplusStartupInput gdiPlusInput;
	Gdiplus::GdiplusStartup(&GdiPlusToken, &gdiPlusInput, nullptr);
	int argumentCount = 0;
	LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	if (arguments != nullptr && argumentCount == 3 && std::wstring_view(arguments[1]) == L"--check-save") {
		const bool valid = d1hellforge::ReadCharacterSummary(arguments[2]).has_value();
		LocalFree(arguments);
		return valid ? 0 : 2;
	}
	if (arguments != nullptr && argumentCount == 3 && std::wstring_view(arguments[1]) == L"--roundtrip-save") {
		const auto summary = d1hellforge::ReadCharacterSummary(arguments[2]);
		bool valid = false;
		if (summary.has_value()) {
			const d1hellforge::EditableStats stats {
			    summary->strength, summary->magic, summary->dexterity,
			    summary->vitality, summary->unspentStatPoints
			};
			valid = d1hellforge::SaveCharacterStats(arguments[2], stats).has_value();
		}
		LocalFree(arguments);
		return valid ? 0 : 3;
	}
	if (arguments != nullptr && argumentCount == 4 && std::wstring_view(arguments[1]) == L"--test-set-magic") {
		const auto summary = d1hellforge::ReadCharacterSummary(arguments[2]);
		const unsigned long magic = wcstoul(arguments[3], nullptr, 10);
		bool valid = false;
		if (summary.has_value() && magic <= 255) {
			const d1hellforge::EditableStats stats {
			    summary->strength, static_cast<uint8_t>(magic), summary->dexterity,
			    summary->vitality, summary->unspentStatPoints
			};
			const auto saved = d1hellforge::SaveCharacterStats(arguments[2], stats);
			const auto gameStats = d1hellforge::ReadActiveGameStats(arguments[2]);
			valid = saved.has_value() && gameStats.has_value() && gameStats->has_value()
			    && (*gameStats)->strength == stats.strength
			    && (*gameStats)->magic == magic
			    && (*gameStats)->dexterity == stats.dexterity
			    && (*gameStats)->vitality == stats.vitality
			    && (*gameStats)->unspentStatPoints == stats.unspentStatPoints;
		}
		LocalFree(arguments);
		return valid ? 0 : 4;
	}
	if (arguments != nullptr && argumentCount == 4 && std::wstring_view(arguments[1]) == L"--export-first-item") {
		const auto summary = d1hellforge::ReadCharacterSummary(arguments[2]);
		bool valid = false;
		if (summary.has_value() && !summary->inventory.empty()) {
			const auto game = summary->game == d1hellforge::Game::Hellfire ? devilution::ItemGame::Hellfire : devilution::ItemGame::Diablo;
			valid = d1hellforge::ExportItemFile(arguments[3], summary->inventory.front().packedBytes, game, false).has_value();
		}
		LocalFree(arguments);
		return valid ? 0 : 5;
	}
	if (arguments != nullptr && argumentCount == 3 && std::wstring_view(arguments[1]) == L"--check-item") {
		const bool valid = d1hellforge::ImportItemFile(arguments[2]).has_value();
		LocalFree(arguments);
		return valid ? 0 : 6;
	}
	if (arguments != nullptr && argumentCount == 3 && std::wstring_view(arguments[1]) == L"--roundtrip-inventory") {
		const auto summary = d1hellforge::ReadCharacterSummary(arguments[2]);
		const bool valid = summary.has_value() && d1hellforge::SaveCharacterInventory(arguments[2], *summary).has_value();
		LocalFree(arguments);
		return valid ? 0 : 8;
	}
	if (arguments != nullptr && argumentCount == 3 && std::wstring_view(arguments[1]) == L"--test-delete-first-inventory") {
		auto summary = d1hellforge::ReadCharacterSummary(arguments[2]);
		bool valid = summary.has_value() && !summary->inventory.empty();
		if (valid) {
			const uint8_t slot = summary->inventory.front().slot;
			for (int8_t &cell : summary->inventoryGrid)
				if (std::abs(static_cast<int>(cell)) == slot + 1) cell = 0;
			summary->inventory.erase(summary->inventory.begin());
			valid = d1hellforge::SaveCharacterInventory(arguments[2], *summary).has_value();
			const auto reopened = d1hellforge::ReadCharacterSummary(arguments[2]);
			valid = valid && reopened.has_value() && reopened->inventory.size() == summary->inventory.size();
		}
		LocalFree(arguments);
		return valid ? 0 : 9;
	}
	if (arguments != nullptr && argumentCount == 4 && std::wstring_view(arguments[1]) == L"--test-import-item-save") {
		auto summary = d1hellforge::ReadCharacterSummary(arguments[2]);
		const auto imported = d1hellforge::ImportItemFile(arguments[3]);
		bool valid = summary.has_value() && imported.has_value();
		if (valid) {
			const auto itemSummary = d1hellforge::SummarizeImportedItem(imported->packedItem, summary->game);
			valid = itemSummary.has_value();
			if (valid) {
				auto item = *itemSummary;
				std::array<bool, 40> used {};
				for (const auto &existing : summary->inventory) used[existing.slot] = true;
				const auto slot = std::find(used.begin(), used.end(), false);
				const auto cell = std::find(summary->inventoryGrid.begin(), summary->inventoryGrid.end(), 0);
				valid = slot != used.end() && cell != summary->inventoryGrid.end();
				if (valid) {
					item.slot = static_cast<uint8_t>(std::distance(used.begin(), slot));
					*cell = static_cast<int8_t>(item.slot + 1);
					summary->inventory.push_back(item);
					valid = d1hellforge::SaveCharacterInventory(arguments[2], *summary).has_value();
					const auto reopened = d1hellforge::ReadCharacterSummary(arguments[2]);
					valid = valid && reopened.has_value() && reopened->inventory.size() == summary->inventory.size();
				}
			}
		}
		LocalFree(arguments);
		return valid ? 0 : 10;
	}
	if (arguments != nullptr && argumentCount == 3 && std::wstring_view(arguments[1]) == L"--test-edit-first-item") {
		auto summary = d1hellforge::ReadCharacterSummary(arguments[2]);
		bool valid = summary.has_value() && !summary->inventory.empty();
		if (valid) {
			auto &item = summary->inventory.front();
			const uint8_t expected = item.maxDurability == 0 ? 0 : static_cast<uint8_t>(item.maxDurability - 1);
			item.durability = expected;
			item.packedBytes[9] = static_cast<std::byte>(expected);
			valid = d1hellforge::SaveCharacterInventory(arguments[2], *summary).has_value();
			const auto reopened = d1hellforge::ReadCharacterSummary(arguments[2]);
			valid = valid && reopened.has_value() && !reopened->inventory.empty() && reopened->inventory.front().durability == expected;
		}
		LocalFree(arguments);
		return valid ? 0 : 11;
	}
	if (arguments != nullptr && argumentCount == 4 && std::wstring_view(arguments[1]) == L"--check-item-for-save") {
		const auto character = d1hellforge::ReadCharacterSummary(arguments[2]);
		const auto imported = d1hellforge::ImportItemFile(arguments[3]);
		bool valid = character.has_value() && imported.has_value();
		if (valid) {
			const auto expectedGame = character->game == d1hellforge::Game::Hellfire ? devilution::ItemGame::Hellfire : devilution::ItemGame::Diablo;
			valid = (imported->game == devilution::ItemGame::Unknown || imported->game == expectedGame)
			    && d1hellforge::SummarizeImportedItem(imported->packedItem, character->game).has_value();
		}
		LocalFree(arguments);
		return valid ? 0 : 7;
	}
	if (arguments != nullptr)
		LocalFree(arguments);

	constexpr wchar_t WindowClass[] = L"D1HellforgeMainWindow";
	WNDCLASSW windowClass {};
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = WindowClass;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
	windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	RegisterClassW(&windowClass);

	SaveDirectory = d1hellforge::DefaultSaveDirectory();
	ItemDirectory = DefaultItemLibraryDirectory();
	LoadRememberedFolders();
	LoadDiabloButtonArt();
	DiabloBackgroundBrush = CreateSolidBrush(RGB(8, 7, 4));
	HMENU menuBar = CreateMenu();
	HMENU viewMenu = CreatePopupMenu();
	AppendMenuW(viewMenu, MF_STRING, IdAppearanceNative, L"Native UI");
	AppendMenuW(viewMenu, MF_STRING, IdAppearanceDiablo, L"Diablo UI");
	AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"View");
	HWND window = CreateWindowExW(0, WindowClass, L"D1Hellforge v0.3.0 - Diablo Save Editor",
	    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, MainWindowWidth, MainWindowHeight,
	    nullptr, nullptr, instance, nullptr);
	if (window == nullptr)
		return 1;
	SetMenu(window, menuBar);
	ApplyAppearance(window);

	ShowWindow(window, showCommand);
	UpdateWindow(window);
	MSG message;
	while (GetMessageW(&message, nullptr, 0, 0) > 0) {
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
	DeleteObject(DiabloBackgroundBrush);
	DiabloButtonNormal.reset();
	DiabloButtonHover.reset();
	DiabloButtonActive.reset();
	Gdiplus::GdiplusShutdown(GdiPlusToken);
	return static_cast<int>(message.wParam);
}
