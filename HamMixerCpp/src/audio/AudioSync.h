#ifndef AUDIOSYNC_H
#define AUDIOSYNC_H

#include <vector>
#include <complex>
#include <atomic>
#include <mutex>
#include <thread>
#include <memory>

/**
 * @brief Audio synchronization using GCC-PHAT algorithm
 *
 * Uses Generalized Cross-Correlation with Phase Transform (GCC-PHAT)
 * for robust time-delay estimation between radio and WebSDR audio.
 *
 * GCC-PHAT is the industry standard for TDE in noisy/reverberant environments.
 * It "whitens" signals by normalizing in frequency domain, making it accurate
 * even with weak SSB voice buried in static or noise.
 */
class AudioSync {
public:
    static constexpr int SAMPLE_RATE = 48000;
    static constexpr float CAPTURE_SECONDS = 2.0f;  // 2 second capture window for correlation
    static constexpr int MAX_DELAY_MS = 750;        // Max search window
    static constexpr float MIN_CONFIDENCE = 0.05f;  // Minimum correlation for success

    // Bandpass filter for voice frequencies (300Hz - 3000Hz)
    static constexpr float BANDPASS_LOW_HZ = 300.0f;
    static constexpr float BANDPASS_HIGH_HZ = 3000.0f;

    struct SyncResult {
        float delayMs = 0.0f;
        float confidence = 0.0f;
        bool success = false;
    };

    AudioSync();
    ~AudioSync();

    // Non-copyable
    AudioSync(const AudioSync&) = delete;
    AudioSync& operator=(const AudioSync&) = delete;

    /**
     * @brief Start capturing audio for sync analysis
     */
    void startCapture();

    /**
     * @brief Check if capture is in progress
     */
    bool isCapturing() const { return m_capturing.load(); }

    /**
     * @brief Add samples during capture
     */
    void addSamples(const float* radioSamples, const float* websdrSamples, int count);

    /**
     * @brief Get capture progress (0.0 to 1.0)
     */
    float getProgress() const;

    /**
     * @brief Check if result is ready
     */
    bool hasResult() const { return m_resultReady.load(); }

    /**
     * @brief Get sync result
     */
    SyncResult getResult();

    /**
     * @brief Cancel ongoing capture
     */
    void cancel();

private:
    std::atomic<bool> m_capturing{false};
    std::atomic<bool> m_resultReady{false};
    std::atomic<int> m_capturedSamples{0};

    std::vector<float> m_radioBuffer;
    std::vector<float> m_websdrBuffer;

    SyncResult m_result;
    std::mutex m_mutex;

    std::unique_ptr<std::thread> m_analysisThread;

    int m_targetSamples;
    int m_fftSize;

    // GCC-PHAT implementation
    void analyzeWithGccPhat();

    // FFT operations (Cooley-Tukey radix-2)
    void fft(std::vector<std::complex<float>>& data, bool inverse = false);

    // Bandpass filter (Butterworth-style IIR)
    void applyBandpassFilter(std::vector<float>& signal);

    // Find next power of 2
    static int nextPowerOf2(int n);

    // Compute RMS of signal
    static float computeRMS(const std::vector<float>& signal);
};

#endif // AUDIOSYNC_H
