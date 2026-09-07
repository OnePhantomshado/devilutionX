#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "utils/item_file.hpp"
#include "save_document.hpp"

namespace d1hellforge {

std::expected<devilution::ItemFile, std::string> ImportItemFile(const std::filesystem::path &path);
std::expected<void, std::string> ExportItemFile(const std::filesystem::path &path, const devilution::PackedItemBytes &item, devilution::ItemGame game, bool raw);
std::expected<void, std::string> ExportItemFile(const std::filesystem::path &path, const CharacterSummary::PackedItemSummary &item, devilution::ItemGame game, bool raw);

} // namespace d1hellforge
