#include "item_advanced_editor.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <limits>
#include <sstream>

#include "devx_adapter.hpp"
#include "item_catalog.hpp"
#include "item_display.hpp"
#include "item_display_win32.hpp"
#include "ui_theme_win32.hpp"
#include "pack.h"
#include "tables/itemdat.h"
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace d1hellforge {
namespace {

template <typename T>
T Read(const std::vector<std::byte> &data, std::size_t offset)
{
	using U = std::make_unsigned_t<T>;
	U value = 0;
	for (std::size_t i = 0; i < sizeof(T); ++i) value |= static_cast<U>(std::to_integer<uint8_t>(data[offset + i])) << (i * 8);
	return static_cast<T>(value);
}

template <typename T>
void Write(std::vector<std::byte> &data, std::size_t offset, T value)
{
	using U = std::make_unsigned_t<T>;
	const U raw = static_cast<U>(value);
	for (std::size_t i = 0; i < sizeof(T); ++i) data[offset + i] = static_cast<std::byte>(raw >> (i * 8));
}

int FindUnique(int mappingId)
{
	for (std::size_t i = 0; i < DevXUniqueItems().size(); ++i)
		if (DevXUniqueItems()[i].mappingId == mappingId) return static_cast<int>(i);
	return -1;
}

devilution::_item_indexes FindBase(int mappingId)
{
	for (std::size_t i = 0; i < devilution::AllItemsList.size(); ++i)
		if (devilution::AllItemsList[i].iMappingId == mappingId) return static_cast<devilution::_item_indexes>(i);
	return devilution::IDI_NONE;
}

devilution::Item Decode(const std::vector<std::byte> &r, Game game)
{
	devilution::Item i;
	i._iSeed = Read<uint32_t>(r, 0); i._iCreateInfo = Read<uint16_t>(r, 4);
	i._itype = static_cast<devilution::ItemType>(Read<int32_t>(r, 8));
	i._iIdentified = Read<uint32_t>(r, 56) != 0; i._iMagical = static_cast<devilution::item_quality>(Read<int8_t>(r, 60));
	std::memcpy(i._iName, r.data() + 61, sizeof(i._iName)); i._iName[sizeof(i._iName) - 1] = 0;
	std::memcpy(i._iIName, r.data() + 125, sizeof(i._iIName)); i._iIName[sizeof(i._iIName) - 1] = 0;
	i._iLoc = static_cast<devilution::item_equip_type>(Read<int8_t>(r, 189)); i._iClass = static_cast<devilution::item_class>(Read<uint8_t>(r, 190));
	i._iCurs = static_cast<uint8_t>(Read<int32_t>(r, 192)); i._ivalue = Read<int32_t>(r, 196); i._iIvalue = Read<int32_t>(r, 200);
	i._iMinDam = static_cast<uint8_t>(Read<int32_t>(r, 204)); i._iMaxDam = static_cast<uint8_t>(Read<int32_t>(r, 208)); i._iAC = static_cast<int16_t>(Read<int32_t>(r, 212));
	i._iFlags = static_cast<devilution::ItemSpecialEffect>(Read<uint32_t>(r, 216)); i._iMiscId = static_cast<devilution::item_misc_id>(Read<int32_t>(r, 220)); i._iSpell = static_cast<devilution::SpellID>(Read<int32_t>(r, 224));
	i._iCharges = Read<int32_t>(r, 228); i._iMaxCharges = Read<int32_t>(r, 232); i._iDurability = Read<int32_t>(r, 236); i._iMaxDur = Read<int32_t>(r, 240);
	int16_t *fields[] = { &i._iPLDam, &i._iPLToHit, &i._iPLAC, &i._iPLStr, &i._iPLMag, &i._iPLDex, &i._iPLVit, &i._iPLFR, &i._iPLLR, &i._iPLMR, &i._iPLMana, &i._iPLHP, &i._iPLDamMod, &i._iPLGetHit, &i._iPLLight };
	for (std::size_t n = 0; n < std::size(fields); ++n) *fields[n] = static_cast<int16_t>(Read<int32_t>(r, 244 + n * 4));
	i._iSplLvlAdd = Read<int8_t>(r, 304); i._iUid = FindUnique(Read<int32_t>(r, 308));
	i._iFMinDam = static_cast<int16_t>(Read<int32_t>(r, 312)); i._iFMaxDam = static_cast<int16_t>(Read<int32_t>(r, 316)); i._iLMinDam = static_cast<int16_t>(Read<int32_t>(r, 320)); i._iLMaxDam = static_cast<int16_t>(Read<int32_t>(r, 324)); i._iPLEnAc = static_cast<int16_t>(Read<int32_t>(r, 328));
	i._iPrePower = static_cast<devilution::item_effect_type>(Read<int8_t>(r, 332)); i._iSufPower = static_cast<devilution::item_effect_type>(Read<int8_t>(r, 333));
	i._iVAdd1 = Read<int32_t>(r, 336); i._iVMult1 = Read<int32_t>(r, 340); i._iVAdd2 = Read<int32_t>(r, 344); i._iVMult2 = Read<int32_t>(r, 348);
	i._iMinStr = Read<int8_t>(r, 352); i._iMinMag = Read<uint8_t>(r, 353); i._iMinDex = Read<int8_t>(r, 354);
	i.IDidx = FindBase(Read<int32_t>(r, 360)); i.dwBuff = Read<uint32_t>(r, 364);
	if (game == Game::Hellfire) i._iDamAcFlags = static_cast<devilution::ItemSpecialEffectHf>(Read<uint32_t>(r, 368));
	return i;
}

std::vector<std::byte> Encode(const AdvancedItemEditorModel &m)
{
	auto r = m.originalRecord; const auto &i = m.staged;
	Write(r, 0, i._iSeed); Write(r, 4, i._iCreateInfo); Write<int32_t>(r, 8, static_cast<int32_t>(i._itype)); Write<uint32_t>(r, 56, i._iIdentified ? 1 : 0); Write(r, 60, static_cast<int8_t>(i._iMagical));
	std::fill(r.begin() + 61, r.begin() + 125, std::byte {}); std::memcpy(r.data() + 61, i._iName, strnlen(i._iName, sizeof(i._iName)));
	std::fill(r.begin() + 125, r.begin() + 189, std::byte {}); std::memcpy(r.data() + 125, i._iIName, strnlen(i._iIName, sizeof(i._iIName)));
	Write(r, 189, static_cast<int8_t>(i._iLoc)); Write(r, 190, static_cast<uint8_t>(i._iClass)); Write<int32_t>(r, 192, i._iCurs); Write(r, 196, i._ivalue); Write(r, 200, i._iIvalue); Write<int32_t>(r, 204, i._iMinDam); Write<int32_t>(r, 208, i._iMaxDam); Write<int32_t>(r, 212, i._iAC); Write<uint32_t>(r, 216, static_cast<uint32_t>(i._iFlags)); Write<int32_t>(r, 220, i._iMiscId); Write<int32_t>(r, 224, static_cast<int8_t>(i._iSpell));
	Write(r, 228, i._iCharges); Write(r, 232, i._iMaxCharges); Write(r, 236, i._iDurability); Write(r, 240, i._iMaxDur);
	const int16_t *fields[] = { &i._iPLDam, &i._iPLToHit, &i._iPLAC, &i._iPLStr, &i._iPLMag, &i._iPLDex, &i._iPLVit, &i._iPLFR, &i._iPLLR, &i._iPLMR, &i._iPLMana, &i._iPLHP, &i._iPLDamMod, &i._iPLGetHit, &i._iPLLight };
	for (std::size_t n = 0; n < std::size(fields); ++n) Write<int32_t>(r, 244 + n * 4, *fields[n]);
	Write(r, 304, i._iSplLvlAdd); Write<int32_t>(r, 308, i._iUid >= 0 && static_cast<std::size_t>(i._iUid) < DevXUniqueItems().size() ? DevXUniqueItems()[i._iUid].mappingId : -1);
	Write<int32_t>(r, 312, i._iFMinDam); Write<int32_t>(r, 316, i._iFMaxDam); Write<int32_t>(r, 320, i._iLMinDam); Write<int32_t>(r, 324, i._iLMaxDam); Write<int32_t>(r, 328, i._iPLEnAc); Write(r, 332, static_cast<int8_t>(i._iPrePower)); Write(r, 333, static_cast<int8_t>(i._iSufPower));
	Write(r, 336, i._iVAdd1); Write(r, 340, i._iVMult1); Write(r, 344, i._iVAdd2); Write(r, 348, i._iVMult2); Write(r, 352, i._iMinStr); Write(r, 353, i._iMinMag); Write(r, 354, i._iMinDex); Write<int32_t>(r, 360, devilution::AllItemsList[i.IDidx].iMappingId); Write(r, 364, i.dwBuff); if (m.game == Game::Hellfire) Write<uint32_t>(r, 368, static_cast<uint32_t>(i._iDamAcFlags));
	return r;
}

} // namespace

std::expected<AdvancedItemEditorModel, std::string> MakeAdvancedItemEditorModel(const CharacterSummary::PackedItemSummary &summary, Game game, bool enforce)
{
	const std::size_t size = game == Game::Hellfire ? 372 : 368;
	AdvancedItemEditorModel model;
	model.game = game;
	model.enforceVanillaRules = enforce;
	if (summary.fullItemRecord.size() == size) {
		model.originalRecord = summary.fullItemRecord;
		model.original = Decode(model.originalRecord, game);
		model.staged = model.original;
		return model;
	}
	if (!summary.fullItemRecord.empty())
		return std::unexpected("The selected item has an invalid expanded single-player record");
	if (summary.activeGameIdentityDiffers)
		return std::unexpected("This item's compact and active-game identities disagree; resolve it before direct editing");
	devilution::ItemPack packed {};
	std::memcpy(&packed, summary.packedBytes.data(), sizeof(packed));
	InitializeDevXContent(game);
	devilution::Player player;
	player._pMaxHPBase = 125 << 6;
	player._pMaxManaBase = 125 << 6;
	DevXUnpackItem(packed, player, model.original, game);
	if (model.original.isEmpty())
		return std::unexpected("DevilutionX could not expand this resolved Workshop item for direct editing");
	model.staged = model.original;
	model.originalRecord.assign(size, std::byte {});
	model.reconstructedFromPacked = true;
	model.originalRecord = Encode(model);
	return model;
}

std::vector<std::string> ValidateAdvancedItem(const AdvancedItemEditorModel &m)
{
	std::vector<std::string> e; const auto &i = m.staged;
	if (i.IDidx < 0 || static_cast<std::size_t>(i.IDidx) >= devilution::AllItemsList.size()) e.emplace_back("Base item is unavailable in the active content profile");
	if (i._iMagical < devilution::ITEM_QUALITY_NORMAL || i._iMagical > devilution::ITEM_QUALITY_UNIQUE) e.emplace_back("Quality is invalid");
	if (i._iMagical == devilution::ITEM_QUALITY_UNIQUE && (i._iUid < 0 || static_cast<std::size_t>(i._iUid) >= DevXUniqueItems().size())) e.emplace_back("Unique quality requires a valid active unique mapping");
	if (i._iMinDam > i._iMaxDam) e.emplace_back("Minimum physical damage cannot exceed maximum physical damage");
	if (i._iFMinDam > i._iFMaxDam) e.emplace_back("Minimum fire damage cannot exceed maximum fire damage");
	if (i._iLMinDam > i._iLMaxDam) e.emplace_back("Minimum lightning damage cannot exceed maximum lightning damage");
	if (i._iMaxDur != DUR_INDESTRUCTIBLE && i._iDurability > i._iMaxDur) e.emplace_back("Current durability cannot exceed maximum durability");
	if (i._iCharges > i._iMaxCharges) e.emplace_back("Current charges cannot exceed maximum charges");
	if (m.game == Game::Diablo && i._iDamAcFlags != devilution::ItemSpecialEffectHf::None) e.emplace_back("Hellfire item effects are invalid in Diablo mode");
	if (m.game == Game::Diablo && (i._iPrePower > devilution::IPL_LASTDIABLO || i._iSufPower > devilution::IPL_LASTDIABLO)) e.emplace_back("Hellfire power metadata is invalid in Diablo mode");
	if (i.IDidx == devilution::IDI_EAR && strnlen(i._iIName, sizeof(i._iIName)) > 16) e.emplace_back("Ear owner names are limited to 16 bytes by Diablo's compact ear format");
	return e;
}

std::vector<std::string> BuildAdvancedItemPreview(const AdvancedItemEditorModel &m)
{
	return RenderItemDisplayPlainText(BuildItemDisplay(m.staged, ItemDisplayMode::DetailedWithMetadata, GetDevXContentProfile(m.game)));
}

void ResetAdvancedItemEditorModel(AdvancedItemEditorModel &m) { m.staged = m.original; }

std::expected<void, std::string> RefreshSummaryFromFullItemRecord(CharacterSummary::PackedItemSummary &summary, Game game)
{
	auto model = MakeAdvancedItemEditorModel(summary, game, true);
	if (!model.has_value()) return std::unexpected(model.error());
	const auto &item = model->staged;
	summary.seed = item._iSeed;
	summary.durability = static_cast<uint8_t>(std::clamp(item._iDurability, 0, 255));
	summary.maxDurability = static_cast<uint8_t>(std::clamp(item._iMaxDur, 0, 255));
	summary.charges = static_cast<uint8_t>(std::clamp(item._iCharges, 0, 255));
	summary.maxCharges = static_cast<uint8_t>(std::clamp(item._iMaxCharges, 0, 255));
	summary.identified = item._iIdentified;
	summary.magicalQuality = static_cast<uint8_t>(item._iMagical);
	summary.cursorGraphic = item._iCurs;
	if (item.IDidx != devilution::IDI_NONE) {
		summary.baseItemId = static_cast<uint16_t>(item.IDidx);
		if (const auto metadata = GetBaseItemMetadata(summary.baseItemId); metadata.has_value()) {
			summary.baseName = metadata->name;
			summary.canBePlacedOnBelt = metadata->canBePlacedOnBelt;
			summary.equipType = metadata->equipType;
		}
	}
	summary.displayName = item.IDidx == devilution::IDI_EAR ? item._iName : item._iIdentified || item._iMagical == devilution::ITEM_QUALITY_NORMAL ? item._iIName : item._iName;
	summary.minimumStrength = static_cast<uint8_t>(std::max<int>(item._iMinStr, 0));
	summary.minimumMagic = item._iMinMag;
	summary.minimumDexterity = static_cast<uint8_t>(std::max<int>(item._iMinDex, 0));
	summary.detailLines = RenderItemDisplayPlainText(BuildItemDisplay(item, ItemDisplayMode::Detailed, GetDevXContentProfile(game)), false);
	return {};
}

std::expected<CharacterSummary::PackedItemSummary, std::string> ApplyAdvancedItemEditorModel(const AdvancedItemEditorModel &m, CharacterSummary::PackedItemSummary summary)
{
	AdvancedItemEditorModel normalized = m;
	if (normalized.staged.IDidx == devilution::IDI_EAR) {
		const std::string earName = std::string("Ear of ") + normalized.staged._iIName;
		strncpy_s(normalized.staged._iName, earName.c_str(), _TRUNCATE);
	} else {
		devilution::Item canonical = normalized.staged;
		canonical.dwBuff &= ~devilution::CF_CUSTOM_NAME;
		canonical._iIdentified = false;
		const std::string canonicalBase(canonical.getName().str());
		canonical._iIdentified = true;
		const std::string canonicalIdentified(canonical.getName().str());
		if (normalized.staged._iName != canonicalBase || normalized.staged._iIName != canonicalIdentified)
			normalized.staged.dwBuff |= devilution::CF_CUSTOM_NAME;
		else
			normalized.staged.dwBuff &= ~devilution::CF_CUSTOM_NAME;
	}
	const auto errors = ValidateAdvancedItem(normalized); if (!errors.empty()) return std::unexpected(errors.front());
	summary.fullItemRecord = Encode(normalized);
	devilution::ItemPack packed {}; DevXPackItem(packed, normalized.staged, normalized.game); std::memcpy(summary.packedBytes.data(), &packed, sizeof(packed));
	summary.baseItemId = static_cast<uint16_t>(normalized.staged.IDidx);
	summary.activeGameIdentityDiffers = false;
	if (auto refreshed = RefreshSummaryFromFullItemRecord(summary, normalized.game); !refreshed.has_value())
		return std::unexpected(refreshed.error());
	return summary;
}

} // namespace d1hellforge

