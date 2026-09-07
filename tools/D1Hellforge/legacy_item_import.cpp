#include "legacy_item_import.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace d1hellforge {

std::string LegacyImportStatus(devilution::ItemFileFormat format)
{
	if (format == devilution::ItemFileFormat::LegacyItm01)
		return "Legacy ITM01 detected.";
	if (format == devilution::ItemFileFormat::LegacyHellfireHif)
		return "Hellfire HIF detected.";
	return "This is not a recognized legacy item format.";
}

namespace {

uint16_t Load16(std::span<const std::byte> bytes, std::size_t offset)
{
	return static_cast<uint16_t>(std::to_integer<uint8_t>(bytes[offset]))
	    | static_cast<uint16_t>(std::to_integer<uint8_t>(bytes[offset + 1]) << 8);
}

uint32_t Load32(std::span<const std::byte> bytes, std::size_t offset)
{
	return static_cast<uint32_t>(Load16(bytes, offset)) | (static_cast<uint32_t>(Load16(bytes, offset + 2)) << 16);
}

void Store16(devilution::PackedItemBytes &bytes, std::size_t offset, uint16_t value)
{
	bytes[offset] = std::byte { static_cast<uint8_t>(value) };
	bytes[offset + 1] = std::byte { static_cast<uint8_t>(value >> 8) };
}

void Store32(devilution::PackedItemBytes &bytes, std::size_t offset, uint32_t value)
{
	Store16(bytes, offset, static_cast<uint16_t>(value));
	Store16(bytes, offset + 2, static_cast<uint16_t>(value >> 16));
}

} // namespace

std::expected<devilution::ItemFile, std::string> DecodeLegacyItem(std::span<const std::byte> bytes, devilution::ItemFileFormat format)
{
	if (bytes.size() != 400)
		return std::unexpected("Legacy item file must be exactly 400 bytes");
	const bool hellfire = format == devilution::ItemFileFormat::LegacyHellfireHif;
	const std::size_t base = hellfire ? 28 : 32;
	constexpr std::string_view ItmMagic = "ITM01.I'll get that al'Thor!";
	constexpr std::string_view HifMagic = "HELLFIRE ITEM FILE";
	const std::string_view magic = hellfire ? HifMagic : ItmMagic;
	if (!std::equal(magic.begin(), magic.end(), reinterpret_cast<const char *>(bytes.data())))
		return std::unexpected("Legacy item header is not recognized");

	// Both files contain the original 32-bit ItemStruct memory layout. ITM01 has
	// a 32-byte header followed by Diablo's 368-byte struct. HIF has a 28-byte
	// header and Hellfire's 372-byte extension. These offsets are verified by the
	// supplied fixture collections and the original Devilution ItemStruct layout.
	const uint32_t itemIndex = Load32(bytes, base + 360);
	if (itemIndex > 0xFFFF)
		return std::unexpected("Legacy item contains an invalid base-item index");

	devilution::ItemFile result;
	result.format = format;
	result.game = hellfire ? devilution::ItemGame::Hellfire : devilution::ItemGame::Diablo;
	const char *storedName = reinterpret_cast<const char *>(bytes.data() + base + 125);
	const char *storedNameEnd = std::find(storedName, storedName + 64, '\0');
	result.legacyStoredName.assign(storedName, storedNameEnd);
	result.fullItemRecord.assign(bytes.begin() + base, bytes.end());
	Store32(result.packedItem, 0, Load32(bytes, base));             // seed
	Store16(result.packedItem, 4, Load16(bytes, base + 4));        // creation info
	Store16(result.packedItem, 6, static_cast<uint16_t>(itemIndex));
	const uint32_t identified = Load32(bytes, base + 56);
	const uint8_t quality = std::to_integer<uint8_t>(bytes[base + 60]);
	result.packedItem[8] = std::byte { static_cast<uint8_t>((quality << 1) | (identified != 0 ? 1 : 0)) };
	result.packedItem[9] = std::byte { static_cast<uint8_t>(std::min<uint32_t>(Load32(bytes, base + 236), 255)) };
	result.packedItem[10] = std::byte { static_cast<uint8_t>(std::min<uint32_t>(Load32(bytes, base + 240), 255)) };
	result.packedItem[11] = std::byte { static_cast<uint8_t>(std::min<uint32_t>(Load32(bytes, base + 228), 255)) };
	result.packedItem[12] = std::byte { static_cast<uint8_t>(std::min<uint32_t>(Load32(bytes, base + 232), 255)) };
	if (itemIndex == 0)
		Store16(result.packedItem, 13, static_cast<uint16_t>(std::min<uint32_t>(Load32(bytes, base + 196), 0xFFFF)));
	if (hellfire)
		Store32(result.packedItem, 15, Load32(bytes, base + 368));
	return result;
}

} // namespace d1hellforge
