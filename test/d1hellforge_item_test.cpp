#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <type_traits>

#include "item_transfer.hpp"
#include "item_advanced_editor.hpp"
#include "item_display.hpp"
#include "item_workshop.hpp"
#include "devx_adapter.hpp"
#include "save_document.hpp"
#include "stash_document.hpp"
#include "engine/assets.hpp"
#include "codec.h"
#include "items.h"
#include "mpq/mpq_writer.hpp"
#include "tables/itemdat.h"
#include "tables/spelldat.h"

namespace d1hellforge {
namespace {

class D1HellforgeItemTest : public ::testing::Test {
protected:
	static void SetUpTestSuite()
	{
		devilution::LoadItemData();
		devilution::LoadSpellData();
	}
};

template <typename T>
void AppendLE(std::vector<std::byte> &payload, T value)
{
	using U = std::make_unsigned_t<T>;
	for (std::size_t byte = 0; byte < sizeof(T); ++byte)
		payload.push_back(static_cast<std::byte>(static_cast<U>(value) >> (byte * 8)));
}

std::vector<std::byte> MakeStashPayload(Game game, uint32_t selectedPage = 4)
{
	std::vector<std::byte> payload;
	AppendLE<uint8_t>(payload, 0);
	AppendLE<uint32_t>(payload, 12345);
	AppendLE<uint32_t>(payload, 1);
	AppendLE<uint32_t>(payload, 4);
	for (unsigned cell = 0; cell < StashCellsPerPage; ++cell) AppendLE<uint16_t>(payload, cell < 2 ? 1 : 0);
	AppendLE<uint32_t>(payload, 1);
	payload.resize(payload.size() + (game == Game::Hellfire ? 372 : 368), std::byte {});
	AppendLE<uint32_t>(payload, selectedPage);
	return payload;
}

TEST_F(D1HellforgeItemTest, DiabloAndHellfireStashPayloadsUseProfileSpecificFullItemSizes)
{
	for (const Game game : { Game::Diablo, Game::Hellfire }) {
		const auto profile = GetDevXContentProfile(game);
		const auto decoded = DecodeStashPayload(MakeStashPayload(game), profile);
		ASSERT_TRUE(decoded.has_value()) << decoded.error();
		EXPECT_EQ(decoded->gold, 12345U);
		EXPECT_EQ(decoded->selectedPage, 4U);
		ASSERT_EQ(decoded->items.size(), 1U);
		EXPECT_EQ(decoded->items[0].fullItemRecord.size(), game == Game::Hellfire ? 372U : 368U);
		EXPECT_EQ(decoded->pages.at(4)[0], 1U);
	}
}

TEST_F(D1HellforgeItemTest, StashPayloadRejectsUnsupportedVersionAndInvalidReferences)
{
	const auto profile = GetDevXContentProfile(Game::Diablo);
	auto payload = MakeStashPayload(Game::Diablo);
	payload[0] = std::byte { 1 };
	EXPECT_FALSE(DecodeStashPayload(payload, profile).has_value());
	payload = MakeStashPayload(Game::Diablo);
	// First grid cell follows version, gold, page count, and page number.
	payload[13] = std::byte { 2 };
	EXPECT_FALSE(DecodeStashPayload(payload, profile).has_value());
	payload = MakeStashPayload(Game::Diablo, 100);
	EXPECT_FALSE(DecodeStashPayload(payload, profile).has_value());
}

TEST_F(D1HellforgeItemTest, StashPayloadRoundTripsAndMovesItemsWithoutChangingTheirRecords)
{
	for (const Game game : { Game::Diablo, Game::Hellfire }) {
		const auto profile = GetDevXContentProfile(game);
		auto decoded = DecodeStashPayload(MakeStashPayload(game), profile);
		ASSERT_TRUE(decoded.has_value()) << decoded.error();
		const auto originalRecord = decoded->items[0].fullItemRecord;
		ASSERT_TRUE(MoveStashItem(*decoded, 1, 7, 8, 7).has_value());
		EXPECT_EQ(decoded->pages.at(7)[8 * 10 + 7], 1);
		EXPECT_EQ(decoded->pages.at(7)[8 * 10 + 8], 1);
		EXPECT_EQ(decoded->pages.at(4)[0], 0);
		EXPECT_EQ(decoded->pages.at(4)[1], 0);
		EXPECT_EQ(decoded->items[0].fullItemRecord.size(), originalRecord.size());
		EXPECT_TRUE(std::equal(originalRecord.begin(), originalRecord.begin() + 12, decoded->items[0].fullItemRecord.begin()));
		EXPECT_TRUE(std::equal(originalRecord.begin() + 20, originalRecord.end(), decoded->items[0].fullItemRecord.begin() + 20));
		const auto encoded = EncodeStashPayload(*decoded);
		ASSERT_TRUE(encoded.has_value()) << encoded.error();
		const auto reopened = DecodeStashPayload(*encoded, profile);
		ASSERT_TRUE(reopened.has_value()) << reopened.error();
		EXPECT_EQ(reopened->selectedPage, 7);
		EXPECT_EQ(reopened->pages.at(7), decoded->pages.at(7));
		EXPECT_EQ(reopened->items[0].fullItemRecord, decoded->items[0].fullItemRecord);
	}
}

TEST_F(D1HellforgeItemTest, StashMoveRejectsCollisionsAndOutOfBoundsDestinations)
{
	const auto profile = GetDevXContentProfile(Game::Diablo);
	auto decoded = DecodeStashPayload(MakeStashPayload(Game::Diablo), profile);
	ASSERT_TRUE(decoded.has_value()) << decoded.error();
	decoded->pages[4][2 * 10 + 2] = 1;
	// Restore a rectangular 1x3 source footprint for this movement check.
	decoded->pages[4][0] = 1; decoded->pages[4][1] = 1;
	EXPECT_FALSE(MoveStashItem(*decoded, 1, 4, 0, 9).has_value());
	decoded->pages[4][5 * 10 + 5] = 2;
	decoded->items.push_back({ 1, std::vector<std::byte>(368) });
	EXPECT_FALSE(MoveStashItem(*decoded, 1, 4, 5, 5).has_value());
}

TEST_F(D1HellforgeItemTest, StashCopyAndDeleteKeepCanonicalReferencesAndRoundTrip)
{
	for (const Game game : { Game::Diablo, Game::Hellfire }) {
		const auto profile = GetDevXContentProfile(game);
		auto stash = DecodeStashPayload(MakeStashPayload(game), profile);
		ASSERT_TRUE(stash.has_value()) << stash.error();
		const auto copied = CopyStashItem(*stash, 1, 4);
		ASSERT_TRUE(copied.has_value()) << copied.error();
		EXPECT_EQ(*copied, 2);
		ASSERT_EQ(stash->items.size(), 2);
		EXPECT_EQ(stash->items[1].index, 1);
		EXPECT_EQ(stash->pages.at(4)[10], 2);
		EXPECT_EQ(stash->pages.at(4)[11], 2);
		ASSERT_TRUE(DeleteStashItem(*stash, 1).has_value());
		ASSERT_EQ(stash->items.size(), 1);
		EXPECT_EQ(stash->items[0].index, 0);
		EXPECT_EQ(stash->pages.at(4)[0], 0);
		EXPECT_EQ(stash->pages.at(4)[1], 0);
		EXPECT_EQ(stash->pages.at(4)[10], 1);
		EXPECT_EQ(stash->pages.at(4)[11], 1);
		const auto encoded = EncodeStashPayload(*stash);
		ASSERT_TRUE(encoded.has_value()) << encoded.error();
		const auto reopened = DecodeStashPayload(*encoded, profile);
		ASSERT_TRUE(reopened.has_value()) << reopened.error();
		EXPECT_EQ(reopened->pages.at(4), stash->pages.at(4));
		EXPECT_EQ(reopened->items[0].fullItemRecord, stash->items[0].fullItemRecord);
	}
}

TEST_F(D1HellforgeItemTest, VerifiedStashWriterBacksUpReopensAndPreservesMovedItem)
{
	const auto profile = GetDevXContentProfile(Game::Hellfire);
	const auto directory = std::filesystem::temp_directory_path() / ("d1hellforge-stash-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(directory);
	const auto path = directory / "stash.sv";
	const auto payload = MakeStashPayload(Game::Hellfire);
	std::vector<std::byte> encoded(devilution::codec_get_encoded_len(payload.size()));
	std::copy(payload.begin(), payload.end(), encoded.begin());
	devilution::codec_encode(encoded.data(), payload.size(), encoded.size(), "xrgyrkj1");
	{
		devilution::MpqWriter writer(path.string(), false);
		ASSERT_TRUE(writer.WriteFile("spstashitems", encoded.data(), encoded.size()));
	}
	auto stash = ReadStashDocument(path, profile);
	ASSERT_TRUE(stash.has_value()) << stash.error();
	ASSERT_TRUE(MoveStashItem(*stash, 1, 6, 3, 4).has_value());
	const auto saved = SaveStashDocument(*stash);
	ASSERT_TRUE(saved.has_value()) << saved.error();
	EXPECT_TRUE(std::filesystem::exists(saved->backupPath));
	const auto reopened = ReadStashDocument(path, profile);
	ASSERT_TRUE(reopened.has_value()) << reopened.error();
	EXPECT_EQ(reopened->selectedPage, 6);
	EXPECT_EQ(reopened->pages.at(6)[3 * 10 + 4], 1);
	EXPECT_EQ(reopened->pages.at(6)[3 * 10 + 5], 1);
	std::filesystem::remove_all(directory);
}

TEST_F(D1HellforgeItemTest, DiabloContentProfileDescribesInitializedBaseContent)
{
	const auto &profile = GetDevXContentProfile(Game::Diablo);
	EXPECT_EQ(profile.baseMode, Game::Diablo);
	EXPECT_TRUE(profile.contentIdentifiers.empty());
	EXPECT_FALSE(profile.looseContentActive);
	EXPECT_EQ(profile.displayLabel, "Diablo");
	EXPECT_TRUE(profile.initialized);
	EXPECT_TRUE(profile.initializationError.empty());
}

TEST_F(D1HellforgeItemTest, HellfireContentProfileDescribesInitializedEffectiveContent)
{
	const auto &profile = GetDevXContentProfile(Game::Hellfire);
	EXPECT_EQ(profile.baseMode, Game::Hellfire);
	const bool hasHellfireIdentifier = std::find(profile.contentIdentifiers.begin(), profile.contentIdentifiers.end(), "hf") != profile.contentIdentifiers.end();
	EXPECT_TRUE(hasHellfireIdentifier || profile.looseContentActive);
	EXPECT_EQ(profile.displayLabel, hasHellfireIdentifier
	        ? profile.looseContentActive ? "Hellfire [content: hf] + loose content" : "Hellfire [content: hf]"
	        : "Hellfire + loose content");
	EXPECT_TRUE(profile.initialized);
	EXPECT_TRUE(profile.initializationError.empty());
}

TEST_F(D1HellforgeItemTest, ContentProfileSwitchDoesNotRetainPriorBaseContent)
{
	const ContentProfile hellfire = GetDevXContentProfile(Game::Hellfire);
	const ContentProfile diablo = GetDevXContentProfile(Game::Diablo);
	EXPECT_EQ(hellfire.baseMode, Game::Hellfire);
	EXPECT_EQ(diablo.baseMode, Game::Diablo);
	EXPECT_TRUE(diablo.contentIdentifiers.empty());
	EXPECT_EQ(diablo.displayLabel, "Diablo");
}

TEST_F(D1HellforgeItemTest, WorkshopCarriesEffectiveContentProfile)
{
	const ContentProfile profile = GetDevXContentProfile(Game::Hellfire);
	const auto model = MakeCreateWorkshopModel(profile, 30, true);
	EXPECT_EQ(model.game, profile.baseMode);
	EXPECT_EQ(model.contentProfile.contentIdentifiers, profile.contentIdentifiers);
	EXPECT_EQ(model.contentProfile.looseContentActive, profile.looseContentActive);
	EXPECT_EQ(model.contentProfile.displayLabel, profile.displayLabel);
}

TEST_F(D1HellforgeItemTest, WorkshopRejectsUninitializedOrMismatchedContentProfile)
{
	ContentProfile uninitialized;
	auto model = MakeCreateWorkshopModel(uninitialized, 30, true);
	auto validation = VanillaItemRules::Validate(model);
	EXPECT_FALSE(validation.valid);
	EXPECT_NE(std::find(validation.errors.begin(), validation.errors.end(), "The content profile is not initialized"), validation.errors.end());

	ContentProfile mismatched = GetDevXContentProfile(Game::Hellfire);
	model = MakeCreateWorkshopModel(mismatched, 30, true);
	model.game = Game::Diablo;
	validation = VanillaItemRules::Validate(model);
	EXPECT_FALSE(validation.valid);
	EXPECT_NE(std::find(validation.errors.begin(), validation.errors.end(), "The Workshop save mode and content profile base mode do not match"), validation.errors.end());
}

TEST_F(D1HellforgeItemTest, AdvancedDirectEditsRoundTripFullSerializedFieldsAndPreserveUnknownFlags)
{
	const auto generated = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, { 12345 });
	ASSERT_TRUE(generated.has_value()) << generated.error();
	auto summary = *generated;
	summary.fullItemRecord.assign(372, std::byte {});
	auto write32 = [&summary](std::size_t offset, uint32_t value) {
		for (int byte = 0; byte < 4; ++byte) summary.fullItemRecord[offset + byte] = static_cast<std::byte>(value >> (byte * 8));
	};
	write32(8, static_cast<uint32_t>(devilution::ItemType::Staff));
	write32(56, 1); summary.fullItemRecord[60] = std::byte { 1 };
	summary.fullItemRecord[190] = static_cast<std::byte>(devilution::ICLASS_WEAPON);
	write32(192, 86); write32(204, 4); write32(208, 12); write32(360, devilution::AllItemsList[devilution::IDI_SHORTSTAFF].iMappingId);
	constexpr uint32_t UnknownFlag = 1U;
	write32(216, UnknownFlag);
	const auto opened = MakeAdvancedItemEditorModel(summary, Game::Hellfire, false);
	ASSERT_TRUE(opened.has_value()) << opened.error();
	auto model = *opened;
	strcpy_s(model.staged._iName, "Great Sword");
	strcpy_s(model.staged._iIName, "Blood Drinker");
	model.staged._iMinDam = 9; model.staged._iMaxDam = 37;
	model.staged._iDurability = 201; model.staged._iMaxDur = 222;
	model.staged._iCharges = 7; model.staged._iMaxCharges = 19;
	model.staged._iPLDam = 137; model.staged._iPLStr = 27; model.staged._iPLFR = 31;
	model.staged._iPLHP = 123 << 6;
	model.staged._iFlags |= devilution::ItemSpecialEffect::Knockback;
	const auto applied = ApplyAdvancedItemEditorModel(model, summary);
	ASSERT_TRUE(applied.has_value()) << applied.error();
	auto reopened = MakeAdvancedItemEditorModel(*applied, Game::Hellfire, false);
	ASSERT_TRUE(reopened.has_value()) << reopened.error();
	EXPECT_EQ(reopened->staged._iMinDam, 9);
	EXPECT_EQ(reopened->staged._iMaxDam, 37);
	EXPECT_STREQ(reopened->staged._iIName, "Blood Drinker");
	EXPECT_NE(reopened->staged.dwBuff & devilution::CF_CUSTOM_NAME, 0U);
	EXPECT_EQ(reopened->staged.getName().str(), "Blood Drinker");
	EXPECT_EQ(reopened->staged._iDurability, 201);
	EXPECT_EQ(reopened->staged._iMaxDur, 222);
	EXPECT_EQ(reopened->staged._iCharges, 7);
	EXPECT_EQ(reopened->staged._iMaxCharges, 19);
	EXPECT_EQ(reopened->staged._iPLDam, 137);
	EXPECT_EQ(reopened->staged._iPLStr, 27);
	EXPECT_EQ(reopened->staged._iPLFR, 31);
	EXPECT_EQ(reopened->staged._iPLHP >> 6, 123);
	EXPECT_NE(static_cast<uint32_t>(reopened->staged._iFlags) & UnknownFlag, 0U);
	EXPECT_TRUE(devilution::HasAnyOf(reopened->staged._iFlags, devilution::ItemSpecialEffect::Knockback));
	EXPECT_EQ(applied->displayName, "Blood Drinker");
	EXPECT_EQ(applied->cursorGraphic, 86);
	EXPECT_EQ(applied->baseItemId, devilution::IDI_SHORTSTAFF);
	devilution::ItemPack packed {};
	std::memcpy(&packed, applied->packedBytes.data(), sizeof(packed));
	uint32_t packedSeed = 0;
	std::memcpy(&packedSeed, applied->packedBytes.data(), sizeof(packedSeed));
	EXPECT_EQ(packedSeed, reopened->staged._iSeed);
	EXPECT_NE(std::find(applied->detailLines.begin(), applied->detailLines.end(), "Damage: 9-37"), applied->detailLines.end());
	EXPECT_NE(std::find(applied->detailLines.begin(), applied->detailLines.end(), "Durability: 201/222"), applied->detailLines.end());
	EXPECT_NE(std::find(applied->detailLines.begin(), applied->detailLines.end(), "Charges: 7/19"), applied->detailLines.end());
}

TEST_F(D1HellforgeItemTest, AdvancedDirectEditorRejectsInvalidRangesAndCancelResetRestoresItem)
{
	AdvancedItemEditorModel model;
	model.game = Game::Diablo;
	model.staged.IDidx = devilution::IDI_SHORTSTAFF;
	model.staged._iMagical = devilution::ITEM_QUALITY_MAGIC;
	model.staged._iMinDam = 20; model.staged._iMaxDam = 10;
	EXPECT_FALSE(ValidateAdvancedItem(model).empty());
	model.original._iPLStr = 5; model.staged._iPLStr = 99;
	ResetAdvancedItemEditorModel(model);
	EXPECT_EQ(model.staged._iPLStr, 5);
}

TEST_F(D1HellforgeItemTest, AdvancedDirectPreviewUsesPlayerFacingValuesWithoutChangingMetadata)
{
	AdvancedItemEditorModel model;
	model.game = Game::Hellfire;
	model.staged.IDidx = devilution::IDI_SHORTSTAFF;
	model.staged._iClass = devilution::ICLASS_WEAPON;
	model.staged._iMagical = devilution::ITEM_QUALITY_MAGIC;
	model.staged._iIdentified = true;
	strcpy_s(model.staged._iIName, "Blood Drinker");
	model.staged._iMinDam = 10; model.staged._iMaxDam = 20;
	model.staged._iMaxDur = DUR_INDESTRUCTIBLE;
	model.staged._iPLHP = 55 << 6; model.staged._iPLMana = 55 << 6;
	model.staged._iFMinDam = 3; model.staged._iFMaxDam = 12;
	model.staged._iMinStr = 75;
	model.staged._iFlags = devilution::ItemSpecialEffect::None;
	const auto preview = BuildAdvancedItemPreview(model);
	EXPECT_NE(std::find(preview.begin(), preview.end(), "Blood Drinker"), preview.end());
	EXPECT_NE(std::find(preview.begin(), preview.end(), "Damage: 10-20"), preview.end());
	EXPECT_NE(std::find(preview.begin(), preview.end(), "Indestructible"), preview.end());
	EXPECT_NE(std::find(preview.begin(), preview.end(), "+55 Hit Points"), preview.end());
	EXPECT_NE(std::find(preview.begin(), preview.end(), "Fire Hit Damage: 3-12"), preview.end());
	EXPECT_EQ(model.staged._iFlags, devilution::ItemSpecialEffect::None);
}

TEST_F(D1HellforgeItemTest, AdvancedDirectEditorExpandsResolvedWorkshopPreviewButRejectsSplitIdentity)
{
	auto preview = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, { 0x73986C15 });
	ASSERT_TRUE(preview.has_value()) << preview.error();
	EXPECT_TRUE(preview->fullItemRecord.empty());
	const auto opened = MakeAdvancedItemEditorModel(*preview, Game::Hellfire, true);
	ASSERT_TRUE(opened.has_value()) << opened.error();
	EXPECT_TRUE(opened->reconstructedFromPacked);
	EXPECT_EQ(opened->originalRecord.size(), 372U);
	const auto applied = ApplyAdvancedItemEditorModel(*opened, *preview);
	ASSERT_TRUE(applied.has_value()) << applied.error();
	EXPECT_EQ(applied->fullItemRecord.size(), 372U);

