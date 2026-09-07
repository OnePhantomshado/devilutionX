#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "content_profile.hpp"

namespace d1hellforge {

struct CharacterSummary {
	struct PackedItemSummary {
		uint8_t slot = 0;
		uint16_t baseItemId = 0;
		std::string baseName;
		uint8_t cursorGraphic = 0;
		uint32_t seed = 0;
		uint8_t durability = 0;
		uint8_t maxDurability = 0;
		uint8_t charges = 0;
		uint8_t maxCharges = 0;
		uint16_t value = 0;
		bool identified = false;
		uint8_t magicalQuality = 0;
		bool canBePlacedOnBelt = false;
		std::string equipType;
		uint8_t minimumStrength = 0;
		uint8_t minimumMagic = 0;
		uint8_t minimumDexterity = 0;
		std::string displayName;
		std::string reconstructedDisplayName;
		std::string storedActiveGameName;
		std::vector<std::string> detailLines;
		bool activeGameIdentityDiffers = false;
		std::vector<std::byte> fullItemRecord;
		std::array<std::byte, 20> packedBytes {};
	};

	std::filesystem::path path;
	std::string name;
	uint8_t characterClass = 0;
	uint8_t level = 0;
	uint8_t strength = 0;
	uint8_t magic = 0;
	uint8_t dexterity = 0;
	uint8_t vitality = 0;
	uint8_t unspentStatPoints = 0;
	uint32_t experience = 0;
	int32_t gold = 0;
	Game game = Game::Diablo;
	ContentProfile contentProfile { Game::Diablo };
	std::filesystem::path gameDataDirectory;
	bool gameGraphicsAvailable = false;
	std::string gameGraphicsStatus;
	std::vector<PackedItemSummary> equipment;
	std::vector<PackedItemSummary> inventory;
	std::vector<PackedItemSummary> belt;
	std::array<int8_t, 40> inventoryGrid {};
};

struct EditableStats {
	uint8_t strength = 0;
	uint8_t magic = 0;
	uint8_t dexterity = 0;
	uint8_t vitality = 0;
	uint8_t unspentStatPoints = 0;
};

struct SaveResult {
	std::filesystem::path backupPath;
};

struct CatalogItem {
	uint16_t baseItemId = 0;
	std::string name;
	std::string category;
};

enum class ItemQualityChoice : uint8_t {
	Normal,
	Magic,
	Unique,
};

enum class ItemEffectFilter : uint8_t {
	Any,
	Damage,
	ToHit,
	Attributes,
	Resistances,
	LifeOrMana,
	AttackSpeed,
};

struct ItemGenerationOptions {
	uint32_t seed = 0;
	ItemQualityChoice quality = ItemQualityChoice::Normal;
	uint8_t level = 30;
	bool onlyGood = true;
	ItemEffectFilter effectFilter = ItemEffectFilter::Any;
	std::string prefixName;
	std::string suffixName;
	std::string uniqueName;
};

struct ItemGenerationChoices {
	std::vector<std::string> prefixes;
	std::vector<std::string> suffixes;
	std::vector<std::string> uniques;
};

std::filesystem::path DefaultSaveDirectory();
std::vector<std::filesystem::path> FindSaves(const std::filesystem::path &directory);
std::expected<CharacterSummary, std::string> ReadCharacterSummary(const std::filesystem::path &path);
std::expected<std::optional<EditableStats>, std::string> ReadActiveGameStats(const std::filesystem::path &path);
std::expected<SaveResult, std::string> SaveCharacterStats(const std::filesystem::path &path, const EditableStats &stats);
std::expected<SaveResult, std::string> SaveCharacterInventory(const std::filesystem::path &path, CharacterSummary inventory);
const char *ClassName(uint8_t characterClass, Game game);
std::expected<CharacterSummary::PackedItemSummary, std::string> SummarizeImportedItem(const std::array<std::byte, 20> &packedBytes, Game game);
std::expected<std::array<std::byte, 20>, std::string> ConvertPackedItem(const std::array<std::byte, 20> &packedBytes, Game source, Game destination);
std::expected<void, std::string> ExportLegacyItem(const std::filesystem::path &path, const CharacterSummary::PackedItemSummary &item, Game game);

} // namespace d1hellforge
