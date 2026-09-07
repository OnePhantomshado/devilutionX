#include "item_workshop.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <format>

namespace d1hellforge {
namespace {

ItemQualityChoice QualityOf(const CharacterSummary::PackedItemSummary &item)
{
	if (item.magicalQuality == 2) return ItemQualityChoice::Unique;
	if (item.magicalQuality == 1) return ItemQualityChoice::Magic;
	return ItemQualityChoice::Normal;
}

VanillaOptionState State(std::string value, bool valid, bool selected, std::string reason = {})
{
	return { std::move(value), selected ? VanillaOptionStatus::Selected : valid ? VanillaOptionStatus::Valid : VanillaOptionStatus::Invalid, valid ? std::string {} : std::move(reason) };
}

} // namespace

WorkshopItemModel MakeCreateWorkshopModel(Game game, uint8_t level, bool enforceVanillaRules)
{
	return MakeCreateWorkshopModel(GetDevXContentProfile(game), level, enforceVanillaRules);
}

WorkshopItemModel MakeCreateWorkshopModel(const ContentProfile &profile, uint8_t level, bool enforceVanillaRules)
{
	WorkshopItemModel model;
	model.mode = WorkshopMode::Create;
	model.game = profile.baseMode;
	model.contentProfile = profile;
	model.enforceVanillaRules = enforceVanillaRules;
	model.generation.level = std::clamp<uint8_t>(level, 1, 63);
	model.generation.seed = static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	return model;
}

WorkshopItemModel MakeInventoryWorkshopModel(WorkshopMode mode, Game game, uint8_t level,
	const CharacterSummary::PackedItemSummary &item, bool enforceVanillaRules)
{
	return MakeInventoryWorkshopModel(mode, GetDevXContentProfile(game), level, item, enforceVanillaRules);
}

WorkshopItemModel MakeInventoryWorkshopModel(WorkshopMode mode, const ContentProfile &profile, uint8_t level,
	const CharacterSummary::PackedItemSummary &item, bool enforceVanillaRules)
{
	WorkshopItemModel model = MakeCreateWorkshopModel(profile, level, enforceVanillaRules);
	model.mode = mode;
	model.workingItem = item;
	model.originalItem = item;
	model.generation.seed = item.seed;
	model.generation.quality = QualityOf(item);
	const uint16_t createInfo = static_cast<uint16_t>(std::to_integer<uint8_t>(item.packedBytes[4]))
	    | static_cast<uint16_t>(std::to_integer<uint8_t>(item.packedBytes[5])) << 8;
	const uint8_t storedLevel = static_cast<uint8_t>(createInfo & 0x3F);
	if (storedLevel != 0) model.generation.level = storedLevel;
	model.generation.onlyGood = (createInfo & (1U << 6)) != 0;
	ItemGenerationChoices choices;
	for (uint8_t candidateLevel = 1; candidateLevel <= 63; ++candidateLevel) {
		const auto atLevel = GetItemGenerationChoices(item.baseItemId, profile, candidateLevel, false);
		auto merge = [](auto &target, const auto &source) {
			for (const auto &value : source)
				if (std::find(target.begin(), target.end(), value) == target.end()) target.push_back(value);
		};
		merge(choices.prefixes, atLevel.prefixes);
		merge(choices.suffixes, atLevel.suffixes);
		merge(choices.uniques, atLevel.uniques);
	}
	const std::string &name = item.reconstructedDisplayName.empty() ? item.displayName : item.reconstructedDisplayName;
	if (model.generation.quality == ItemQualityChoice::Unique) {
		if (std::find(choices.uniques.begin(), choices.uniques.end(), name) != choices.uniques.end())
			model.generation.uniqueName = name;
	} else if (model.generation.quality == ItemQualityChoice::Magic) {
		for (const auto &prefix : choices.prefixes) {
			if (name.starts_with(prefix + " ") && prefix.size() > model.generation.prefixName.size())
				model.generation.prefixName = prefix;
		}
		for (const auto &suffix : choices.suffixes) {
			if (name.ends_with(" of " + suffix) && suffix.size() > model.generation.suffixName.size())
				model.generation.suffixName = suffix;
		}
	}
	return model;
}

void ResetWorkshopModel(WorkshopItemModel &model)
{
	if (!model.originalItem.has_value()) return;
	model.workingItem = *model.originalItem;
	model.generation.seed = model.workingItem.seed;
	model.generation.quality = QualityOf(model.workingItem);
	model.generation.prefixName.clear();
	model.generation.suffixName.clear();
	model.generation.uniqueName.clear();
	model.dirty = false;
}

WorkshopValidationState VanillaItemRules::Validate(const WorkshopItemModel &model)
{
	WorkshopValidationState result;
	if (model.game != model.contentProfile.baseMode)
		result.errors.emplace_back("The Workshop save mode and content profile base mode do not match");
	if (const auto validProfile = ValidateDevXContentProfile(model.contentProfile); !validProfile.has_value())
		result.errors.push_back(validProfile.error());
	if (model.generation.level < 1 || model.generation.level > 63)
		result.errors.emplace_back("Generation level must be from 1 to 63");
	const auto catalog = GetItemCatalog(model.contentProfile);
	const bool baseValid = std::any_of(catalog.begin(), catalog.end(), [&model](const auto &entry) { return entry.baseItemId == model.workingItem.baseItemId; });
	if (!baseValid)
		result.errors.emplace_back(model.game == Game::Diablo ? "Base item is not available in Diablo" : "Base item is not available in Hellfire");
	const auto choices = GetItemGenerationChoices(model.workingItem.baseItemId, model.contentProfile, model.generation.level, model.generation.onlyGood);
	auto requireChoice = [&result](const std::string &selected, const auto &choices, std::string message) {
		if (!selected.empty() && std::find(choices.begin(), choices.end(), selected) == choices.end()) result.errors.push_back(std::move(message));
	};
	requireChoice(model.generation.prefixName, choices.prefixes, "Prefix is invalid for this base item, game, or generation level");
	requireChoice(model.generation.suffixName, choices.suffixes, "Suffix is invalid for this base item, game, or generation level");
	requireChoice(model.generation.uniqueName, choices.uniques, "Unique is invalid for this base item, game, or generation level");
	if (model.generation.quality != ItemQualityChoice::Magic && (!model.generation.prefixName.empty() || !model.generation.suffixName.empty()))
		result.errors.emplace_back("Prefixes and suffixes require Magic quality");
	if (model.generation.quality != ItemQualityChoice::Unique && !model.generation.uniqueName.empty())
		result.errors.emplace_back("A named unique requires Unique quality");
	result.valid = result.errors.empty();
	result.reproducible = !model.workingItem.activeGameIdentityDiffers;
	if (!result.reproducible) result.warnings.emplace_back("Compact and active-game item identities disagree");
	return result;
}

VanillaConstraintState VanillaItemRules::Evaluate(const WorkshopItemModel &model)
{
	VanillaConstraintState result;
	const auto catalog = GetItemCatalog(model.contentProfile);
	for (const auto &entry : catalog) {
		bool valid = true;
		std::string reason;
		const auto choices = GetItemGenerationChoices(entry.baseItemId, model.contentProfile, model.generation.level, model.generation.onlyGood);
		if (model.generation.quality == ItemQualityChoice::Unique && choices.uniques.empty()) { valid = false; reason = "No compatible unique for this base item"; }
		if (!model.generation.uniqueName.empty() && std::find(choices.uniques.begin(), choices.uniques.end(), model.generation.uniqueName) == choices.uniques.end()) { valid = false; reason = "Not the selected unique's base item"; }
		if (!model.generation.prefixName.empty() && std::find(choices.prefixes.begin(), choices.prefixes.end(), model.generation.prefixName) == choices.prefixes.end()) { valid = false; reason = "Not valid with the selected prefix"; }
		if (!model.generation.suffixName.empty() && std::find(choices.suffixes.begin(), choices.suffixes.end(), model.generation.suffixName) == choices.suffixes.end()) { valid = false; reason = "Not valid with the selected suffix"; }
		result.baseItems.push_back(State(entry.name, valid, entry.baseItemId == model.workingItem.baseItemId, std::move(reason)));
	}
	const auto choices = GetItemGenerationChoices(model.workingItem.baseItemId, model.contentProfile, model.generation.level, model.generation.onlyGood);
	for (const auto &value : choices.prefixes) result.prefixes.push_back(State(value, true, value == model.generation.prefixName));
	for (const auto &value : choices.suffixes) result.suffixes.push_back(State(value, true, value == model.generation.suffixName));
	for (const auto &value : choices.uniques) result.uniques.push_back(State(value, true, value == model.generation.uniqueName));
	result.validation = Validate(model);
	return result;
}

std::expected<CharacterSummary::PackedItemSummary, std::string> VanillaItemRules::Generate(const WorkshopItemModel &model)
{
	if (model.enforceVanillaRules) {
		const auto validation = Validate(model);
		if (!validation.valid) return std::unexpected(validation.errors.front());
	}
	return CreateCatalogItem(model.workingItem.baseItemId, model.contentProfile, model.generation);
}

uint32_t ReforgeSearchBudget(ReforgeSkill skill)
{
	switch (skill) {
	case ReforgeSkill::Novice: return 512;
	case ReforgeSkill::Competent: return 4096;
	case ReforgeSkill::Master: return 32768;
	}
	return 4096;
}

namespace {

double NumbersInDetails(const CharacterSummary::PackedItemSummary &item, std::string_view wanted)
{
	double total = 0;
	for (const auto &line : item.detailLines) {
		if (!wanted.empty() && line.find(wanted) == std::string::npos) continue;
		for (std::size_t index = 0; index < line.size();) {
			if (!std::isdigit(static_cast<unsigned char>(line[index])) && line[index] != '-') { ++index; continue; }
			char *end = nullptr;
			const long value = std::strtol(line.c_str() + index, &end, 10);
			if (end == line.c_str() + index) { ++index; continue; }
			total += std::max<long>(value, 0);
			index = static_cast<std::size_t>(end - line.c_str());
		}
	}
	return total;
}

double Similarity(const CharacterSummary::PackedItemSummary &candidate, const CharacterSummary::PackedItemSummary &original)
{
	double difference = 0;
	for (std::size_t index = 0; index < candidate.packedBytes.size(); ++index)
		difference += std::abs(static_cast<int>(std::to_integer<uint8_t>(candidate.packedBytes[index])) - static_cast<int>(std::to_integer<uint8_t>(original.packedBytes[index])));
	return std::max(0.0, 100.0 - difference * 100.0 / (255.0 * candidate.packedBytes.size()));
}

double Similarity(std::span<const std::byte> candidate, std::span<const std::byte> original)
{
	double difference = 0;
	for (std::size_t index = 0; index < std::min(candidate.size(), original.size()); ++index)
		difference += std::abs(static_cast<int>(std::to_integer<uint8_t>(candidate[index])) - static_cast<int>(std::to_integer<uint8_t>(original[index])));
	return std::max(0.0, 100.0 - difference * 100.0 / (255.0 * candidate.size()));
}

double ScoreNative(const devilution::Item &item, std::span<const std::byte> packed, const ReforgeRequest &request)
{
	auto positive = [](int value) { return static_cast<double>(std::max(value, 0)); };
	const double damage = positive(item._iMinDam) + positive(item._iMaxDam) + positive(item._iPLDam) + positive(item._iPLDamMod)
	    + positive(item._iPLToHit) + positive(item._iFMinDam) + positive(item._iFMaxDam) + positive(item._iLMinDam) + positive(item._iLMaxDam);
	const double resistances = positive(item._iPLFR) + positive(item._iPLLR) + positive(item._iPLMR);
	const double defense = positive(item._iAC) + positive(item._iPLAC) + resistances + positive(item._iPLEnAc);
	const double attributes = positive(item._iPLStr) + positive(item._iPLMag) + positive(item._iPLDex) + positive(item._iPLVit);
	const double lifeMana = positive(item._iPLHP >> 6) + positive(item._iPLMana >> 6);
	const double all = damage + defense + attributes + lifeMana + positive(item._iMaxDur) + positive(item._iMaxCharges) + positive(item._iIvalue);
	double score = all;
	switch (request.preference) {
	case ReforgePreference::Magic: score = all + damage + defense + attributes + lifeMana; break;
	case ReforgePreference::BaseRoll: score = positive(item._iMaxDur) + positive(item._iAC); break;
	case ReforgePreference::Damage: score = damage * 3 + all * .1; break;
	case ReforgePreference::Defense: score = defense * 3 + all * .1; break;
	case ReforgePreference::Attributes: score = attributes * 4 + all * .1; break;
	case ReforgePreference::Resistances: score = resistances * 5 + all * .1; break;
	case ReforgePreference::LifeMana: score = lifeMana * 5 + all * .1; break;
	case ReforgePreference::ClosestToOriginal:
		return request.desired.originalItem.has_value() ? Similarity(packed, request.desired.originalItem->packedBytes) : 0;
	case ReforgePreference::Balanced: break;
	}
	if (request.preserveClosestRolls && request.desired.originalItem.has_value()) score += Similarity(packed, request.desired.originalItem->packedBytes);
	return 100.0 * score / (score + 250.0);
}

double ScoreCandidate(const CharacterSummary::PackedItemSummary &item, const ReforgeRequest &request)
{
	const double all = NumbersInDetails(item, {});
	const double damage = NumbersInDetails(item, "damage") + NumbersInDetails(item, "Damage") + NumbersInDetails(item, "to hit");
	const double defense = NumbersInDetails(item, "Armor") + NumbersInDetails(item, "Resist") + NumbersInDetails(item, "block") + NumbersInDetails(item, "recovery");
	const double attributes = NumbersInDetails(item, "Str") + NumbersInDetails(item, "Mag") + NumbersInDetails(item, "Dex") + NumbersInDetails(item, "Vit");
	const double lifeMana = NumbersInDetails(item, "life") + NumbersInDetails(item, "mana") + NumbersInDetails(item, "Life") + NumbersInDetails(item, "Mana");
	double score = all;
	switch (request.preference) {
	case ReforgePreference::Magic: score = all + damage + defense + attributes + lifeMana; break;
	case ReforgePreference::BaseRoll: score = item.maxDurability + NumbersInDetails(item, "Armor"); break;
	case ReforgePreference::Damage: score = damage * 3 + all * .1; break;
	case ReforgePreference::Defense: score = defense * 3 + all * .1; break;
	case ReforgePreference::Attributes: score = attributes * 4 + all * .1; break;
	case ReforgePreference::Resistances: score = NumbersInDetails(item, "Resist") * 5 + all * .1; break;
	case ReforgePreference::LifeMana: score = lifeMana * 5 + all * .1; break;
	case ReforgePreference::ClosestToOriginal:
		return request.desired.originalItem.has_value() ? Similarity(item, *request.desired.originalItem) : 0;
	case ReforgePreference::Balanced: break;
	}
	if (request.preserveClosestRolls && request.desired.originalItem.has_value()) score += Similarity(item, *request.desired.originalItem);
	return 100.0 * score / (score + 250.0);
}

} // namespace

std::expected<ReforgeResult, std::string> SearchReforgeCandidates(const ReforgeRequest &request)
{
	return SearchReforgeCandidates(request, {});
}

std::expected<ReforgeResult, std::string> SearchReforgeCandidates(const ReforgeRequest &request, const ReforgeProgressCallback &progress)
{
	const auto searchStarted = std::chrono::steady_clock::now();
	ReforgeResult result;
	const auto catalog = GetItemCatalog(request.desired.contentProfile);
	if (catalog.empty()) return std::unexpected("The active game's item catalog is unavailable");
	const uint32_t budget = ReforgeSearchBudget(request.skill);
	struct FastWinner { std::array<std::byte, 20> packed; uint32_t seed; float score; };
	std::vector<FastWinner> fastWinners;
	for (uint32_t attempt = 0; attempt < budget; ++attempt) {
		if (progress && (attempt % 64 == 0)) {
			const ReforgeProgress update { result.candidatesExamined, budget, result.generationCalls,
				std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - searchStarted).count() };
			if (!progress(update)) return std::unexpected("Reforge search cancelled");
		}
		WorkshopItemModel candidateModel = request.desired;
		candidateModel.generation.level = std::clamp<uint8_t>(request.generationLevel, 1, 63);
		candidateModel.generation.seed = request.desired.generation.seed + (request.searchOffset + attempt) * 0x9E3779B9U;
		if (!request.lockBase) candidateModel.workingItem.baseItemId = catalog[(request.searchOffset + attempt) % catalog.size()].baseItemId;
		if (!request.lockQuality) candidateModel.generation.quality = static_cast<ItemQualityChoice>((request.searchOffset + attempt) % 3);
		if (!request.lockPrefix && !request.preserveCurrentAffixes) candidateModel.generation.prefixName.clear();
		if (!request.lockSuffix && !request.preserveCurrentAffixes) candidateModel.generation.suffixName.clear();
		if (!request.lockUnique) candidateModel.generation.uniqueName.clear();
		++result.candidatesExamined;
		if (request.engine == ReforgeEngine::Fast) {
			const auto generationStarted = std::chrono::steady_clock::now();
			auto generated = GenerateExactSeedNative(candidateModel.workingItem.baseItemId, candidateModel.contentProfile, candidateModel.generation);
			result.generationMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - generationStarted).count();
			if (!generated.has_value()) continue;
			result.generationCalls += generated->generationCalls;
			if (generated->generationCalls != 1 || generated->item._iSeed != candidateModel.generation.seed)
				return std::unexpected("Fast Reforge exact-seed generation contract was violated");
			++result.validCandidates;
			const auto scoreStarted = std::chrono::steady_clock::now();
			const float score = static_cast<float>(ScoreNative(generated->item, generated->packedBytes, request));
			result.scoringMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - scoreStarted).count();
			const uint32_t actualSeed = generated->item._iSeed;
			if (std::any_of(fastWinners.begin(), fastWinners.end(), [actualSeed](const auto &winner) { return winner.seed == actualSeed; })) continue;
			fastWinners.push_back({ generated->packedBytes, actualSeed, score });
			std::sort(fastWinners.begin(), fastWinners.end(), [](const auto &a, const auto &b) { return a.score > b.score; });
			if (fastWinners.size() > 3) fastWinners.resize(3);
			continue;
		}
		const auto generationStarted = std::chrono::steady_clock::now();
		const auto generated = VanillaItemRules::Generate(candidateModel);
		result.generationMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - generationStarted).count();
		if (!generated.has_value()) continue;
		++result.validCandidates;
		ReforgeCandidate candidate;
		candidate.item = *generated;
		candidate.seed = generated->seed;
		const auto scoreStarted = std::chrono::steady_clock::now();
		candidate.overallScore = static_cast<float>(ScoreCandidate(*generated, request));
		result.scoringMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - scoreStarted).count();
		candidate.scoreDetails.push_back(std::format("Overall: {:.1f}%", candidate.overallScore));
		candidate.scoreDetails.push_back(std::format("Seed: 0x{:08X}", candidate.seed));
		if (std::any_of(result.topCandidates.begin(), result.topCandidates.end(), [&candidate](const auto &existing) { return existing.seed == candidate.seed; })) continue;
		result.topCandidates.push_back(std::move(candidate));
		std::sort(result.topCandidates.begin(), result.topCandidates.end(), [](const auto &a, const auto &b) { return a.overallScore > b.overallScore; });
		if (result.topCandidates.size() > 3) result.topCandidates.resize(3);
	}
	if (request.engine == ReforgeEngine::Fast) {
		const auto summaryStarted = std::chrono::steady_clock::now();
		for (const auto &winner : fastWinners) {
			auto summary = SummarizeImportedItem(winner.packed, request.desired.game);
			if (!summary.has_value()) continue;
			ReforgeCandidate candidate;
			candidate.item = std::move(*summary); candidate.seed = winner.seed; candidate.overallScore = winner.score;
			candidate.scoreDetails.push_back(std::format("Overall: {:.1f}%", candidate.overallScore));
			candidate.scoreDetails.push_back(std::format("Seed: 0x{:08X}", candidate.seed));
			result.topCandidates.push_back(std::move(candidate));
		}
		result.summaryMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - summaryStarted).count();
	}
	result.nextSearchOffset = request.searchOffset + budget;
	result.elapsedMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - searchStarted).count();
	if (progress && !progress({ result.candidatesExamined, budget, result.generationCalls, result.elapsedMilliseconds }))
		return std::unexpected("Reforge search cancelled");
	if (result.topCandidates.empty()) return std::unexpected("No reproducible item matched the selected locks and vanilla constraints");
	return result;
}

} // namespace d1hellforge
