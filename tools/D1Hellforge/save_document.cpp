#include "save_document.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <fstream>
#include <format>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "codec.h"
#include "diablo.h"
#include "devx_adapter.hpp"
#include "engine/random.hpp"
#include "items.h"
#include "item_catalog.hpp"
#include "item_display.hpp"
#include "item_advanced_editor.hpp"
#include "item_generator.hpp"
#include "loadsave.h"
#include "mpq/mpq_reader.hpp"
#include "mpq/mpq_writer.hpp"
#include "pack.h"
#include "tables/itemdat.h"
#include "tables/spelldat.h"
#include "utils/endian_swap.hpp"

namespace d1hellforge {
namespace {

constexpr char SinglePlayerPassword[] = "xrgyrkj1";
constexpr char MultiplayerPassword[] = "szqnlsk1";
constexpr char SpawnSinglePlayerPassword[] = "adslhfb1";
constexpr char SpawnMultiplayerPassword[] = "lshbkfg1";

std::string Lowercase(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

const char *PasswordFor(const std::filesystem::path &path)
{
	const std::string filename = Lowercase(path.filename().string());
	if (filename.starts_with("share_"))
		return SpawnMultiplayerPassword;
	if (filename.starts_with("spawn_"))
		return SpawnSinglePlayerPassword;
	if (filename.starts_with("multi_"))
		return MultiplayerPassword;
	return SinglePlayerPassword;
}

bool IsSpawnSave(const std::filesystem::path &path)
{
	const std::string filename = Lowercase(path.filename().string());
	return filename.starts_with("share_") || filename.starts_with("spawn_");
}

std::optional<std::filesystem::path> FindFileIgnoringCase(const std::filesystem::path &directory, std::string_view filename)
{
	std::error_code error;
	for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
		if (!it->is_regular_file(error))
			continue;
		if (Lowercase(it->path().filename().string()) == Lowercase(std::string(filename)))
			return it->path();
	}
	return std::nullopt;
}

bool HasInventoryGraphics(const std::filesystem::path &mpqPath)
{
	const auto archive = devilution::MpqArchive::Open(mpqPath.string().c_str());
	return archive.has_value()
	    && archive->HasFile("data\\inv\\objcurs.cel")
	    && archive->HasFile("data\\inv\\inv.cel");
}

bool HasHellfireInventoryGraphics(const std::filesystem::path &mpqPath)
{
	const auto archive = devilution::MpqArchive::Open(mpqPath.string().c_str());
	return archive.has_value() && archive->HasFile("data\\inv\\objcurs2.cel");
}

void LocateGameData(CharacterSummary &summary)
{
	std::vector<std::filesystem::path> candidates;
	auto addWithParents = [&candidates](std::filesystem::path directory) {
		for (int i = 0; i < 4 && !directory.empty(); ++i) {
			candidates.push_back(directory);
			directory = directory.parent_path();
		}
	};
	addWithParents(summary.path.parent_path());
#ifdef _WIN32
	std::wstring executable(MAX_PATH, L'\0');
	const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
	if (length != 0) {
		executable.resize(length);
		addWithParents(std::filesystem::path(executable).parent_path());
	}
	candidates.emplace_back("C:/GOG Games/Diablo");
	candidates.emplace_back("C:/Program Files/GOG Galaxy/Games/Diablo");
	candidates.emplace_back("C:/Program Files (x86)/GOG Galaxy/Games/Diablo");
#endif

	for (const auto &candidate : candidates) {
		const auto diabdat = FindFileIgnoringCase(candidate, "diabdat.mpq");
		if (!diabdat.has_value() || !HasInventoryGraphics(*diabdat))
			continue;
		if (summary.game == Game::Hellfire) {
			auto hellfire = FindFileIgnoringCase(candidate, "hellfire.mpq");
			if (!hellfire.has_value()) hellfire = FindFileIgnoringCase(candidate / "hellfire", "hellfire.mpq");
			if (!hellfire.has_value() || !HasHellfireInventoryGraphics(*hellfire))
				continue;
		}
		summary.gameDataDirectory = candidate;
		summary.gameGraphicsAvailable = true;
		summary.gameGraphicsStatus = summary.game == Game::Hellfire
		    ? "Hellfire rules + Diablo/Hellfire MPQs (including Hellfire item sprites)"
		    : "Diablo rules + Diablo MPQ item sprites";
		return;
	}
	summary.gameGraphicsStatus = "No validated MPQ inventory assets found; text fallback active";
}

std::expected<devilution::PlayerPack, std::string> ReadPlayerPack(const std::filesystem::path &path)
{
	auto archive = devilution::MpqArchive::Open(path.string().c_str());
	if (!archive.has_value())
		return std::unexpected("Unable to open the save archive: " + archive.error());

	std::size_t encodedSize = 0;
	int32_t error = 0;
	auto encoded = archive->ReadFile("hero", encodedSize, error);
	if (error != 0 || encoded == nullptr)
		return std::unexpected("The save archive does not contain a readable hero record");

	const std::size_t decodedSize = devilution::codec_decode(encoded.get(), encodedSize, PasswordFor(path));
	if (decodedSize != sizeof(devilution::PlayerPack))
		return std::unexpected("The hero record is corrupt or uses an unsupported save format");

	devilution::PlayerPack result {};
	std::memcpy(&result, encoded.get(), sizeof(result));
	return result;
}

std::expected<std::vector<std::byte>, std::string> ReadDecodedEntry(const std::filesystem::path &path, std::string_view name)
{
	auto archive = devilution::MpqArchive::Open(path.string().c_str());
	if (!archive.has_value())
		return std::unexpected("Unable to open the save archive: " + archive.error());
	if (!archive->HasFile(name))
		return std::vector<std::byte> {};
	std::size_t encodedSize = 0;
	int32_t error = 0;
	auto encoded = archive->ReadFile(name, encodedSize, error);
	if (error != 0 || encoded == nullptr)
		return std::unexpected("Unable to read the " + std::string(name) + " record");
	const std::size_t decodedSize = devilution::codec_decode(encoded.get(), encodedSize, PasswordFor(path));
	if (decodedSize == 0)
		return std::unexpected("Unable to decode the " + std::string(name) + " record");
	return std::vector<std::byte>(encoded.get(), encoded.get() + decodedSize);
}

uint32_t ReadLE32(const std::byte *source)
{
	return static_cast<uint32_t>(source[0])
	    | (static_cast<uint32_t>(source[1]) << 8)
	    | (static_cast<uint32_t>(source[2]) << 16)
	    | (static_cast<uint32_t>(source[3]) << 24);
}

void WriteLE32(std::byte *destination, uint32_t value)
{
	destination[0] = static_cast<std::byte>(value);
	destination[1] = static_cast<std::byte>(value >> 8);
	destination[2] = static_cast<std::byte>(value >> 16);
	destination[3] = static_cast<std::byte>(value >> 24);
}

void WriteLE16(std::byte *destination, uint16_t value)
{
	destination[0] = static_cast<std::byte>(value);
	destination[1] = static_cast<std::byte>(value >> 8);
}

template <typename T>
void WriteItemField(std::vector<std::byte> &record, std::size_t offset, T value)
{
	static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
	const uint64_t converted = static_cast<uint64_t>(value);
	for (std::size_t i = 0; i < sizeof(T); ++i)
		record[offset + i] = static_cast<std::byte>(converted >> (i * 8));
}

std::expected<std::vector<std::byte>, std::string> ExpandPackedItem(const CharacterSummary::PackedItemSummary *summary, Game game)
{
	const bool hellfire = game == Game::Hellfire;
	const std::size_t size = hellfire ? 372 : 368;
	std::vector<std::byte> record(size);
	if (summary == nullptr) {
		WriteItemField<int32_t>(record, 8, -1);
		WriteItemField<int32_t>(record, 360, 0);
		return record;
	}
	if (summary->fullItemRecord.size() == size)
		return summary->fullItemRecord;
	devilution::ItemPack packed {};
	std::memcpy(&packed, summary->packedBytes.data(), sizeof(packed));
	InitializeDevXContent(game);
	devilution::Player player;
	player._pMaxHPBase = 125 << 6;
	player._pMaxManaBase = 125 << 6;
	devilution::Item item;
	DevXUnpackItem(packed, player, item, game);
	if (item.isEmpty())
		return std::unexpected("DevilutionX rejected " + summary->baseName + " while expanding it for the active game");

	WriteItemField(record, 0, item._iSeed);
	WriteItemField(record, 4, item._iCreateInfo);
	WriteItemField<int32_t>(record, 8, static_cast<int32_t>(item._itype));
	WriteItemField<uint32_t>(record, 20, item._iAnimFlag ? 1 : 0);
	WriteItemField<int32_t>(record, 28, item.AnimInfo.numberOfFrames);
	WriteItemField<int32_t>(record, 32, item.AnimInfo.currentFrame + 1);
	WriteItemField<int32_t>(record, 36, 96);
	WriteItemField<int32_t>(record, 40, 48);
	WriteItemField<uint8_t>(record, 48, static_cast<uint8_t>(item.selectionRegion));
	WriteItemField<uint32_t>(record, 52, item._iPostDraw ? 1 : 0);
	WriteItemField<uint32_t>(record, 56, item._iIdentified ? 1 : 0);
	WriteItemField<int8_t>(record, 60, item._iMagical);
	std::memcpy(record.data() + 61, item._iName, sizeof(item._iName));
	std::memcpy(record.data() + 125, item._iIName, sizeof(item._iIName));
	WriteItemField<int8_t>(record, 189, item._iLoc);
	WriteItemField<uint8_t>(record, 190, item._iClass);
	WriteItemField<int32_t>(record, 192, item._iCurs);
	WriteItemField<int32_t>(record, 196, item._ivalue);
	WriteItemField<int32_t>(record, 200, item._iIvalue);
	WriteItemField<int32_t>(record, 204, item._iMinDam);
	WriteItemField<int32_t>(record, 208, item._iMaxDam);
	WriteItemField<int32_t>(record, 212, item._iAC);
	WriteItemField<uint32_t>(record, 216, static_cast<uint32_t>(item._iFlags));
	WriteItemField<int32_t>(record, 220, item._iMiscId);
	WriteItemField<int32_t>(record, 224, static_cast<int8_t>(item._iSpell));
	WriteItemField<int32_t>(record, 228, item._iCharges);
	WriteItemField<int32_t>(record, 232, item._iMaxCharges);
	WriteItemField<int32_t>(record, 236, item._iDurability);
	WriteItemField<int32_t>(record, 240, item._iMaxDur);
	WriteItemField<int32_t>(record, 244, item._iPLDam);
	WriteItemField<int32_t>(record, 248, item._iPLToHit);
	WriteItemField<int32_t>(record, 252, item._iPLAC);
	WriteItemField<int32_t>(record, 256, item._iPLStr);
	WriteItemField<int32_t>(record, 260, item._iPLMag);
	WriteItemField<int32_t>(record, 264, item._iPLDex);
	WriteItemField<int32_t>(record, 268, item._iPLVit);
	WriteItemField<int32_t>(record, 272, item._iPLFR);
	WriteItemField<int32_t>(record, 276, item._iPLLR);
	WriteItemField<int32_t>(record, 280, item._iPLMR);
	WriteItemField<int32_t>(record, 284, item._iPLMana);
	WriteItemField<int32_t>(record, 288, item._iPLHP);
	WriteItemField<int32_t>(record, 292, item._iPLDamMod);
	WriteItemField<int32_t>(record, 296, item._iPLGetHit);
	WriteItemField<int32_t>(record, 300, item._iPLLight);
	WriteItemField<int8_t>(record, 304, item._iSplLvlAdd);
	WriteItemField<int8_t>(record, 305, item._iRequest ? 1 : 0);
	const auto uniqueItems = DevXUniqueItems();
	WriteItemField<int32_t>(record, 308, item._iUid >= 0 && static_cast<std::size_t>(item._iUid) < uniqueItems.size() ? uniqueItems[item._iUid].mappingId : 0);
	WriteItemField<int32_t>(record, 312, item._iFMinDam);
	WriteItemField<int32_t>(record, 316, item._iFMaxDam);
	WriteItemField<int32_t>(record, 320, item._iLMinDam);
	WriteItemField<int32_t>(record, 324, item._iLMaxDam);
	WriteItemField<int32_t>(record, 328, item._iPLEnAc);
	WriteItemField<int8_t>(record, 332, item._iPrePower);
	WriteItemField<int8_t>(record, 333, item._iSufPower);
	WriteItemField<int32_t>(record, 336, item._iVAdd1);
	WriteItemField<int32_t>(record, 340, item._iVMult1);
	WriteItemField<int32_t>(record, 344, item._iVAdd2);
	WriteItemField<int32_t>(record, 348, item._iVMult2);
	WriteItemField<int8_t>(record, 352, item._iMinStr);
	WriteItemField<uint8_t>(record, 353, item._iMinMag);
	WriteItemField<int8_t>(record, 354, item._iMinDex);
	WriteItemField<uint32_t>(record, 356, item._iStatFlag ? 1 : 0);
	WriteItemField<int32_t>(record, 360, devilution::Swap16LE(packed.idx));
	WriteItemField<uint32_t>(record, 364, item.dwBuff);
	if (hellfire)
		WriteItemField<uint32_t>(record, 368, static_cast<uint32_t>(item._iDamAcFlags));
	return record;
}

void PopulateGameItemDetails(CharacterSummary::PackedItemSummary &summary, const devilution::ItemPack &packed, Game game, bool isSpawn)
{
	const bool hellfire = game == Game::Hellfire;
	InitializeDevXContent(game, isSpawn);
	devilution::Player player;
	player._pMaxHPBase = 125 << 6;
	player._pMaxManaBase = 125 << 6;
	devilution::Item item;
	DevXUnpackItem(packed, player, item, game);
	if (item.isEmpty()) return;
	// Town-generated packed items can recreate a different base item from their
	// seed; the regenerated item is what DevilutionX will actually use.
	const auto regeneratedId = static_cast<uint16_t>(item.IDidx);
	if (const auto entry = GetBaseItemMetadata(regeneratedId); entry.has_value()) {
		summary.baseItemId = regeneratedId;
		summary.baseName = entry->name;
		summary.cursorGraphic = entry->cursorGraphic;
		summary.canBePlacedOnBelt = entry->canBePlacedOnBelt;
		summary.equipType = entry->equipType;
		summary.minimumStrength = entry->minimumStrength;
		summary.minimumMagic = entry->minimumMagic;
		summary.minimumDexterity = entry->minimumDexterity;
	}
	summary.displayName = item._iIdentified || item._iMagical == devilution::ITEM_QUALITY_NORMAL ? item._iIName : item._iName;
	if (summary.displayName.empty()) summary.displayName = summary.baseName;
	summary.reconstructedDisplayName = summary.displayName;
	summary.detailLines = RenderItemDisplayPlainText(BuildItemDisplay(item, ItemDisplayMode::Detailed, GetDevXContentProfile(game, isSpawn)), false);
}

std::expected<std::size_t, std::string> FindGameStatsOffset(const std::vector<std::byte> &gameData, const devilution::PlayerPack &player)
{
	if (gameData.empty())
		return std::unexpected("Save has no active-game record");
	constexpr std::size_t BytesAfterNameToStrength = devilution::PlayerNameLength + 4;
	constexpr std::size_t StatBlockSize = 9 * sizeof(uint32_t);
	const std::size_t nameLength = strnlen(player.pName, devilution::PlayerNameLength);
	std::optional<std::size_t> match;
	for (std::size_t offset = 0; offset + BytesAfterNameToStrength + StatBlockSize <= gameData.size(); ++offset) {
		if (std::memcmp(&gameData[offset], player.pName, nameLength) != 0)
			continue;
		if (nameLength < devilution::PlayerNameLength && gameData[offset + nameLength] != std::byte { 0 })
			continue;
		if (gameData[offset + devilution::PlayerNameLength] != static_cast<std::byte>(player.pClass))
			continue;
		const std::size_t statsOffset = offset + BytesAfterNameToStrength;
		if (match.has_value())
			return std::unexpected("The active game contains more than one matching character attribute block");
		match = statsOffset;
	}
	if (match.has_value())
		return *match;
	return std::unexpected("Unable to locate the character attribute block in the active game");
}

std::expected<void, std::string> UpdateGameStats(std::vector<std::byte> &gameData, const devilution::PlayerPack &player, const EditableStats &stats)
{
	if (gameData.empty())
		return {};
	const auto locatedOffset = FindGameStatsOffset(gameData, player);
	if (!locatedOffset.has_value())
		return std::unexpected(locatedOffset.error());
	const std::size_t strengthOffset = *locatedOffset;
	const std::size_t pointsOffset = strengthOffset + 8 * sizeof(uint32_t);

	const uint32_t newBaseValues[] { stats.strength, stats.magic, stats.dexterity, stats.vitality };
	for (std::size_t i = 0; i < 4; ++i) {
		const std::size_t currentOffset = strengthOffset + i * 2 * sizeof(uint32_t);
		const std::size_t baseOffset = currentOffset + sizeof(uint32_t);
		const int32_t current = static_cast<int32_t>(ReadLE32(&gameData[currentOffset]));
		const int32_t oldBase = static_cast<int32_t>(ReadLE32(&gameData[baseOffset]));
		const int32_t adjustedCurrent = current + static_cast<int32_t>(newBaseValues[i]) - oldBase;
		WriteLE32(&gameData[currentOffset], static_cast<uint32_t>(adjustedCurrent));
		WriteLE32(&gameData[baseOffset], newBaseValues[i]);
	}
	WriteLE32(&gameData[pointsOffset], stats.unspentStatPoints);
	return {};
}

std::filesystem::path BackupPathFor(const std::filesystem::path &path)
{
	const auto now = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
	const std::string timestamp = std::format("{:%Y-%m-%d_%H-%M-%S}", now);
	const std::filesystem::path backupDirectory = path.parent_path() / "Backups";
	const std::string filename = path.stem().string() + "_" + timestamp;
	std::filesystem::path candidate = backupDirectory / (filename + path.extension().string());
	for (unsigned suffix = 2; std::filesystem::exists(candidate); ++suffix)
		candidate = backupDirectory / (filename + "-" + std::to_string(suffix) + path.extension().string());
	return candidate;
}

CharacterSummary::PackedItemSummary SummarizeItem(const devilution::ItemPack &item, uint8_t slot, Game game, bool isSpawn)
{
	auto itemIndex = static_cast<devilution::_item_indexes>(devilution::Swap16LE(item.idx));
	if (isSpawn)
		itemIndex = devilution::RemapItemIdxFromSpawn(itemIndex);
	if (game == Game::Diablo)
		itemIndex = devilution::RemapItemIdxFromDiablo(itemIndex);
	const uint16_t baseItemId = static_cast<uint16_t>(itemIndex);
	const auto catalogEntry = GetBaseItemMetadata(baseItemId);
	CharacterSummary::PackedItemSummary result {
	    slot,
	    baseItemId,
	    catalogEntry.has_value() ? catalogEntry->name : std::string {},
	    catalogEntry.has_value() ? catalogEntry->cursorGraphic : uint8_t { 0 },
	    devilution::Swap32LE(item.iSeed),
	    item.bDur,
	    item.bMDur,
	    item.bCh,
	    item.bMCh,
	    devilution::Swap16LE(item.wValue),
	    (item.bId & 1) != 0,
	    static_cast<uint8_t>(item.bId >> 1),
	    catalogEntry.has_value() && catalogEntry->canBePlacedOnBelt,
	    catalogEntry.has_value() ? catalogEntry->equipType : std::string {},
	    catalogEntry.has_value() ? catalogEntry->minimumStrength : uint8_t { 0 },
	    catalogEntry.has_value() ? catalogEntry->minimumMagic : uint8_t { 0 },
	    catalogEntry.has_value() ? catalogEntry->minimumDexterity : uint8_t { 0 },
	};
	static_assert(sizeof(item) <= result.packedBytes.size());
	// Item files reserve 20 bytes for compatibility; the current packed save record occupies 19.
	std::memcpy(result.packedBytes.data(), &item, sizeof(item));
	PopulateGameItemDetails(result, item, game, isSpawn);
	return result;
}

void ApplyActiveGameIdentity(CharacterSummary::PackedItemSummary &summary, const std::byte *record)
{
	const uint32_t itemIndex = ReadLE32(record + 360);
	const auto entry = GetBaseItemMetadata(static_cast<uint16_t>(itemIndex));
	if (!entry.has_value()) return;
	const char *identifiedName = reinterpret_cast<const char *>(record + 125);
	const char *nameEnd = std::find(identifiedName, identifiedName + 64, '\0');
	const std::string finalName(identifiedName, nameEnd);
	summary.storedActiveGameName = finalName;
	summary.activeGameIdentityDiffers = (!finalName.empty() && finalName != summary.displayName) || itemIndex != summary.baseItemId;
	if (!finalName.empty()) summary.displayName = finalName;
	summary.baseItemId = static_cast<uint16_t>(itemIndex);
	summary.baseName = entry->name;
	const uint32_t cursor = ReadLE32(record + 192);
	if (cursor <= 255) summary.cursorGraphic = static_cast<uint8_t>(cursor);
	summary.canBePlacedOnBelt = entry->canBePlacedOnBelt;
	summary.equipType = entry->equipType;
	summary.minimumStrength = entry->minimumStrength;
	summary.minimumMagic = entry->minimumMagic;
	summary.minimumDexterity = entry->minimumDexterity;
}

void OverlayActiveGameItemIdentities(CharacterSummary &summary, const devilution::PlayerPack &player, std::span<const std::byte> gameData)
{
	if (gameData.empty()) return;
	const std::size_t itemSize = summary.game == Game::Hellfire ? 372 : 368;
	const std::size_t arraysSize = (devilution::NUM_INVLOC + devilution::InventoryGridCells) * itemSize;
	std::optional<std::size_t> gridOffset;
	for (std::size_t candidate = arraysSize + 4; candidate + devilution::InventoryGridCells + devilution::MaxBeltItems * itemSize <= gameData.size(); ++candidate) {
		if (ReadLE32(&gameData[candidate - 4]) != player._pNumInv
		    || std::memcmp(&gameData[candidate], player.InvGrid, devilution::InventoryGridCells) != 0)
			continue;
		const std::size_t bodyStart = candidate - 4 - arraysSize;
		auto seedMatches = [&](const devilution::ItemPack &packed, std::size_t record) {
			return packed.idx == 0xFFFF || ReadLE32(&gameData[record]) == devilution::Swap32LE(packed.iSeed);
		};
		bool matches = true;
		for (std::size_t i = 0; i < devilution::NUM_INVLOC && matches; ++i) matches = seedMatches(player.InvBody[i], bodyStart + i * itemSize);
		for (std::size_t i = 0; i < player._pNumInv && matches; ++i) matches = seedMatches(player.InvList[i], bodyStart + (devilution::NUM_INVLOC + i) * itemSize);
		for (std::size_t i = 0; i < devilution::MaxBeltItems && matches; ++i) matches = seedMatches(player.SpdList[i], candidate + devilution::InventoryGridCells + i * itemSize);
		if (!matches) continue;
		if (gridOffset.has_value()) return; // Ambiguous: keep the verified compact interpretation.
		gridOffset = candidate;
	}
	if (!gridOffset.has_value()) return;
	const std::size_t bodyStart = *gridOffset - 4 - arraysSize;
	for (auto &item : summary.equipment) ApplyActiveGameIdentity(item, &gameData[bodyStart + item.slot * itemSize]);
	for (auto &item : summary.inventory) ApplyActiveGameIdentity(item, &gameData[bodyStart + (devilution::NUM_INVLOC + item.slot) * itemSize]);
	const std::size_t beltStart = *gridOffset + devilution::InventoryGridCells;
	for (auto &item : summary.belt) ApplyActiveGameIdentity(item, &gameData[beltStart + item.slot * itemSize]);
}

void OverlayHeroItems(CharacterSummary &summary, std::span<const std::byte> heroItems)
{
	const std::size_t itemSize = summary.game == Game::Hellfire ? 372 : 368;
	const std::size_t itemCount = devilution::NUM_INVLOC + devilution::InventoryGridCells + devilution::MaxBeltItems;
	if (heroItems.size() != 1 + itemCount * itemSize) return;
	if (std::to_integer<uint8_t>(heroItems[0]) != (summary.game == Game::Hellfire ? 1 : 0)) return;
	auto attach = [&](auto &items, std::size_t firstRecord) {
		for (auto &item : items) {
			const std::size_t offset = 1 + (firstRecord + item.slot) * itemSize;
			item.fullItemRecord.assign(heroItems.begin() + offset, heroItems.begin() + offset + itemSize);
			ApplyActiveGameIdentity(item, item.fullItemRecord.data());
			// In single-player the matching full hero item is authoritative; retaining the
			// original record prevents compact reconstruction from normalizing direct fields.
			item.activeGameIdentityDiffers = false;
			(void)RefreshSummaryFromFullItemRecord(item, summary.game);
		}
	};
	attach(summary.equipment, 0);
	attach(summary.inventory, devilution::NUM_INVLOC);
	attach(summary.belt, devilution::NUM_INVLOC + devilution::InventoryGridCells);
}

} // namespace

std::filesystem::path DefaultSaveDirectory()
{
	if (const char *appData = std::getenv("APPDATA"); appData != nullptr)
		return std::filesystem::path(appData) / "diasurgical" / "devilution";
	return std::filesystem::current_path();
}

std::vector<std::filesystem::path> FindSaves(const std::filesystem::path &directory)
{
	std::vector<std::filesystem::path> result;
	std::error_code error;
	for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
		if (!it->is_regular_file(error))
			continue;
		const std::string filename = Lowercase(it->path().filename().string());
		if (filename.find(".backup-") != std::string::npos || filename.find(".d1hellforge-writing") != std::string::npos)
			continue;
		const std::string extension = Lowercase(it->path().extension().string());
		if (extension == ".sv" || extension == ".hsv")
			result.push_back(it->path());
	}
	std::sort(result.begin(), result.end());
	return result;
}

