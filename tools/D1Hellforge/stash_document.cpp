#include "stash_document.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <format>
#include <limits>
#include <type_traits>

#include "codec.h"
#include "devx_adapter.hpp"
#include "mpq/mpq_reader.hpp"
#include "mpq/mpq_writer.hpp"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace d1hellforge {
namespace {

constexpr uint8_t SupportedStashVersion = 0;
constexpr char SinglePlayerPassword[] = "xrgyrkj1";
constexpr char SpawnSinglePlayerPassword[] = "adslhfb1";

class PayloadReader {
public:
	explicit PayloadReader(std::span<const std::byte> bytes)
	    : bytes_(bytes)
	{
	}

	template <typename T>
	std::expected<T, std::string> Next()
	{
		if (remaining() < sizeof(T)) return std::unexpected("The stash payload ends unexpectedly");
		using U = std::make_unsigned_t<T>;
		U value = 0;
		for (std::size_t index = 0; index < sizeof(T); ++index)
			value |= static_cast<U>(std::to_integer<uint8_t>(bytes_[offset_ + index])) << (index * 8);
		offset_ += sizeof(T);
		return static_cast<T>(value);
	}

	std::expected<std::vector<std::byte>, std::string> NextBytes(std::size_t size)
	{
		if (remaining() < size) return std::unexpected("The stash item list ends unexpectedly");
		std::vector<std::byte> result(bytes_.begin() + offset_, bytes_.begin() + offset_ + size);
		offset_ += size;
		return result;
	}

	std::size_t remaining() const { return bytes_.size() - offset_; }

private:
	std::span<const std::byte> bytes_;
	std::size_t offset_ = 0;
};

template <typename T>
void AppendLE(std::vector<std::byte> &payload, T value)
{
	using U = std::make_unsigned_t<T>;
	for (std::size_t index = 0; index < sizeof(T); ++index)
		payload.push_back(static_cast<std::byte>(static_cast<U>(value) >> (index * 8)));
}

void WriteLE32(std::vector<std::byte> &record, std::size_t offset, uint32_t value)
{
	for (std::size_t index = 0; index < sizeof(value); ++index)
		record[offset + index] = static_cast<std::byte>(value >> (index * 8));
}

const char *PasswordForStash(const std::filesystem::path &path)
{
	std::string stem = path.stem().string();
	std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
	return stem.starts_with("stash_spawn") ? SpawnSinglePlayerPassword : SinglePlayerPassword;
}

std::filesystem::path StashBackupPath(const std::filesystem::path &path)
{
	const auto now = std::chrono::system_clock::now();
	const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
	const std::string stamp = std::format("{:%Y%m%d-%H%M%S}", seconds);
	const auto directory = path.parent_path() / "Backups";
	auto candidate = directory / (path.stem().string() + "-backup-" + stamp + path.extension().string());
	for (unsigned suffix = 2; std::filesystem::exists(candidate); ++suffix)
		candidate = directory / (path.stem().string() + "-backup-" + stamp + "-" + std::to_string(suffix) + path.extension().string());
	return candidate;
}

} // namespace

std::filesystem::path StashPathForCharacterSave(const std::filesystem::path &characterSave)
{
	std::string stem = characterSave.stem().string();
	std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
	const bool spawn = stem.starts_with("spawn_") || stem.starts_with("share_");
	return characterSave.parent_path() / ((spawn ? "stash_spawn" : "stash") + characterSave.extension().string());
}