#ifdef _WIN32
namespace d1hellforge {
namespace {
constexpr int NumericFieldCount = 32;
struct DirectWindowState {
	AdvancedItemEditorModel model;
	CharacterSummary::PackedItemSummary summary;
	HWND displayName {};
	HWND baseName {};
	HWND quality {};
	HWND identified {};
	HWND indestructible {};
	HWND preview {};
	HWND edits[NumericFieldCount] {};
	std::optional<CharacterSummary::PackedItemSummary> result;
};

std::wstring PreviewWide(std::string_view text)
{
	if (text.empty()) return {};
	const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	std::wstring result(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
	return result;
}

int DirectWindowSetting(const wchar_t *key, int fallback, int minimum)
{
	wchar_t executable[MAX_PATH] {};
	GetModuleFileNameW(nullptr, executable, MAX_PATH);
	const auto ini = std::filesystem::path(executable).parent_path() / "D1Hellforge.ini";
	return std::max(minimum, static_cast<int>(GetPrivateProfileIntW(L"Windows", key, fallback, ini.c_str())));
}

void UpdatePreview(DirectWindowState &s)
{
	std::vector<std::wstring> lines;
	for (const std::string &line : BuildAdvancedItemPreview(s.model)) lines.push_back(PreviewWide(line));
	SetItemDisplayText(s.preview, lines);
}

int GetField(const devilution::Item &i, int n)
{
	const int values[] = { i._iMinDam, i._iMaxDam, i._iAC, i._iPLDam, i._iPLToHit, i._iPLAC, i._iPLStr, i._iPLMag, i._iPLDex, i._iPLVit, i._iPLFR, i._iPLLR, i._iPLMR, i._iPLHP >> 6, i._iPLMana >> 6, i._iPLDamMod, i._iPLGetHit, i._iPLLight, i._iSplLvlAdd, i._iPLEnAc, i._iFMinDam, i._iFMaxDam, i._iLMinDam, i._iLMaxDam, i._iMinStr, i._iMinMag, i._iMinDex, i._iDurability, i._iMaxDur, i._iCharges, i._iMaxCharges, i._iIvalue };
	return values[n];
}

void SetField(devilution::Item &i, int n, int v)
{
	switch (n) {
	case 0: i._iMinDam = v; break; case 1: i._iMaxDam = v; break; case 2: i._iAC = v; break;
	case 3: i._iPLDam = v; break; case 4: i._iPLToHit = v; break; case 5: i._iPLAC = v; break;
	case 6: i._iPLStr = v; break; case 7: i._iPLMag = v; break; case 8: i._iPLDex = v; break; case 9: i._iPLVit = v; break;
	case 10: i._iPLFR = v; break; case 11: i._iPLLR = v; break; case 12: i._iPLMR = v; break;
	case 13: i._iPLHP = v << 6; break; case 14: i._iPLMana = v << 6; break; case 15: i._iPLDamMod = v; break;
	case 16: i._iPLGetHit = v; break; case 17: i._iPLLight = v; break; case 18: i._iSplLvlAdd = v; break; case 19: i._iPLEnAc = v; break;
	case 20: i._iFMinDam = v; break; case 21: i._iFMaxDam = v; break; case 22: i._iLMinDam = v; break; case 23: i._iLMaxDam = v; break;
	case 24: i._iMinStr = v; break; case 25: i._iMinMag = v; break; case 26: i._iMinDex = v; break;
	case 27: i._iDurability = v; break; case 28: i._iMaxDur = v; break; case 29: i._iCharges = v; break; case 30: i._iMaxCharges = v; break;
	case 31: i._iIvalue = v; break;
	}
}

void Fill(DirectWindowState &s)
{
	SetWindowTextA(s.displayName, s.model.staged._iIName);
	SetWindowTextA(s.baseName, s.model.staged._iName);
	SendMessageW(s.quality, CB_SETCURSEL, static_cast<WPARAM>(s.model.staged._iMagical), 0);
	SendMessageW(s.identified, BM_SETCHECK, s.model.staged._iIdentified ? BST_CHECKED : BST_UNCHECKED, 0);
	for (int n = 0; n < NumericFieldCount; ++n)
		SetWindowTextW(s.edits[n], std::to_wstring(GetField(s.model.staged, n)).c_str());
	SendMessageW(s.indestructible, BM_SETCHECK, s.model.staged._iMaxDur == DUR_INDESTRUCTIBLE ? BST_CHECKED : BST_UNCHECKED, 0);
	UpdatePreview(s);
}

bool ReadControls(HWND window, DirectWindowState &s, bool reportErrors = true)
{
	char name[64] {};
	GetWindowTextA(s.displayName, name, static_cast<int>(std::size(name)));
	if (name[0] == '\0') {
		if (reportErrors) MessageBoxW(window, L"The identified/display name cannot be empty.", L"Invalid name", MB_OK | MB_ICONWARNING);
		return false;
	}
	strncpy_s(s.model.staged._iIName, name, _TRUNCATE);
	GetWindowTextA(s.baseName, name, static_cast<int>(std::size(name)));
	if (name[0] == '\0') {
		if (reportErrors) MessageBoxW(window, L"The unidentified/base name cannot be empty.", L"Invalid name", MB_OK | MB_ICONWARNING);
		return false;
	}
	strncpy_s(s.model.staged._iName, name, _TRUNCATE);
	const LRESULT quality = SendMessageW(s.quality, CB_GETCURSEL, 0, 0);
	if (quality == CB_ERR) return false;
	s.model.staged._iMagical = static_cast<devilution::item_quality>(quality);
	s.model.staged._iIdentified = SendMessageW(s.identified, BM_GETCHECK, 0, 0) == BST_CHECKED;
	for (int n = 0; n < NumericFieldCount; ++n) {
		wchar_t buffer[32] {};
		GetWindowTextW(s.edits[n], buffer, static_cast<int>(std::size(buffer)));
		wchar_t *end = nullptr;
		const long value = wcstol(buffer, &end, 10);
		const bool byteValue = n < 2 || (n >= 25 && n <= 30);
		const bool pointValue = n == 13 || n == 14;
		if (end == buffer || *end != 0 || value < (pointValue ? -512 : byteValue ? 0 : -32768) || value > (n == 31 ? std::numeric_limits<int32_t>::max() : pointValue ? 511 : byteValue ? 255 : 32767)) {
			if (reportErrors) {
				MessageBoxW(window, L"A value is invalid or outside its safe serialized range.", L"Invalid value", MB_OK | MB_ICONWARNING);
				SetFocus(s.edits[n]);
			}
			return false;
		}
		SetField(s.model.staged, n, static_cast<int>(value));
	}
	if (SendMessageW(s.indestructible, BM_GETCHECK, 0, 0) == BST_CHECKED) {
		s.model.staged._iDurability = DUR_INDESTRUCTIBLE;
		s.model.staged._iMaxDur = DUR_INDESTRUCTIBLE;
	}
	return true;
}

LRESULT CALLBACK DirectProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto *state = reinterpret_cast<DirectWindowState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if (message == WM_CREATE) {
		InheritDiabloTheme(window);
		state = static_cast<DirectWindowState *>(reinterpret_cast<CREATESTRUCTW *>(lParam)->lpCreateParams);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
		const wchar_t *explanation = state->model.reconstructedFromPacked
		    ? L"Advanced Direct Item Editing\r\nThis resolved Workshop preview was expanded from its game-native packed identity; Apply will create its full single-player record."
		    : L"Advanced Direct Item Editing\r\nThese are the exact stored single-player item properties. Custom values may be normalized outside that save path.";
		CreateWindowW(L"STATIC", explanation, WS_CHILD | WS_VISIBLE, 16, 10, 1135, 38, window, nullptr, nullptr, nullptr);
		CreateWindowW(L"STATIC", L"LIVE ITEM PREVIEW", WS_CHILD | WS_VISIBLE, 810, 52, 250, 22, window, nullptr, nullptr, nullptr);
		state->preview = CreateItemDisplayControl(window, 810, 75, 340, 465);
		CreateWindowW(L"STATIC", L"Identified / display name", WS_CHILD | WS_VISIBLE, 16, 53, 165, 22, window, nullptr, nullptr, nullptr);
		state->displayName = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 185, 50, 310, 24, window, nullptr, nullptr, nullptr);
		CreateWindowW(L"STATIC", L"Unidentified / base name", WS_CHILD | WS_VISIBLE, 520, 53, 165, 22, window, nullptr, nullptr, nullptr);
		state->baseName = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 690, 50, 310, 24, window, nullptr, nullptr, nullptr);
		if (state->model.staged.IDidx == devilution::IDI_EAR)
			EnableWindow(state->baseName, FALSE); // Ear of <owner> is derived from the compact owner-name field.
		CreateWindowW(L"STATIC", L"Quality", WS_CHILD | WS_VISIBLE, 16, 87, 75, 22, window, nullptr, nullptr, nullptr);
		state->quality = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 92, 83, 150, 120, window, nullptr, nullptr, nullptr);
		for (const wchar_t *quality : { L"Normal", L"Magic", L"Unique" }) SendMessageW(state->quality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(quality));
		state->identified = CreateWindowW(L"BUTTON", L"Identified", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 270, 85, 120, 24, window, nullptr, nullptr, nullptr);
		const wchar_t *names[] = { L"Minimum Damage", L"Maximum Damage", L"Armor Class", L"Damage Bonus (%)", L"Chance to Hit (%)", L"Armor Bonus (%)", L"Strength", L"Magic", L"Dexterity", L"Vitality", L"Fire Resistance (%)", L"Lightning Resistance (%)", L"Magic Resistance (%)", L"Hit Points", L"Mana", L"Additional Damage", L"Damage Taken Modifier", L"Light Radius", L"Spell Levels", L"Armor Penetration", L"Minimum Fire Damage", L"Maximum Fire Damage", L"Minimum Lightning Damage", L"Maximum Lightning Damage", L"Strength Requirement", L"Magic Requirement", L"Dexterity Requirement", L"Current Durability", L"Maximum Durability", L"Current Charges", L"Maximum Charges", L"Identified Value" };
		const int order[] = { 0, 1, 2, 27, 28, 29, 30, 3, 4, 15, 19, 20, 21, 22, 23, 5, 10, 11, 12, 16, 6, 7, 8, 9, 13, 14, 18, 17, 24, 25, 26, 31 };
		for (int n = 0; n < NumericFieldCount; ++n) {
			const int field = order[n];
			const int column = n / 11;
			const int row = n % 11;
			const int x = 16 + column * 265;
			const int y = 125 + row * 36;
			CreateWindowW(L"STATIC", names[field], WS_CHILD | WS_VISIBLE, x, y + 4, 165, 22, window, nullptr, nullptr, nullptr);
			state->edits[field] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_RIGHT, x + 165, y, 85, 24, window, nullptr, nullptr, nullptr);
		}
		state->indestructible = CreateWindowW(L"BUTTON", L"Indestructible", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 16, 530, 145, 24, window, reinterpret_cast<HMENU>(1002), nullptr, nullptr);
		Fill(*state);
		CreateWindowW(L"STATIC", L"Values and effect flags remain independent; Apply stages changes for Workshop.", WS_CHILD | WS_VISIBLE, 16, 570, 650, 22, window, nullptr, nullptr, nullptr);
		CreateWindowW(L"BUTTON", L"Reset Changes", WS_CHILD | WS_VISIBLE, 795, 570, 125, 32, window, reinterpret_cast<HMENU>(1001), nullptr, nullptr);
		CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 930, 570, 90, 32, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
		CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 1030, 570, 120, 32, window, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
		AttachThemedChoiceChildren(window);
		return 0;
	}
	if (message == WM_NOTIFY) {
		LRESULT result = 0;
		if (HandleThemedChoiceCustomDraw(lParam, result))
			return result;
	}
	if (message == WM_COMMAND && state != nullptr) {
		if (LOWORD(wParam) == 1001) { ResetAdvancedItemEditorModel(state->model); Fill(*state); return 0; }
		if (LOWORD(wParam) == 1002) {
			if (SendMessageW(state->indestructible, BM_GETCHECK, 0, 0) == BST_CHECKED) {
				SetWindowTextW(state->edits[27], L"255"); SetWindowTextW(state->edits[28], L"255");
			}
			if (ReadControls(window, *state, false)) UpdatePreview(*state);
			return 0;
		}
		if (LOWORD(wParam) == IDCANCEL) { DestroyWindow(window); return 0; }
		if (LOWORD(wParam) == IDOK) {
			if (!ReadControls(window, *state)) return 0;
			auto result = ApplyAdvancedItemEditorModel(state->model, state->summary);
			if (!result.has_value()) { MessageBoxA(window, result.error().c_str(), "Cannot Apply", MB_OK | MB_ICONWARNING); return 0; }
			state->result = *result;
			DestroyWindow(window);
			return 0;
		}
		if (reinterpret_cast<HWND>(lParam) != state->preview && (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == BN_CLICKED)) {
			if (ReadControls(window, *state, false)) UpdatePreview(*state);
		}
	}
	if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
	return DefWindowProcW(window, message, wParam, lParam);
}
}
std::optional<CharacterSummary::PackedItemSummary> ShowAdvancedItemEditor(NativeWindow owner, const CharacterSummary::PackedItemSummary &summary, Game game, bool enforce)
{
	HWND parent = static_cast<HWND>(owner);
	auto model = MakeAdvancedItemEditorModel(summary, game, enforce);
	if (!model.has_value()) { MessageBoxA(parent, model.error().c_str(), "Advanced Item Editor", MB_OK | MB_ICONWARNING); return std::nullopt; }
	DirectWindowState state { *model, summary };
	static bool registered = false;
	if (!registered) {
		WNDCLASSW windowClass {};
		windowClass.lpfnWndProc = DirectProc;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.lpszClassName = L"D1HellforgeDirectItem";
		windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
		windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
		registered = RegisterClassW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}
	EnableWindow(parent, FALSE);
	const int width = DirectWindowSetting(L"AdvancedEditorWidth", 1190, 1190);
	const int height = DirectWindowSetting(L"AdvancedEditorHeight", 655, 655);
	HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, L"D1HellforgeDirectItem", L"I'm Feeling Lucky — Advanced Item Editor", WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, width, height, parent, nullptr, GetModuleHandleW(nullptr), &state);
	ShowWindow(window, SW_SHOW);
	MSG message;
	while (IsWindow(window) && GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
	EnableWindow(parent, TRUE);
	SetForegroundWindow(parent);
	return state.result;
}
} // namespace d1hellforge
#endif
