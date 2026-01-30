#include "audio/MixerCore.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MixerCore::MixerCore(int sampleRate, int bufferSize)
    : m_sampleRate(sampleRate)
    , m_bufferSize(bufferSize)
{
    // Create delay buffer (max 2000ms at sample rate for distant KiwiSDR sites)
    int maxDelaySamples = static_cast<int>(2000.0f * sampleRate / 1000.0f);
    m_delayBuffer = std::make_unique<DelayBuffer>(maxDelaySamples, sampleRate);

    // Create audio sync
    m_audioSync = std::make_unique<AudioSync>();
}

// Channel 1 controls
void MixerCore::setChannel1Volume(float volume)
{
    m_ch1Volume.store(std::clamp(volume, 0.0f, 1.5f));
}

void MixerCore::setChannel1Pan(float pan)
{
    m_ch1Pan.store(std::clamp(pan, -1.0f, 1.0f));
}

void MixerCore::setChannel1Mute(bool muted)
{
    m_ch1Muted.store(muted);
}

float MixerCore::getChannel1Volume() const
{
    return m_ch1Volume.load();
}

float MixerCore::getChannel1Pan() const
{
    return m_ch1Pan.load();
}

bool MixerCore::isChannel1Muted() const
{
    return m_ch1Muted.load();
}

// Channel 2 controls
void MixerCore::setChannel2Volume(float volume)
{
    m_ch2Volume.store(std::clamp(volume, 0.0f, 1.5f));
}

void MixerCore::setChannel2Pan(float pan)
{
    m_ch2Pan.store(std::clamp(pan, -1.0f, 1.0f));
}

void MixerCore::setChannel2Mute(bool muted)
{
    m_ch2Muted.store(muted);
}

float MixerCore::getChannel2Volume() const
{
    return m_ch2Volume.load();
}

float MixerCore::getChannel2Pan() const
{
    return m_ch2Pan.load();
}

bool MixerCore::isChannel2Muted() const
{
    return m_ch2Muted.load();
}

// Delay control
void MixerCore::setDelayMs(float delayMs)
{
    m_delayBuffer->setDelayMs(delayMs);
}

float MixerCore::getDelayMs() const
{
    return m_delayBuffer->getCurrentDelayMs();
}

float MixerCore::getTargetDelayMs() const
{
    return m_delayBuffer->getTargetDelayMs();
}

// Master controls
void MixerCore::setMasterVolume(float volume)
{
    m_masterVolume.store(std::clamp(volume, 0.0f, 1.0f));
}

void MixerCore::setMasterMute(bool muted)
{
    m_masterMuted.store(muted);
}

float MixerCore::getMasterVolume() const
{
    return m_masterVolume.load();
}

bool MixerCore::isMasterMuted() const
{
    return m_masterMuted.load();
}

float MixerCore::linearToDb(float linear) const
{
    if (linear <= 0.0f) {
        return LEVEL_MIN_DB;
    }
    float db = 20.0f * std::log10(linear);
    return std::clamp(db, LEVEL_MIN_DB, LEVEL_MAX_DB);
}

void MixerCore::applyPan(float mono, float pan, float& left, float& right) const
{
    // Constant-power panning
    // Convert pan [-1, 1] to angle [0, pi/2]
    float angle = (pan + 1.0f) * static_cast<float>(M_PI) / 4.0f;

    float leftGain = std::cos(angle);
    float rightGain = std::sin(angle);

    left = mono * leftGain;
    right = mono * rightGain;
}

float MixerCore::softClip(float sample) const
{
    float absVal = std::abs(sample);
    if (absVal <= SOFT_CLIP_THRESHOLD) {
        return sample;
    }

    float sign = (sample > 0) ? 1.0f : -1.0f;
    float excess = (absVal - SOFT_CLIP_THRESHOLD) / (1.0f - SOFT_CLIP_THRESHOLD + 1e-10f);
    return sign * (SOFT_CLIP_THRESHOLD + (1.0f - SOFT_CLIP_THRESHOLD) * std::tanh(excess));
}

