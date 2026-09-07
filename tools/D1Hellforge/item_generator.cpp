#include "item_generator.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <mutex>
#include <optional>

#include "diablo.h"
#include "devx_adapter.hpp"
#include "engine/random.hpp"
#include "items.h"
#include "pack.h"
#include "tables/itemdat.h"
#include "utils/endian_swap.hpp"

namespace d1hellforge {
namespace {

devilution::AffixItemType AffixTypeFor(uint16_t baseItemId)
{
	const auto *baseItem = DevXBaseItem(baseItemId);
	if (baseItem == nullptr)
		return devilution::AffixItemType::None;
	switch (baseItem->itype) {
	case devilution::ItemType::Sword:
	case devilution::ItemType::Axe:
	case devilution::ItemType::Mace: return devilution::AffixItemType::Weapon;
	case devilution::ItemType::Bow: return devilution::AffixItemType::Bow;
	case devilution::ItemType::Shield: return devilution::AffixItemType::Shield;
	case devilution::ItemType::LightArmor:
	case devilution::ItemType::MediumArmor:
	case devilution::ItemType::HeavyArmor:
	case devilution::ItemType::Helm: return devilution::AffixItemType::Armor;
	case devilution::ItemType::Staff: return devilution::AffixItemType::Staff;
	case devilution::ItemType::Ring:
	case devilution::ItemType::Amulet: return devilution::AffixItemType::Misc;
	default: return devilution::AffixItemType::None;
	}
}

bool MagicNameMatches(const devilution::Item &item, const ItemGenerationOptions &options)
{
	const std::string name = item._iIName;
	if (!options.prefixName.empty() && !name.starts_with(options.prefixName + " "))
		return false;
	if (!options.suffixName.empty()) {
		const std::string ending = " of " + options.suffixName;
		if (!name.ends_with(ending))
			return false;
	}
	return true;
}

bool EffectMatches(const devilution::Item &candidate, ItemEffectFilter effectFilter)
{
	switch (effectFilter) {
	case ItemEffectFilter::Any: return true;
	case ItemEffectFilter::Damage: return candidate._iPLDam != 0 || candidate._iPLDamMod != 0 || candidate._iFMaxDam != 0 || candidate._iLMaxDam != 0;
	case ItemEffectFilter::ToHit: return candidate._iPLToHit != 0;
	case ItemEffectFilter::Attributes: return candidate._iPLStr != 0 || candidate._iPLMag != 0 || candidate._iPLDex != 0 || candidate._iPLVit != 0;
	case ItemEffectFilter::Resistances: return candidate._iPLFR != 0 || candidate._iPLLR != 0 || candidate._iPLMR != 0;
	case ItemEffectFilter::LifeOrMana: return candidate._iPLHP != 0 || candidate._iPLMana != 0
		    || devilution::HasAnyOf(candidate._iFlags, devilution::ItemSpecialEffect::StealMana3 | devilution::ItemSpecialEffect::StealMana5 | devilution::ItemSpecialEffect::StealLife3 | devilution::ItemSpecialEffect::StealLife5);
	case ItemEffectFilter::AttackSpeed: return devilution::HasAnyOf(candidate._iFlags, devilution::ItemSpecialEffect::QuickAttack | devilution::ItemSpecialEffect::FastAttack | devilution::ItemSpecialEffect::FasterAttack | devilution::ItemSpecialEffect::FastestAttack);
	}
	return true;
}

} // namespace

ItemGenerationChoices GetItemGenerationChoices(uint16_t baseItemId, Game game, uint8_t level, bool onlyGood)
{
	return GetItemGenerationChoices(baseItemId, GetDevXContentProfile(game), level, onlyGood);
}

ItemGenerationChoices GetItemGenerationChoices(uint16_t baseItemId, const ContentProfile &profile, uint8_t level, bool onlyGood)
{
	if (!ValidateDevXContentProfile(profile).has_value()) return {};
	struct CachedChoices {
		uint16_t baseItemId;
		uint8_t level;
		bool onlyGood;
		ItemGenerationChoices choices;
	};
	static std::mutex cacheMutex;
	static std::optional<ContentProfile> cachedProfile;
	static std::vector<CachedChoices> cache;
	std::scoped_lock lock(cacheMutex);
	if (!cachedProfile.has_value() || *cachedProfile != profile) {
		cachedProfile = profile;
		cache.clear();
	}
	const auto cached = std::find_if(cache.begin(), cache.end(), [baseItemId, level, onlyGood](const CachedChoices &entry) {
		return entry.baseItemId == baseItemId && entry.level == level && entry.onlyGood == onlyGood;
	});
	if (cached != cache.end()) return cached->choices;
	ItemGenerationChoices choices;
	const auto type = AffixTypeFor(baseItemId);
	if (type != devilution::AffixItemType::None) {
		auto appendAffixes = [type, level, onlyGood](const auto &source, auto &destination) {
			for (const auto &affix : source) {
				if (!devilution::HasAnyOf(type, affix.PLIType) || affix.PLMinLvl < level / 2 || affix.PLMinLvl > level || (onlyGood && !affix.PLOk))
					continue;
				if (std::find(destination.begin(), destination.end(), affix.PLName) == destination.end())
					destination.push_back(affix.PLName);
			}
		};
		appendAffixes(DevXPrefixes(), choices.prefixes);
		appendAffixes(DevXSuffixes(), choices.suffixes);
	}
	if (const auto *baseItem = DevXBaseItem(baseItemId); baseItem != nullptr) {
		const auto uniqueBase = baseItem->iItemId;
		for (const auto &unique : DevXUniqueItems()) {
			if (unique.UIItemId == uniqueBase && unique.UIMinLvl <= level)
				choices.uniques.push_back(unique.UIName);
		}
	}
	cache.push_back({ baseItemId, level, onlyGood, choices });
	return choices;
}

std::expected<CharacterSummary::PackedItemSummary, std::string> CreateCatalogItem(uint16_t baseItemId, Game game)
{
	return CreateCatalogItem(baseItemId, GetDevXContentProfile(game));
}

std::expected<CharacterSummary::PackedItemSummary, std::string> CreateCatalogItem(uint16_t baseItemId, const ContentProfile &profile)
{
	ItemGenerationOptions options;
	options.seed = static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	return CreateCatalogItem(baseItemId, profile, options);
}

std::expected<CharacterSummary::PackedItemSummary, std::string> CreateCatalogItem(uint16_t baseItemId, Game game, ItemGenerationOptions options)
{
	return CreateCatalogItem(baseItemId, GetDevXContentProfile(game), std::move(options));
}

std::expected<NativeGeneratedItem, std::string> GenerateCatalogItemNative(uint16_t baseItemId, const ContentProfile &profile, ItemGenerationOptions options)
{
	const Game game = profile.baseMode;
	if (const auto valid = ValidateDevXContentProfile(profile); !valid.has_value())
		return std::unexpected(valid.error());
	const auto *baseItem = DevXBaseItem(baseItemId);
	if (baseItem == nullptr)
		return std::unexpected("The selected catalog item identifier is invalid");
	devilution::Item item;
	uint32_t generationCalls = 0;
	if (options.quality == ItemQualityChoice::Normal) {
		devilution::SetRndSeed(options.seed);
		DevXInitializeItem(item, baseItemId);
		++generationCalls;
		item._iSeed = options.seed;
	} else {
		devilution::Player player;
		player._pMaxHPBase = 125 << 6;
		player._pMaxManaBase = 125 << 6;
		const auto wanted = options.quality == ItemQualityChoice::Unique ? devilution::ITEM_QUALITY_UNIQUE : devilution::ITEM_QUALITY_MAGIC;
		if (options.quality == ItemQualityChoice::Unique) {
			const auto uniqueBase = baseItem->iItemId;
			const auto uniqueItems = DevXUniqueItems();
			const bool hasUnique = std::any_of(uniqueItems.begin(), uniqueItems.end(), [uniqueBase](const auto &entry) { return entry.UIItemId == uniqueBase; });
			if (!hasUnique) return std::unexpected("This base item has no game-defined unique variant");
			if (!options.uniqueName.empty()) {
				std::vector<std::size_t> valid;
				for (std::size_t index = 0; index < uniqueItems.size(); ++index) {
					const auto &unique = uniqueItems[index];
					if (unique.UIItemId == uniqueBase && unique.UIMinLvl <= options.level)
						valid.push_back(index);
				}
				const auto selected = std::find_if(valid.begin(), valid.end(), [&options, uniqueItems](std::size_t index) { return uniqueItems[index].UIName == options.uniqueName; });
				if (selected == valid.end()) return std::unexpected("The selected unique is not compatible with this base item and level");
				const int uniqueOffset = static_cast<int>(valid.size() - 1 - std::distance(valid.begin(), selected));
				DevXGenerateItem(player, item, baseItemId, options.seed,
				    std::clamp<int>(options.level, 1, 63), 100, options.onlyGood, false, uniqueOffset, false);
				++generationCalls;
				if (item.isEmpty() || item._iMagical != devilution::ITEM_QUALITY_UNIQUE || item._iIName != options.uniqueName)
					return std::unexpected("The game could not generate the selected unique with these options");
				devilution::SetupItem(item);
			}
		}
		bool found = options.quality == ItemQualityChoice::Unique && !options.uniqueName.empty();
		std::string lastGeneratedName;
		const uint32_t maximumAttempts = (!options.prefixName.empty() || !options.suffixName.empty()) ? 65536 : 4096;
		for (uint32_t attempt = 0; !found && attempt < maximumAttempts; ++attempt) {
			const uint32_t candidateSeed = options.seed + attempt * 0x9E3779B9U;
			item = {};
			DevXGenerateItem(player, item, baseItemId, candidateSeed,
			    std::clamp<int>(options.level, 1, 63), 15, options.onlyGood, false, 0,
			    options.quality == ItemQualityChoice::Magic);
			++generationCalls;
			lastGeneratedName = item._iIName;
			if (!item.isEmpty() && item._iMagical == wanted && EffectMatches(item, options.effectFilter) && MagicNameMatches(item, options)) {
				devilution::SetupItem(item);
				found = true;
				break;
			}
		}
		if (!found)
			return std::unexpected(options.quality == ItemQualityChoice::Unique
			        ? "No game-valid unique variant was found for this base item and level"
			        : std::format("No game-valid magic item matching prefix '{}', suffix '{}', and the selected effect was found; last generated name was '{}'", options.prefixName, options.suffixName, lastGeneratedName));
		item._iIdentified = true;
	}
	devilution::ItemPack packed;
	DevXPackItem(packed, item, game);
	std::array<std::byte, 20> bytes {};
	std::memcpy(bytes.data(), &packed, sizeof(packed));
	return NativeGeneratedItem { std::move(item), bytes, generationCalls };
}

std::expected<NativeGeneratedItem, std::string> GenerateExactSeedNative(uint16_t baseItemId, const ContentProfile &profile, const ItemGenerationOptions &options)
{
	const Game game = profile.baseMode;
	if (const auto valid = ValidateDevXContentProfile(profile); !valid.has_value())
		return std::unexpected(valid.error());
	const auto *baseItem = DevXBaseItem(baseItemId);
	if (baseItem == nullptr)
		return std::unexpected("The selected catalog item identifier is invalid");

	devilution::Item item;
	if (options.quality == ItemQualityChoice::Normal) {
		devilution::SetRndSeed(options.seed);
		DevXInitializeItem(item, baseItemId);
		item._iSeed = options.seed;
	} else {
		devilution::Player player;
		player._pMaxHPBase = 125 << 6;
		player._pMaxManaBase = 125 << 6;
		int uniqueOffset = 0;
		if (options.quality == ItemQualityChoice::Unique && !options.uniqueName.empty()) {
			const auto uniqueItems = DevXUniqueItems();
			std::vector<std::size_t> valid;
			for (std::size_t index = 0; index < uniqueItems.size(); ++index) {
				const auto &unique = uniqueItems[index];
				if (unique.UIItemId == baseItem->iItemId && unique.UIMinLvl <= options.level)
					valid.push_back(index);
			}
			const auto selected = std::find_if(valid.begin(), valid.end(), [&options, uniqueItems](std::size_t index) { return uniqueItems[index].UIName == options.uniqueName; });
			if (selected == valid.end()) return std::unexpected("The selected unique is not compatible with this base item and level");
			uniqueOffset = static_cast<int>(valid.size() - 1 - std::distance(valid.begin(), selected));
		}
		DevXGenerateItem(player, item, baseItemId, options.seed,
		    std::clamp<int>(options.level, 1, 63),
		    options.quality == ItemQualityChoice::Unique ? 100 : 15,
		    options.onlyGood, false, uniqueOffset,
		    options.quality == ItemQualityChoice::Magic);
	}

	if (item.isEmpty() || item._iSeed != options.seed)
		return std::unexpected("DevilutionX did not produce the requested exact seed");
	const auto wanted = options.quality == ItemQualityChoice::Unique ? devilution::ITEM_QUALITY_UNIQUE
	    : options.quality == ItemQualityChoice::Magic ? devilution::ITEM_QUALITY_MAGIC
	                                                 : devilution::ITEM_QUALITY_NORMAL;
	if (item._iMagical != wanted || !EffectMatches(item, options.effectFilter) || !MagicNameMatches(item, options))
		return std::unexpected("The exact seed does not match the selected quality or affix locks");
	if (options.quality != ItemQualityChoice::Normal) {
		item._iIdentified = true;
		devilution::SetupItem(item);
	}

	devilution::ItemPack packed;
	DevXPackItem(packed, item, game);
	std::array<std::byte, 20> bytes {};
	std::memcpy(bytes.data(), &packed, sizeof(packed));
	return NativeGeneratedItem { std::move(item), bytes, 1 };
}

std::expected<CharacterSummary::PackedItemSummary, std::string> CreateCatalogItem(uint16_t baseItemId, const ContentProfile &profile, ItemGenerationOptions options)
{
	auto generated = GenerateCatalogItemNative(baseItemId, profile, std::move(options));
	if (!generated.has_value()) return std::unexpected(generated.error());
	return SummarizeImportedItem(generated->packedBytes, profile.baseMode);
}

std::expected<CharacterSummary::PackedItemSummary, std::string> RegenerateItemWithSeed(const CharacterSummary::PackedItemSummary &item, Game game, uint32_t seed)
{
	devilution::ItemPack packed {};
	std::memcpy(&packed, item.packedBytes.data(), sizeof(packed));
	packed.iSeed = devilution::Swap32LE(seed);
	std::array<std::byte, 20> bytes {};
	std::memcpy(bytes.data(), &packed, sizeof(packed));
	auto regenerated = SummarizeImportedItem(bytes, game);
	if (!regenerated.has_value())
		return std::unexpected(regenerated.error());
	regenerated->slot = item.slot;
	return regenerated;
}

} // namespace d1hellforge
