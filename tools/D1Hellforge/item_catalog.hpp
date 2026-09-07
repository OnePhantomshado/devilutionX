#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "save_document.hpp"
#include "devx_adapter.hpp"

namespace d1hellforge {

struct BaseItemMetadata {
	std::string name;
	uint8_t cursorGraphic = 0;
	bool canBePlacedOnBelt = false;
	std::string equipType;
	uint8_t minimumStrength = 0;
	uint8_t minimumMagic = 0;
	uint8_t minimumDexterity = 0;
};

std::optional<BaseItemMetadata> GetBaseItemMetadata(uint16_t baseItemId);
bool IsItemCompatibleWithEquipmentSlot(const CharacterSummary::PackedItemSummary &item, const ContentProfile &profile, uint8_t equipmentSlot);
std::vector<CatalogItem> GetItemCatalog(const ContentProfile &profile);
std::vector<CatalogItem> GetItemCatalog(Game game);

} // namespace d1hellforge
