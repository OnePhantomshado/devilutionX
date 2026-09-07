#include "item_catalog.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "tables/itemdat.h"

namespace d1hellforge {
namespace {

std::vector<std::string> SplitTabs(const std::string &line)
{
	std::vector<std::string> fields;
	std::size_t start = 0;
	while (start <= line.size()) {
		const std::size_t end = line.find('\t', start);
		fields.emplace_back(line.substr(start, end - start));
		if (end == std::string::npos) break;
		start = end + 1;
	}
	return fields;
}

const std::vector<BaseItemMetadata> &BaseItemCatalog()
{
	static const std::vector<BaseItemMetadata> catalog = [] {
		std::vector<std::filesystem::path> candidates;
#ifdef _WIN32
		std::wstring executable(MAX_PATH, L'\0');
		const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
		if (length != 0) {
			executable.resize(length);
			const auto executableDirectory = std::filesystem::path(executable).parent_path();
			candidates.push_back(executableDirectory / "assets/txtdata/items/itemdat.tsv");
			candidates.push_back(executableDirectory / "../../assets/txtdata/items/itemdat.tsv");
		}
#endif
		candidates.push_back(std::filesystem::current_path() / "assets/txtdata/items/itemdat.tsv");
		for (const auto &candidate : candidates) {
			std::ifstream input(candidate);
			if (!input) continue;
			std::vector<BaseItemMetadata> result;
			std::string line;
			std::getline(input, line);
			while (std::getline(input, line)) {
				const auto fields = SplitTabs(line);
				BaseItemMetadata entry;
				if (fields.size() > 7) entry.name = fields[7];
				if (fields.size() > 4) {
					const auto parsed = devilution::ParseItemCursorGraphicId(fields[4]);
					if (parsed.has_value()) entry.cursorGraphic = static_cast<uint8_t>(*parsed);
				}
				if (fields.size() > 21) entry.canBePlacedOnBelt = fields[5] != "Gold" && fields[21] == "true";
				if (fields.size() > 17) {
					entry.equipType = fields[3];
					entry.minimumStrength = static_cast<uint8_t>(std::clamp(std::atoi(fields[15].c_str()), 0, 255));
					entry.minimumMagic = static_cast<uint8_t>(std::clamp(std::atoi(fields[16].c_str()), 0, 255));
					entry.minimumDexterity = static_cast<uint8_t>(std::clamp(std::atoi(fields[17].c_str()), 0, 255));
				}
				result.push_back(std::move(entry));
			}
			return result;
		}
		return std::vector<BaseItemMetadata> {};
	}();
	return catalog;
}

bool SameContentIdentity(const ContentProfile &left, const ContentProfile &right)
{
	return left.baseMode == right.baseMode
	    && left.contentIdentifiers == right.contentIdentifiers
	    && left.looseContentActive == right.looseContentActive
	    && left.initialized == right.initialized
	    && left.initializationError == right.initializationError;
}

std::string EquipCategory(const devilution::ItemData &item)
{
	switch (item.iLoc) {
	case devilution::ILOC_ONEHAND: return "One-handed";
	case devilution::ILOC_TWOHAND: return "Two-handed";
	case devilution::ILOC_ARMOR: return "Armor";
	case devilution::ILOC_HELM: return "Helm";
	case devilution::ILOC_RING: return "Ring";
	case devilution::ILOC_AMULET: return "Amulet";
	case devilution::ILOC_BELT: return "Belt items";
	case devilution::ILOC_NONE:
	case devilution::ILOC_UNEQUIPABLE:
	case devilution::ILOC_INVALID: return item.iUsable ? "Belt items" : "Other";
	}
	return "Other";
}

} // namespace

std::optional<BaseItemMetadata> GetBaseItemMetadata(uint16_t baseItemId)
{
	const auto &catalog = BaseItemCatalog();
	if (baseItemId >= catalog.size()) return std::nullopt;
	return catalog[baseItemId];
}

bool IsItemCompatibleWithEquipmentSlot(const CharacterSummary::PackedItemSummary &item, const ContentProfile &profile, uint8_t equipmentSlot)
{
	if (!ValidateDevXContentProfile(profile).has_value()) return false;
	const auto *base = DevXBaseItem(item.baseItemId);
	if (base == nullptr) return false;
	switch (equipmentSlot) {
	case 0: return base->iLoc == devilution::ILOC_HELM;
	case 1:
	case 2: return base->iLoc == devilution::ILOC_RING;
	case 3: return base->iLoc == devilution::ILOC_AMULET;
	case 4:
	case 5: return base->iLoc == devilution::ILOC_ONEHAND || base->iLoc == devilution::ILOC_TWOHAND;
	case 6: return base->iLoc == devilution::ILOC_ARMOR;
	default: return false;
	}
}

std::vector<CatalogItem> GetItemCatalog(Game game)
{
	return GetItemCatalog(GetDevXContentProfile(game));
}

std::vector<CatalogItem> GetItemCatalog(const ContentProfile &profile)
{
	if (!ValidateDevXContentProfile(profile).has_value()) return {};
	static std::mutex cacheMutex;
	static std::optional<ContentProfile> cachedProfile;
	static std::vector<CatalogItem> cachedCatalog;
	std::scoped_lock lock(cacheMutex);
	if (cachedProfile.has_value() && SameContentIdentity(*cachedProfile, profile))
		return cachedCatalog;
	std::vector<CatalogItem> result;
	const auto &catalog = BaseItemCatalog();
	const auto effectiveItems = DevXBaseItems();
	for (std::size_t index = 0; index < effectiveItems.size(); ++index) {
		const auto &item = effectiveItems[index];
		if (item.iName.empty() || index == static_cast<std::size_t>(devilution::IDI_NONE)) continue;
		std::string category;
		if (index < catalog.size()) category = catalog[index].equipType;
		if (category.empty() || category == "None") category = EquipCategory(item);
		result.push_back({ static_cast<uint16_t>(index), item.iName, std::move(category) });
	}
	cachedProfile = profile;
	cachedCatalog = result;
	return cachedCatalog;
}

} // namespace d1hellforge