std::expected<CharacterSummary, std::string> ReadCharacterSummary(const std::filesystem::path &path)
{
	const auto decoded = ReadPlayerPack(path);
	if (!decoded.has_value())
		return std::unexpected(decoded.error());
	const devilution::PlayerPack &player = *decoded;

	CharacterSummary result;
	result.path = path;
	result.name.assign(player.pName, strnlen(player.pName, devilution::PlayerNameLength));
	result.characterClass = player.pClass;
	result.level = player.pLevel;
	result.strength = player.pBaseStr;
	result.magic = player.pBaseMag;
	result.dexterity = player.pBaseDex;
	result.vitality = player.pBaseVit;
	result.unspentStatPoints = player.pStatPts;
	result.experience = player.pExperience;
	result.gold = player.pGold;
	result.game = player.bIsHellfire != 0 ? Game::Hellfire : Game::Diablo;
	result.contentProfile = GetDevXContentProfile(result.game, IsSpawnSave(path));
	LocateGameData(result);
	const bool isSpawn = IsSpawnSave(path);
	for (uint8_t i = 0; i < devilution::NUM_INVLOC; ++i) {
		if (player.InvBody[i].idx != 0xFFFF)
			result.equipment.push_back(SummarizeItem(player.InvBody[i], i, result.game, isSpawn));
	}
	const uint8_t inventoryCount = std::min<uint8_t>(player._pNumInv, devilution::InventoryGridCells);
	for (uint8_t i = 0; i < inventoryCount; ++i) {
		if (player.InvList[i].idx != 0xFFFF)
			result.inventory.push_back(SummarizeItem(player.InvList[i], i, result.game, isSpawn));
	}
	for (uint8_t i = 0; i < devilution::MaxBeltItems; ++i) {
		if (player.SpdList[i].idx != 0xFFFF)
			result.belt.push_back(SummarizeItem(player.SpdList[i], i, result.game, isSpawn));
	}
	std::copy(std::begin(player.InvGrid), std::end(player.InvGrid), result.inventoryGrid.begin());
	const auto activeGame = ReadDecodedEntry(path, "game");
	if (activeGame.has_value()) OverlayActiveGameItemIdentities(result, player, *activeGame);
	const auto heroItems = ReadDecodedEntry(path, "heroitems");
	if (heroItems.has_value()) OverlayHeroItems(result, *heroItems);
	return result;
}

