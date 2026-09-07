#include "devx_adapter.hpp"

#include <filesystem>
#include <optional>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "diablo.h"
#include "engine/assets.hpp"
#include "mods/mod_identity.h"
#include "tables/spelldat.h"
#include "utils/paths.h"

namespace d1hellforge {
namespace {

std::optional<Game> LoadedGame;
ContentProfile ActiveContentProfile;

std::string ContentDisplayLabel(const ContentProfile &profile)
{
	std::string label = profile.baseMode == Game::Hellfire ? "Hellfire" : "Diablo";
	if (!profile.contentIdentifiers.empty()) {
		label += " [content: ";
		for (std::size_t index = 0; index < profile.contentIdentifiers.size(); ++index) {
			if (index != 0) label += ", ";
			label += profile.contentIdentifiers[index];
		}
		label += "]";
	}
	if (profile.looseContentActive)
		label += " + loose content";
	return label;
}

void CaptureContentProfile(Game game)
{
	ActiveContentProfile = {};
	ActiveContentProfile.baseMode = game;
	for (const auto &identifier : devilution::ActiveModIdentifiers)
		ActiveContentProfile.contentIdentifiers.push_back(identifier.name);
	ActiveContentProfile.looseContentActive = devilution::HasLooseLogicAssets();
	ActiveContentProfile.displayLabel = ContentDisplayLabel(ActiveContentProfile);
	ActiveContentProfile.initialized = true;
}

} // namespace

void InitializeDevXContent(Game game, bool isSpawn)
{
	devilution::gbIsHellfire = game == Game::Hellfire;
	devilution::gbIsSpawn = isSpawn;
	devilution::gbIsMultiplayer = false;
	if (LoadedGame == game) return;
	ActiveContentProfile = {};
	ActiveContentProfile.baseMode = game;
	ActiveContentProfile.displayLabel = game == Game::Hellfire ? "Hellfire" : "Diablo";
#ifdef _WIN32
	std::wstring executable(MAX_PATH, L'\0');
	const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
	if (length != 0) {
		executable.resize(length);
		const auto assets = std::filesystem::path(executable).parent_path().parent_path().parent_path() / "assets";
		if (std::filesystem::is_directory(assets)) devilution::paths::SetAssetsPath(assets.string());
	}
#endif
	devilution::UnloadModArchives();
	if (game == Game::Hellfire) devilution::LoadModArchives({ { "hf" } });
	devilution::LoadSpellData();
	devilution::LoadItemData();
	LoadedGame = game;
	CaptureContentProfile(game);
}

const ContentProfile &GetDevXContentProfile(Game game, bool isSpawn)
{
	InitializeDevXContent(game, isSpawn);
	return ActiveContentProfile;
}

std::expected<void, std::string> ValidateDevXContentProfile(const ContentProfile &profile)
{
	if (!profile.initialized)
		return std::unexpected(profile.initializationError.empty() ? "The content profile is not initialized" : profile.initializationError);
	if (!profile.initializationError.empty())
		return std::unexpected(profile.initializationError);
	const auto &active = GetDevXContentProfile(profile.baseMode);
	if (active.baseMode != profile.baseMode || active.contentIdentifiers != profile.contentIdentifiers
	    || active.looseContentActive != profile.looseContentActive)
		return std::unexpected("The requested content profile does not match the initialized DevilutionX content");
	return {};
}

const devilution::ItemData *DevXBaseItem(uint16_t baseItemId)
{
	if (baseItemId >= devilution::AllItemsList.size()) return nullptr;
	return &devilution::AllItemsList[baseItemId];
}

std::span<const devilution::ItemData> DevXBaseItems() { return devilution::AllItemsList; }

std::span<const devilution::PLStruct> DevXPrefixes() { return devilution::ItemPrefixes; }
std::span<const devilution::PLStruct> DevXSuffixes() { return devilution::ItemSuffixes; }
std::span<const devilution::UniqueItem> DevXUniqueItems() { return devilution::UniqueItems; }

void DevXInitializeItem(devilution::Item &item, uint16_t baseItemId)
{
	devilution::InitializeItem(item, static_cast<devilution::_item_indexes>(baseItemId));
}

void DevXGenerateItem(const devilution::Player &player, devilution::Item &item, uint16_t baseItemId,
	uint32_t seed, int level, int uniqueProbability, bool onlyGood, bool preGenerated,
	int uniqueOffset, bool forceNotUnique)
{
	devilution::SetupAllItems(player, item, static_cast<devilution::_item_indexes>(baseItemId), seed,
	    level, uniqueProbability, onlyGood, preGenerated, uniqueOffset, forceNotUnique);
}

void DevXPackItem(devilution::ItemPack &packed, const devilution::Item &item, Game game)
{
	devilution::PackItem(packed, item, game == Game::Hellfire);
}

void DevXUnpackItem(const devilution::ItemPack &packed, const devilution::Player &player,
	devilution::Item &item, Game game)
{
	devilution::UnPackItem(packed, player, item, game == Game::Hellfire);
}

} // namespace d1hellforge
