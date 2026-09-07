#include "inventory_renderer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <format>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "cursor_defs.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/load_clx.hpp"
#include "engine/surface.hpp"
#include "mpq/mpq_reader.hpp"
#include "utils/cel_to_clx.hpp"

namespace d1hellforge {
namespace {

std::optional<std::filesystem::path> FindFileIgnoringCase(const std::filesystem::path &directory, std::string_view filename)
{
	std::error_code error;
	for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
		if (it->is_regular_file(error) && _stricmp(it->path().filename().string().c_str(), std::string(filename).c_str()) == 0)
			return it->path();
	}
	return std::nullopt;
}

std::expected<std::vector<std::byte>, std::string> ReadMpqFile(devilution::MpqArchive &archive, std::string_view name)
{
	std::size_t size = 0;
	int32_t error = 0;
	auto data = archive.ReadFile(name, size, error);
	if (error != 0 || data == nullptr)
		return std::unexpected("Unable to read MPQ asset " + std::string(name));
	return std::vector<std::byte>(data.get(), data.get() + size);
}

std::expected<std::vector<uint16_t>, std::string> ReadCursorWidths(std::string_view filename = "objcurs-widths.txt")
{
	std::wstring executable(MAX_PATH, L'\0');
	const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
	if (length == 0)
		return std::unexpected("Unable to locate the editor assets directory");
	executable.resize(length);
	const auto directory = std::filesystem::path(executable).parent_path();
	const std::filesystem::path candidates[] {
	    directory / "assets/data/inv" / filename,
	    directory / "../../assets/data/inv" / filename,
	    directory / "../../mods/hf/data/inv" / filename,
	    std::filesystem::current_path() / "assets/data/inv" / filename,
	    std::filesystem::current_path() / "mods/hf/data/inv" / filename,
	};
	for (const auto &candidate : candidates) {
		std::ifstream input(candidate);
		if (!input)
			continue;
		std::vector<uint16_t> widths;
		unsigned width;
		while (input >> width)
			widths.push_back(static_cast<uint16_t>(width));
		if (!widths.empty())
			return widths;
	}
	return std::unexpected("Unable to read item-sprite width metadata");
}

std::optional<std::filesystem::path> FindHellfireMpq(const CharacterSummary &character)
{
	if (character.game != Game::Hellfire) return std::nullopt;
	auto path = FindFileIgnoringCase(character.gameDataDirectory, "hellfire.mpq");
	if (!path.has_value()) path = FindFileIgnoringCase(character.gameDataDirectory / "hellfire", "hellfire.mpq");
	return path;
}

void DrawBox(const devilution::Surface &surface, int x, int y, int width, int height)
{
	for (int row = 0; row < height; ++row) {
		for (int column = 0; column < width; ++column) {
			const bool border = row == 0 || column == 0 || row == height - 1 || column == width - 1;
			surface.SetPixel({ x + column, y + row }, border ? 194 : 0);
		}
	}
}

} // namespace

