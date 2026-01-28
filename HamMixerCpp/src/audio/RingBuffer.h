#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <vector>
#include <atomic>
#include <cstdint>
#include <mutex>

/**
 * @brief Thread-safe circular buffer for audio streaming.
 *
 * Single-producer, single-consumer lock-free ring buffer for
 * inter-thread audio data transfer.
 */
class RingBuffer {
public:
    /**
     * @brief Construct a new Ring Buffer
     * @param capacityFrames Maximum number of frames (samples per channel)
     * @param channels Number of audio channels (1=mono, 2=stereo)
     */
    RingBuffer(int capacityFrames = 4096, int channels = 2);
    ~RingBuffer() = default;

    // Non-copyable
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    /**
     * @brief Write frames to the buffer
     * @param data Pointer to interleaved audio data (int16_t)
     * @param frameCount Number of frames to write
     * @return Number of frames actually written
     */
    int write(const int16_t* data, int frameCount);

    /**
     * @brief Read frames from the buffer
     * @param data Output buffer for audio data
     * @param frameCount Number of frames to read
     * @return Number of frames actually read (pads with zeros if underrun)
     */
    int read(int16_t* data, int frameCount);

    /**
     * @brief Get number of frames available for reading
     */
    int available() const;

    /**
     * @brief Get number of frames free for writing
     */
    int freeSpace() const;

    /**
     * @brief Get buffer fill level (0.0 to 1.0)
     */
    float fillLevel() const;

    /**
     * @brief Clear the buffer
     */
    void clear();

    /**
     * @brief Get buffer capacity in frames
     */
    int capacity() const { return m_capacityFrames; }

    /**
     * @brief Get number of channels
     */
    int channels() const { return m_channels; }

private:
    std::vector<int16_t> m_buffer;
    int m_capacityFrames;
    int m_channels;

    std::atomic<int> m_writePos{0};
    std::atomic<int> m_readPos{0};
    std::atomic<int> m_available{0};

    mutable std::mutex m_mutex;
};

#endif // RINGBUFFER_H
