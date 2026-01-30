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
 * 2. Voice Activity Detection (VAD) - only correlates speech segments (Voice mode)
 * 3. Multiband Analysis - weights reliable frequency bands
 * 4. GCC-PHAT-beta - adjustable whitening for noise robustness
 * 5. Envelope Correlation - pitch-independent for CW mode
 *
 * Handles QSB (fading), QRM (interference), and volume differences.
 * Automatically adapts to Voice (SSB/AM/FM) or CW mode based on radio setting.
 */
class AudioSync {
public:
    /**
     * @brief Signal mode for sync algorithm selection
     * Automatically determined from radio's current operating mode
     */
    enum SignalMode {
        VOICE,  // SSB, AM, FM - uses VAD, wider bandpass (300-3000 Hz)
        CW      // CW, RTTY - no VAD, narrower bandpass (400-1000 Hz), envelope correlation
    };

    static constexpr int SAMPLE_RATE = 48000;
    static constexpr float CAPTURE_SECONDS = 1.5f;  // 1.5 second capture - sweet spot for short CQ calls
    static constexpr float CAPTURE_SECONDS_CW = 3.0f;  // 3.0 seconds for CW - longer to capture unique patterns and avoid false matches
    static constexpr int MAX_DELAY_MS = 2000;       // Max search window (matches delay slider)
    static constexpr float MIN_CONFIDENCE = 0.05f;  // Minimum correlation for success

    // Bandpass filter for voice frequencies (300Hz - 3000Hz)
    static constexpr float BANDPASS_LOW_HZ = 300.0f;
    static constexpr float BANDPASS_HIGH_HZ = 3000.0f;

    // Bandpass filter for CW ENVELOPE frequencies (after Hilbert transform)
    // The envelope of CW is a baseband signal with energy at the keying rate
    // Typical CW speeds (15-25 WPM) produce keying patterns at 10-30 Hz
    // Use low frequency range to capture the on/off keying pattern
    static constexpr float CW_ENVELOPE_LOW_HZ = 1.0f;    // Near DC
    static constexpr float CW_ENVELOPE_HIGH_HZ = 100.0f; // Captures keying harmonics

    // === ROBUSTNESS PARAMETERS ===

    // GCC-PHAT-beta: Controls whitening strength (1.0 = full PHAT, 0.0 = standard CC)
    // Lower values (0.5-0.7) are more robust to noise
    static constexpr float PHAT_BETA = 0.7f;

    // Multiband analysis: number of frequency sub-bands
    static constexpr int NUM_BANDS = 4;

    // Voice Activity Detection: minimum RMS threshold for "active" frame
    // Lower threshold (0.005) accepts quieter frames for weak signal detection
    static constexpr float VAD_THRESHOLD = 0.005f;

    // VAD frame size in samples (20ms frames)
    static constexpr int VAD_FRAME_SIZE = SAMPLE_RATE / 50;

    struct SyncResult {
        float delayMs = 0.0f;       // Signed: positive = WebSDR behind (add delay), negative = WebSDR ahead (reduce delay)
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
     * @param mode Signal mode (VOICE or CW) - determines algorithm parameters
     */
    void startCapture(SignalMode mode = VOICE);

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

    // Current signal mode (Voice or CW) - set at capture start
    SignalMode m_signalMode{VOICE};

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

    // Envelope extraction using Hilbert transform
    // More robust to phase distortions from QSB/fading
    void extractEnvelope(std::vector<float>& signal);
};

#endif // AUDIOSYNC_H