std::expected<std::pair<int, int>, std::string> GetItemFootprint(const CharacterSummary &character, uint8_t cursorGraphic)
{
	const auto diabdatPath = FindFileIgnoringCase(character.gameDataDirectory, "diabdat.mpq");
	if (!diabdatPath.has_value())
		return std::unexpected("The validated Diablo MPQ is no longer available");
	auto archiveResult = devilution::MpqArchive::Open(diabdatPath->string().c_str());
	if (!archiveResult.has_value())
		return std::unexpected(archiveResult.error());
	auto cursorData = ReadMpqFile(*archiveResult, "data\\inv\\objcurs.cel");
	auto widths = ReadCursorWidths();
	if (!cursorData.has_value() || !widths.has_value())
		return std::unexpected("Unable to load item cursor dimensions");
	auto cursorClx = devilution::CelToClx(reinterpret_cast<const uint8_t *>(cursorData->data()), cursorData->size(), devilution::PointerOrValue<uint16_t> { widths->data() });
	const auto sprites = cursorClx.list();
	const int cursorId = cursorGraphic + devilution::CURSOR_FIRSTITEM;
	if (cursorId <= 0) return std::unexpected("Imported item cursor graphic is invalid");
	std::optional<devilution::ClxSprite> selected;
	std::vector<std::byte> secondaryData;
	std::vector<uint16_t> secondaryWidths;
	std::optional<devilution::OwnedClxSpriteListOrSheet> secondaryClx;
	if (static_cast<uint32_t>(cursorId) <= sprites.numSprites()) {
		selected = sprites[cursorId - 1];
	} else if (const auto hellfirePath = FindHellfireMpq(character); hellfirePath.has_value()) {
		auto hellfireArchive = devilution::MpqArchive::Open(hellfirePath->string().c_str());
		if (hellfireArchive.has_value()) {
			auto data = ReadMpqFile(*hellfireArchive, "data\\inv\\objcurs2.cel");
			auto widths2 = ReadCursorWidths("objcurs2-widths.txt");
			if (data.has_value() && widths2.has_value()) {
				secondaryData = std::move(*data);
				secondaryWidths = std::move(*widths2);
				secondaryClx.emplace(devilution::CelToClx(reinterpret_cast<const uint8_t *>(secondaryData.data()), secondaryData.size(), devilution::PointerOrValue<uint16_t> { secondaryWidths.data() }));
				const auto extra = secondaryClx->list();
				const uint32_t index = cursorId - sprites.numSprites() - 1;
				if (index < extra.numSprites()) selected = extra[index];
			}
		}
	}
	if (!selected.has_value()) return std::unexpected("Imported item cursor graphic is invalid for the selected game data");
	const auto sprite = *selected;
	return std::pair { std::max(1, (sprite.width() + 27) / 28), std::max(1, (sprite.height() + 27) / 28) };
}

std::expected<RenderedItemPreview, std::string> RenderItemPreview(const CharacterSummary &character, uint8_t cursorGraphic)
{
	const auto diabdatPath = FindFileIgnoringCase(character.gameDataDirectory, "diabdat.mpq");
	if (!diabdatPath.has_value())
		return std::unexpected("MPQ item graphics are unavailable");
	auto archiveResult = devilution::MpqArchive::Open(diabdatPath->string().c_str());
	if (!archiveResult.has_value())
		return std::unexpected(archiveResult.error());
	auto cursorData = ReadMpqFile(*archiveResult, "data\\inv\\objcurs.cel");
	auto paletteData = ReadMpqFile(*archiveResult, "levels\\towndata\\town.pal");
	auto widths = ReadCursorWidths();
	if (!cursorData.has_value() || !paletteData.has_value() || paletteData->size() < 256 * 3 || !widths.has_value())
		return std::unexpected("Unable to load the MPQ item preview assets");
	auto cursorClx = devilution::CelToClx(reinterpret_cast<const uint8_t *>(cursorData->data()), cursorData->size(), devilution::PointerOrValue<uint16_t> { widths->data() });
	const auto sprites = cursorClx.list();
	const int cursorId = cursorGraphic + devilution::CURSOR_FIRSTITEM;
	if (cursorId <= 0) return std::unexpected("Item sprite is unavailable");
	std::optional<devilution::ClxSprite> selected;
	std::vector<std::byte> secondaryData;
	std::vector<uint16_t> secondaryWidths;
	std::optional<devilution::OwnedClxSpriteListOrSheet> secondaryClx;
	if (static_cast<uint32_t>(cursorId) <= sprites.numSprites()) selected = sprites[cursorId - 1];
	else if (const auto hellfirePath = FindHellfireMpq(character); hellfirePath.has_value()) {
		auto hellfireArchive = devilution::MpqArchive::Open(hellfirePath->string().c_str());
		if (hellfireArchive.has_value()) {
			auto data = ReadMpqFile(*hellfireArchive, "data\\inv\\objcurs2.cel");
			auto widths2 = ReadCursorWidths("objcurs2-widths.txt");
			if (data.has_value() && widths2.has_value()) {
				secondaryData = std::move(*data);
				secondaryWidths = std::move(*widths2);
				secondaryClx.emplace(devilution::CelToClx(reinterpret_cast<const uint8_t *>(secondaryData.data()), secondaryData.size(), devilution::PointerOrValue<uint16_t> { secondaryWidths.data() }));
				const auto extra = secondaryClx->list();
				const uint32_t index = cursorId - sprites.numSprites() - 1;
				if (index < extra.numSprites()) selected = extra[index];
			}
		}
	}
	if (!selected.has_value()) return std::unexpected("Item sprite is unavailable for the selected game data");
	constexpr int Size = 112;
	devilution::OwnedSurface surface(Size, Size);
	std::memset(surface.begin(), 0, surface.pitch() * surface.h());
	const auto sprite = *selected;
	const int x = (Size - sprite.width()) / 2;
	const int bottom = (Size + sprite.height()) / 2 - 1;
	devilution::ClxDraw(surface, { x, bottom }, sprite);
	RenderedItemPreview result { Size, Size, std::vector<uint32_t>(Size * Size) };
	for (int y = 0; y < Size; ++y) {
		for (int xPixel = 0; xPixel < Size; ++xPixel) {
			const uint8_t index = surface[{ xPixel, y }];
			const auto *rgb = reinterpret_cast<const uint8_t *>(paletteData->data()) + index * 3;
			result.pixels[static_cast<std::size_t>(y) * Size + xPixel] = (static_cast<uint32_t>(rgb[0]) << 16) | (static_cast<uint32_t>(rgb[1]) << 8) | rgb[2];
		}
	}
	return result;
}