void MixerCore::updateLevels(float left, float right,
                             std::atomic<float>& levelLeft,
                             std::atomic<float>& levelRight)
{
    float absLeft = std::abs(left);
    float absRight = std::abs(right);

    // Peak hold with decay
    float currentLeft = levelLeft.load();
    float currentRight = levelRight.load();

    levelLeft.store(std::max(absLeft, currentLeft * PEAK_DECAY));
    levelRight.store(std::max(absRight, currentRight * PEAK_DECAY));
}

void MixerCore::process(const int16_t* radioIn, const int16_t* websdrIn,
                        int16_t* output, int frameCount)
{
    // Get control values
    float ch1Vol = m_ch1Volume.load();
    float ch1Pan = m_ch1Pan.load();
    bool ch1Muted = m_ch1Muted.load();

    float ch2Vol = m_ch2Volume.load();
    float ch2Pan = m_ch2Pan.load();
    bool ch2Muted = m_ch2Muted.load();

    float masterVol = m_masterVolume.load();
    bool masterMuted = m_masterMuted.load();

    // Temporary buffers for mono processing
    std::vector<float> ch1Mono(frameCount);
    std::vector<float> ch2Mono(frameCount);
    std::vector<float> ch1Delayed(frameCount);

    // Peak tracking for this buffer
    float ch1PeakLeft = 0.0f, ch1PeakRight = 0.0f;
    float ch2PeakLeft = 0.0f, ch2PeakRight = 0.0f;
    float masterPeakLeft = 0.0f, masterPeakRight = 0.0f;

    // Convert stereo to mono and normalize
    for (int i = 0; i < frameCount; i++) {
        // Radio (channel 1)
        float radioL = radioIn[i * 2] / 32768.0f;
        float radioR = radioIn[i * 2 + 1] / 32768.0f;
        ch1Mono[i] = (radioL + radioR) * 0.5f;

        // WebSDR (channel 2)
        float websdrL = websdrIn[i * 2] / 32768.0f;
        float websdrR = websdrIn[i * 2 + 1] / 32768.0f;
        ch2Mono[i] = (websdrL + websdrR) * 0.5f;
    }

    // Feed samples to AudioSync if capturing
    if (m_audioSync && m_audioSync->isCapturing()) {
        m_audioSync->addSamples(ch1Mono.data(), ch2Mono.data(), frameCount);
    }

    // Apply delay to channel 1
    m_delayBuffer->process(ch1Mono.data(), ch1Delayed.data(), frameCount);

    // Process each sample
    for (int i = 0; i < frameCount; i++) {
        float mixLeft = 0.0f;
        float mixRight = 0.0f;

        // Channel 1 (Radio)
        {
            float mono = ch1Delayed[i] * ch1Vol;

            // Always track level for metering (even when muted)
            // This allows peak detection while channel is muted
            float absMono = std::abs(mono);
            ch1PeakLeft = std::max(ch1PeakLeft, absMono);
            ch1PeakRight = std::max(ch1PeakRight, absMono);

            // Only add to mix if not muted
            if (!ch1Muted) {
                float left, right;
                applyPan(mono, ch1Pan, left, right);
                mixLeft += left;
                mixRight += right;
            }
        }

        // Channel 2 (WebSDR)
        {
            float mono = ch2Mono[i] * ch2Vol;

            // Always track level for metering (even when muted)
            // This allows peak detection while channel is muted
            float absMono = std::abs(mono);
            ch2PeakLeft = std::max(ch2PeakLeft, absMono);
            ch2PeakRight = std::max(ch2PeakRight, absMono);

            // Only add to mix if not muted
            if (!ch2Muted) {
                float left, right;
                applyPan(mono, ch2Pan, left, right);
                mixLeft += left;
                mixRight += right;
            }
        }

        // Apply master volume and mute
        if (!masterMuted) {
            mixLeft *= masterVol;
            mixRight *= masterVol;
        } else {
            mixLeft = 0.0f;
            mixRight = 0.0f;
        }

        // Soft clipping
        mixLeft = softClip(mixLeft);
        mixRight = softClip(mixRight);

        // Fade-in for smooth startup
        if (m_fadeInSamples < FADE_IN_DURATION) {
            float fadeProgress = static_cast<float>(m_fadeInSamples) / FADE_IN_DURATION;
            float fadeGain = 0.5f * (1.0f - std::cos(fadeProgress * static_cast<float>(M_PI)));
            mixLeft *= fadeGain;
            mixRight *= fadeGain;
            m_fadeInSamples++;
        }

        // Track master peaks
        masterPeakLeft = std::max(masterPeakLeft, std::abs(mixLeft));
        masterPeakRight = std::max(masterPeakRight, std::abs(mixRight));

        // Convert to int16 and clip
        mixLeft = std::clamp(mixLeft, -1.0f, 1.0f);
        mixRight = std::clamp(mixRight, -1.0f, 1.0f);

        output[i * 2] = static_cast<int16_t>(mixLeft * 32767.0f);
        output[i * 2 + 1] = static_cast<int16_t>(mixRight * 32767.0f);
    }

    // Update level meters with peak hold
    // Use a threshold to ensure levels reach zero when there's no signal
    constexpr float SILENCE_THRESHOLD = 0.0001f;  // Below this, treat as silence

    auto updateLevel = [&](float peak, std::atomic<float>& level) {
        float current = level.load();
        float decayed = current * PEAK_DECAY;
        if (decayed < SILENCE_THRESHOLD) decayed = 0.0f;
        level.store(std::max(peak, decayed));
    };

    updateLevel(ch1PeakLeft, m_ch1LevelLeft);
    updateLevel(ch1PeakRight, m_ch1LevelRight);
    updateLevel(ch2PeakLeft, m_ch2LevelLeft);
    updateLevel(ch2PeakRight, m_ch2LevelRight);
    updateLevel(masterPeakLeft, m_masterLevelLeft);
    updateLevel(masterPeakRight, m_masterLevelRight);
}

