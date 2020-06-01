#pragma once

#include <stdint.h>

struct Bookmark
{
	uint64_t m_frequency = 0;
	uint8_t m_mode[8] = {0};
	uint32_t m_passband = 0;
	float m_squelch = 0;
};

