#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <utility>
#include <vector>

#include "save_document.hpp"

namespace d1hellforge {

enum class InventoryArea : uint8_t {
	Equipment,
	Inventory,
	Belt,
};

struct InventoryHitRegion {
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	std::string description;
	InventoryArea area = InventoryArea::Inventory;
	uint8_t slot = 0;
	uint8_t footprintWidth = 1;
	uint8_t footprintHeight = 1;
};

struct RenderedInventory {
	int width = 0;
	int height = 0;
	std::vector<uint32_t> pixels;
	std::vector<InventoryHitRegion> items;
};

struct RenderedItemPreview {
	int width = 0;
	int height = 0;
	std::vector<uint32_t> pixels;
};

std::expected<RenderedInventory, std::string> RenderInventory(const CharacterSummary &character);
std::expected<std::pair<int, int>, std::string> GetItemFootprint(const CharacterSummary &character, uint8_t cursorGraphic);
std::expected<RenderedItemPreview, std::string> RenderItemPreview(const CharacterSummary &character, uint8_t cursorGraphic);
std::expected<RenderedItemPreview, std::string> RenderStashBackground(const CharacterSummary &character);

} // namespace d1hellforge
