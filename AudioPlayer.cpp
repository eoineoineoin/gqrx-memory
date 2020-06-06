#include <AudioPlayer.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

#if defined(HAS_PULSEAUDIO)
#include <pulse/simple.h>
#endif

AudioPlayer::AudioPlayer()
{
	int sampleRate = 44100;

#if defined(HAS_PULSEAUDIO)
	pa_sample_spec ss;
	ss.format = PA_SAMPLE_S16NE;
	ss.channels = 1;
	ss.rate = sampleRate;
	m_pa = pa_simple_new(NULL, "gqrx-memory", PA_STREAM_PLAYBACK,
			NULL, "Beeps", &ss, NULL, NULL, NULL);
#else
	m_pa = nullptr;
#endif

	const float f1 = 440;
	const float f2 = f1 * 2;
	const float firstBeepDuration = 0.15f;
	const float secondBeepDuration = 0.10f;
	const float beepOverlap = 0.01f; // Avoids a pop between the two beeps
	const float totalDuration = firstBeepDuration + secondBeepDuration + beepOverlap;

	int numSamples = int(float(sampleRate) * totalDuration);
	m_beepSound.reserve(numSamples);
	const float invRate = 1.0f / float(sampleRate);

	auto sig1 = [=](float t) {
		float a1 = 1.0f;
		float a2 = 0.15f;
		float a3 = 0.0125f;
		return (a1 * sinf(t * f1 * 1 * 2 * M_PI)
			+ a2 * sinf(t * f1 * 2 * 2 * M_PI)
			+ a3 * sinf(t * f1 * 3 * 2 * M_PI)) / (a1 + a2 + a3);
	};

	auto sig2 = [=](float t) {
		float a1 = 1.0f;
		float a2 = 0.8f;
		return (a1 * sinf(t * f2 * 1 * 2 * M_PI)
			+ a2 * sinf(t * (f2 + f1) * 2 * 2 * M_PI)) / (a1 + a2);
	};

	for(int i = 0; i < numSamples; i++) {
		float time = i * invRate;

		float sample;
		if(time < firstBeepDuration) {
			sample = sig1(time);
		}
		else if(time < firstBeepDuration + beepOverlap) {
			float f2Frac = (time - firstBeepDuration) / beepOverlap;
			sample = (1.0f - f2Frac) * sig1(time) + f2Frac * sig2(time);
		}
		else {
			sample = sig2(time);
		}

		// Slowly decrease the volume over the whole sample
		sample *= 1.0f - (time / totalDuration);

		// Ramp up volume at start to avoid a pop
		{
			const float smoothStepTime = 0.03f;
			float frac = std::min(time / smoothStepTime, 1.0f);
			sample *= 3 * frac * frac - 2 * frac * frac * frac;
		}

		// Reduce the volume so it's unintrusive
		sample *= 0.25f;

		int32_t iSample = std::clamp(int32_t(sample * INT16_MAX), INT16_MIN, INT16_MAX);
		m_beepSound.push_back(int16_t(iSample));
	}
	for(int i = 0; i < 16000; i++)
		m_beepSound.push_back(0);
}

AudioPlayer::~AudioPlayer() = default;

void AudioPlayer::beep()
{
#if defined(HAS_PULSEAUDIO)
	pa_simple_write(m_pa, &m_beepSound[0], m_beepSound.size() * sizeof(m_beepSound[0]), nullptr);
#endif
}
