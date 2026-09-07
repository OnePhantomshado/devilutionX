#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "utils/item_file.hpp"

namespace devilution {
namespace {

PackedItemBytes TestItem()
{
	PackedItemBytes item {};
	for (std::size_t i = 0; i < item.size(); ++i)
		item[i] = std::byte { static_cast<unsigned char>(i * 7) };
	return item;
}

TEST(ItemFile, RawItemPackRoundTrip)
{
	const PackedItemBytes item = TestItem();
	const std::vector<std::byte> encoded = EncodeRawItemPack(item);

	EXPECT_EQ(DetectItemFileFormat(encoded), ItemFileFormat::RawItemPack);
	const auto decoded = DecodeItemFile(encoded);
	ASSERT_TRUE(decoded.has_value()) << decoded.error();
	EXPECT_EQ(decoded->format, ItemFileFormat::RawItemPack);
	EXPECT_EQ(decoded->game, ItemGame::Unknown);
	EXPECT_EQ(decoded->packedItem, item);
}

TEST(ItemFile, DevilutionXRoundTripPreservesGameAndPayload)
{
	const PackedItemBytes item = TestItem();
	const std::vector<std::byte> encoded = EncodeDevilutionXItem(item, ItemGame::Hellfire);

	EXPECT_EQ(DetectItemFileFormat(encoded), ItemFileFormat::DevilutionX);
	const auto decoded = DecodeItemFile(encoded);
	ASSERT_TRUE(decoded.has_value()) << decoded.error();
	EXPECT_EQ(decoded->game, ItemGame::Hellfire);
	EXPECT_EQ(decoded->packedItem, item);
}

TEST(ItemFile, DevilutionXRoundTripPreservesExpandedSinglePlayerItem)
{
	const auto packed = TestItem();
	std::vector<std::byte> expanded(372);
	for (std::size_t i = 0; i < expanded.size(); ++i) expanded[i] = static_cast<std::byte>(i);
	const auto encoded = EncodeDevilutionXItem(packed, ItemGame::Hellfire, expanded);
	const auto decoded = DecodeItemFile(encoded);
	ASSERT_TRUE(decoded.has_value()) << decoded.error();
	EXPECT_EQ(decoded->game, ItemGame::Hellfire);
	EXPECT_EQ(decoded->packedItem, packed);
	EXPECT_EQ(decoded->fullItemRecord, expanded);
}

TEST(ItemFile, RejectsCorruptedPayload)
{
	std::vector<std::byte> encoded = EncodeDevilutionXItem(TestItem(), ItemGame::Diablo);
	encoded.back() ^= std::byte { 0x40 };

	const auto decoded = DecodeItemFile(encoded);
	ASSERT_FALSE(decoded.has_value());
	EXPECT_EQ(decoded.error(), "DevilutionX item checksum mismatch");
}

TEST(ItemFile, DetectsButDoesNotGuessItm01Variant)
{
	const std::array<std::byte, 8> legacy {
		std::byte { 'I' }, std::byte { 'T' }, std::byte { 'M' }, std::byte { '0' },
		std::byte { '1' }, std::byte { 0 }, std::byte { 1 }, std::byte { 2 }
	};

	EXPECT_EQ(DetectItemFileFormat(legacy), ItemFileFormat::LegacyItm01);
	const auto decoded = DecodeItemFile(legacy);
	ASSERT_FALSE(decoded.has_value());
	EXPECT_NE(decoded.error().find("ITM01"), std::string::npos);
}

TEST(ItemFile, DetectsHellfireHifFixtureShape)
{
	std::array<std::byte, 400> legacy {};
	constexpr std::string_view Magic = "HELLFIRE ITEM FILE";
	for (std::size_t i = 0; i < Magic.size(); ++i)
		legacy[i] = std::byte { static_cast<unsigned char>(Magic[i]) };

	EXPECT_EQ(DetectItemFileFormat(legacy), ItemFileFormat::LegacyHellfireHif);
	const auto decoded = DecodeItemFile(legacy);
	ASSERT_FALSE(decoded.has_value());
	EXPECT_NE(decoded.error().find("HIF"), std::string::npos);

	legacy.back() = std::byte { 1 };
	legacy[18] = std::byte { 'X' };
	EXPECT_EQ(DetectItemFileFormat(legacy), ItemFileFormat::LegacyHellfireHif);
}

TEST(ItemFile, RejectsUnknownAndWrongSizedFiles)
{
	const std::vector<std::byte> empty;
	EXPECT_EQ(DetectItemFileFormat(empty), ItemFileFormat::Unknown);
	EXPECT_FALSE(DecodeItemFile(empty).has_value());

	std::vector<std::byte> wrongSize(PackedItemSize + 1);
	EXPECT_EQ(DetectItemFileFormat(wrongSize), ItemFileFormat::Unknown);
	EXPECT_FALSE(DecodeItemFile(wrongSize).has_value());
}

} // namespace
} // namespace devilution
