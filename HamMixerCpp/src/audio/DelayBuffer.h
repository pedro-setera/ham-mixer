#ifndef DELAYBUFFER_H
#define DELAYBUFFER_H

#include <vector>
#include <atomic>
#include <cstdint>

/**
 * @brief Circular delay buffer with smooth crossfade transitions.
 *
 * Provides 0-2000ms audio delay with click-free transitions when
 * delay value changes during playback. Extended range for distant KiwiSDR sites.
 */
class DelayBuffer {
public:
    static constexpr int MAX_DELAY_MS = 2000;  // Extended for distant KiwiSDR sites
    static constexpr int SAMPLE_RATE = 48000;
    static constexpr int CROSSFADE_MS = 50;

    /**
     * @brief Construct a new Delay Buffer
     * @param maxDelaySamples Maximum delay in samples (default: 2000ms at 48kHz)
     * @param sampleRate Audio sample rate in Hz
     */
    DelayBuffer(int maxDelaySamples = 96000, int sampleRate = 48000);
    ~DelayBuffer() = default;

    // Non-copyable
    DelayBuffer(const DelayBuffer&) = delete;
    DelayBuffer& operator=(const DelayBuffer&) = delete;

    /**
     * @brief Set delay in milliseconds (0 to MAX_DELAY_MS)
     * @param delayMs Delay in milliseconds
     */
    void setDelayMs(float delayMs);

    /**
     * @brief Get current effective delay in milliseconds
     */
    float getCurrentDelayMs() const;

    /**
     * @brief Get target delay in milliseconds
     */
    float getTargetDelayMs() const;

    /**
     * @brief Process audio samples through delay buffer
     * @param input Input samples (mono, float)
     * @param output Output buffer (same size as input)
     * @param sampleCount Number of samples
     */
    void process(const float* input, float* output, int sampleCount);

    /**
     * @brief Reset buffer and state
     */
    void reset();

private:
    std::vector<float> m_buffer;
    int m_bufferSize;
    int m_sampleRate;
    int m_maxDelaySamples;

    int m_writePos;
    int m_currentDelaySamples;
    int m_targetDelaySamples;
    int m_oldDelaySamples;

    // Crossfade state
    bool m_crossfadeActive;
    int m_crossfadeProgress;
    int m_crossfadeSamples;

    // Helper methods
    int msToSamples(float ms) const;
    float samplesToMs(int samples) const;
};

#endif // DELAYBUFFER_H
