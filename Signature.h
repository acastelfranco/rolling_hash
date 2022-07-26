#pragma once

#include <cstdint>

struct Signature
{
	uint32_t id;
	uint32_t pos;
	uint32_t hash;
	uint32_t size;
};