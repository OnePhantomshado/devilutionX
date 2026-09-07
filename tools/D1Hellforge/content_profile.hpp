#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace d1hellforge {

enum class Game : uint8_t {
	Diablo,
	Hellfire,
};

struct ContentProfile {
	Game baseMode = Game::Diablo;
	std::vector<std::string> contentIdentifiers;
	bool looseContentActive = false;
	std::string displayLabel;
	bool initialized = false;
	std::string initializationError;
	bool operator==(const ContentProfile &) const = default;
};

} // namespace d1hellforge
