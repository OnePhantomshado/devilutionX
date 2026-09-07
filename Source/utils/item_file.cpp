/**
 * @file item_file.cpp
 *
 * Portable and legacy item-file container support for save-editor tools.
 */
#include "utils/item_file.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace devilution {
namespace {

constexpr std::array<std::byte, 8> DevilutionXItemMagic {
	std::byte { 'D' }, std::byte { 'X' }, std::byte { 'I' }, std::byte { 'T' },
	std::byte { 'E' }, std::byte { 'M' }, std::byte { '\r' }, std::byte { '\n' }
};
constexpr uint8_t DevilutionXItemVersion = 2;
constexpr std::size_t HeaderSize = 16;
constexpr std::size_t HellfireHifSize = 400;

uint16_t ReadLE16(const std::byte *src)
{
	return static_cast<uint16_t>(std::to_integer<uint8_t>(src[0]))
	    | (static_cast<uint16_t>(std::to_integer<uint8_t>(src[1])) << 8);
}

uint32_t ReadLE32(const std::byte *src)
{
	return static_cast<uint32_t>(std::to_integer<uint8_t>(src[0]))
	    | (static_cast<uint32_t>(std::to_integer<uint8_t>(src[1])) << 8)
	    | (static_cast<uint32_t>(std::to_integer<uint8_t>(src[2])) << 16)
	    | (static_cast<uint32_t>(std::to_integer<uint8_t>(src[3])) << 24);
}

void WriteLE16(std::byte *dst, uint16_t value)
{
	dst[0] = std::byte { static_cast<uint8_t>(value) };
	dst[1] = std::byte { static_cast<uint8_t>(value >> 8) };
}

void WriteLE32(std::byte *dst, uint32_t value)
{
	dst[0] = std::byte { static_cast<uint8_t>(value) };
	dst[1] = std::byte { static_cast<uint8_t>(value >> 8) };
	dst[2] = std::byte { static_cast<uint8_t>(value >> 16) };
	dst[3] = std::byte { static_cast<uint8_t>(value >> 24) };
}

uint32_t Crc32(std::span<const std::byte> data)
{
	uint32_t crc = 0xFFFFFFFF;
	for (const std::byte value : data) {
		crc ^= std::to_integer<uint8_t>(value);
		for (int bit = 0; bit < 8; ++bit)
			crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
	}
	return ~crc;
}

bool StartsWith(std::span<const std::byte> data, std::span<const std::byte> prefix)
{
	return data.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin());
}

} // namespace

ItemFileFormat DetectItemFileFormat(std::span<const std::byte> data)
{
	if (StartsWith(data, DevilutionXItemMagic))
		return ItemFileFormat::DevilutionX;

	constexpr std::array<std::byte, 5> Itm01Magic {
		std::byte { 'I' }, std::byte { 'T' }, std::byte { 'M' }, std::byte { '0' }, std::byte { '1' }
	};
	if (StartsWith(data, Itm01Magic))
		return ItemFileFormat::LegacyItm01;

	constexpr std::array<std::byte, 18> HellfireHifMagic {
		std::byte { 'H' }, std::byte { 'E' }, std::byte { 'L' }, std::byte { 'L' },
		std::byte { 'F' }, std::byte { 'I' }, std::byte { 'R' }, std::byte { 'E' },
		std::byte { ' ' }, std::byte { 'I' }, std::byte { 'T' }, std::byte { 'E' },
		std::byte { 'M' }, std::byte { ' ' }, std::byte { 'F' }, std::byte { 'I' },
		std::byte { 'L' }, std::byte { 'E' }
	};
	if (data.size() == HellfireHifSize && StartsWith(data, HellfireHifMagic))
		return ItemFileFormat::LegacyHellfireHif;

	if (data.size() == PackedItemSize)
		return ItemFileFormat::RawItemPack;

	return ItemFileFormat::Unknown;
}

std::expected<ItemFile, std::string> DecodeItemFile(std::span<const std::byte> data)
{
	const ItemFileFormat format = DetectItemFileFormat(data);
	if (format == ItemFileFormat::RawItemPack) {
		ItemFile result;
		result.format = format;
		std::copy(data.begin(), data.end(), result.packedItem.begin());
		return result;
	}

	if (format == ItemFileFormat::LegacyItm01) {
		return std::unexpected("ITM01 item file detected, but this historical variant is not supported yet");
	}
	if (format == ItemFileFormat::LegacyHellfireHif) {
		return std::unexpected("Hellfire HIF item file detected, but conversion to ItemPack is not supported yet");
	}

	if (format != ItemFileFormat::DevilutionX)
		return std::unexpected("Unrecognized item file format");

	if (data.size() < HeaderSize)
		return std::unexpected("Truncated DevilutionX item header");
	const uint8_t version = std::to_integer<uint8_t>(data[8]);
	if (version != 1 && version != DevilutionXItemVersion)
		return std::unexpected("Unsupported DevilutionX item version");
	const uint16_t payloadSize = ReadLE16(&data[10]);
	if (payloadSize != PackedItemSize && payloadSize != PackedItemSize + 368 && payloadSize != PackedItemSize + 372)
		return std::unexpected("Unexpected DevilutionX item payload size");
	if (version == 1 && payloadSize != PackedItemSize)
		return std::unexpected("Version 1 DevilutionX items must contain only ItemPack data");
	if (data.size() != HeaderSize + payloadSize)
		return std::unexpected("DevilutionX item file has trailing or missing data");

	const uint8_t gameValue = std::to_integer<uint8_t>(data[9]);
	if (gameValue > static_cast<uint8_t>(ItemGame::Hellfire))
		return std::unexpected("Invalid game identifier in DevilutionX item file");

	const std::span<const std::byte> payload = data.subspan(HeaderSize, payloadSize);
	if (ReadLE32(&data[12]) != Crc32(payload))
		return std::unexpected("DevilutionX item checksum mismatch");

	ItemFile result;
	result.format = format;
	result.game = static_cast<ItemGame>(gameValue);
	std::copy_n(payload.begin(), PackedItemSize, result.packedItem.begin());
	if (payload.size() > PackedItemSize) result.fullItemRecord.assign(payload.begin() + PackedItemSize, payload.end());
	return result;
}

std::vector<std::byte> EncodeDevilutionXItem(const PackedItemBytes &packedItem, ItemGame game)
{
	return EncodeDevilutionXItem(packedItem, game, {});
}

std::vector<std::byte> EncodeDevilutionXItem(const PackedItemBytes &packedItem, ItemGame game, std::span<const std::byte> fullItemRecord)
{
	const std::size_t payloadSize = PackedItemSize + fullItemRecord.size();
	std::vector<std::byte> result(HeaderSize + payloadSize);
	std::copy(DevilutionXItemMagic.begin(), DevilutionXItemMagic.end(), result.begin());
	result[8] = std::byte { DevilutionXItemVersion };
	result[9] = std::byte { static_cast<uint8_t>(game) };
	WriteLE16(&result[10], static_cast<uint16_t>(payloadSize));
	std::copy(packedItem.begin(), packedItem.end(), result.begin() + HeaderSize);
	std::copy(fullItemRecord.begin(), fullItemRecord.end(), result.begin() + HeaderSize + PackedItemSize);
	WriteLE32(&result[12], Crc32(std::span<const std::byte>(result).subspan(HeaderSize)));
	return result;
}

std::vector<std::byte> EncodeRawItemPack(const PackedItemBytes &packedItem)
{
	return { packedItem.begin(), packedItem.end() };
}

} // namespace devilution
