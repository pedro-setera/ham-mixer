#ifndef RECORDER_H
#define RECORDER_H

#include <QString>
#include <QFile>
#include <QDataStream>
#include <QElapsedTimer>
#include <atomic>
#include <mutex>
#include <cstdint>

/**
 * @brief WAV file recorder for mixed audio output
 *
 * Records stereo 16-bit PCM audio to WAV files with
 * auto-timestamped filenames.
 */
class Recorder {
public:
    static constexpr int SAMPLE_RATE = 48000;
    static constexpr int CHANNELS = 2;
    static constexpr int BITS_PER_SAMPLE = 16;

    /**
     * @brief Construct recorder
     * @param sampleRate Audio sample rate
     * @param channels Number of channels
     */
    Recorder(int sampleRate = SAMPLE_RATE, int channels = CHANNELS);
    ~Recorder();

    // Non-copyable
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    /**
     * @brief Set recording directory
     * @param directory Path to save recordings
     */
    void setRecordingDirectory(const QString& directory);

    /**
     * @brief Get recording directory
     */
    QString recordingDirectory() const { return m_recordingDir; }

    /**
     * @brief Set sample rate (must be called before recording starts)
     * @param sampleRate Audio sample rate in Hz
     */
    void setSampleRate(int sampleRate) { m_sampleRate = sampleRate; }

    /**
     * @brief Start recording
     * @return Filename of the new recording, or empty on failure
     */
    QString startRecording();

    /**
     * @brief Stop recording and finalize file
     */
    void stopRecording();

    /**
     * @brief Write audio samples to file
     * @param samples Interleaved stereo samples (int16_t)
     * @param frameCount Number of frames
     */
    void writeSamples(const int16_t* samples, int frameCount);

    /**
     * @brief Check if recording is active
     */
    bool isRecording() const { return m_recording.load(); }

    /**
     * @brief Get elapsed recording time in seconds
     */
    float getElapsedTime() const;

    /**
     * @brief Get elapsed time as formatted string (HH:MM:SS)
     */
    QString getElapsedTimeFormatted() const;

    /**
     * @brief Get current file size in bytes
     */
    qint64 getFileSize() const;

    /**
     * @brief Get file size as formatted string
     */
    QString getFileSizeFormatted() const;

    /**
     * @brief Get current recording filename
     */
    QString currentFilename() const { return m_currentFilename; }

    /**
     * @brief Check available disk space
     * @return Available space in MB
     */
    qint64 checkDiskSpace() const;

    /**
     * @brief Check if disk space is low (<500MB)
     */
    bool isDiskSpaceLow() const;

private:
    int m_sampleRate;
    int m_channels;
    QString m_recordingDir;
    QString m_currentFilename;

    QFile* m_file = nullptr;
    std::atomic<bool> m_recording{false};
    std::atomic<qint64> m_sampleCount{0};
    qint64 m_dataSize = 0;
    std::mutex m_mutex;

    // Real-time elapsed timer for consistent display updates
    QElapsedTimer m_elapsedTimer;

    // WAV file helpers
    void writeWavHeader();
    void finalizeWavHeader();
    QString generateFilename() const;
};

#endif // RECORDER_H