std::expected<StashDocument, std::string> DecodeStashPayload(std::span<const std::byte> payload, const ContentProfile &profile)
{
	if (const auto valid = ValidateDevXContentProfile(profile); !valid.has_value())
		return std::unexpected(valid.error());
	PayloadReader reader(payload);
	StashDocument result;
	result.contentProfile = profile;
	auto version = reader.Next<uint8_t>();
	if (!version.has_value()) return std::unexpected(version.error());
	result.version = *version;
	if (result.version > SupportedStashVersion)
		return std::unexpected("This stash uses a newer unsupported format version");
	auto gold = reader.Next<uint32_t>();
	auto pageCount = reader.Next<uint32_t>();
	if (!gold.has_value() || !pageCount.has_value()) return std::unexpected("The stash header is incomplete");
	result.gold = *gold;
	if (*pageCount > StashPageCount) return std::unexpected("The stash contains more than 100 page records");
	for (uint32_t pageIndex = 0; pageIndex < *pageCount; ++pageIndex) {
		auto pageNumber = reader.Next<uint32_t>();
		if (!pageNumber.has_value() || *pageNumber >= StashPageCount) return std::unexpected("The stash contains an invalid page number");
		if (result.pages.contains(*pageNumber)) return std::unexpected("The stash contains a duplicate page record");
		std::array<uint16_t, StashCellsPerPage> grid {};
		for (uint16_t &cell : grid) {
			auto value = reader.Next<uint16_t>();
			if (!value.has_value()) return std::unexpected(value.error());
			cell = *value;
		}
		result.pages.emplace(*pageNumber, grid);
	}
	auto itemCount = reader.Next<uint32_t>();
	if (!itemCount.has_value()) return std::unexpected(itemCount.error());
	const std::size_t itemSize = profile.baseMode == Game::Hellfire ? 372 : 368;
	if (*itemCount > reader.remaining() / itemSize) return std::unexpected("The stash item count exceeds the payload size");
	result.items.reserve(*itemCount);
	for (uint32_t index = 0; index < *itemCount; ++index) {
		auto record = reader.NextBytes(itemSize);
		if (!record.has_value()) return std::unexpected(record.error());
		result.items.push_back({ index, std::move(*record) });
	}
	auto selectedPage = reader.Next<uint32_t>();
	if (!selectedPage.has_value() || *selectedPage >= StashPageCount) return std::unexpected("The stash has an invalid selected page");
	result.selectedPage = *selectedPage;
	if (reader.remaining() != 0) return std::unexpected("The stash payload size does not match its page and item counts");

	std::vector<int> itemPage(*itemCount, -1);
	for (const auto &[page, grid] : result.pages) {
		for (const uint16_t cell : grid) {
			if (cell == 0) continue;
			if (cell > *itemCount) return std::unexpected("A stash grid cell references a missing item");
			const std::size_t item = cell - 1;
			if (itemPage[item] != -1 && itemPage[item] != static_cast<int>(page))
				return std::unexpected("A stash item is referenced by more than one page");
			itemPage[item] = static_cast<int>(page);
		}
	}
	if (std::find(itemPage.begin(), itemPage.end(), -1) != itemPage.end())
		return std::unexpected("The stash contains an item that is not placed on any page");
	return result;
}