	preview->activeGameIdentityDiffers = true;
	EXPECT_FALSE(MakeAdvancedItemEditorModel(*preview, Game::Hellfire, true).has_value());
}

TEST_F(D1HellforgeItemTest, SharedItemDisplayClassifiesDamageFromEnemiesOnce)
{
	InitializeDevXContent(Game::Hellfire);
	devilution::Item item;
	item.IDidx = devilution::IDI_SHORTSTAFF;
	item._iClass = devilution::ICLASS_WEAPON;
	item._iMagical = devilution::ITEM_QUALITY_MAGIC;
	item._iIdentified = true;
	strcpy_s(item._iIName, "Shared Display Staff");
	item._iPLGetHit = -4;
	auto display = BuildItemDisplay(item, ItemDisplayMode::Detailed, GetDevXContentProfile(Game::Hellfire));
	auto damageFromEnemies = std::find_if(display.lines.begin(), display.lines.end(), [](const auto &line) {
		return line.text.find("Damage From Enemies") != std::string::npos;
	});
	ASSERT_NE(damageFromEnemies, display.lines.end());
	EXPECT_EQ(damageFromEnemies->tone, ItemDisplayTone::Positive);

	item._iPLGetHit = 5;
	display = BuildItemDisplay(item, ItemDisplayMode::Detailed, GetDevXContentProfile(Game::Hellfire));
	damageFromEnemies = std::find_if(display.lines.begin(), display.lines.end(), [](const auto &line) {
		return line.text.find("Damage From Enemies") != std::string::npos;
	});
	ASSERT_NE(damageFromEnemies, display.lines.end());
	EXPECT_EQ(damageFromEnemies->tone, ItemDisplayTone::Negative);
	EXPECT_NE(damageFromEnemies->text.find("☠"), std::string::npos);
}

TEST_F(D1HellforgeItemTest, EarOwnerNameRoundTripsThroughCompactGameLoadPath)
{
	InitializeDevXContent(Game::Hellfire);
	devilution::Item ear;
	devilution::RecreateEar(ear, 0x1234, 0x56789ABC, 0, "Darklord");
	devilution::ItemPack packed {};
	DevXPackItem(packed, ear, Game::Hellfire);
	CharacterSummary::PackedItemSummary summary;
	std::memcpy(summary.packedBytes.data(), &packed, sizeof(packed));
	summary.baseItemId = devilution::IDI_EAR;
	const auto opened = MakeAdvancedItemEditorModel(summary, Game::Hellfire, true);
	ASSERT_TRUE(opened.has_value()) << opened.error();
	auto edited = *opened;
	strcpy_s(edited.staged._iIName, "Blood Drinker");
	const auto applied = ApplyAdvancedItemEditorModel(edited, summary);
	ASSERT_TRUE(applied.has_value()) << applied.error();
	EXPECT_EQ(applied->displayName, "Ear of Blood Drinker");
	std::memcpy(&packed, applied->packedBytes.data(), sizeof(packed));
	devilution::Player player;
	devilution::Item loaded;
	DevXUnpackItem(packed, player, loaded, Game::Hellfire);
	EXPECT_EQ(loaded.IDidx, devilution::IDI_EAR);
	EXPECT_STREQ(loaded._iIName, "Blood Drinker");
	EXPECT_STREQ(loaded._iName, "Ear of Blood Drinker");
}

TEST_F(D1HellforgeItemTest, CatalogSeedIsDeterministic)
{
	constexpr uint32_t Seed = 0x1234ABCD;
	const auto first = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, { Seed });
	const auto second = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, { Seed });
	ASSERT_TRUE(first.has_value()) << first.error();
	ASSERT_TRUE(second.has_value()) << second.error();
	EXPECT_EQ(first->seed, Seed);
	EXPECT_EQ(first->packedBytes, second->packedBytes);
}