std::expected<std::optional<EditableStats>, std::string> ReadActiveGameStats(const std::filesystem::path &path)
{
	const auto player = ReadPlayerPack(path);
	if (!player.has_value())
		return std::unexpected(player.error());
	const auto decoded = ReadDecodedEntry(path, "game");
	if (!decoded.has_value())
		return std::unexpected(decoded.error());
	if (decoded->empty())
		return std::optional<EditableStats> {};
	const auto locatedOffset = FindGameStatsOffset(*decoded, *player);
	if (!locatedOffset.has_value())
		return std::unexpected(locatedOffset.error());
	const std::size_t strengthOffset = *locatedOffset;
	const std::size_t pointsOffset = strengthOffset + 8 * sizeof(uint32_t);
	EditableStats result;
	result.strength = static_cast<uint8_t>(ReadLE32(&(*decoded)[strengthOffset + 4]));
	result.magic = static_cast<uint8_t>(ReadLE32(&(*decoded)[strengthOffset + 12]));
	result.dexterity = static_cast<uint8_t>(ReadLE32(&(*decoded)[strengthOffset + 20]));
	result.vitality = static_cast<uint8_t>(ReadLE32(&(*decoded)[strengthOffset + 28]));
	result.unspentStatPoints = static_cast<uint8_t>(ReadLE32(&(*decoded)[pointsOffset]));
	return std::optional<EditableStats> { result };
}