std::expected<std::vector<std::byte>, std::string> EncodeStashPayload(const StashDocument &stash)
{
	if (const auto valid = ValidateDevXContentProfile(stash.contentProfile); !valid.has_value()) return std::unexpected(valid.error());
	if (stash.version > SupportedStashVersion) return std::unexpected("This stash uses a newer unsupported format version");
	if (stash.selectedPage >= StashPageCount) return std::unexpected("The stash has an invalid selected page");
	const std::size_t itemSize = stash.contentProfile.baseMode == Game::Hellfire ? 372 : 368;
	for (std::size_t index = 0; index < stash.items.size(); ++index) {
		if (stash.items[index].index != index || stash.items[index].fullItemRecord.size() != itemSize)
			return std::unexpected("The stash item list is not canonical for its content profile");
	}
	std::vector<uint32_t> pages;
	std::vector<int> itemPage(stash.items.size(), -1);
	for (const auto &[page, grid] : stash.pages) {
		if (page >= StashPageCount) return std::unexpected("The stash contains an invalid page number");
		bool occupied = false;
		for (uint16_t reference : grid) {
			if (reference == 0) continue;
			occupied = true;
			if (reference > stash.items.size()) return std::unexpected("A stash grid cell references a missing item");
			const std::size_t item = reference - 1;
			if (itemPage[item] != -1 && itemPage[item] != static_cast<int>(page)) return std::unexpected("A stash item is referenced by more than one page");
			itemPage[item] = static_cast<int>(page);
		}
		if (occupied) pages.push_back(page);
	}
	if (std::find(itemPage.begin(), itemPage.end(), -1) != itemPage.end()) return std::unexpected("The stash contains an item that is not placed on any page");
	std::vector<std::byte> payload;
	AppendLE<uint8_t>(payload, stash.version);
	AppendLE<uint32_t>(payload, stash.gold);
	AppendLE<uint32_t>(payload, static_cast<uint32_t>(pages.size()));
	for (uint32_t page : pages) {
		AppendLE<uint32_t>(payload, page);
		for (uint16_t reference : stash.pages.at(page)) AppendLE<uint16_t>(payload, reference);
	}
	AppendLE<uint32_t>(payload, static_cast<uint32_t>(stash.items.size()));
	for (const auto &item : stash.items) payload.insert(payload.end(), item.fullItemRecord.begin(), item.fullItemRecord.end());
	AppendLE<uint32_t>(payload, stash.selectedPage);
	return payload;
}

std::expected<void, std::string> MoveStashItem(StashDocument &stash, uint16_t itemReference, uint32_t destinationPage, unsigned column, unsigned row)
{
	if (itemReference == 0 || itemReference > stash.items.size()) return std::unexpected("The selected stash item does not exist");
	if (destinationPage >= StashPageCount) return std::unexpected("The destination stash page is invalid");
	int sourcePage = -1;
	unsigned minimumColumn = 10, maximumColumn = 0, minimumRow = 10, maximumRow = 0;
	for (const auto &[page, grid] : stash.pages) {
		for (unsigned x = 0; x < 10; ++x) for (unsigned y = 0; y < 10; ++y) {
			if (grid[x * 10 + y] != itemReference) continue;
			if (sourcePage != -1 && sourcePage != static_cast<int>(page)) return std::unexpected("The stash item spans multiple pages");
			sourcePage = static_cast<int>(page);
			minimumColumn = std::min(minimumColumn, x); maximumColumn = std::max(maximumColumn, x);
			minimumRow = std::min(minimumRow, y); maximumRow = std::max(maximumRow, y);
		}
	}
	if (sourcePage < 0) return std::unexpected("The selected stash item is not placed on a page");
	const unsigned width = maximumColumn - minimumColumn + 1;
	const unsigned height = maximumRow - minimumRow + 1;
	if (column + width > 10 || row + height > 10) return std::unexpected("The item does not fit at that stash position");
	auto &destination = stash.pages[destinationPage];
	for (unsigned x = column; x < column + width; ++x) for (unsigned y = row; y < row + height; ++y)
		if (destination[x * 10 + y] != 0 && destination[x * 10 + y] != itemReference) return std::unexpected("That stash position is occupied");
	for (auto &[page, grid] : stash.pages) for (uint16_t &reference : grid) if (reference == itemReference) reference = 0;
	for (unsigned x = column; x < column + width; ++x) for (unsigned y = row; y < row + height; ++y) destination[x * 10 + y] = itemReference;
	auto &record = stash.items[itemReference - 1].fullItemRecord;
	if (record.size() < 20) return std::unexpected("The selected stash item record is truncated");
	WriteLE32(record, 12, column);
	WriteLE32(record, 16, row + height - 1);
	stash.selectedPage = destinationPage;
	return {};
}

