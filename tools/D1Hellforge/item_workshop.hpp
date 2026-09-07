#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "item_catalog.hpp"
#include "item_generator.hpp"

namespace d1hellforge {

enum class WorkshopMode : uint8_t {
	Create,
	Edit,
	Reforge,
};

enum class VanillaOptionStatus : uint8_t {
	Valid,
	Invalid,
	Selected,
};

struct VanillaOptionState {
	std::string value;
	VanillaOptionStatus status = VanillaOptionStatus::Valid;
	std::string reason;
};

struct WorkshopValidationState {
	bool valid = true;
	bool reproducible = true;
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
};

struct WorkshopItemModel {
	WorkshopMode mode = WorkshopMode::Create;
	Game game = Game::Diablo;
	ContentProfile contentProfile;
	CharacterSummary::PackedItemSummary workingItem;
	std::optional<CharacterSummary::PackedItemSummary> originalItem;
	ItemGenerationOptions generation;
	bool enforceVanillaRules = true;
	bool grayInvalidOptions = true;
	bool dirty = false;
};

struct VanillaConstraintState {
	std::vector<VanillaOptionState> baseItems;
	std::vector<VanillaOptionState> prefixes;
	std::vector<VanillaOptionState> suffixes;
	std::vector<VanillaOptionState> uniques;
	WorkshopValidationState validation;
};

class VanillaItemRules {
public:
	static VanillaConstraintState Evaluate(const WorkshopItemModel &model);
	static WorkshopValidationState Validate(const WorkshopItemModel &model);
	static std::expected<CharacterSummary::PackedItemSummary, std::string> Generate(const WorkshopItemModel &model);
};

enum class ReforgeSkill : uint8_t { Novice, Competent, Master };
enum class ReforgePreference : uint8_t { Balanced, Magic, BaseRoll, Damage, Defense, Attributes, Resistances, LifeMana, ClosestToOriginal };
enum class ReforgeEngine : uint8_t { Fast, Compatible };

struct ReforgeRequest {
	WorkshopItemModel desired;
	ReforgeSkill skill = ReforgeSkill::Competent;
	ReforgePreference preference = ReforgePreference::Balanced;
	uint8_t generationLevel = 30;
	bool lockBase = true;
	bool lockQuality = true;
	bool lockPrefix = false;
	bool lockSuffix = false;
	bool lockUnique = false;
	bool preserveClosestRolls = false;
	bool searchBestValidSeeds = true;
	bool preferPerfectRolls = false;
	bool preserveCurrentAffixes = false;
	uint32_t searchOffset = 0;
	ReforgeEngine engine = ReforgeEngine::Fast;
};

struct ReforgeCandidate {
	CharacterSummary::PackedItemSummary item;
	uint32_t seed = 0;
	float overallScore = 0;
	unsigned perfectRollCount = 0;
	std::vector<std::string> scoreDetails;
};

struct ReforgeResult {
	std::vector<ReforgeCandidate> topCandidates;
	uint32_t candidatesExamined = 0;
	uint32_t validCandidates = 0;
	uint32_t nextSearchOffset = 0;
	uint64_t generationCalls = 0;
	double elapsedMilliseconds = 0;
	double generationMilliseconds = 0;
	double summaryMilliseconds = 0;
	double scoringMilliseconds = 0;
};

struct ReforgeProgress {
	uint32_t candidatesExamined = 0;
	uint32_t budget = 0;
	uint64_t generationCalls = 0;
	double elapsedMilliseconds = 0;
};

using ReforgeProgressCallback = std::function<bool(const ReforgeProgress &)>;

uint32_t ReforgeSearchBudget(ReforgeSkill skill);
std::expected<ReforgeResult, std::string> SearchReforgeCandidates(const ReforgeRequest &request);
std::expected<ReforgeResult, std::string> SearchReforgeCandidates(const ReforgeRequest &request, const ReforgeProgressCallback &progress);

WorkshopItemModel MakeCreateWorkshopModel(Game game, uint8_t level, bool enforceVanillaRules);
WorkshopItemModel MakeCreateWorkshopModel(const ContentProfile &profile, uint8_t level, bool enforceVanillaRules);
WorkshopItemModel MakeInventoryWorkshopModel(WorkshopMode mode, Game game, uint8_t level,
	const CharacterSummary::PackedItemSummary &item, bool enforceVanillaRules);
WorkshopItemModel MakeInventoryWorkshopModel(WorkshopMode mode, const ContentProfile &profile, uint8_t level,
	const CharacterSummary::PackedItemSummary &item, bool enforceVanillaRules);
void ResetWorkshopModel(WorkshopItemModel &model);

} // namespace d1hellforge