std::expected<SaveResult, std::string> SaveCharacterStats(const std::filesystem::path &path, const EditableStats &stats)
{
	const auto original = ReadPlayerPack(path);
	if (!original.has_value())
		return std::unexpected(original.error());

	devilution::PlayerPack edited = *original;
	edited.pBaseStr = stats.strength;
	edited.pBaseMag = stats.magic;
	edited.pBaseDex = stats.dexterity;
	edited.pBaseVit = stats.vitality;
	edited.pStatPts = stats.unspentStatPoints;
	const auto decodedGame = ReadDecodedEntry(path, "game");
	if (!decodedGame.has_value())
		return std::unexpected(decodedGame.error());
	std::vector<std::byte> editedGame = *decodedGame;
	if (auto updateResult = UpdateGameStats(editedGame, *original, stats); !updateResult.has_value())
		return std::unexpected(updateResult.error());

	const std::filesystem::path backupPath = BackupPathFor(path);
	const std::filesystem::path temporaryPath = path.parent_path() / (path.stem().string() + ".d1hellforge-writing" + path.extension().string());
	std::error_code error;
	std::filesystem::create_directories(backupPath.parent_path(), error);
	if (error)
		return std::unexpected("Unable to create the backup directory: " + error.message());
	std::filesystem::copy_file(path, backupPath, std::filesystem::copy_options::none, error);
	if (error)
		return std::unexpected("Unable to create backup: " + error.message());
	std::filesystem::copy_file(path, temporaryPath, std::filesystem::copy_options::overwrite_existing, error);
	if (error)
		return std::unexpected("Unable to create temporary save: " + error.message());

	const std::size_t encodedSize = devilution::codec_get_encoded_len(sizeof(edited));
	std::vector<std::byte> encoded(encodedSize);
	std::memcpy(encoded.data(), &edited, sizeof(edited));
	devilution::codec_encode(encoded.data(), sizeof(edited), encoded.size(), PasswordFor(path));
	std::vector<std::byte> encodedGame;
	if (!editedGame.empty()) {
		encodedGame.resize(devilution::codec_get_encoded_len(editedGame.size()));
		std::memcpy(encodedGame.data(), editedGame.data(), editedGame.size());
		devilution::codec_encode(encodedGame.data(), editedGame.size(), encodedGame.size(), PasswordFor(path));
	}
	{
		devilution::MpqWriter writer(temporaryPath.string(), true);
		if (!writer.WriteFile("hero", encoded.data(), encoded.size())) {
			std::filesystem::remove(temporaryPath, error);
			return std::unexpected("Unable to write the temporary hero record");
		}
		if (!encodedGame.empty() && !writer.WriteFile("game", encodedGame.data(), encodedGame.size())) {
			std::filesystem::remove(temporaryPath, error);
			return std::unexpected("Unable to write the temporary saved-game record");
		}
	}

	const auto verified = ReadPlayerPack(temporaryPath);
	if (!verified.has_value() || std::memcmp(&*verified, &edited, sizeof(edited)) != 0) {
		std::filesystem::remove(temporaryPath, error);
		return std::unexpected("Temporary save verification failed; the original was not changed");
	}
	const auto verifiedGame = ReadDecodedEntry(temporaryPath, "game");
	if (!verifiedGame.has_value() || *verifiedGame != editedGame) {
		std::filesystem::remove(temporaryPath, error);
		return std::unexpected("Temporary saved-game verification failed; the original was not changed");
	}

#ifdef _WIN32
	if (!MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		std::filesystem::remove(temporaryPath, error);
		return std::unexpected("Unable to replace the original save after verification");
	}
#else
	std::filesystem::rename(temporaryPath, path, error);
	if (error)
		return std::unexpected("Unable to replace the original save after verification: " + error.message());
#endif

	const auto finalCheck = ReadPlayerPack(path);
	if (!finalCheck.has_value() || std::memcmp(&*finalCheck, &edited, sizeof(edited)) != 0)
		return std::unexpected("Final verification failed; restore the reported backup before playing");
	const auto finalGameCheck = ReadDecodedEntry(path, "game");
	if (!finalGameCheck.has_value() || *finalGameCheck != editedGame)
		return std::unexpected("Final saved-game verification failed; restore the reported backup before playing");
	return SaveResult { backupPath };
}

