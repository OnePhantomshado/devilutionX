#pragma once

#include <cstdint>
#include <expected>
#include <span>

#include "content_profile.hpp"
#include "save_document.hpp"
#include "items.h"
#include "pack.h"
#include "tables/itemdat.h"

namespace d1hellforge {

void InitializeDevXContent(Game game, bool isSpawn = false);
const ContentProfile &GetDevXContentProfile(Game game, bool isSpawn = false);
std::expected<void, std::string> ValidateDevXContentProfile(const ContentProfile &profile);
const devilution::ItemData *DevXBaseItem(uint16_t baseItemId);
std::span<const devilution::ItemData> DevXBaseItems();
std::span<const devilution::PLStruct> DevXPrefixes();
std::span<const devilution::PLStruct> DevXSuffixes();
std::span<const devilution::UniqueItem> DevXUniqueItems();

void DevXInitializeItem(devilution::Item &item, uint16_t baseItemId);
void DevXGenerateItem(const devilution::Player &player, devilution::Item &item, uint16_t baseItemId,
	uint32_t seed, int level, int uniqueProbability, bool onlyGood, bool preGenerated,
	int uniqueOffset = 0, bool forceNotUnique = false);
void DevXPackItem(devilution::ItemPack &packed, const devilution::Item &item, Game game);
void DevXUnpackItem(const devilution::ItemPack &packed, const devilution::Player &player,
	devilution::Item &item, Game game);

} // namespace d1hellforge
