#pragma once

#include "devx_adapter.hpp"
#include "save_document.hpp"

namespace d1hellforge {

struct NativeGeneratedItem {
	devilution::Item item;
	std::array<std::byte, 20> packedBytes {};
	uint32_t generationCalls = 0;
};

ItemGenerationChoices GetItemGenerationChoices(uint16_t baseItemId, const ContentProfile &profile, uint8_t level, bool onlyGood);
ItemGenerationChoices GetItemGenerationChoices(uint16_t baseItemId, Game game, uint8_t level, bool onlyGood);
std::expected<CharacterSummary::PackedItemSummary, std::string> CreateCatalogItem(uint16_t baseItemId, const ContentProfile &profile, ItemGenerationOptions options);
std::expected<CharacterSummary::PackedItemSummary, std::string> CreateCatalogItem(uint16_t baseItemId, const ContentProfile &profile);
std::expected<CharacterSummary::PackedItemSummary, std::string> CreateCatalogItem(uint16_t baseItemId, Game game, ItemGenerationOptions options);
std::expected<CharacterSummary::PackedItemSummary, std::string> CreateCatalogItem(uint16_t baseItemId, Game game);
std::expected<NativeGeneratedItem, std::string> GenerateCatalogItemNative(uint16_t baseItemId, const ContentProfile &profile, ItemGenerationOptions options);
std::expected<NativeGeneratedItem, std::string> GenerateExactSeedNative(uint16_t baseItemId, const ContentProfile &profile, const ItemGenerationOptions &options);
std::expected<CharacterSummary::PackedItemSummary, std::string> RegenerateItemWithSeed(const CharacterSummary::PackedItemSummary &item, Game game, uint32_t seed);

} // namespace d1hellforge