TEST_F(D1HellforgeItemTest, CatalogEnumeratesEffectiveContentTableAndIsStableWhenCached)
{
	for (const Game game : { Game::Diablo, Game::Hellfire }) {
		const auto first = GetItemCatalog(game);
		const auto second = GetItemCatalog(game);
		ASSERT_FALSE(first.empty());
		ASSERT_EQ(first.size(), second.size());
		for (std::size_t index = 0; index < first.size(); ++index) {
			EXPECT_EQ(first[index].baseItemId, second[index].baseItemId);
			EXPECT_EQ(first[index].name, second[index].name);
			EXPECT_EQ(first[index].category, second[index].category);
			const auto *effective = DevXBaseItem(first[index].baseItemId);
			ASSERT_NE(effective, nullptr);
			EXPECT_EQ(first[index].name, effective->iName);
		}
	}
}

TEST_F(D1HellforgeItemTest, GenerationChoicesAreStableWhenCachedAndInvalidateAcrossProfiles)
{
	const auto hellfireProfile = GetDevXContentProfile(Game::Hellfire);
	const auto first = GetItemGenerationChoices(devilution::IDI_SHORTSTAFF, hellfireProfile, 30, true);
	const auto second = GetItemGenerationChoices(devilution::IDI_SHORTSTAFF, hellfireProfile, 30, true);
	EXPECT_EQ(first.prefixes, second.prefixes);
	EXPECT_EQ(first.suffixes, second.suffixes);
	EXPECT_EQ(first.uniques, second.uniques);

	const auto diabloProfile = GetDevXContentProfile(Game::Diablo);
	const auto diablo = GetItemGenerationChoices(devilution::IDI_SHORTSTAFF, diabloProfile, 30, true);
	const auto diabloAgain = GetItemGenerationChoices(devilution::IDI_SHORTSTAFF, diabloProfile, 30, true);
	EXPECT_EQ(diablo.prefixes, diabloAgain.prefixes);
	EXPECT_EQ(diablo.suffixes, diabloAgain.suffixes);
}

