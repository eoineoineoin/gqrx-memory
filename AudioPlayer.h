#pragma once
#include <cstdint>
#include <vector>

class AudioPlayer
{
	public:
		AudioPlayer();
		~AudioPlayer();
		void beep();

	protected:
		struct pa_simple* m_pa;
		std::vector<int16_t> m_beepSound;
};

