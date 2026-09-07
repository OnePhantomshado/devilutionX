#pragma once

#include <string>
#include <vector>

#include "content_profile.hpp"
#include "items.h"

namespace d1hellforge {

enum class ItemDisplayMode {
	Compact,
	GameTooltip,
	Detailed,
	DetailedWithMetadata,
};

enum class ItemDisplayTone {
	Neutral,
	Positive,
	Negative,
	Warning,
	Metadata,
};

enum class ItemDisplaySection {
	Identity,
	Core,
	Offense,
	Defense,
	Elemental,
	Attributes,
	Requirements,
	Utility,
	SpecialEffects,
	Affixes,
	Value,
	Metadata,
	Warnings,
};

struct ItemDisplayLine {
	ItemDisplaySection section = ItemDisplaySection::Core;
	ItemDisplayTone tone = ItemDisplayTone::Neutral;
	std::string text;
};

struct ItemDisplayModel {
	std::string displayName;
	std::vector<ItemDisplayLine> lines;
};

ItemDisplayModel BuildItemDisplay(const devilution::Item &item, ItemDisplayMode mode, const ContentProfile &profile);
std::vector<std::string> RenderItemDisplayPlainText(const ItemDisplayModel &display, bool includeName = true);

} // namespace d1hellforge