TEST_F(D1HellforgeItemTest, AmuletUsesNativeEquipmentLocationForNeckSlotDrop)
{
	const auto profile = GetDevXContentProfile(Game::Hellfire);
	const auto items = DevXBaseItems();
	const auto amulet = std::find_if(items.begin(), items.end(), [](const devilution::ItemData &item) {
		return item.iLoc == devilution::ILOC_AMULET;
	});
	ASSERT_NE(amulet, items.end());
	CharacterSummary::PackedItemSummary summary;
	summary.baseItemId = static_cast<uint16_t>(std::distance(items.begin(), amulet));
	summary.equipType.clear(); // Compatibility must not depend on a fragile display label.
	EXPECT_TRUE(IsItemCompatibleWithEquipmentSlot(summary, profile, 3));
	EXPECT_FALSE(IsItemCompatibleWithEquipmentSlot(summary, profile, 1));
	EXPECT_FALSE(IsItemCompatibleWithEquipmentSlot(summary, profile, 2));
}

TEST_F(D1HellforgeItemTest, MagicCatalogGenerationIsDeterministicAndGameNative)
{
	ItemGenerationOptions options;
	options.seed = 0x13572468;
	options.quality = ItemQualityChoice::Magic;
	options.level = 30;
	const auto first = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, options);
	const auto second = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, options);
	ASSERT_TRUE(first.has_value()) << first.error();
	ASSERT_TRUE(second.has_value()) << second.error();
	EXPECT_EQ(first->magicalQuality, devilution::ITEM_QUALITY_MAGIC);
	EXPECT_EQ(first->packedBytes, second->packedBytes);
}

