#pragma once

#include <expected>
#include <string>
#include <vector>

#include "save_document.hpp"
#include "items.h"

namespace d1hellforge {

#ifdef _WIN32
using NativeWindow = void *;
#endif

struct AdvancedItemEditorModel {
	devilution::Item original;
	devilution::Item staged;
	std::vector<std::byte> originalRecord;
	Game game = Game::Diablo;
	bool enforceVanillaRules = true;
	bool reconstructedFromPacked = false;
};

std::expected<AdvancedItemEditorModel, std::string> MakeAdvancedItemEditorModel(
	const CharacterSummary::PackedItemSummary &summary, Game game, bool enforceVanillaRules);
std::vector<std::string> ValidateAdvancedItem(const AdvancedItemEditorModel &model);
std::vector<std::string> BuildAdvancedItemPreview(const AdvancedItemEditorModel &model);
void ResetAdvancedItemEditorModel(AdvancedItemEditorModel &model);
std::expected<CharacterSummary::PackedItemSummary, std::string> ApplyAdvancedItemEditorModel(
	const AdvancedItemEditorModel &model, CharacterSummary::PackedItemSummary summary);
std::expected<void, std::string> RefreshSummaryFromFullItemRecord(CharacterSummary::PackedItemSummary &summary, Game game);
#ifdef _WIN32
std::optional<CharacterSummary::PackedItemSummary> ShowAdvancedItemEditor(NativeWindow owner,
	const CharacterSummary::PackedItemSummary &summary, Game game, bool enforceVanillaRules);
#endif

} // namespace d1hellforge
