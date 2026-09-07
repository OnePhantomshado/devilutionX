#include "item_transfer.hpp"

#include "legacy_item_import.hpp"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <vector>

namespace d1hellforge {

std::expected<devilution::ItemFile, std::string> ImportItemFile(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary);
	if (!input)
		return std::unexpected("Unable to open the item file");
	const std::vector<char> characters { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	std::vector<std::byte> bytes(characters.size());
	for (std::size_t i = 0; i < characters.size(); ++i)
		bytes[i] = std::byte { static_cast<unsigned char>(characters[i]) };
	const auto format = devilution::DetectItemFileFormat(bytes);
	if (format == devilution::ItemFileFormat::LegacyItm01 || format == devilution::ItemFileFormat::LegacyHellfireHif)
		return DecodeLegacyItem(bytes, format);
	return devilution::DecodeItemFile(bytes);
}

std::expected<void, std::string> ExportItemFile(const std::filesystem::path &path, const devilution::PackedItemBytes &item, devilution::ItemGame game, bool raw)
{
	const std::vector<std::byte> bytes = raw ? devilution::EncodeRawItemPack(item) : devilution::EncodeDevilutionXItem(item, game);
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output)
		return std::unexpected("Unable to create the item file");
	output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!output)
		return std::unexpected("Unable to finish writing the item file");
	return {};
}

std::expected<void, std::string> ExportItemFile(const std::filesystem::path &path, const CharacterSummary::PackedItemSummary &item, devilution::ItemGame game, bool raw)
{
	const std::vector<std::byte> bytes = raw ? devilution::EncodeRawItemPack(item.packedBytes) : devilution::EncodeDevilutionXItem(item.packedBytes, game, item.fullItemRecord);
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output) return std::unexpected("Unable to create the item file");
	output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!output) return std::unexpected("Unable to finish writing the item file");
	return {};
}

} // namespace d1hellforge