TEST_F(D1HellforgeItemTest, CompatibleNamedPrefixCanBeGeneratedDeterministically)
{
	const auto catalog = GetItemCatalog(Game::Hellfire);
	const auto base = std::find_if(catalog.begin(), catalog.end(), [](const auto &item) { return item.name == "Bastard Sword"; });
	ASSERT_NE(base, catalog.end());
	const auto choices = GetItemGenerationChoices(base->baseItemId, Game::Hellfire, 30, true);
	ASSERT_FALSE(choices.prefixes.empty());
	const auto prefix = std::find(choices.prefixes.begin(), choices.prefixes.end(), "Mithril");
	ASSERT_NE(prefix, choices.prefixes.end());
	ItemGenerationOptions options;
	options.seed = 0x24681357;
	options.quality = ItemQualityChoice::Magic;
	options.level = 30;
	options.prefixName = *prefix;
	const auto first = CreateCatalogItem(base->baseItemId, Game::Hellfire, options);
	const auto second = CreateCatalogItem(base->baseItemId, Game::Hellfire, options);
	ASSERT_TRUE(first.has_value()) << first.error();
	ASSERT_TRUE(second.has_value()) << second.error();
	EXPECT_TRUE(first->displayName.starts_with(options.prefixName + " "));
	EXPECT_EQ(first->packedBytes, second->packedBytes);
}

