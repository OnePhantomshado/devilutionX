#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "content_profile.hpp"

namespace d1hellforge {

constexpr unsigned StashPageCount = 100;
constexpr unsigned StashCellsPerPage = 100;

struct StashItemRecord {
	uint32_t index = 0;
	std::vector<std::byte> fullItemRecord;
};

struct StashDocument {
	std::filesystem::path path;
	ContentProfile contentProfile;
	uint8_t version = 0;
	uint32_t gold = 0;
	uint32_t selectedPage = 0;
	std::map<uint32_t, std::array<uint16_t, StashCellsPerPage>> pages;
	std::vector<StashItemRecord> items;
};

struct StashSaveResult {
	std::filesystem::path backupPath;
};

std::filesystem::path StashPathForCharacterSave(const std::filesystem::path &characterSave);
std::expected<StashDocument, std::string> DecodeStashPayload(
	std::span<const std::byte> payload, const ContentProfile &profile);
std::expected<std::vector<std::byte>, std::string> EncodeStashPayload(const StashDocument &stash);
std::expected<void, std::string> MoveStashItem(
	StashDocument &stash, uint16_t itemReference, uint32_t destinationPage, unsigned column, unsigned row);
std::expected<uint16_t, std::string> CopyStashItem(
	StashDocument &stash, uint16_t itemReference, uint32_t destinationPage);
std::expected<uint16_t, std::string> AddStashItem(StashDocument &stash, std::vector<std::byte> fullItemRecord,
	unsigned width, unsigned height, uint32_t destinationPage);
std::expected<void, std::string> DeleteStashItem(StashDocument &stash, uint16_t itemReference);
std::expected<StashSaveResult, std::string> SaveStashDocument(const StashDocument &stash);
std::expected<StashDocument, std::string> ReadStashDocument(
	const std::filesystem::path &path, const ContentProfile &profile);

} // namespace d1hellforge