std::expected<SaveResult, std::string> SaveCharacterInventory(const std::filesystem::path &path, CharacterSummary inventory)
{
	const auto original = ReadPlayerPack(path);
	if (!original.has_value())
		return std::unexpected(original.error());
	if (inventory.inventory.size() > devilution::InventoryGridCells)
		return std::unexpected("The backpack contains too many item records");
	auto hasSplitIdentity = [](const auto &items) { return std::any_of(items.begin(), items.end(), [](const auto &item) { return item.activeGameIdentityDiffers; }); };
	if (hasSplitIdentity(inventory.equipment) || hasSplitIdentity(inventory.inventory) || hasSplitIdentity(inventory.belt))
		return std::unexpected("This save contains an item whose compact hero record and active-game identity disagree. D1Hellforge will not rewrite it until the Item Workshop repairs it to one canonical game-native result.");
	auto hasDuplicateSlots = [](const auto &items) {
		std::array<bool, devilution::InventoryGridCells> used {};
		for (const auto &item : items) {
			if (item.slot >= used.size() || used[item.slot]) return true;
			used[item.slot] = true;
		}
		return false;
	};
	if (hasDuplicateSlots(inventory.equipment))
		return std::unexpected("The equipment preview contains duplicate slot records; refresh and retry the move");
	if (hasDuplicateSlots(inventory.inventory))
		return std::unexpected("The backpack preview contains duplicate item records; refresh and retry the move");
	if (hasDuplicateSlots(inventory.belt))
		return std::unexpected("The belt preview contains duplicate slot records; refresh and retry the move");

	std::sort(inventory.inventory.begin(), inventory.inventory.end(), [](const auto &a, const auto &b) { return a.slot < b.slot; });
	std::array<int8_t, devilution::InventoryGridCells + 1> remap {};
	for (std::size_t index = 0; index < inventory.inventory.size(); ++index) {
		remap[inventory.inventory[index].slot + 1] = static_cast<int8_t>(index + 1);
		inventory.inventory[index].slot = static_cast<uint8_t>(index);
	}
	for (int8_t &cell : inventory.inventoryGrid) {
		if (cell == 0)
			continue;
		const int sign = cell < 0 ? -1 : 1;
		const int oldIndex = std::abs(static_cast<int>(cell));
		if (oldIndex >= static_cast<int>(remap.size()) || remap[oldIndex] == 0)
			return std::unexpected("The backpack grid refers to a missing item");
		cell = static_cast<int8_t>(sign * remap[oldIndex]);
	}

	devilution::PlayerPack edited = *original;
	for (auto &item : edited.InvBody) {
		item = {};
		item.idx = 0xFFFF;
	}
	for (auto &item : edited.InvList) {
		item = {};
		item.idx = 0xFFFF;
	}
	for (auto &item : edited.SpdList) {
		item = {};
		item.idx = 0xFFFF;
	}
	auto copyPacked = [](devilution::ItemPack &destination, const CharacterSummary::PackedItemSummary &source) {
		std::memcpy(&destination, source.packedBytes.data(), sizeof(destination));
	};
	for (const auto &item : inventory.equipment) {
		if (item.slot >= devilution::NUM_INVLOC)
			return std::unexpected("An equipment item has an invalid slot");
		copyPacked(edited.InvBody[item.slot], item);
	}
	for (const auto &item : inventory.inventory)
		copyPacked(edited.InvList[item.slot], item);
	for (const auto &item : inventory.belt) {
		if (item.slot >= devilution::MaxBeltItems)
			return std::unexpected("A belt item has an invalid slot");
		copyPacked(edited.SpdList[item.slot], item);
	}
	edited._pNumInv = static_cast<uint8_t>(inventory.inventory.size());
	std::copy(inventory.inventoryGrid.begin(), inventory.inventoryGrid.end(), std::begin(edited.InvGrid));

	const auto decodedGame = ReadDecodedEntry(path, "game");
	if (!decodedGame.has_value())
		return std::unexpected(decodedGame.error());
	std::vector<std::byte> editedGame = *decodedGame;
	const auto decodedHeroItems = ReadDecodedEntry(path, "heroitems");
	if (!decodedHeroItems.has_value()) return std::unexpected(decodedHeroItems.error());
	std::vector<std::byte> editedHeroItems = *decodedHeroItems;
	if (!editedHeroItems.empty()) {
		const std::size_t itemSize = inventory.game == Game::Hellfire ? 372 : 368;
		const std::size_t itemCount = devilution::NUM_INVLOC + devilution::InventoryGridCells + devilution::MaxBeltItems;
		if (editedHeroItems.size() != 1 + itemCount * itemSize)
			return std::unexpected("The heroitems record has an unsupported size");
		auto itemAt = [](const auto &items, uint8_t slot) -> const CharacterSummary::PackedItemSummary * {
			const auto found = std::find_if(items.begin(), items.end(), [slot](const auto &item) { return item.slot == slot; });
			return found == items.end() ? nullptr : &*found;
		};
		auto writeRange = [&](const auto &items, std::size_t firstRecord, std::size_t count) -> std::expected<void, std::string> {
			for (std::size_t slot = 0; slot < count; ++slot) {
				const auto record = ExpandPackedItem(itemAt(items, static_cast<uint8_t>(slot)), inventory.game);
				if (!record.has_value()) return std::unexpected(record.error());
				std::copy(record->begin(), record->end(), editedHeroItems.begin() + 1 + (firstRecord + slot) * itemSize);
			}
			return {};
		};
		if (auto written = writeRange(inventory.equipment, 0, devilution::NUM_INVLOC); !written.has_value()) return std::unexpected(written.error());
		if (auto written = writeRange(inventory.inventory, devilution::NUM_INVLOC, devilution::InventoryGridCells); !written.has_value()) return std::unexpected(written.error());
		if (auto written = writeRange(inventory.belt, devilution::NUM_INVLOC + devilution::InventoryGridCells, devilution::MaxBeltItems); !written.has_value()) return std::unexpected(written.error());
	}
	if (!editedGame.empty()) {
		const std::size_t itemSize = inventory.game == Game::Hellfire ? 372 : 368;
		const std::size_t arraysSize = (devilution::NUM_INVLOC + devilution::InventoryGridCells) * itemSize;
		std::optional<std::size_t> gridOffset;
		for (std::size_t candidate = arraysSize + 4; candidate + devilution::InventoryGridCells + devilution::MaxBeltItems * itemSize <= editedGame.size(); ++candidate) {
			if (ReadLE32(&editedGame[candidate - 4]) != original->_pNumInv
			    || std::memcmp(&editedGame[candidate], original->InvGrid, devilution::InventoryGridCells) != 0)
				continue;
			const std::size_t bodyStart = candidate - 4 - arraysSize;
			bool matches = true;
			auto seedMatches = [&](const devilution::ItemPack &packed, std::size_t record) {
				return packed.idx == 0xFFFF || ReadLE32(&editedGame[record]) == devilution::Swap32LE(packed.iSeed);
			};
			for (std::size_t i = 0; i < devilution::NUM_INVLOC && matches; ++i)
				matches = seedMatches(original->InvBody[i], bodyStart + i * itemSize);
			for (std::size_t i = 0; i < original->_pNumInv && matches; ++i)
				matches = seedMatches(original->InvList[i], bodyStart + (devilution::NUM_INVLOC + i) * itemSize);
			for (std::size_t i = 0; i < devilution::MaxBeltItems && matches; ++i)
				matches = seedMatches(original->SpdList[i], candidate + devilution::InventoryGridCells + i * itemSize);
			if (!matches)
				continue;
			if (gridOffset.has_value())
				return std::unexpected("The active game contains more than one matching inventory block");
			gridOffset = candidate;
		}
		if (!gridOffset.has_value())
			return std::unexpected("Unable to locate the verified inventory block in the active game");
		const std::size_t bodyStart = *gridOffset - 4 - arraysSize;
		auto itemAt = [](const auto &items, uint8_t slot) -> const CharacterSummary::PackedItemSummary * {
			const auto found = std::find_if(items.begin(), items.end(), [slot](const auto &item) { return item.slot == slot; });
			return found == items.end() ? nullptr : &*found;
		};
		for (uint8_t slot = 0; slot < devilution::NUM_INVLOC; ++slot) {
			const auto record = ExpandPackedItem(itemAt(inventory.equipment, slot), inventory.game);
			if (!record.has_value()) return std::unexpected(record.error());
			std::copy(record->begin(), record->end(), editedGame.begin() + bodyStart + slot * itemSize);
		}
		for (uint8_t slot = 0; slot < devilution::InventoryGridCells; ++slot) {
			const auto record = ExpandPackedItem(itemAt(inventory.inventory, slot), inventory.game);
			if (!record.has_value()) return std::unexpected(record.error());
			std::copy(record->begin(), record->end(), editedGame.begin() + bodyStart + (devilution::NUM_INVLOC + slot) * itemSize);
		}
		WriteLE32(&editedGame[*gridOffset - 4], edited._pNumInv);
		for (std::size_t i = 0; i < inventory.inventoryGrid.size(); ++i)
			editedGame[*gridOffset + i] = static_cast<std::byte>(inventory.inventoryGrid[i]);
		const std::size_t beltStart = *gridOffset + devilution::InventoryGridCells;
		for (uint8_t slot = 0; slot < devilution::MaxBeltItems; ++slot) {
			const auto record = ExpandPackedItem(itemAt(inventory.belt, slot), inventory.game);
			if (!record.has_value()) return std::unexpected(record.error());
			std::copy(record->begin(), record->end(), editedGame.begin() + beltStart + slot * itemSize);
		}
	}

	const std::filesystem::path backupPath = BackupPathFor(path);
	const std::filesystem::path temporaryPath = path.parent_path() / (path.stem().string() + ".d1hellforge-writing" + path.extension().string());
	std::error_code error;
	std::filesystem::create_directories(backupPath.parent_path(), error);
	if (error) return std::unexpected("Unable to create the backup directory: " + error.message());
	std::filesystem::copy_file(path, backupPath, std::filesystem::copy_options::none, error);
	if (error) return std::unexpected("Unable to create backup: " + error.message());
	std::filesystem::copy_file(path, temporaryPath, std::filesystem::copy_options::overwrite_existing, error);
	if (error) return std::unexpected("Unable to create temporary save: " + error.message());
	const std::size_t encodedSize = devilution::codec_get_encoded_len(sizeof(edited));
	std::vector<std::byte> encoded(encodedSize);
	std::memcpy(encoded.data(), &edited, sizeof(edited));
	devilution::codec_encode(encoded.data(), sizeof(edited), encoded.size(), PasswordFor(path));
	std::vector<std::byte> encodedGame;
	std::vector<std::byte> encodedHeroItems;
	if (!editedGame.empty()) {
		encodedGame.resize(devilution::codec_get_encoded_len(editedGame.size()));
		std::memcpy(encodedGame.data(), editedGame.data(), editedGame.size());
		devilution::codec_encode(encodedGame.data(), editedGame.size(), encodedGame.size(), PasswordFor(path));
	}
	if (!editedHeroItems.empty()) {
		encodedHeroItems.resize(devilution::codec_get_encoded_len(editedHeroItems.size()));
		std::memcpy(encodedHeroItems.data(), editedHeroItems.data(), editedHeroItems.size());
		devilution::codec_encode(encodedHeroItems.data(), editedHeroItems.size(), encodedHeroItems.size(), PasswordFor(path));
	}
	{
		devilution::MpqWriter writer(temporaryPath.string(), true);
		if (!writer.WriteFile("hero", encoded.data(), encoded.size())
		    || (!encodedGame.empty() && !writer.WriteFile("game", encodedGame.data(), encodedGame.size()))
		    || (!encodedHeroItems.empty() && !writer.WriteFile("heroitems", encodedHeroItems.data(), encodedHeroItems.size()))) {
			std::filesystem::remove(temporaryPath, error);
			return std::unexpected("Unable to write the temporary inventory save");
		}
	}
	const auto verified = ReadPlayerPack(temporaryPath);
	const auto verifiedGame = ReadDecodedEntry(temporaryPath, "game");
	const auto verifiedHeroItems = ReadDecodedEntry(temporaryPath, "heroitems");
	if (!verified.has_value() || std::memcmp(&*verified, &edited, sizeof(edited)) != 0
	    || !verifiedGame.has_value() || *verifiedGame != editedGame
	    || !verifiedHeroItems.has_value() || *verifiedHeroItems != editedHeroItems) {
		std::filesystem::remove(temporaryPath, error);
		return std::unexpected("Temporary inventory verification failed; the original was not changed");
	}
#ifdef _WIN32
	if (!MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		std::filesystem::remove(temporaryPath, error);
		return std::unexpected("Unable to replace the original save after inventory verification");
	}
#else
	std::filesystem::rename(temporaryPath, path, error);
	if (error) return std::unexpected("Unable to replace the original save: " + error.message());
#endif
	const auto finalCheck = ReadPlayerPack(path);
	const auto finalHeroItems = ReadDecodedEntry(path, "heroitems");
	if (!finalCheck.has_value() || std::memcmp(&*finalCheck, &edited, sizeof(edited)) != 0
	    || !finalHeroItems.has_value() || *finalHeroItems != editedHeroItems)
		return std::unexpected("Final inventory verification failed; restore the reported backup before playing");
	return SaveResult { backupPath };
}

