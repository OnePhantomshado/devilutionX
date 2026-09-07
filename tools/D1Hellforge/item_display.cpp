#include "item_display.hpp"

#include <cstdlib>
#include <format>
#include <sstream>

#include "spells.h"
#include "tables/itemdat.h"
#include "tables/spelldat.h"

namespace d1hellforge {
namespace {

const char *QualityName(devilution::item_quality quality)
{
	if (quality == devilution::ITEM_QUALITY_UNIQUE) return "Unique";
	if (quality == devilution::ITEM_QUALITY_MAGIC) return "Magic";
	return "Normal";
}

void Add(ItemDisplayModel &result, ItemDisplaySection section, ItemDisplayTone tone, std::string text)
{
	result.lines.push_back({ section, tone, std::move(text) });
}

void AddSigned(ItemDisplayModel &result, ItemDisplaySection section, int value, std::string_view suffix,
    bool positiveIsGood = true)
{
	if (value == 0) return;
	const bool beneficial = positiveIsGood ? value > 0 : value < 0;
	const auto tone = beneficial ? ItemDisplayTone::Positive : ItemDisplayTone::Negative;
	Add(result, section, tone, std::format("{}{}{}", tone == ItemDisplayTone::Negative ? "☠ " : "", value > 0 ? "+" : "", value) + std::string(suffix));
}

void AddFlag(ItemDisplayModel &result, const devilution::Item &item, devilution::ItemSpecialEffect flag,
    std::string_view text, ItemDisplayTone tone = ItemDisplayTone::Positive)
{
	if (devilution::HasAnyOf(item._iFlags, flag))
		Add(result, ItemDisplaySection::SpecialEffects, tone, std::string(tone == ItemDisplayTone::Negative ? "☠ " : "") + std::string(text));
}

void AddHellfireFlag(ItemDisplayModel &result, const devilution::Item &item, devilution::ItemSpecialEffectHf flag,
    std::string_view text, ItemDisplayTone tone = ItemDisplayTone::Positive)
{
	if (devilution::HasAnyOf(item._iDamAcFlags, flag))
		Add(result, ItemDisplaySection::SpecialEffects, tone, std::string(tone == ItemDisplayTone::Negative ? "☠ " : "") + std::string(text));
}

} // namespace

ItemDisplayModel BuildItemDisplay(const devilution::Item &item, ItemDisplayMode mode, const ContentProfile &profile)
{
	ItemDisplayModel result;
	result.displayName = item.IDidx == devilution::IDI_EAR ? item._iName
	    : (item._iIdentified || item._iMagical == devilution::ITEM_QUALITY_NORMAL ? item._iIName : item._iName);
	Add(result, ItemDisplaySection::Identity, ItemDisplayTone::Neutral, result.displayName);
	Add(result, ItemDisplaySection::Identity, ItemDisplayTone::Metadata,
	    std::format("Quality: {} | {}", QualityName(item._iMagical), item._iIdentified ? "Identified" : "Unidentified"));

	const bool compactTooltip = mode == ItemDisplayMode::Compact || mode == ItemDisplayMode::GameTooltip;
	if (item._iClass == devilution::ICLASS_WEAPON)
		Add(result, ItemDisplaySection::Core, ItemDisplayTone::Neutral, compactTooltip && item._iMaxDur != 0
		        ? std::format("Damage: {}-{} | Durability: {}/{}", item._iMinDam, item._iMaxDam, item._iDurability, item._iMaxDur)
		        : std::format("Damage: {}-{}", item._iMinDam, item._iMaxDam));
	else if (item._iClass == devilution::ICLASS_ARMOR)
		Add(result, ItemDisplaySection::Core, ItemDisplayTone::Neutral, compactTooltip && item._iMaxDur != 0
		        ? std::format("Armor: {} | Durability: {}/{}", item._iAC, item._iDurability, item._iMaxDur)
		        : std::format("Armor Class: {}", item._iAC));
	if (!compactTooltip && item._iMaxDur == DUR_INDESTRUCTIBLE)
		Add(result, ItemDisplaySection::Core, ItemDisplayTone::Positive, "Indestructible");
	else if (!compactTooltip && item._iMaxDur != 0)
		Add(result, ItemDisplaySection::Core, ItemDisplayTone::Neutral, std::format("Durability: {}/{}", item._iDurability, item._iMaxDur));
	else if (compactTooltip && item._iClass != devilution::ICLASS_WEAPON && item._iClass != devilution::ICLASS_ARMOR && item._iMaxDur != 0)
		Add(result, ItemDisplaySection::Core, ItemDisplayTone::Neutral, std::format("Durability: {}/{}", item._iDurability, item._iMaxDur));
	if (item._iMaxCharges != 0) {
		std::string text = std::format("Charges: {}/{}", item._iCharges, item._iMaxCharges);
		const int spell = static_cast<int>(item._iSpell);
		if (spell > static_cast<int>(devilution::SpellID::Null) && static_cast<std::size_t>(spell) < devilution::SpellsData.size())
			text += std::format(" | Spell: {}", devilution::SpellsData[spell].sNameText);
		Add(result, ItemDisplaySection::Utility, ItemDisplayTone::Neutral, std::move(text));
	}
	if (item._iMinStr != 0 || item._iMinMag != 0 || item._iMinDex != 0) {
		std::ostringstream text; text << "Required:";
		if (item._iMinStr != 0) text << ' ' << static_cast<int>(item._iMinStr) << " Str";
		if (item._iMinMag != 0) text << ' ' << static_cast<int>(item._iMinMag) << " Mag";
		if (item._iMinDex != 0) text << ' ' << static_cast<int>(item._iMinDex) << " Dex";
		Add(result, ItemDisplaySection::Requirements, ItemDisplayTone::Neutral, text.str());
	}

	if (mode == ItemDisplayMode::Detailed || mode == ItemDisplayMode::DetailedWithMetadata) {
		AddSigned(result, ItemDisplaySection::Offense, item._iPLDam, "% Damage");
		AddSigned(result, ItemDisplaySection::Offense, item._iPLToHit, "% Chance to Hit");
		AddSigned(result, ItemDisplaySection::Offense, item._iPLDamMod, " points to Damage");
		AddSigned(result, ItemDisplaySection::Offense, item._iPLEnAc, " Enemy Armor Reduction");
		AddSigned(result, ItemDisplaySection::Defense, item._iPLAC, "% Armor");
		AddSigned(result, ItemDisplaySection::Defense, item._iPLGetHit, " Damage From Enemies", false);
		AddSigned(result, ItemDisplaySection::Defense, item._iPLFR, "% Resist Fire");
		AddSigned(result, ItemDisplaySection::Defense, item._iPLLR, "% Resist Lightning");
		AddSigned(result, ItemDisplaySection::Defense, item._iPLMR, "% Resist Magic");
		if (item._iFMinDam != 0 || item._iFMaxDam != 0) Add(result, ItemDisplaySection::Elemental, ItemDisplayTone::Positive, std::format("Fire Hit Damage: {}-{}", item._iFMinDam, item._iFMaxDam));
		if (item._iLMinDam != 0 || item._iLMaxDam != 0) Add(result, ItemDisplaySection::Elemental, ItemDisplayTone::Positive, std::format("Lightning Hit Damage: {}-{}", item._iLMinDam, item._iLMaxDam));
		AddSigned(result, ItemDisplaySection::Attributes, item._iPLStr, " to Strength");
		AddSigned(result, ItemDisplaySection::Attributes, item._iPLMag, " to Magic");
		AddSigned(result, ItemDisplaySection::Attributes, item._iPLDex, " to Dexterity");
		AddSigned(result, ItemDisplaySection::Attributes, item._iPLVit, " to Vitality");
		AddSigned(result, ItemDisplaySection::Attributes, item._iPLHP >> 6, " Hit Points");
		AddSigned(result, ItemDisplaySection::Attributes, item._iPLMana >> 6, " Mana");
		AddSigned(result, ItemDisplaySection::Utility, item._iPLLight, " Light Radius");
		AddSigned(result, ItemDisplaySection::Utility, item._iSplLvlAdd, " Spell Levels");

		AddFlag(result, item, devilution::ItemSpecialEffect::QuickAttack, "Quick Attack");
		AddFlag(result, item, devilution::ItemSpecialEffect::FastAttack, "Fast Attack");
		AddFlag(result, item, devilution::ItemSpecialEffect::FasterAttack, "Faster Attack");
		AddFlag(result, item, devilution::ItemSpecialEffect::FastestAttack, "Fastest Attack");
		AddFlag(result, item, devilution::ItemSpecialEffect::FastHitRecovery, "Fast Hit Recovery");
		AddFlag(result, item, devilution::ItemSpecialEffect::FasterHitRecovery, "Faster Hit Recovery");
		AddFlag(result, item, devilution::ItemSpecialEffect::FastestHitRecovery, "Fastest Hit Recovery");
		AddFlag(result, item, devilution::ItemSpecialEffect::FastBlock, "Fast Block");
		AddFlag(result, item, devilution::ItemSpecialEffect::RandomStealLife, "Random Life Steal");
		AddFlag(result, item, devilution::ItemSpecialEffect::RandomArrowVelocity, "Random Arrow Velocity");
		AddFlag(result, item, devilution::ItemSpecialEffect::FireArrows, "Fire Arrows");
		AddFlag(result, item, devilution::ItemSpecialEffect::LightningArrows, "Lightning Arrows");
		AddFlag(result, item, devilution::ItemSpecialEffect::Knockback, "Knockback");
		AddFlag(result, item, devilution::ItemSpecialEffect::MultipleArrows, "Multiple Arrows");
		AddFlag(result, item, devilution::ItemSpecialEffect::HalfTrapDamage, "Half Trap Damage");
		AddFlag(result, item, devilution::ItemSpecialEffect::TripleDemonDamage, "Triple Damage to Demons");
		AddFlag(result, item, devilution::ItemSpecialEffect::Thorns, "Attacker Takes Damage");
		AddFlag(result, item, devilution::ItemSpecialEffect::StealMana3, "Steal 3% Mana");
		AddFlag(result, item, devilution::ItemSpecialEffect::StealMana5, "Steal 5% Mana");
		AddFlag(result, item, devilution::ItemSpecialEffect::StealLife3, "Steal 3% Life");
		AddFlag(result, item, devilution::ItemSpecialEffect::StealLife5, "Steal 5% Life");
		AddFlag(result, item, devilution::ItemSpecialEffect::DrainLife, "Drain Life", ItemDisplayTone::Negative);
		AddFlag(result, item, devilution::ItemSpecialEffect::NoMana, "No Mana", ItemDisplayTone::Negative);
		AddFlag(result, item, devilution::ItemSpecialEffect::ZeroResistance, "Zero All Resistances", ItemDisplayTone::Negative);
		if (profile.baseMode == Game::Hellfire) {
			AddHellfireFlag(result, item, devilution::ItemSpecialEffectHf::Devastation, "Devastation");
			AddHellfireFlag(result, item, devilution::ItemSpecialEffectHf::Decay, "Decay", ItemDisplayTone::Negative);
			AddHellfireFlag(result, item, devilution::ItemSpecialEffectHf::Peril, "Peril", ItemDisplayTone::Negative);
			AddHellfireFlag(result, item, devilution::ItemSpecialEffectHf::Jesters, "Jester's Effect");
			AddHellfireFlag(result, item, devilution::ItemSpecialEffectHf::Doppelganger, "Doppelganger");
			AddHellfireFlag(result, item, devilution::ItemSpecialEffectHf::ACAgainstDemons, "Armor Against Demons");
			AddHellfireFlag(result, item, devilution::ItemSpecialEffectHf::ACAgainstUndead, "Armor Against Undead");
		}
	}

	if (item._iPrePower != devilution::IPL_INVALID) Add(result, ItemDisplaySection::Affixes, ItemDisplayTone::Neutral, std::string(devilution::PrintItemPower(item._iPrePower, item).str()));
	if (item._iSufPower != devilution::IPL_INVALID) Add(result, ItemDisplaySection::Affixes, ItemDisplayTone::Neutral, std::string(devilution::PrintItemPower(item._iSufPower, item).str()));
	Add(result, ItemDisplaySection::Value, ItemDisplayTone::Neutral,
	    item.IDidx == devilution::IDI_GOLD ? std::format("Gold: {}", item._ivalue) : std::format("Value: {}", item._iIvalue));
	if (mode == ItemDisplayMode::DetailedWithMetadata) {
		Add(result, ItemDisplaySection::Metadata, ItemDisplayTone::Metadata, std::format("Seed: {} | Create Info: 0x{:04X}", item._iSeed, item._iCreateInfo));
		Add(result, ItemDisplaySection::Metadata, ItemDisplayTone::Metadata, std::format("Content: {}", profile.displayLabel.empty() ? (profile.baseMode == Game::Hellfire ? "Hellfire" : "Diablo") : profile.displayLabel));
		if (!profile.initialized) Add(result, ItemDisplaySection::Warnings, ItemDisplayTone::Warning, "Warning: active content profile is not verified");
	}
	return result;
}

std::vector<std::string> RenderItemDisplayPlainText(const ItemDisplayModel &display, bool includeName)
{
	std::vector<std::string> result;
	for (const auto &line : display.lines) {
		if (!includeName && line.section == ItemDisplaySection::Identity && line.text == display.displayName) continue;
		result.push_back(line.text);
	}
	return result;
}

} // namespace d1hellforge