std::expected<uint16_t, std::string> CopyStashItem(StashDocument &stash, uint16_t itemReference, uint32_t destinationPage)
{
	if (itemReference == 0 || itemReference > stash.items.size()) return std::unexpected("The selected stash item does not exist");
	if (destinationPage >= StashPageCount) return std::unexpected("The destination stash page is invalid");
	unsigned minimumColumn = 10, maximumColumn = 0, minimumRow = 10, maximumRow = 0;
	bool found = false;
	for (const auto &[page, grid] : stash.pages) {
		for (unsigned x = 0; x < 10; ++x) for (unsigned y = 0; y < 10; ++y) {
			if (grid[x * 10 + y] != itemReference) continue;
			found = true;
			minimumColumn = std::min(minimumColumn, x); maximumColumn = std::max(maximumColumn, x);
			minimumRow = std::min(minimumRow, y); maximumRow = std::max(maximumRow, y);
		}
	}
	if (!found) return std::unexpected("The selected stash item is not placed on a page");
	const unsigned width = maximumColumn - minimumColumn + 1;
	const unsigned height = maximumRow - minimumRow + 1;
	return AddStashItem(stash, stash.items[itemReference - 1].fullItemRecord, width, height, destinationPage);
}

std::expected<uint16_t, std::string> AddStashItem(StashDocument &stash, std::vector<std::byte> fullItemRecord,
	unsigned width, unsigned height, uint32_t destinationPage)
{
	if (destinationPage >= StashPageCount) return std::unexpected("The destination stash page is invalid");
	if (width == 0 || height == 0 || width > 10 || height > 10) return std::unexpected("The item has an invalid stash footprint");
	if (stash.items.size() >= std::numeric_limits<uint16_t>::max()) return std::unexpected("The stash item list is full");
	const std::size_t itemSize = stash.contentProfile.baseMode == Game::Hellfire ? 372 : 368;
	if (fullItemRecord.size() != itemSize) return std::unexpected("The item record does not match the stash content profile");
	auto &destination = stash.pages[destinationPage];
	unsigned targetColumn = 0, targetRow = 0;
	bool fits = false;
	for (unsigned row = 0; row + height <= 10 && !fits; ++row) {
		for (unsigned column = 0; column + width <= 10; ++column) {
			fits = true;
			for (unsigned x = column; x < column + width && fits; ++x)
				for (unsigned y = row; y < row + height; ++y)
					fits = fits && destination[x * 10 + y] == 0;
			if (fits) { targetColumn = column; targetRow = row; break; }
		}
	}
	if (!fits) return std::unexpected("There is no open stash area large enough for this item on the selected page");
	WriteLE32(fullItemRecord, 12, targetColumn);
	WriteLE32(fullItemRecord, 16, targetRow + height - 1);
	stash.items.push_back({ static_cast<uint32_t>(stash.items.size()), std::move(fullItemRecord) });
	const uint16_t newReference = static_cast<uint16_t>(stash.items.size());
	for (unsigned x = targetColumn; x < targetColumn + width; ++x)
		for (unsigned y = targetRow; y < targetRow + height; ++y)
			destination[x * 10 + y] = newReference;
	stash.selectedPage = destinationPage;
	return newReference;
}

std::expected<void, std::string> DeleteStashItem(StashDocument &stash, uint16_t itemReference)
{
	if (itemReference == 0 || itemReference > stash.items.size()) return std::unexpected("The selected stash item does not exist");
	for (auto &[page, grid] : stash.pages) {
		for (uint16_t &reference : grid) {
			if (reference == itemReference) reference = 0;
			else if (reference > itemReference) --reference;
		}
	}
	stash.items.erase(stash.items.begin() + itemReference - 1);
	for (std::size_t index = 0; index < stash.items.size(); ++index)
		stash.items[index].index = static_cast<uint32_t>(index);
	std::erase_if(stash.pages, [](const auto &entry) {
		return std::all_of(entry.second.begin(), entry.second.end(), [](uint16_t reference) { return reference == 0; });
	});
	return {};
}