std::expected<CharacterSummary::PackedItemSummary, std::string> SummarizeImportedItem(const std::array<std::byte, 20> &packedBytes, Game game)
{
	static_assert(sizeof(devilution::ItemPack) <= packedBytes.size());
	devilution::ItemPack item;
	std::memcpy(&item, packedBytes.data(), sizeof(item));
	if (item.idx == 0xFFFF)
		return std::unexpected("The imported item record is empty");
	auto summary = SummarizeItem(item, 0, game, false);
	if (summary.baseName.empty())
		return std::unexpected("The imported item has an unknown base item identifier");
	return summary;
}

std::expected<std::array<std::byte, 20>, std::string> ConvertPackedItem(const std::array<std::byte, 20> &packedBytes, Game source, Game destination)
{
	if (source == destination)
		return packedBytes;
	devilution::ItemPack packed {};
	std::memcpy(&packed, packedBytes.data(), sizeof(packed));
	auto index = static_cast<devilution::_item_indexes>(devilution::Swap16LE(packed.idx));
	if (source == Game::Diablo && destination == Game::Hellfire)
		index = devilution::RemapItemIdxFromDiablo(index);
	else if (source == Game::Hellfire && destination == Game::Diablo)
		index = devilution::RemapItemIdxToDiablo(index);
	packed.idx = devilution::Swap16LE(static_cast<uint16_t>(index));
	std::array<std::byte, 20> converted {};
	std::memcpy(converted.data(), &packed, sizeof(packed));
	if (!SummarizeImportedItem(converted, destination).has_value())
		return std::unexpected("The source item has no verified equivalent in the destination game");
	return converted;
}

