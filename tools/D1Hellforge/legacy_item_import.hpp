#pragma once

#include <expected>
#include <span>
#include <string>

#include "utils/item_file.hpp"

namespace d1hellforge {

std::string LegacyImportStatus(devilution::ItemFileFormat format);
std::expected<devilution::ItemFile, std::string> DecodeLegacyItem(std::span<const std::byte> bytes, devilution::ItemFileFormat format);

} // namespace d1hellforge