TEST_F(D1HellforgeItemTest, NamedUniqueSelectorCreatesRequestedGameUnique)
{
	uint16_t baseItemId = 0;
	std::string uniqueName;
	for (const auto &catalogItem : GetItemCatalog(Game::Hellfire)) {
		const auto choices = GetItemGenerationChoices(catalogItem.baseItemId, Game::Hellfire, 63, true);
		if (!choices.uniques.empty()) {
			baseItemId = catalogItem.baseItemId;
			uniqueName = choices.uniques.front();
			break;
		}
	}
	ASSERT_FALSE(uniqueName.empty());
	ItemGenerationOptions options;
	options.seed = 0x10293847;
	options.quality = ItemQualityChoice::Unique;
	options.level = 63;
	options.uniqueName = uniqueName;
	const auto item = CreateCatalogItem(baseItemId, Game::Hellfire, options);
	ASSERT_TRUE(item.has_value()) << item.error();
	EXPECT_EQ(item->magicalQuality, devilution::ITEM_QUALITY_UNIQUE);
	EXPECT_EQ(item->displayName, uniqueName);
}

TEST_F(D1HellforgeItemTest, EditWorkshopUsesWorkingCopyAndCanReset)
{
	const auto original = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, { 1234 });
	ASSERT_TRUE(original.has_value()) << original.error();
	auto model = MakeInventoryWorkshopModel(WorkshopMode::Edit, Game::Hellfire, 30, *original, true);
	model.workingItem.seed = 9876;
	model.dirty = true;
	EXPECT_EQ(original->seed, 1234U);
	ResetWorkshopModel(model);
	EXPECT_EQ(model.workingItem.packedBytes, original->packedBytes);
	EXPECT_FALSE(model.dirty);
}

TEST_F(D1HellforgeItemTest, EditWorkshopRecoversRecognizedAffixAndStoredLevel)
{
	const auto catalog = GetItemCatalog(Game::Hellfire);
	const auto base = std::find_if(catalog.begin(), catalog.end(), [](const auto &item) { return item.name == "Bastard Sword"; });
	ASSERT_NE(base, catalog.end());
	ItemGenerationOptions options;
	options.seed = 0xCAFEBABE;
	options.level = 30;
	options.quality = ItemQualityChoice::Magic;
	options.prefixName = "Mithril";
	const auto item = CreateCatalogItem(base->baseItemId, Game::Hellfire, options);
	ASSERT_TRUE(item.has_value()) << item.error();
	const auto model = MakeInventoryWorkshopModel(WorkshopMode::Edit, Game::Hellfire, 1, *item, true);
	EXPECT_EQ(model.generation.prefixName, "Mithril") << item->displayName << " / " << item->baseName << " / " << item->baseItemId;
	EXPECT_EQ(model.generation.level, 30);
}

TEST_F(D1HellforgeItemTest, WorkshopAffixesFollowDevilutionXReconstructionInsteadOfLegacyStoredName)
{
	const auto catalog = GetItemCatalog(Game::Hellfire);
	const auto base = std::find_if(catalog.begin(), catalog.end(), [](const auto &item) { return item.name == "Bastard Sword"; });
	ASSERT_NE(base, catalog.end());
	ItemGenerationOptions options;
	options.seed = 0x10203040;
	options.level = 30;
	options.quality = ItemQualityChoice::Magic;
	options.prefixName = "Mithril";
	auto item = CreateCatalogItem(base->baseItemId, Game::Hellfire, options);
	ASSERT_TRUE(item.has_value()) << item.error();
	item->storedActiveGameName = "Iron Bastard Sword of the sky";
	item->displayName = item->storedActiveGameName;
	item->activeGameIdentityDiffers = true;
	const auto model = MakeInventoryWorkshopModel(WorkshopMode::Edit, Game::Hellfire, 30, *item, true);
	EXPECT_EQ(model.generation.prefixName, "Mithril");
	EXPECT_TRUE(model.generation.suffixName.empty());
}