std::expected<RenderedItemPreview, std::string> RenderStashBackground(const CharacterSummary &character)
{
	const auto diabdatPath = FindFileIgnoringCase(character.gameDataDirectory, "diabdat.mpq");
	if (!diabdatPath.has_value()) return std::unexpected("MPQ palette data is unavailable");
	auto archive = devilution::MpqArchive::Open(diabdatPath->string().c_str());
	if (!archive.has_value()) return std::unexpected(archive.error());
	auto paletteData = ReadMpqFile(*archive, "levels\\towndata\\town.pal");
	if (!paletteData.has_value() || paletteData->size() < 256 * 3)
		return std::unexpected("Unable to read the Diablo town palette");
	auto background = devilution::LoadClxWithStatus("data\\stash.clx");
	if (!background.has_value() || background->numSprites() == 0)
		return std::unexpected("Unable to load DevilutionX stash panel artwork");
	const auto sprite = (*background)[0];
	devilution::OwnedSurface surface(sprite.width(), sprite.height());
	std::memset(surface.begin(), 0, surface.pitch() * surface.h());
	devilution::RenderClxSprite(surface, sprite, { 0, 0 });
	RenderedItemPreview result { sprite.width(), sprite.height(), std::vector<uint32_t>(static_cast<std::size_t>(sprite.width()) * sprite.height()) };
	for (int y = 0; y < result.height; ++y) {
		for (int x = 0; x < result.width; ++x) {
			const uint8_t index = surface[{ x, y }];
			const auto *rgb = reinterpret_cast<const uint8_t *>(paletteData->data()) + index * 3;
			result.pixels[static_cast<std::size_t>(y) * result.width + x] = (static_cast<uint32_t>(rgb[0]) << 16) | (static_cast<uint32_t>(rgb[1]) << 8) | rgb[2];
		}
	}
	return result;
}

