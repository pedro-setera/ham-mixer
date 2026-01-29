#include "audio/AudioSync.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AudioSync::AudioSync()
    : m_targetSamples(static_cast<int>(SAMPLE_RATE * CAPTURE_SECONDS))
{
    // FFT size must be power of 2, large enough for both signals + max delay
    m_fftSize = nextPowerOf2(m_targetSamples * 2);

    m_radioBuffer.reserve(m_targetSamples);
    m_websdrBuffer.reserve(m_targetSamples);

    qDebug() << "AudioSync initialized: target samples =" << m_targetSamples
             << ", FFT size =" << m_fftSize;
}

AudioSync::~AudioSync()
{
    cancel();
}

int AudioSync::nextPowerOf2(int n)
{
    int power = 1;
    while (power < n) {
        power *= 2;
    }
    return power;
}

float AudioSync::computeRMS(const std::vector<float>& signal)
{
    if (signal.empty()) return 0.0f;

    float sumSq = 0.0f;
    for (const auto& s : signal) {
        sumSq += s * s;
    }
    return std::sqrt(sumSq / signal.size());
}

void AudioSync::startCapture()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_capturing.load()) {
        return;
    }

    // Clear previous data
    m_radioBuffer.clear();
    m_websdrBuffer.clear();
    m_capturedSamples.store(0);
    m_resultReady.store(false);
    m_result = SyncResult();

    m_capturing.store(true);
    qDebug() << "GCC-PHAT audio sync capture started (2 second window)";
}

void AudioSync::addSamples(const float* radioSamples, const float* websdrSamples, int count)
{
    if (!m_capturing.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    int remaining = m_targetSamples - static_cast<int>(m_radioBuffer.size());
    int toAdd = std::min(count, remaining);

    if (toAdd <= 0) {
        return;
    }

    // Add samples to buffers
    m_radioBuffer.insert(m_radioBuffer.end(), radioSamples, radioSamples + toAdd);
    m_websdrBuffer.insert(m_websdrBuffer.end(), websdrSamples, websdrSamples + toAdd);

    m_capturedSamples.store(static_cast<int>(m_radioBuffer.size()));

    // Check if capture is complete
    if (m_radioBuffer.size() >= static_cast<size_t>(m_targetSamples)) {
        m_capturing.store(false);
        qDebug() << "Audio sync capture complete, starting GCC-PHAT analysis...";

        // Start analysis in background thread
        if (m_analysisThread && m_analysisThread->joinable()) {
            m_analysisThread->join();
        }
        m_analysisThread = std::make_unique<std::thread>(&AudioSync::analyzeWithRobustGccPhat, this);
    }
}

float AudioSync::getProgress() const
{
    return static_cast<float>(m_capturedSamples.load()) / m_targetSamples;
}

AudioSync::SyncResult AudioSync::getResult()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_result;
}

void AudioSync::cancel()
{
    m_capturing.store(false);

    if (m_analysisThread && m_analysisThread->joinable()) {
        m_analysisThread->join();
    }
    m_analysisThread.reset();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_radioBuffer.clear();
    m_websdrBuffer.clear();
    m_resultReady.store(false);
}

// Cooley-Tukey radix-2 FFT
void AudioSync::fft(std::vector<std::complex<float>>& data, bool inverse)
{
    const int n = static_cast<int>(data.size());
    if (n <= 1) return;

    // Bit-reversal permutation
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            std::swap(data[i], data[j]);
        }
        int k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }

    // Cooley-Tukey iterative FFT
    for (int len = 2; len <= n; len *= 2) {
        float angle = static_cast<float>(2.0 * M_PI / len) * (inverse ? 1.0f : -1.0f);
        std::complex<float> wLen(std::cos(angle), std::sin(angle));

        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int jj = 0; jj < len / 2; jj++) {
                std::complex<float> u = data[i + jj];
                std::complex<float> t = w * data[i + jj + len / 2];
                data[i + jj] = u + t;
                data[i + jj + len / 2] = u - t;
                w *= wLen;
            }
        }
    }

    // Scale for inverse FFT
    if (inverse) {
        for (auto& x : data) {
            x /= static_cast<float>(n);
        }
    }
}

