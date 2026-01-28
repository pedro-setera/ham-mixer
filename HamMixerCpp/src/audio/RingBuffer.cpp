#include "audio/RingBuffer.h"
#include <algorithm>
#include <cstring>

RingBuffer::RingBuffer(int capacityFrames, int channels)
    : m_capacityFrames(capacityFrames)
    , m_channels(channels)
    , m_writePos(0)
    , m_readPos(0)
    , m_available(0)
{
    // Allocate buffer for all samples (frames * channels)
    m_buffer.resize(capacityFrames * channels, 0);
}

int RingBuffer::write(const int16_t* data, int frameCount)
{
    if (frameCount <= 0 || data == nullptr) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    int free = m_capacityFrames - m_available.load();
    int toWrite = std::min(frameCount, free);

    if (toWrite <= 0) {
        return 0;
    }

    int writePos = m_writePos.load();
    int samplesPerFrame = m_channels;

    // Calculate how many frames fit before wrap-around
    int framesBeforeWrap = m_capacityFrames - writePos;
    int firstChunk = std::min(toWrite, framesBeforeWrap);
    int secondChunk = toWrite - firstChunk;

    // Copy first chunk
    std::memcpy(
        m_buffer.data() + writePos * samplesPerFrame,
        data,
        firstChunk * samplesPerFrame * sizeof(int16_t)
    );

    // Copy second chunk (wrapped around)
    if (secondChunk > 0) {
        std::memcpy(
            m_buffer.data(),
            data + firstChunk * samplesPerFrame,
            secondChunk * samplesPerFrame * sizeof(int16_t)
        );
    }

    // Update write position
    m_writePos.store((writePos + toWrite) % m_capacityFrames);
    m_available.fetch_add(toWrite);

    return toWrite;
}

int RingBuffer::read(int16_t* data, int frameCount)
{
    if (frameCount <= 0 || data == nullptr) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    int avail = m_available.load();
    int toRead = std::min(frameCount, avail);
    int samplesPerFrame = m_channels;

    if (toRead > 0) {
        int readPos = m_readPos.load();

        // Calculate how many frames fit before wrap-around
        int framesBeforeWrap = m_capacityFrames - readPos;
        int firstChunk = std::min(toRead, framesBeforeWrap);
        int secondChunk = toRead - firstChunk;

        // Copy first chunk
        std::memcpy(
            data,
            m_buffer.data() + readPos * samplesPerFrame,
            firstChunk * samplesPerFrame * sizeof(int16_t)
        );

        // Copy second chunk (wrapped around)
        if (secondChunk > 0) {
            std::memcpy(
                data + firstChunk * samplesPerFrame,
                m_buffer.data(),
                secondChunk * samplesPerFrame * sizeof(int16_t)
            );
        }

        // Update read position
        m_readPos.store((readPos + toRead) % m_capacityFrames);
        m_available.fetch_sub(toRead);
    }

    // If underrun, pad remaining with zeros
    if (toRead < frameCount) {
        int remaining = frameCount - toRead;
        std::memset(
            data + toRead * samplesPerFrame,
            0,
            remaining * samplesPerFrame * sizeof(int16_t)
        );
    }

    return toRead;
}

int RingBuffer::available() const
{
    return m_available.load();
}

int RingBuffer::freeSpace() const
{
    return m_capacityFrames - m_available.load();
}

float RingBuffer::fillLevel() const
{
    return static_cast<float>(m_available.load()) / m_capacityFrames;
}

void RingBuffer::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_writePos.store(0);
    m_readPos.store(0);
    m_available.store(0);
    std::fill(m_buffer.begin(), m_buffer.end(), 0);
}
