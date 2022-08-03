#pragma once

#include <cstdint>
#include <memory>

enum class DeltaCommand {
	AddChunk,
	KeepChunk,
};

struct Delta {
	uint32_t id;
	DeltaCommand command;
	uint32_t pos;
	uint32_t size;
	std::unique_ptr <uint8_t []> data;
};