std::expected<RenderedInventory, std::string> RenderInventory(const CharacterSummary &character)
{
	if (!character.gameGraphicsAvailable || character.gameDataDirectory.empty())
		return std::unexpected(character.gameGraphicsStatus);
	const auto diabdatPath = FindFileIgnoringCase(character.gameDataDirectory, "diabdat.mpq");
	if (!diabdatPath.has_value())
		return std::unexpected("The validated Diablo MPQ is no longer available");
	auto archiveResult = devilution::MpqArchive::Open(diabdatPath->string().c_str());
	if (!archiveResult.has_value())
		return std::unexpected(archiveResult.error());
	auto &archive = *archiveResult;

	const char *backgroundName = "data\\inv\\inv.cel";
	if (character.characterClass == 1 || character.characterClass == 4)
		backgroundName = "data\\inv\\inv_rog.cel";
	else if (character.characterClass == 2 || character.characterClass == 3)
		backgroundName = "data\\inv\\inv_sor.cel";

	auto backgroundData = ReadMpqFile(archive, backgroundName);
	if (!backgroundData.has_value())
		return std::unexpected(backgroundData.error());
	auto cursorData = ReadMpqFile(archive, "data\\inv\\objcurs.cel");
	if (!cursorData.has_value())
		return std::unexpected(cursorData.error());
	auto paletteData = ReadMpqFile(archive, "levels\\towndata\\town.pal");
	if (!paletteData.has_value() || paletteData->size() < 256 * 3)
		return std::unexpected("Unable to read the Diablo town palette");
	auto widths = ReadCursorWidths();
	if (!widths.has_value())
		return std::unexpected(widths.error());

	auto backgroundClx = devilution::CelToClx(reinterpret_cast<const uint8_t *>(backgroundData->data()), backgroundData->size(), devilution::PointerOrValue<uint16_t> { 320 });
	auto cursorClx = devilution::CelToClx(reinterpret_cast<const uint8_t *>(cursorData->data()), cursorData->size(), devilution::PointerOrValue<uint16_t> { widths->data() });
	const auto backgroundSprites = backgroundClx.list();
	const auto itemSprites = cursorClx.list();
	std::vector<std::byte> secondaryData;
	std::vector<uint16_t> secondaryWidths;
	std::optional<devilution::OwnedClxSpriteListOrSheet> secondaryClx;
	if (const auto hellfirePath = FindHellfireMpq(character); hellfirePath.has_value()) {
		auto hellfireArchive = devilution::MpqArchive::Open(hellfirePath->string().c_str());
		if (hellfireArchive.has_value()) {
			auto data = ReadMpqFile(*hellfireArchive, "data\\inv\\objcurs2.cel");
			auto widths2 = ReadCursorWidths("objcurs2-widths.txt");
			if (data.has_value() && widths2.has_value()) {
				secondaryData = std::move(*data);
				secondaryWidths = std::move(*widths2);
				secondaryClx.emplace(devilution::CelToClx(reinterpret_cast<const uint8_t *>(secondaryData.data()), secondaryData.size(), devilution::PointerOrValue<uint16_t> { secondaryWidths.data() }));
			}
		}
	}
	if (backgroundSprites.numSprites() == 0)
		return std::unexpected("The inventory background contains no frames");

	constexpr int Width = 320;
	constexpr int InventoryHeight = 352;
	constexpr int BeltHeight = 42;
	devilution::OwnedSurface surface(Width, InventoryHeight + BeltHeight);
	std::memset(surface.begin(), 0, surface.pitch() * surface.h());
	devilution::RenderClxSprite(surface, backgroundSprites[0], { 0, 0 });
	for (int slot = 0; slot < 8; ++slot)
		DrawBox(surface, 15 + slot * 37, InventoryHeight + 5, 31, 31);

	std::vector<InventoryHitRegion> hitRegions;
	auto drawItem = [&](const CharacterSummary::PackedItemSummary &item, int x, int bottomY, std::string_view location) {
		const int cursorId = item.cursorGraphic + devilution::CURSOR_FIRSTITEM;
		if (cursorId <= 0) return;
		std::optional<devilution::ClxSprite> selected;
		if (static_cast<uint32_t>(cursorId) <= itemSprites.numSprites()) selected = itemSprites[cursorId - 1];
		else if (secondaryClx.has_value()) {
			const auto extra = secondaryClx->list();
			const uint32_t index = cursorId - itemSprites.numSprites() - 1;
			if (index < extra.numSprites()) selected = extra[index];
		}
		if (!selected.has_value()) return;
		const auto sprite = *selected;
		devilution::ClxDraw(surface, { x, bottomY }, sprite);
		const std::string name = item.baseName.empty() ? std::format("Base item #{}", item.baseItemId) : item.baseName;
		hitRegions.push_back({ x, bottomY - static_cast<int>(sprite.height()) + 1,
		    sprite.width(), sprite.height(),
		    std::format("{} — {} | Durability {}/{} | Charges {}/{} | Value {}",
		        location, name, item.durability, item.maxDurability, item.charges, item.maxCharges, item.value) });
		hitRegions.back().footprintWidth = static_cast<uint8_t>(std::max(1, (sprite.width() + 27) / 28));
		hitRegions.back().footprintHeight = static_cast<uint8_t>(std::max(1, (sprite.height() + 27) / 28));
	};
	constexpr std::array<devilution::Point, 7> EquipmentPositions {{
	    { 133, 59 }, { 48, 205 }, { 249, 205 }, { 205, 60 },
	    { 17, 160 }, { 248, 160 }, { 133, 160 }
	}};
	const std::array<InventoryHitRegion, 7> EquipmentHitBoxes {{
	    { 132, 4, 58, 58, {} }, { 47, 176, 29, 29, {} }, { 248, 176, 29, 29, {} },
	    { 204, 32, 29, 29, {} }, { 16, 75, 58, 87, {} }, { 247, 75, 58, 87, {} },
	    { 132, 75, 58, 87, {} }
	}};
	for (const auto &item : character.equipment) {
		if (item.slot < EquipmentPositions.size()) {
			drawItem(item, EquipmentPositions[item.slot].x, EquipmentPositions[item.slot].y, "Equipped");
			if (!hitRegions.empty()) {
				hitRegions.back().area = InventoryArea::Equipment;
				hitRegions.back().slot = item.slot;
				hitRegions.back().x = EquipmentHitBoxes[item.slot].x;
				hitRegions.back().y = EquipmentHitBoxes[item.slot].y;
				hitRegions.back().width = EquipmentHitBoxes[item.slot].width;
				hitRegions.back().height = EquipmentHitBoxes[item.slot].height;
			}
		}
	}
	for (const auto &item : character.inventory) {
		for (std::size_t cell = 0; cell < character.inventoryGrid.size(); ++cell) {
			if (character.inventoryGrid[cell] != static_cast<int8_t>(item.slot + 1))
				continue;
			const int x = 17 + static_cast<int>(cell % 10) * 29;
			const int bottomY = 222 + static_cast<int>(cell / 10) * 29 + 29;
			drawItem(item, x, bottomY, std::format("Inventory item {}", item.slot + 1));
			if (!hitRegions.empty()) {
				hitRegions.back().area = InventoryArea::Inventory;
				hitRegions.back().slot = item.slot;
				int minimumColumn = 10;
				int maximumColumn = -1;
				int minimumRow = 4;
				int maximumRow = -1;
				for (std::size_t occupiedCell = 0; occupiedCell < character.inventoryGrid.size(); ++occupiedCell) {
					if (std::abs(static_cast<int>(character.inventoryGrid[occupiedCell])) != item.slot + 1)
						continue;
					const int column = static_cast<int>(occupiedCell % 10);
					const int row = static_cast<int>(occupiedCell / 10);
					minimumColumn = std::min(minimumColumn, column);
					maximumColumn = std::max(maximumColumn, column);
					minimumRow = std::min(minimumRow, row);
					maximumRow = std::max(maximumRow, row);
				}
				if (maximumColumn >= minimumColumn && maximumRow >= minimumRow) {
					hitRegions.back().x = 16 + minimumColumn * 29;
					hitRegions.back().y = 222 + minimumRow * 29;
					hitRegions.back().width = (maximumColumn - minimumColumn + 1) * 29;
					hitRegions.back().height = (maximumRow - minimumRow + 1) * 29;
					hitRegions.back().footprintWidth = static_cast<uint8_t>(maximumColumn - minimumColumn + 1);
					hitRegions.back().footprintHeight = static_cast<uint8_t>(maximumRow - minimumRow + 1);
				}
			}
			break;
		}
	}
	for (const auto &item : character.belt) {
		if (item.slot < 8) {
			drawItem(item, 16 + item.slot * 37, InventoryHeight + 34, std::format("Belt slot {}", item.slot + 1));
			if (!hitRegions.empty()) {
				hitRegions.back().area = InventoryArea::Belt;
				hitRegions.back().slot = item.slot;
				hitRegions.back().x = 15 + item.slot * 37;
				hitRegions.back().y = InventoryHeight + 5;
				hitRegions.back().width = 31;
				hitRegions.back().height = 31;
			}
		}
	}

	RenderedInventory result;
	result.width = Width;
	result.height = InventoryHeight + BeltHeight;
	result.pixels.resize(static_cast<std::size_t>(result.width) * result.height);
	result.items = std::move(hitRegions);
	for (int y = 0; y < result.height; ++y) {
		for (int x = 0; x < result.width; ++x) {
			const uint8_t index = surface[{ x, y }];
			const auto *rgb = reinterpret_cast<const uint8_t *>(paletteData->data()) + index * 3;
			result.pixels[static_cast<std::size_t>(y) * result.width + x] = (static_cast<uint32_t>(rgb[0]) << 16) | (static_cast<uint32_t>(rgb[1]) << 8) | rgb[2];
		}
	}
	return result;
}

} // namespace d1hellforge
