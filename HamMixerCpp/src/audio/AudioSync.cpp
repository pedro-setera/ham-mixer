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
        m_analysisThread = std::make_unique<std::thread>(&AudioSync::analyzeWithGccPhat, this);
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

// Apply bandpass filter in frequency domain (300Hz - 3000Hz)
void AudioSync::applyBandpassFilter(std::vector<float>& signal)
{
    // This is now a placeholder - we'll do frequency domain filtering in analyzeWithGccPhat
    // Just remove DC offset here
    float mean = std::accumulate(signal.begin(), signal.end(), 0.0f) / signal.size();
    for (auto& s : signal) {
        s -= mean;
    }
}

void AudioSync::analyzeWithGccPhat()
{
    qDebug() << "Starting GCC-PHAT analysis...";

    std::vector<float> radio;
    std::vector<float> websdr;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        radio = m_radioBuffer;
        websdr = m_websdrBuffer;
    }

    if (radio.empty() || websdr.empty()) {
        qDebug() << "GCC-PHAT: Empty buffers";
        m_resultReady.store(true);
        return;
    }

    // Step 1: Remove DC offset
    float radioMean = std::accumulate(radio.begin(), radio.end(), 0.0f) / radio.size();
    float websdrMean = std::accumulate(websdr.begin(), websdr.end(), 0.0f) / websdr.size();

    for (auto& s : radio) s -= radioMean;
    for (auto& s : websdr) s -= websdrMean;

    // Check signal levels
    float radioRMS = computeRMS(radio);
    float websdrRMS = computeRMS(websdr);

    qDebug() << "Signal levels - Radio RMS:" << radioRMS << ", WebSDR RMS:" << websdrRMS;

    if (radioRMS < 0.001f || websdrRMS < 0.001f) {
        qDebug() << "GCC-PHAT: Signal too weak";
        std::lock_guard<std::mutex> lock(m_mutex);
        m_result.success = false;
        m_result.confidence = 0.0f;
        m_resultReady.store(true);
        return;
    }

    // Step 2: Prepare FFT buffers (zero-pad to FFT size)
    std::vector<std::complex<float>> radioFFT(m_fftSize, {0.0f, 0.0f});
    std::vector<std::complex<float>> websdrFFT(m_fftSize, {0.0f, 0.0f});

    for (size_t i = 0; i < radio.size() && i < static_cast<size_t>(m_fftSize); i++) {
        radioFFT[i] = {radio[i], 0.0f};
        websdrFFT[i] = {websdr[i], 0.0f};
    }

    // Step 3: Perform FFT on both signals
    qDebug() << "Performing FFT (size" << m_fftSize << ")...";
    fft(radioFFT, false);
    fft(websdrFFT, false);

    // Step 4: Apply frequency-domain bandpass filter (300Hz - 3000Hz)
    // and compute GCC-PHAT
    // Frequency bin k corresponds to frequency: k * sampleRate / fftSize
    int lowBin = static_cast<int>(BANDPASS_LOW_HZ * m_fftSize / SAMPLE_RATE);
    int highBin = static_cast<int>(BANDPASS_HIGH_HZ * m_fftSize / SAMPLE_RATE);

    qDebug() << "Bandpass bins:" << lowBin << "to" << highBin
             << "(frequencies" << BANDPASS_LOW_HZ << "to" << BANDPASS_HIGH_HZ << "Hz)";

    std::vector<std::complex<float>> gccPhat(m_fftSize, {0.0f, 0.0f});

    for (int i = 0; i < m_fftSize; i++) {
        // Check if this frequency bin is within the bandpass range
        // For real signals, we need to consider both positive and negative frequencies
        int freqBin = (i <= m_fftSize / 2) ? i : (m_fftSize - i);

        bool inBand = (freqBin >= lowBin && freqBin <= highBin);

        if (!inBand) {
            gccPhat[i] = {0.0f, 0.0f};
            continue;
        }

        // Cross-spectrum: WebSDR * conj(Radio)
        // WebSDR is delayed relative to Radio, so this gives positive lag for the delay
        std::complex<float> crossSpectrum = websdrFFT[i] * std::conj(radioFFT[i]);

        // PHAT weighting: normalize by magnitude
        float magnitude = std::abs(crossSpectrum);
        if (magnitude > 1e-10f) {
            gccPhat[i] = crossSpectrum / magnitude;
        } else {
            gccPhat[i] = {0.0f, 0.0f};
        }
    }

    // Step 5: Inverse FFT to get cross-correlation
    qDebug() << "Performing inverse FFT...";
    fft(gccPhat, true);

    // Step 6: Find peak in valid delay range
    // We're looking for positive lags where WebSDR is behind Radio
    // Positive lags: indices 0 to maxDelaySamples correspond to delays 0 to MAX_DELAY_MS
    int maxDelaySamples = static_cast<int>(MAX_DELAY_MS * SAMPLE_RATE / 1000.0f);

    // Minimum delay to search (skip first 10ms to avoid artifacts at lag 0)
    int minDelaySamples = static_cast<int>(10.0f * SAMPLE_RATE / 1000.0f);  // 10ms minimum

    float maxCorrelation = -1e30f;  // Start with very negative to find actual peak
    int bestLag = 0;

    qDebug() << "Searching lags from" << minDelaySamples << "to" << maxDelaySamples << "samples";

    // Search positive lags (WebSDR delayed behind Radio)
    // These are at indices 0 to maxDelaySamples in the IFFT result
    for (int lag = minDelaySamples; lag < maxDelaySamples && lag < m_fftSize / 2; lag++) {
        float corrValue = gccPhat[lag].real();
        if (corrValue > maxCorrelation) {
            maxCorrelation = corrValue;
            bestLag = lag;
        }
    }

    // Also check if maybe the correlation is in the "negative" lag region
    // (wrapped around at the end of the FFT result)
    // This would mean Radio is behind WebSDR (unusual but check anyway)
    float maxNegCorrelation = -1e30f;
    int bestNegLag = 0;
    for (int lag = minDelaySamples; lag < maxDelaySamples && lag < m_fftSize / 2; lag++) {
        int idx = m_fftSize - lag;
        if (idx >= 0 && idx < m_fftSize) {
            float corrValue = gccPhat[idx].real();
            if (corrValue > maxNegCorrelation) {
                maxNegCorrelation = corrValue;
                bestNegLag = lag;
            }
        }
    }

    qDebug() << "Best positive lag:" << bestLag << "with correlation:" << maxCorrelation;
    qDebug() << "Best negative lag:" << bestNegLag << "with correlation:" << maxNegCorrelation;

    // Use whichever has better correlation
    // But prefer positive lag since WebSDR should be behind Radio
    float finalCorrelation = maxCorrelation;
    int finalLag = bestLag;

    if (maxNegCorrelation > maxCorrelation * 1.5f) {
        // Only use negative lag if it's significantly better
        qDebug() << "Warning: Negative lag has better correlation - unusual configuration";
        finalCorrelation = maxNegCorrelation;
        finalLag = bestNegLag;
    }

    // Calculate confidence based on peak sharpness
    // Find the average correlation value for comparison
    float sumCorr = 0.0f;
    int countCorr = 0;
    for (int i = minDelaySamples; i < maxDelaySamples && i < m_fftSize / 2; i++) {
        sumCorr += std::abs(gccPhat[i].real());
        countCorr++;
    }
    float avgCorr = (countCorr > 0) ? (sumCorr / countCorr) : 1e-10f;

    // Peak-to-average ratio
    float peakRatio = (avgCorr > 1e-10f) ? (finalCorrelation / avgCorr) : 0.0f;

    // Normalize confidence (ratio of 5+ is good, 10+ is excellent)
    float confidence = std::min(1.0f, std::max(0.0f, (peakRatio - 1.0f) / 9.0f));

    // Convert lag to milliseconds
    float delayMs = static_cast<float>(finalLag) * 1000.0f / SAMPLE_RATE;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_result.delayMs = delayMs;
        m_result.confidence = confidence;
        m_result.success = (confidence >= MIN_CONFIDENCE) && (finalCorrelation > 0.001f);
    }

    m_resultReady.store(true);

    qDebug() << "GCC-PHAT complete:";
    qDebug() << "  - Best lag:" << finalLag << "samples =" << delayMs << "ms";
    qDebug() << "  - Correlation value:" << finalCorrelation;
    qDebug() << "  - Average correlation:" << avgCorr;
    qDebug() << "  - Peak ratio:" << peakRatio;
    qDebug() << "  - Confidence:" << (confidence * 100.0f) << "%";
    qDebug() << "  - Success:" << m_result.success;
}