TEST_F(D1HellforgeItemTest, EditWorkshopKeepsRecognizedAffixesEvenWhenStoredLevelIsTooLow)
{
	const auto catalog = GetItemCatalog(Game::Hellfire);
	const auto base = std::find_if(catalog.begin(), catalog.end(), [](const auto &item) { return item.name == "Great Sword"; });
	ASSERT_NE(base, catalog.end());
	CharacterSummary::PackedItemSummary item;
	item.baseItemId = base->baseItemId;
	item.baseName = "Great Sword";
	item.displayName = "Iron Great Sword of the sky";
	item.magicalQuality = 1;
	item.packedBytes[4] = std::byte { 1 };
	const auto model = MakeInventoryWorkshopModel(WorkshopMode::Edit, Game::Hellfire, 1, item, true);
	EXPECT_EQ(model.generation.level, 1);
	EXPECT_EQ(model.generation.prefixName, "Iron");
	EXPECT_EQ(model.generation.suffixName, "the sky");
}

TEST_F(D1HellforgeItemTest, UniqueQualityDisablesBasesWithoutCompatibleUnique)
{
	auto model = MakeCreateWorkshopModel(Game::Hellfire, 30, true);
	model.generation.quality = ItemQualityChoice::Unique;
	const auto constraints = VanillaItemRules::Evaluate(model);
	EXPECT_TRUE(std::any_of(constraints.baseItems.begin(), constraints.baseItems.end(), [](const auto &option) {
		return option.status == VanillaOptionStatus::Invalid && option.reason == "No compatible unique for this base item";
	}));
}

TEST_F(D1HellforgeItemTest, ReforgeSkillOnlyChangesCentralSearchDepth)
{
	EXPECT_EQ(ReforgeSearchBudget(ReforgeSkill::Novice), 512U);
	EXPECT_EQ(ReforgeSearchBudget(ReforgeSkill::Competent), 4096U);
	EXPECT_EQ(ReforgeSearchBudget(ReforgeSkill::Master), 32768U);
}

TEST_F(D1HellforgeItemTest, ExactSeedGenerationUsesOneNativeGenerationInDiabloAndHellfire)
{
	for (const Game game : { Game::Diablo, Game::Hellfire }) {
		SCOPED_TRACE(game == Game::Diablo ? "Diablo" : "Hellfire");
		ItemGenerationOptions options;
		options.seed = 0x12345678;
		options.quality = ItemQualityChoice::Normal;
		const auto generated = GenerateExactSeedNative(devilution::IDI_SHORTSTAFF, GetDevXContentProfile(game), options);
		ASSERT_TRUE(generated.has_value()) << generated.error();
		EXPECT_EQ(generated->generationCalls, 1U);
		EXPECT_EQ(generated->item._iSeed, options.seed);
	}
}

TEST_F(D1HellforgeItemTest, ReforgeReturnsRankedReproducibleTopThree)
{
	const auto original = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, { 4321 });
	ASSERT_TRUE(original.has_value()) << original.error();
	ReforgeRequest request;
	request.desired = MakeInventoryWorkshopModel(WorkshopMode::Reforge, Game::Hellfire, 30, *original, true);
	request.skill = ReforgeSkill::Novice;
	request.lockBase = true;
	request.lockQuality = true;
	const auto result = SearchReforgeCandidates(request);
	ASSERT_TRUE(result.has_value()) << result.error();
	ASSERT_FALSE(result->topCandidates.empty());
	EXPECT_LE(result->topCandidates.size(), 3U);
	EXPECT_TRUE(std::is_sorted(result->topCandidates.begin(), result->topCandidates.end(), [](const auto &a, const auto &b) { return a.overallScore > b.overallScore; }));
	for (const auto &candidate : result->topCandidates) {
		const auto recreated = SummarizeImportedItem(candidate.item.packedBytes, Game::Hellfire);
		ASSERT_TRUE(recreated.has_value()) << recreated.error();
		EXPECT_EQ(recreated->packedBytes, candidate.item.packedBytes);
	}
}

TEST_F(D1HellforgeItemTest, FastAndCompatibleReforgeEnginesMaterializeValidDiabloAndHellfireWinners)
{
	for (const Game game : { Game::Diablo, Game::Hellfire }) {
		SCOPED_TRACE(game == Game::Diablo ? "Diablo" : "Hellfire");
		const auto original = CreateCatalogItem(devilution::IDI_SHORTSTAFF, game, { 9876 });
		ASSERT_TRUE(original.has_value()) << original.error();
		for (const ReforgeEngine engine : { ReforgeEngine::Fast, ReforgeEngine::Compatible }) {
			SCOPED_TRACE(engine == ReforgeEngine::Fast ? "Fast" : "Compatible");
			ReforgeRequest request;
			request.desired = MakeInventoryWorkshopModel(WorkshopMode::Reforge, game, 20, *original, true);
			request.skill = ReforgeSkill::Novice;
			request.lockBase = true;
			request.lockQuality = true;
			request.engine = engine;
			const auto result = SearchReforgeCandidates(request);
			ASSERT_TRUE(result.has_value()) << result.error();
			ASSERT_FALSE(result->topCandidates.empty());
			EXPECT_EQ(result->candidatesExamined, ReforgeSearchBudget(ReforgeSkill::Novice));
			EXPECT_GT(result->elapsedMilliseconds, 0);
			if (engine == ReforgeEngine::Fast) EXPECT_EQ(result->generationCalls, result->candidatesExamined);
			for (const auto &candidate : result->topCandidates) {
				const auto recreated = SummarizeImportedItem(candidate.item.packedBytes, game);
				ASSERT_TRUE(recreated.has_value()) << recreated.error();
				EXPECT_EQ(recreated->packedBytes, candidate.item.packedBytes);
				EXPECT_EQ(recreated->seed, candidate.seed);
			}
		}
	}
}

