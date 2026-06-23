#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>

/**
 * @file wav_file.h
 * @brief Simple WAV file I/O wrapper using libsndfile
 *
 * Provides convenient C++ interface for reading and writing WAV files.
 * Hides libsndfile complexity behind a clean API.
 */

// Forward declaration matching libsndfile's own typedef (sndfile.h),
// so this header doesn't need to include sndfile.h directly.
typedef struct sf_private_tag SNDFILE;

namespace audiofilter {

/**
 * @class WAVFile
 * @brief Read and write WAV audio files
 *
 * Uses libsndfile for robust audio file handling.
 * Supports mono, stereo, and multi-channel audio.
 *
 * **Usage - Reading:**
 * ```cpp
 * WAVFile input("audio.wav");
 * uint32_t sr = input.getSampleRate();
 * int channels = input.getChannels();
 * uint64_t frames = input.getNumFrames();
 *
 * auto samples = input.readFrames(1024);  // Read 1024 frames
 * ```
 *
 * **Usage - Writing:**
 * ```cpp
 * WAVFile output("output.wav", 48000, 2);  // 48kHz stereo
 * std::vector<float> samples = {...};
 * output.writeFrames(samples.data(), samples.size() / 2);
 * ```
 */
class WAVFile {
public:
    // ===== Reading =====

    /**
     * @brief Open a WAV file for reading
     *
     * @param filename Path to WAV file
     *
     * @throw std::runtime_error if file cannot be opened or is not valid WAV
     */
    explicit WAVFile(const std::string& filename);

    /**
     * @brief Constructor for writing (creates new file)
     *
     * @param filename Output filename
     * @param sample_rate Sample rate in Hz (e.g., 44100, 48000)
     * @param num_channels Number of channels (1=mono, 2=stereo, etc.)
     *
     * @throw std::runtime_error if file cannot be created
     */
    WAVFile(const std::string& filename, uint32_t sample_rate, int num_channels);

    /**
     * @brief Destructor - closes file if open
     */
    ~WAVFile();

    // ===== File Information =====

    /**
     * @brief Get sample rate of the file
     *
     * @return Sample rate in Hz
     */
    uint32_t getSampleRate() const noexcept {
        return m_sample_rate;
    }

    /**
     * @brief Get number of channels
     *
     * @return Number of channels (1=mono, 2=stereo, etc.)
     */
    int getChannels() const noexcept {
        return m_num_channels;
    }

    /**
     * @brief Get total number of frames in file
     *
     * Frame = one sample per channel (e.g., one stereo pair = 1 frame)
     *
     * @return Number of frames (0 if writing new file)
     */
    uint64_t getNumFrames() const noexcept {
        return m_num_frames;
    }

    /**
     * @brief Get total duration in seconds
     *
     * @return Duration = num_frames / sample_rate
     */
    double getDurationSeconds() const noexcept {
        return static_cast<double>(m_num_frames) / m_sample_rate;
    }

    /**
     * @brief Check if file is open for reading
     *
     * @return true if opened for read, false otherwise
     */
    bool isOpen() const noexcept {
        return m_file_handle != nullptr;
    }

    // ===== Reading =====

    /**
     * @brief Read frames from file
     *
     * Reads num_frames frames of interleaved audio.
     * Returns fewer frames if end-of-file is reached.
     *
     * @param num_frames Number of frames to read
     *
     * @return Vector of interleaved samples (size = num_frames * num_channels)
     *         May be smaller than requested if EOF reached
     *
     * @throw std::runtime_error if file not opened for reading
     *
     * **Example - Stereo:**
     * ```cpp
     * WAVFile input("stereo.wav");
     * auto samples = input.readFrames(1024);
     * // samples.size() == 1024 * 2 == 2048 (stereo samples)
     * ```
     */
    std::vector<float> readFrames(uint64_t num_frames);

    /**
     * @brief Read all remaining frames from current position
     *
     * Reads until end-of-file.
     *
     * @return Vector of all remaining interleaved samples
     *
     * @throw std::runtime_error if file not opened for reading
     */
    std::vector<float> readAll();

    /**
     * @brief Seek to a specific frame in the file
     *
     * @param frame_pos Frame position (0 = start of file)
     *
     * @throw std::runtime_error if seek fails
     */
    void seekFrame(uint64_t frame_pos);

    /**
     * @brief Get current read position
     *
     * @return Current frame position
     */
    uint64_t getCurrentFrame() const noexcept;

    // ===== Writing =====

    /**
     * @brief Write frames to file
     *
     * Writes num_frames frames of interleaved audio.
     * File must be opened in write mode.
     *
     * @param samples Pointer to interleaved sample data
     * @param num_frames Number of frames to write
     *
     * @throw std::runtime_error if file not opened for writing
     * @throw std::invalid_argument if samples is nullptr
     *
     * **Example - Stereo:**
     * ```cpp
     * WAVFile output("output.wav", 48000, 2);
     * float samples[2048];  // 1024 stereo samples
     * output.writeFrames(samples, 1024);
     * ```
     */
    void writeFrames(const float* samples, uint64_t num_frames);

    /**
     * @brief Write frames from vector
     *
     * Convenience overload for std::vector.
     *
     * @param samples Vector of interleaved samples
     */
    void writeFrames(const std::vector<float>& samples) {
        if (samples.empty()) return;
        writeFrames(samples.data(), samples.size() / m_num_channels);
    }

    // ===== File Control =====

    /**
     * @brief Close the file
     *
     * Automatically called by destructor, but can be called explicitly.
     */
    void close() noexcept;

private:
    // libsndfile file handle
    SNDFILE* m_file_handle = nullptr;

    // File metadata
    uint32_t m_sample_rate = 0;
    int m_num_channels = 0;
    uint64_t m_num_frames = 0;

    // Mode: true=read, false=write
    bool m_read_mode = true;

    /**
     * @brief Check if file is open
     *
     * @throw std::runtime_error if not open
     */
    void ensureOpen(const std::string& operation) const;
};

}  // namespace audiofilter