std::expected<StashSaveResult, std::string> SaveStashDocument(const StashDocument &stash)
{
	if (stash.path.empty()) return std::unexpected("The stash has no source archive path");
	const auto payload = EncodeStashPayload(stash);
	if (!payload.has_value()) return std::unexpected(payload.error());
	const auto backupPath = StashBackupPath(stash.path);
	const auto temporaryPath = stash.path.parent_path() / (stash.path.stem().string() + ".d1hellforge-writing" + stash.path.extension().string());
	std::error_code error;
	std::filesystem::create_directories(backupPath.parent_path(), error);
	if (error) return std::unexpected("Unable to create the stash backup directory: " + error.message());
	std::filesystem::copy_file(stash.path, backupPath, std::filesystem::copy_options::none, error);
	if (error) return std::unexpected("Unable to create the stash backup: " + error.message());
	std::filesystem::copy_file(stash.path, temporaryPath, std::filesystem::copy_options::overwrite_existing, error);
	if (error) return std::unexpected("Unable to create the temporary stash: " + error.message());
	std::vector<std::byte> encoded(devilution::codec_get_encoded_len(payload->size()));
	std::copy(payload->begin(), payload->end(), encoded.begin());
	devilution::codec_encode(encoded.data(), payload->size(), encoded.size(), PasswordForStash(stash.path));
	{
		devilution::MpqWriter writer(temporaryPath.string(), true);
		if (!writer.WriteFile("spstashitems", encoded.data(), encoded.size())) {
			std::filesystem::remove(temporaryPath, error);
			return std::unexpected("Unable to write the temporary stash record");
		}
	}
	const auto temporary = ReadStashDocument(temporaryPath, stash.contentProfile);
	const auto temporaryPayload = temporary.has_value() ? EncodeStashPayload(*temporary) : std::expected<std::vector<std::byte>, std::string> { std::unexpected("Temporary stash could not be reopened") };
	if (!temporaryPayload.has_value() || *temporaryPayload != *payload) {
		std::filesystem::remove(temporaryPath, error);
		return std::unexpected("Temporary stash verification failed; the original was not changed");
	}
#ifdef _WIN32
	if (!MoveFileExW(temporaryPath.c_str(), stash.path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		std::filesystem::remove(temporaryPath, error);
		return std::unexpected("Unable to replace the original stash after verification");
	}
#else
	std::filesystem::rename(temporaryPath, stash.path, error);
	if (error) return std::unexpected("Unable to replace the original stash after verification: " + error.message());
#endif
	const auto final = ReadStashDocument(stash.path, stash.contentProfile);
	const auto finalPayload = final.has_value() ? EncodeStashPayload(*final) : std::expected<std::vector<std::byte>, std::string> { std::unexpected("Final stash could not be reopened") };
	if (!finalPayload.has_value() || *finalPayload != *payload)
		return std::unexpected("Final stash verification failed; restore the reported backup before playing");
	return StashSaveResult { backupPath };
}

std::expected<StashDocument, std::string> ReadStashDocument(const std::filesystem::path &path, const ContentProfile &profile)
{
	auto archive = devilution::MpqArchive::Open(path.string().c_str());
	if (!archive.has_value()) return std::unexpected("Unable to open the stash archive: " + archive.error());
	constexpr char EntryName[] = "spstashitems";
	if (!archive->HasFile(EntryName)) return std::unexpected("The stash archive does not contain a single-player stash record");
	std::size_t encodedSize = 0;
	int32_t error = 0;
	auto encoded = archive->ReadFile(EntryName, encodedSize, error);
	if (error != 0 || encoded == nullptr) return std::unexpected("Unable to read the single-player stash record");
	const std::size_t decodedSize = devilution::codec_decode(encoded.get(), encodedSize, PasswordForStash(path));
	if (decodedSize == 0) return std::unexpected("Unable to decode the single-player stash record");
	auto result = DecodeStashPayload(std::span<const std::byte>(encoded.get(), decodedSize), profile);
	if (result.has_value()) result->path = path;
	return result;
}

} // namespace d1hellforge
