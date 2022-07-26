#pragma once

#include <cstdint>

enum class DeltaCommand {
	AddChunk,
	KeepChunk,
};

struct Delta {
	uint32_t id;
	uint32_t command;
	uint32_t pos;
	uint32_t size;
	uint8_t  *data;
};