void MixerCore::getLevels(float& ch1Left, float& ch1Right,
                          float& ch2Left, float& ch2Right,
                          float& masterLeft, float& masterRight) const
{
    ch1Left = linearToDb(m_ch1LevelLeft.load());
    ch1Right = linearToDb(m_ch1LevelRight.load());
    ch2Left = linearToDb(m_ch2LevelLeft.load());
    ch2Right = linearToDb(m_ch2LevelRight.load());
    masterLeft = linearToDb(m_masterLevelLeft.load());
    masterRight = linearToDb(m_masterLevelRight.load());
}

void MixerCore::getRawLevels(float& ch1, float& ch2) const
{
    // Return max of left/right for each channel (linear scale 0.0 - 1.0)
    ch1 = std::max(m_ch1LevelLeft.load(), m_ch1LevelRight.load());
    ch2 = std::max(m_ch2LevelLeft.load(), m_ch2LevelRight.load());
}

void MixerCore::reset()
{
    m_delayBuffer->reset();

    m_ch1LevelLeft.store(0.0f);
    m_ch1LevelRight.store(0.0f);
    m_ch2LevelLeft.store(0.0f);
    m_ch2LevelRight.store(0.0f);
    m_masterLevelLeft.store(0.0f);
    m_masterLevelRight.store(0.0f);

    m_fadeInSamples = 0;

    if (m_audioSync) {
        m_audioSync->cancel();
    }
}

// Audio sync methods
void MixerCore::startSyncCapture(AudioSync::SignalMode mode)
{
    if (m_audioSync) {
        m_audioSync->startCapture(mode);
    }
}

void MixerCore::cancelSyncCapture()
{
    if (m_audioSync) {
        m_audioSync->cancel();
    }
}

bool MixerCore::isSyncCapturing() const
{
    return m_audioSync && m_audioSync->isCapturing();
}

float MixerCore::getSyncProgress() const
{
    if (m_audioSync) {
        return m_audioSync->getProgress();
    }
    return 0.0f;
}

bool MixerCore::hasSyncResult() const
{
    return m_audioSync && m_audioSync->hasResult();
}

AudioSync::SyncResult MixerCore::getSyncResult()
{
    if (m_audioSync) {
        return m_audioSync->getResult();
    }
    return AudioSync::SyncResult();
}