// Normalize signal to unit variance (addresses volume differences)
void AudioSync::normalizeSignal(std::vector<float>& signal)
{
    if (signal.empty()) return;

    // Remove DC offset
    float mean = std::accumulate(signal.begin(), signal.end(), 0.0f) / signal.size();
    for (auto& s : signal) {
        s -= mean;
    }

    // Normalize to unit variance
    float rms = computeRMS(signal);
    if (rms > 1e-10f) {
        for (auto& s : signal) {
            s /= rms;
        }
    }
}

// Voice Activity Detection - returns mask of active frames
std::vector<bool> AudioSync::detectVoiceActivity(const std::vector<float>& signal)
{
    int numFrames = static_cast<int>(signal.size()) / VAD_FRAME_SIZE;
    std::vector<bool> mask(numFrames, false);

    for (int frame = 0; frame < numFrames; frame++) {
        int start = frame * VAD_FRAME_SIZE;
        int end = std::min(start + VAD_FRAME_SIZE, static_cast<int>(signal.size()));

        // Compute frame energy (RMS)
        float sumSq = 0.0f;
        for (int i = start; i < end; i++) {
            sumSq += signal[i] * signal[i];
        }
        float frameRms = std::sqrt(sumSq / (end - start));

        // Frame is "active" if energy exceeds threshold
        mask[frame] = (frameRms > VAD_THRESHOLD);
    }

    return mask;
}

// Apply VAD mask - zero out inactive frames
void AudioSync::applyVadMask(std::vector<float>& signal, const std::vector<bool>& mask)
{
    int numFrames = static_cast<int>(mask.size());

    for (int frame = 0; frame < numFrames; frame++) {
        if (!mask[frame]) {
            int start = frame * VAD_FRAME_SIZE;
            int end = std::min(start + VAD_FRAME_SIZE, static_cast<int>(signal.size()));
            for (int i = start; i < end; i++) {
                signal[i] = 0.0f;
            }
        }
    }
}

// Compute GCC-PHAT-beta for a frequency band, returns band SNR estimate
float AudioSync::computeBandGccPhat(
    const std::vector<std::complex<float>>& radioFFT,
    const std::vector<std::complex<float>>& websdrFFT,
    int lowBin, int highBin, float beta,
    std::vector<std::complex<float>>& bandGcc)
{
    float totalMagnitude = 0.0f;
    int binCount = 0;

    for (int i = lowBin; i <= highBin && i < static_cast<int>(radioFFT.size()); i++) {
        // Cross-spectrum: WebSDR * conj(Radio)
        std::complex<float> crossSpectrum = websdrFFT[i] * std::conj(radioFFT[i]);
        float magnitude = std::abs(crossSpectrum);

        totalMagnitude += magnitude;
        binCount++;

        if (magnitude > 1e-10f) {
            // GCC-PHAT-beta weighting: divide by magnitude^beta
            // beta=1.0: full PHAT (standard)
            // beta<1.0: reduced whitening, more robust to noise
            float weight = std::pow(magnitude, beta);
            bandGcc[i] = crossSpectrum / weight;
        } else {
            bandGcc[i] = {0.0f, 0.0f};
        }

        // Also handle negative frequencies (mirror in FFT)
        int mirrorIdx = static_cast<int>(radioFFT.size()) - i;
        if (mirrorIdx > 0 && mirrorIdx < static_cast<int>(radioFFT.size()) && mirrorIdx != i) {
            std::complex<float> crossSpectrumMirror = websdrFFT[mirrorIdx] * std::conj(radioFFT[mirrorIdx]);
            float magnitudeMirror = std::abs(crossSpectrumMirror);
            if (magnitudeMirror > 1e-10f) {
                float weight = std::pow(magnitudeMirror, beta);
                bandGcc[mirrorIdx] = crossSpectrumMirror / weight;
            }
        }
    }

    // Return average magnitude as SNR estimate for this band
    return (binCount > 0) ? (totalMagnitude / binCount) : 0.0f;
}