std::expected<void, std::string> ExportLegacyItem(const std::filesystem::path &path, const CharacterSummary::PackedItemSummary &item, Game game)
{
	const auto record = ExpandPackedItem(&item, game);
	if (!record.has_value())
		return std::unexpected(record.error());
	std::array<std::byte, 400> output {};
	const std::string_view magic = game == Game::Hellfire ? "HELLFIRE ITEM FILE" : "ITM01.I'll get that al'Thor!";
	const std::size_t base = game == Game::Hellfire ? 28 : 32;
	std::memcpy(output.data(), magic.data(), magic.size());
	if (base + record->size() != output.size())
		return std::unexpected("The regenerated legacy item record has an unexpected size");
	std::memcpy(output.data() + base, record->data(), record->size());
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream)
		return std::unexpected("Unable to create the legacy item file");
	stream.write(reinterpret_cast<const char *>(output.data()), output.size());
	if (!stream)
		return std::unexpected("Unable to finish writing the legacy item file");
	return {};
}

const char *ClassName(uint8_t characterClass, Game game)
{
	constexpr std::array<const char *, 3> DiabloClasses { "Warrior", "Rogue", "Sorcerer" };
	constexpr std::array<const char *, 6> HellfireClasses { "Warrior", "Rogue", "Sorcerer", "Monk", "Bard", "Barbarian" };
	if (game == Game::Hellfire && characterClass < HellfireClasses.size())
		return HellfireClasses[characterClass];
	if (characterClass < DiabloClasses.size())
		return DiabloClasses[characterClass];
	return "Unknown";
}

} // namespace d1hellforge
