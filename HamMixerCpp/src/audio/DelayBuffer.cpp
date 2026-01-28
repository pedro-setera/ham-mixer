#include "audio/DelayBuffer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

DelayBuffer::DelayBuffer(int maxDelaySamples, int sampleRate)
    : m_sampleRate(sampleRate)
    , m_maxDelaySamples(maxDelaySamples)
    , m_writePos(0)
    , m_currentDelaySamples(0)
    , m_targetDelaySamples(0)
    , m_oldDelaySamples(0)
    , m_crossfadeActive(false)
    , m_crossfadeProgress(0)
{
    // Buffer size: max delay + some headroom for processing
    m_bufferSize = maxDelaySamples + 8192;
    m_buffer.resize(m_bufferSize, 0.0f);

    // Crossfade duration in samples (~50ms)
    m_crossfadeSamples = msToSamples(CROSSFADE_MS);
}

int DelayBuffer::msToSamples(float ms) const
{
    return static_cast<int>(ms * m_sampleRate / 1000.0f);
}

float DelayBuffer::samplesToMs(int samples) const
{
    return static_cast<float>(samples) * 1000.0f / m_sampleRate;
}

void DelayBuffer::setDelayMs(float delayMs)
{
    // Clamp delay to valid range
    delayMs = std::max(0.0f, std::min(static_cast<float>(MAX_DELAY_MS), delayMs));
    int targetSamples = msToSamples(delayMs);

    // Only start crossfade if delay actually changed
    if (targetSamples != m_targetDelaySamples) {
        m_oldDelaySamples = m_currentDelaySamples;
        m_targetDelaySamples = targetSamples;
        m_crossfadeActive = true;
        m_crossfadeProgress = 0;
    }
}

float DelayBuffer::getCurrentDelayMs() const
{
    return samplesToMs(m_currentDelaySamples);
}

float DelayBuffer::getTargetDelayMs() const
{
    return samplesToMs(m_targetDelaySamples);
}

void DelayBuffer::process(const float* input, float* output, int sampleCount)
{
    for (int i = 0; i < sampleCount; i++) {
        // Write input to buffer
        m_buffer[m_writePos] = input[i];

        // Handle crossfade transition
        if (m_crossfadeActive) {
            m_crossfadeProgress++;

            if (m_crossfadeProgress >= m_crossfadeSamples) {
                // Crossfade complete
                m_crossfadeActive = false;
                m_currentDelaySamples = m_targetDelaySamples;
            } else {
                // Interpolate between old and new delay
                float fadeProgress = static_cast<float>(m_crossfadeProgress) / m_crossfadeSamples;
                // Smooth cosine interpolation
                float smoothFade = 0.5f * (1.0f - std::cos(fadeProgress * 3.14159265f));
                m_currentDelaySamples = static_cast<int>(
                    m_oldDelaySamples * (1.0f - smoothFade) +
                    m_targetDelaySamples * smoothFade
                );
            }
        }

        // Read from delay position
        int readPos = m_writePos - m_currentDelaySamples;
        if (readPos < 0) {
            readPos += m_bufferSize;
        }
        output[i] = m_buffer[readPos];

        // Advance write position
        m_writePos = (m_writePos + 1) % m_bufferSize;
    }
}

void DelayBuffer::reset()
{
    std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
    m_writePos = 0;
    m_currentDelaySamples = 0;
    m_targetDelaySamples = 0;
    m_oldDelaySamples = 0;
    m_crossfadeActive = false;
    m_crossfadeProgress = 0;
}