TEST_F(D1HellforgeItemTest, ReforgeProgressCanCancelWithoutChangingTheRequestedItem)
{
	const auto original = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, { 2468 });
	ASSERT_TRUE(original.has_value()) << original.error();
	ReforgeRequest request;
	request.desired = MakeInventoryWorkshopModel(WorkshopMode::Reforge, Game::Hellfire, 30, *original, true);
	request.skill = ReforgeSkill::Master;
	const auto originalBytes = request.desired.workingItem.packedBytes;
	uint32_t lastProgress = 0;
	const auto result = SearchReforgeCandidates(request, [&lastProgress](const ReforgeProgress &progress) {
		lastProgress = progress.candidatesExamined;
		return progress.candidatesExamined < 64;
	});
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), "Reforge search cancelled");
	EXPECT_GE(lastProgress, 64U);
	EXPECT_EQ(request.desired.workingItem.packedBytes, originalBytes);
}

TEST_F(D1HellforgeItemTest, SeedRegenerationPreservesPackedCreationIdentity)
{
	const auto original = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, { 100 });
	ASSERT_TRUE(original.has_value()) << original.error();
	const auto regenerated = RegenerateItemWithSeed(*original, Game::Hellfire, 200);
	ASSERT_TRUE(regenerated.has_value()) << regenerated.error();
	EXPECT_EQ(regenerated->seed, 200U);
	EXPECT_EQ(regenerated->baseItemId, original->baseItemId);
	EXPECT_EQ(regenerated->packedBytes[4], original->packedBytes[4]);
	EXPECT_EQ(regenerated->packedBytes[5], original->packedBytes[5]);
}

TEST_F(D1HellforgeItemTest, LegacyHellfireExportReimportsSemantically)
{
	const auto item = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Hellfire, { 0x10203040 });
	ASSERT_TRUE(item.has_value()) << item.error();
	const auto path = std::filesystem::temp_directory_path() / "d1hellforge-roundtrip-test.hif";
	const auto exported = ExportLegacyItem(path, *item, Game::Hellfire);
	ASSERT_TRUE(exported.has_value()) << exported.error();
	const auto imported = ImportItemFile(path);
	std::error_code error;
	std::filesystem::remove(path, error);
	ASSERT_TRUE(imported.has_value()) << imported.error();
	const auto summary = SummarizeImportedItem(imported->packedItem, Game::Hellfire);
	ASSERT_TRUE(summary.has_value()) << summary.error();
	EXPECT_EQ(summary->seed, item->seed);
	EXPECT_EQ(summary->baseItemId, item->baseItemId);
	EXPECT_EQ(summary->durability, item->durability);
	EXPECT_EQ(summary->maxDurability, item->maxDurability);
	EXPECT_EQ(summary->charges, item->charges);
	EXPECT_EQ(summary->maxCharges, item->maxCharges);
}

TEST_F(D1HellforgeItemTest, LegacyDiabloExportReimportsSemantically)
{
	const auto item = CreateCatalogItem(devilution::IDI_GOLD, Game::Diablo, { 0x90ABCDEF });
	ASSERT_TRUE(item.has_value()) << item.error();
	const auto path = std::filesystem::temp_directory_path() / "d1hellforge-roundtrip-test.itm";
	const auto exported = ExportLegacyItem(path, *item, Game::Diablo);
	ASSERT_TRUE(exported.has_value()) << exported.error();
	const auto imported = ImportItemFile(path);
	std::error_code error;
	std::filesystem::remove(path, error);
	ASSERT_TRUE(imported.has_value()) << imported.error();
	const auto summary = SummarizeImportedItem(imported->packedItem, Game::Diablo);
	ASSERT_TRUE(summary.has_value()) << summary.error();
	EXPECT_EQ(summary->seed, item->seed);
	EXPECT_EQ(summary->baseItemId, item->baseItemId);
	EXPECT_EQ(summary->durability, item->durability);
	EXPECT_EQ(summary->maxDurability, item->maxDurability);
}

TEST_F(D1HellforgeItemTest, DiabloToHellfireConversionRoundTripsBaseIdentity)
{
	const auto diablo = CreateCatalogItem(devilution::IDI_SHORTSTAFF, Game::Diablo, { 0x55667788 });
	ASSERT_TRUE(diablo.has_value()) << diablo.error();
	const auto hellfireBytes = ConvertPackedItem(diablo->packedBytes, Game::Diablo, Game::Hellfire);
	ASSERT_TRUE(hellfireBytes.has_value()) << hellfireBytes.error();
	const auto hellfire = SummarizeImportedItem(*hellfireBytes, Game::Hellfire);
	ASSERT_TRUE(hellfire.has_value()) << hellfire.error();
	EXPECT_EQ(hellfire->baseName, diablo->baseName);
	EXPECT_EQ(hellfire->seed, diablo->seed);
}

TEST_F(D1HellforgeItemTest, ConvertedLegacyPremiumItemUsesHellfireReconstruction)
{
	devilution::PackedItemBytes diablo {};
	const uint32_t seed = 2063309567;
	const uint16_t createInfo = 2071;
	const uint16_t diabloIndex = 124;
	std::memcpy(diablo.data(), &seed, sizeof(seed));
	std::memcpy(diablo.data() + 4, &createInfo, sizeof(createInfo));
	std::memcpy(diablo.data() + 6, &diabloIndex, sizeof(diabloIndex));
	diablo[8] = std::byte { 3 }; // Identified magic item.
	diablo[9] = std::byte { 100 };
	diablo[10] = std::byte { 100 };
	const auto converted = ConvertPackedItem(diablo, Game::Diablo, Game::Hellfire);
	ASSERT_TRUE(converted.has_value()) << converted.error();
	const auto summary = SummarizeImportedItem(*converted, Game::Hellfire);
	ASSERT_TRUE(summary.has_value()) << summary.error();
	EXPECT_EQ(summary->baseName, "War Staff");
	EXPECT_EQ(summary->displayName, "Cobalt War Staff of speed");
}

} // namespace
} // namespace d1hellforge
