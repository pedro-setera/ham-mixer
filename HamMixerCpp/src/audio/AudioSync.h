#ifndef AUDIOSYNC_H
#define AUDIOSYNC_H

#include <vector>
#include <complex>
#include <atomic>
#include <mutex>
#include <thread>
#include <memory>

/**
 * @brief Robust audio synchronization using enhanced GCC-PHAT algorithm
 *
 * Uses Generalized Cross-Correlation with Phase Transform (GCC-PHAT)
 * with multiple robustness improvements for challenging ham radio conditions:
 *
 * 1. Signal Normalization - equalizes volume differences
 * 2. Voice Activity Detection (VAD) - only correlates speech segments
 * 3. Multiband Analysis - weights reliable frequency bands
 * 4. GCC-PHAT-beta - adjustable whitening for noise robustness
 *
 * Handles QSB (fading), QRM (interference), and volume differences.
 */
class AudioSync {
public:
    static constexpr int SAMPLE_RATE = 48000;
    static constexpr float CAPTURE_SECONDS = 2.0f;  // 2 second capture window for correlation
    static constexpr int MAX_DELAY_MS = 2000;       // Max search window (matches delay slider)
    static constexpr float MIN_CONFIDENCE = 0.05f;  // Minimum correlation for success

    // Bandpass filter for voice frequencies (300Hz - 3000Hz)
    static constexpr float BANDPASS_LOW_HZ = 300.0f;
    static constexpr float BANDPASS_HIGH_HZ = 3000.0f;

    // === ROBUSTNESS PARAMETERS ===

    // GCC-PHAT-beta: Controls whitening strength (1.0 = full PHAT, 0.0 = standard CC)
    // Lower values (0.5-0.7) are more robust to noise
    static constexpr float PHAT_BETA = 0.7f;

    // Multiband analysis: number of frequency sub-bands
    static constexpr int NUM_BANDS = 4;

    // Voice Activity Detection: minimum RMS threshold for "active" frame
    static constexpr float VAD_THRESHOLD = 0.01f;

    // VAD frame size in samples (20ms frames)
    static constexpr int VAD_FRAME_SIZE = SAMPLE_RATE / 50;

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

    // Main analysis with all robustness improvements
    void analyzeWithRobustGccPhat();

    // FFT operations (Cooley-Tukey radix-2)
    void fft(std::vector<std::complex<float>>& data, bool inverse = false);

    // Find next power of 2
    static int nextPowerOf2(int n);

    // Compute RMS of signal
    static float computeRMS(const std::vector<float>& signal);

    // === ROBUSTNESS HELPERS ===

    // Normalize signal to unit variance
    static void normalizeSignal(std::vector<float>& signal);

    // Voice Activity Detection - returns mask of active frames
    std::vector<bool> detectVoiceActivity(const std::vector<float>& signal);

    // Apply VAD mask to extract only voiced segments
    void applyVadMask(std::vector<float>& signal, const std::vector<bool>& mask);

    // Compute GCC-PHAT-beta for a single frequency band
    float computeBandGccPhat(
        const std::vector<std::complex<float>>& radioFFT,
        const std::vector<std::complex<float>>& websdrFFT,
        int lowBin, int highBin, float beta,
        std::vector<std::complex<float>>& bandGcc);

    // Find second-highest peak for confidence estimation
    float findSecondPeak(const std::vector<std::complex<float>>& gcc,
                         int bestLag, int minLag, int maxLag);
};

#endif // AUDIOSYNC_H