// Find second-highest peak for confidence estimation
float AudioSync::findSecondPeak(const std::vector<std::complex<float>>& gcc,
                                 int bestLag, int minLag, int maxLag)
{
    float secondPeak = 0.0f;
    int exclusionZone = SAMPLE_RATE / 100;  // 10ms exclusion around main peak

    for (int lag = minLag; lag < maxLag && lag < static_cast<int>(gcc.size()) / 2; lag++) {
        // Skip the region around the main peak
        if (std::abs(lag - bestLag) < exclusionZone) continue;

        float value = gcc[lag].real();
        if (value > secondPeak) {
            secondPeak = value;
        }
    }

    return secondPeak;
}

void AudioSync::analyzeWithRobustGccPhat()
{
    qDebug() << "Starting ROBUST GCC-PHAT analysis...";
    qDebug() << "  Improvements: Signal Normalization, VAD, Multiband, PHAT-beta=" << PHAT_BETA;

    std::vector<float> radio;
    std::vector<float> websdr;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        radio = m_radioBuffer;
        websdr = m_websdrBuffer;
    }

    if (radio.empty() || websdr.empty()) {
        qDebug() << "Robust GCC-PHAT: Empty buffers";
        m_resultReady.store(true);
        return;
    }

    // ========== IMPROVEMENT 1: Signal Normalization ==========
    // Equalizes volume differences between channels
    qDebug() << "Step 1: Signal normalization...";
    float radioRmsBefore = computeRMS(radio);
    float websdrRmsBefore = computeRMS(websdr);
    qDebug() << "  Before - Radio RMS:" << radioRmsBefore << ", WebSDR RMS:" << websdrRmsBefore;

    normalizeSignal(radio);
    normalizeSignal(websdr);

    float radioRmsAfter = computeRMS(radio);
    float websdrRmsAfter = computeRMS(websdr);
    qDebug() << "  After  - Radio RMS:" << radioRmsAfter << ", WebSDR RMS:" << websdrRmsAfter;

    if (radioRmsBefore < 0.001f || websdrRmsBefore < 0.001f) {
        qDebug() << "Robust GCC-PHAT: Signal too weak before normalization";
        std::lock_guard<std::mutex> lock(m_mutex);
        m_result.success = false;
        m_result.confidence = 0.0f;
        m_resultReady.store(true);
        return;
    }

    // ========== IMPROVEMENT 2: Voice Activity Detection (VAD) ==========
    // Only correlate segments where both channels have speech
    qDebug() << "Step 2: Voice Activity Detection...";
    std::vector<bool> radioVad = detectVoiceActivity(radio);
    std::vector<bool> websdrVad = detectVoiceActivity(websdr);

    // Count active frames in each channel
    int radioActive = std::count(radioVad.begin(), radioVad.end(), true);
    int websdrActive = std::count(websdrVad.begin(), websdrVad.end(), true);
    int totalFrames = static_cast<int>(radioVad.size());

    qDebug() << "  Radio active frames:" << radioActive << "/" << totalFrames
             << "(" << (100 * radioActive / std::max(1, totalFrames)) << "%)";
    qDebug() << "  WebSDR active frames:" << websdrActive << "/" << totalFrames
             << "(" << (100 * websdrActive / std::max(1, totalFrames)) << "%)";

    // Combined VAD mask - require activity in BOTH channels
    std::vector<bool> combinedVad(totalFrames);
    int bothActive = 0;
    for (int i = 0; i < totalFrames; i++) {
        combinedVad[i] = radioVad[i] && websdrVad[i];
        if (combinedVad[i]) bothActive++;
    }
    qDebug() << "  Both active:" << bothActive << "/" << totalFrames
             << "(" << (100 * bothActive / std::max(1, totalFrames)) << "%)";

    // Apply VAD mask - zero out inactive segments
    applyVadMask(radio, combinedVad);
    applyVadMask(websdr, combinedVad);

    // Check if enough voiced content remains
    if (bothActive < totalFrames / 10) {  // Less than 10% voiced
        qDebug() << "Robust GCC-PHAT: Insufficient voiced content in both channels";
        std::lock_guard<std::mutex> lock(m_mutex);
        m_result.success = false;
        m_result.confidence = 0.0f;
        m_resultReady.store(true);
        return;
    }

    // ========== FFT PREPARATION ==========
    qDebug() << "Step 3: FFT preparation (size" << m_fftSize << ")...";
    std::vector<std::complex<float>> radioFFT(m_fftSize, {0.0f, 0.0f});
    std::vector<std::complex<float>> websdrFFT(m_fftSize, {0.0f, 0.0f});

    for (size_t i = 0; i < radio.size() && i < static_cast<size_t>(m_fftSize); i++) {
        radioFFT[i] = {radio[i], 0.0f};
        websdrFFT[i] = {websdr[i], 0.0f};
    }

    fft(radioFFT, false);
    fft(websdrFFT, false);

    // ========== IMPROVEMENT 3 & 4: Multiband GCC-PHAT-beta ==========
    // Divide spectrum into bands, compute GCC for each, weight by SNR
    qDebug() << "Step 4: Multiband GCC-PHAT-beta analysis (" << NUM_BANDS << " bands)...";

    int lowBin = static_cast<int>(BANDPASS_LOW_HZ * m_fftSize / SAMPLE_RATE);
    int highBin = static_cast<int>(BANDPASS_HIGH_HZ * m_fftSize / SAMPLE_RATE);
    int bandWidth = (highBin - lowBin) / NUM_BANDS;

    // Store GCC results for each band
    std::vector<std::vector<std::complex<float>>> bandGccResults(NUM_BANDS);
    std::vector<float> bandSnr(NUM_BANDS);

    for (int band = 0; band < NUM_BANDS; band++) {
        int bandLow = lowBin + band * bandWidth;
        int bandHigh = (band == NUM_BANDS - 1) ? highBin : (bandLow + bandWidth - 1);

        bandGccResults[band].resize(m_fftSize, {0.0f, 0.0f});
        bandSnr[band] = computeBandGccPhat(radioFFT, websdrFFT, bandLow, bandHigh,
                                           PHAT_BETA, bandGccResults[band]);

        float bandFreqLow = bandLow * SAMPLE_RATE / static_cast<float>(m_fftSize);
        float bandFreqHigh = bandHigh * SAMPLE_RATE / static_cast<float>(m_fftSize);
        qDebug() << "  Band" << band << ":" << bandFreqLow << "-" << bandFreqHigh
                 << "Hz, SNR estimate:" << bandSnr[band];
    }

    // Combine bands with SNR-based weighting
    float totalSnr = 0.0f;
    for (int band = 0; band < NUM_BANDS; band++) {
        totalSnr += bandSnr[band];
    }

    std::vector<std::complex<float>> combinedGcc(m_fftSize, {0.0f, 0.0f});
    for (int i = 0; i < m_fftSize; i++) {
        for (int band = 0; band < NUM_BANDS; band++) {
            // Weight each band by its relative SNR
            float weight = (totalSnr > 1e-10f) ? (bandSnr[band] / totalSnr) : (1.0f / NUM_BANDS);
            combinedGcc[i] += bandGccResults[band][i] * weight;
        }
    }

    // Inverse FFT to get cross-correlation
    qDebug() << "Step 5: Inverse FFT...";
    fft(combinedGcc, true);

    // ========== PEAK FINDING ==========
    qDebug() << "Step 6: Peak detection...";
    int maxDelaySamples = static_cast<int>(MAX_DELAY_MS * SAMPLE_RATE / 1000.0f);
    int minDelaySamples = static_cast<int>(10.0f * SAMPLE_RATE / 1000.0f);

    float maxCorrelation = -1e30f;
    int bestLag = 0;

    // Search positive lags (WebSDR delayed behind Radio)
    for (int lag = minDelaySamples; lag < maxDelaySamples && lag < m_fftSize / 2; lag++) {
        float corrValue = combinedGcc[lag].real();
        if (corrValue > maxCorrelation) {
            maxCorrelation = corrValue;
            bestLag = lag;
        }
    }

    // Also check negative lags
    float maxNegCorrelation = -1e30f;
    int bestNegLag = 0;
    for (int lag = minDelaySamples; lag < maxDelaySamples && lag < m_fftSize / 2; lag++) {
        int idx = m_fftSize - lag;
        if (idx >= 0 && idx < m_fftSize) {
            float corrValue = combinedGcc[idx].real();
            if (corrValue > maxNegCorrelation) {
                maxNegCorrelation = corrValue;
                bestNegLag = lag;
            }
        }
    }

    qDebug() << "  Best positive lag:" << bestLag << "with correlation:" << maxCorrelation;
    qDebug() << "  Best negative lag:" << bestNegLag << "with correlation:" << maxNegCorrelation;

    float finalCorrelation = maxCorrelation;
    int finalLag = bestLag;

    if (maxNegCorrelation > maxCorrelation * 1.5f) {
        qDebug() << "  Warning: Using negative lag (unusual configuration)";
        finalCorrelation = maxNegCorrelation;
        finalLag = bestNegLag;
    }

    // ========== IMPROVED CONFIDENCE ESTIMATION ==========
    // Use peak-to-second-peak ratio instead of just peak-to-average
    float secondPeak = findSecondPeak(combinedGcc, finalLag, minDelaySamples, maxDelaySamples);

    // Peak-to-second-peak ratio (better confidence metric)
    float peakToSecondRatio = (secondPeak > 1e-10f) ? (finalCorrelation / secondPeak) : 10.0f;

    // Also compute peak-to-average for backup
    float sumCorr = 0.0f;
    int countCorr = 0;
    for (int i = minDelaySamples; i < maxDelaySamples && i < m_fftSize / 2; i++) {
        sumCorr += std::abs(combinedGcc[i].real());
        countCorr++;
    }
    float avgCorr = (countCorr > 0) ? (sumCorr / countCorr) : 1e-10f;
    float peakToAvgRatio = (avgCorr > 1e-10f) ? (finalCorrelation / avgCorr) : 0.0f;

    // Combined confidence: use the more conservative of the two metrics
    // Peak-to-second ratio > 2.0 is good, > 3.0 is excellent
    float conf1 = std::min(1.0f, std::max(0.0f, (peakToSecondRatio - 1.0f) / 3.0f));
    // Peak-to-average ratio > 5 is good, > 10 is excellent
    float conf2 = std::min(1.0f, std::max(0.0f, (peakToAvgRatio - 1.0f) / 9.0f));
    float confidence = std::min(conf1, conf2);

    // Convert lag to milliseconds
    float delayMs = static_cast<float>(finalLag) * 1000.0f / SAMPLE_RATE;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_result.delayMs = delayMs;
        m_result.confidence = confidence;
        m_result.success = (confidence >= MIN_CONFIDENCE) && (finalCorrelation > 0.001f);
    }

    m_resultReady.store(true);

    qDebug() << "=== ROBUST GCC-PHAT COMPLETE ===";
    qDebug() << "  Delay:" << delayMs << "ms (" << finalLag << " samples)";
    qDebug() << "  Peak correlation:" << finalCorrelation;
    qDebug() << "  Second peak:" << secondPeak;
    qDebug() << "  Peak-to-second ratio:" << peakToSecondRatio;
    qDebug() << "  Peak-to-average ratio:" << peakToAvgRatio;
    qDebug() << "  Confidence:" << (confidence * 100.0f) << "%";
    qDebug() << "  Success:" << m_result.success;
}
