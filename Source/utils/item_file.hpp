/**
 * @file item_file.hpp
 *
 * Portable and legacy item-file container support for save-editor tools.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace devilution {

constexpr std::size_t PackedItemSize = 20;
using PackedItemBytes = std::array<std::byte, PackedItemSize>;

enum class ItemFileFormat : uint8_t {
	Unknown,
	RawItemPack,
	DevilutionX,
	LegacyItm01,
	LegacyHellfireHif,
};

enum class ItemGame : uint8_t {
	Unknown,
	Diablo,
	Hellfire,
};

struct ItemFile {
	ItemFileFormat format = ItemFileFormat::Unknown;
	ItemGame game = ItemGame::Unknown;
	PackedItemBytes packedItem {};
	std::vector<std::byte> fullItemRecord;
	// Informational only. Legacy full records can store a display name that the
	// compact DevilutionX item representation cannot preserve.
	std::string legacyStoredName;
};

/** Detect an item container without trusting its file extension. */
ItemFileFormat DetectItemFileFormat(std::span<const std::byte> data);

/**
 * Decode a supported item container.
 *
 * Raw 20-byte ItemPack files are accepted for compatibility. ITM01 and
 * Hellfire Item File (HIF) records are detected, but rejected until their
 * historical layouts have dedicated converters backed by fixtures.
 */
std::expected<ItemFile, std::string> DecodeItemFile(std::span<const std::byte> data);

/** Encode a versioned, checksummed DevilutionX portable item container. */
std::vector<std::byte> EncodeDevilutionXItem(const PackedItemBytes &packedItem, ItemGame game);
std::vector<std::byte> EncodeDevilutionXItem(const PackedItemBytes &packedItem, ItemGame game, std::span<const std::byte> fullItemRecord);

/** Export the exact 20-byte ItemPack payload used by classic raw importers. */
std::vector<std::byte> EncodeRawItemPack(const PackedItemBytes &packedItem);

} // namespace devilution
