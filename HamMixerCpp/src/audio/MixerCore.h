#ifndef MIXERCORE_H
#define MIXERCORE_H

#include <atomic>
#include <mutex>
#include <memory>
#include <cstdint>

#include "audio/DelayBuffer.h"
#include "audio/AudioSync.h"

/**
 * @brief Core audio DSP processing engine
 *
 * Handles mixing of two input channels with:
 * - Independent volume and pan per channel
 * - Delay line on channel 1 (radio)
 * - Master volume control
 * - Soft clipping to prevent distortion
 * - Level metering for all channels
 */
class MixerCore {
public:
    static constexpr float LEVEL_MIN_DB = -80.0f;  // Matches S-meter input range (S0 = -80 dBFS)
    static constexpr float LEVEL_MAX_DB = 0.0f;
    static constexpr float SOFT_CLIP_THRESHOLD = 0.95f;

    /**
     * @brief Construct MixerCore
     * @param sampleRate Audio sample rate
     * @param bufferSize Processing buffer size
     */
    MixerCore(int sampleRate = 48000, int bufferSize = 1024);
    ~MixerCore() = default;

    // Non-copyable
    MixerCore(const MixerCore&) = delete;
    MixerCore& operator=(const MixerCore&) = delete;

    // Channel 1 (Radio) controls
    void setChannel1Volume(float volume);
    void setChannel1Pan(float pan);
    void setChannel1Mute(bool muted);
    float getChannel1Volume() const;
    float getChannel1Pan() const;
    bool isChannel1Muted() const;

    // Channel 2 (WebSDR) controls
    void setChannel2Volume(float volume);
    void setChannel2Pan(float pan);
    void setChannel2Mute(bool muted);
    float getChannel2Volume() const;
    float getChannel2Pan() const;
    bool isChannel2Muted() const;

    // Delay control (channel 1 only)
    void setDelayMs(float delayMs);
    float getDelayMs() const;
    float getTargetDelayMs() const;

    // Master controls
    void setMasterVolume(float volume);
    void setMasterMute(bool muted);
    float getMasterVolume() const;
    bool isMasterMuted() const;

    /**
     * @brief Process and mix audio
     * @param radioIn Radio input samples (interleaved stereo int16)
     * @param websdrIn WebSDR input samples (interleaved stereo int16)
     * @param output Output buffer (interleaved stereo int16)
     * @param frameCount Number of frames
     */
    void process(const int16_t* radioIn, const int16_t* websdrIn,
                 int16_t* output, int frameCount);

    /**
     * @brief Get current level meters
     * @param ch1Left Channel 1 left level in dB
     * @param ch1Right Channel 1 right level in dB
     * @param ch2Left Channel 2 left level in dB
     * @param ch2Right Channel 2 right level in dB
     * @param masterLeft Master left level in dB
     * @param masterRight Master right level in dB
     */
    void getLevels(float& ch1Left, float& ch1Right,
                   float& ch2Left, float& ch2Right,
                   float& masterLeft, float& masterRight) const;

    /**
     * @brief Get raw linear levels (0.0 - 1.0 scale) for peak detection
     * @param ch1 Channel 1 peak level (linear)
     * @param ch2 Channel 2 peak level (linear)
     * @return Max of left/right for each channel
     */
    void getRawLevels(float& ch1, float& ch2) const;

    /**
     * @brief Reset all state
     */
    void reset();

    // Audio sync methods
    void startSyncCapture(AudioSync::SignalMode mode = AudioSync::VOICE);
    void cancelSyncCapture();
    bool isSyncCapturing() const;
    float getSyncProgress() const;
    bool hasSyncResult() const;
    AudioSync::SyncResult getSyncResult();

private:
    int m_sampleRate;
    int m_bufferSize;

    // Channel 1 controls
    std::atomic<float> m_ch1Volume{1.0f};
    std::atomic<float> m_ch1Pan{0.0f};
    std::atomic<bool> m_ch1Muted{false};

    // Channel 2 controls
    std::atomic<float> m_ch2Volume{1.0f};
    std::atomic<float> m_ch2Pan{0.0f};
    std::atomic<bool> m_ch2Muted{false};

    // Master controls
    std::atomic<float> m_masterVolume{0.8f};
    std::atomic<bool> m_masterMuted{false};

    // Delay buffer for channel 1
    std::unique_ptr<DelayBuffer> m_delayBuffer;

    // Audio sync for auto-delay detection
    std::unique_ptr<AudioSync> m_audioSync;

    // Level meters (peak values in linear scale)
    std::atomic<float> m_ch1LevelLeft{0.0f};
    std::atomic<float> m_ch1LevelRight{0.0f};
    std::atomic<float> m_ch2LevelLeft{0.0f};
    std::atomic<float> m_ch2LevelRight{0.0f};
    std::atomic<float> m_masterLevelLeft{0.0f};
    std::atomic<float> m_masterLevelRight{0.0f};

    // Peak decay
    static constexpr float PEAK_DECAY = 0.95f;

    // Fade-in state
    int m_fadeInSamples{0};
    static constexpr int FADE_IN_DURATION = 2048;

    // Helper methods
    float linearToDb(float linear) const;
    void applyPan(float mono, float pan, float& left, float& right) const;
    float softClip(float sample) const;
    void updateLevels(float left, float right, std::atomic<float>& levelLeft, std::atomic<float>& levelRight);
};

#endif // MIXERCORE_H
