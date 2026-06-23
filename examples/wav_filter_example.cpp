/**
 * @file wav_filter_example.cpp
 * @brief Example: Apply audio filters to WAV files
 *
 * Demonstrates complete workflow:
 *   1. Load WAV file (mono or stereo)
 *   2. Create IIR or FIR filter
 *   3. Process audio
 *   4. Write filtered output to WAV file
 *
 * **Usage:**
 * ```bash
 * ./wav_filter_example \
 *     --input input.wav \
 *     --output output.wav \
 *     --type iir \
 *     --design butterworth \
 *     --cutoff 1000 \
 *     --order 4
 *
 * ./wav_filter_example \
 *     --input input.wav \
 *     --output output.wav \
 *     --type fir \
 *     --cutoff 5000 \
 *     --ripple 80
 * ```
 */

#include "wav_file.h"
#include "iir_designer.h"
#include "fir_designer.h"
#include "filter_base.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <iomanip>

using namespace audiofilter;

// ===== Command-line parsing =====

struct Options {
    std::string input_file;
    std::string output_file;
    std::string filter_type = "iir";      // "iir" or "fir"
    std::string design_method = "butterworth";  // "butterworth", "chebyshev", etc.
    float cutoff_freq = 1000.0f;
    float ripple_db = 0.5f;
    int order = 4;
    float transition_width = 100.0f;
    bool show_help = false;
};

void printUsage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --input FILE              Input WAV file (required)\n"
              << "  --output FILE             Output WAV file (required)\n"
              << "  --type TYPE               Filter type: iir, fir (default: iir)\n"
              << "  --design METHOD           Design: butterworth, chebyshev (default: butterworth)\n"
              << "  --cutoff FREQ             Cutoff frequency in Hz (default: 1000)\n"
              << "  --order ORDER             IIR filter order (default: 4)\n"
              << "  --ripple DB               Chebyshev ripple in dB (default: 0.5)\n"
              << "  --transition WIDTH        FIR transition width in Hz (default: 100)\n"
              << "  --help                    Show this help message\n";
}

bool parseArguments(int argc, char* argv[], Options& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--input" && i + 1 < argc) {
            opts.input_file = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            opts.output_file = argv[++i];
        } else if (arg == "--type" && i + 1 < argc) {
            opts.filter_type = argv[++i];
        } else if (arg == "--design" && i + 1 < argc) {
            opts.design_method = argv[++i];
        } else if (arg == "--cutoff" && i + 1 < argc) {
            opts.cutoff_freq = std::stof(argv[++i]);
        } else if (arg == "--order" && i + 1 < argc) {
            opts.order = std::stoi(argv[++i]);
        } else if (arg == "--ripple" && i + 1 < argc) {
            opts.ripple_db = std::stof(argv[++i]);
        } else if (arg == "--transition" && i + 1 < argc) {
            opts.transition_width = std::stof(argv[++i]);
        } else if (arg == "--help") {
            opts.show_help = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }

    return true;
}

// ===== Main =====

int main(int argc, char* argv[]) {
    Options opts;

    // Parse arguments
    if (!parseArguments(argc, argv, opts) || opts.show_help) {
        printUsage(argv[0]);
        if (!opts.show_help) return 1;
        return 0;
    }

    // Validate inputs
    if (opts.input_file.empty() || opts.output_file.empty()) {
        std::cerr << "Error: --input and --output are required\n\n";
        printUsage(argv[0]);
        return 1;
    }

    try {
        // ===== Load Input File =====
        std::cout << "Loading: " << opts.input_file << " ... ";
        std::cout.flush();

        WAVFile input(opts.input_file);
        uint32_t sample_rate = input.getSampleRate();
        int num_channels = input.getChannels();
        uint64_t num_frames = input.getNumFrames();

        std::cout << "OK\n"
                  << "  Sample Rate: " << sample_rate << " Hz\n"
                  << "  Channels: " << num_channels << "\n"
                  << "  Frames: " << num_frames << "\n"
                  << "  Duration: " << std::fixed << std::setprecision(2)
                  << input.getDurationSeconds() << " seconds\n\n";

        // ===== Create Filter =====
        std::vector<FilterPtr> filters;

        if (opts.filter_type == "iir") {
            std::cout << "Designing IIR filter:\n"
                      << "  Type: " << opts.design_method << "\n"
                      << "  Cutoff: " << opts.cutoff_freq << " Hz\n"
                      << "  Order: " << opts.order << "\n";

            IIRDesigner designer;

            if (opts.design_method == "butterworth") {
                filters = designer.designButterworthLowpass(
                    sample_rate, opts.cutoff_freq, opts.order);
            } else if (opts.design_method == "chebyshev") {
                std::cout << "  Ripple: " << opts.ripple_db << " dB\n";
                filters = designer.designChebyshevLowpass(
                    sample_rate, opts.cutoff_freq, opts.order, opts.ripple_db);
            } else {
                std::cerr << "Error: Unknown IIR design method: " << opts.design_method << "\n";
                return 1;
            }

            std::cout << "  Filter stages (biquads): " << filters.size() << "\n\n";

        } else if (opts.filter_type == "fir") {
            std::cout << "Designing FIR filter:\n"
                      << "  Cutoff: " << opts.cutoff_freq << " Hz\n"
                      << "  Transition: " << opts.transition_width << " Hz\n"
                      << "  Ripple: " << opts.ripple_db << " dB\n";

            FIRDesigner designer;
            auto taps = designer.designKaiserLowpass(
                sample_rate, opts.cutoff_freq, opts.ripple_db, opts.transition_width);

            std::cout << "  Filter length (taps): " << taps.size() << "\n"
                      << "  Latency: " << std::fixed << std::setprecision(3)
                      << (float)(taps.size() - 1) / 2.0f / sample_rate * 1000.0f
                      << " ms\n\n";

            auto fir = designer.createFIRFilter(taps);
            fir->configure(sample_rate, num_channels);
            filters.push_back(std::move(fir));

        } else {
            std::cerr << "Error: Unknown filter type: " << opts.filter_type << "\n";
            return 1;
        }

        // ===== Process Audio =====
        std::cout << "Processing audio ... ";
        std::cout.flush();

        // Create output file
        WAVFile output(opts.output_file, sample_rate, num_channels);

        // Process in blocks (more efficient than sample-by-sample)
        const size_t block_size = 4096;
        std::vector<float> block_in(block_size * num_channels);
        std::vector<float> block_out(block_size * num_channels);

        uint64_t frames_processed = 0;

        while (frames_processed < num_frames) {
            // Read block
            size_t frames_to_read = std::min(block_size, static_cast<size_t>(num_frames - frames_processed));
            auto input_block = input.readFrames(frames_to_read);

            if (input_block.empty()) break;

            // Copy to working buffer
            std::copy(input_block.begin(), input_block.end(), block_out.begin());

            // Apply filter cascade (each channel separately)
            if (num_channels == 1) {
                // Mono: single filter
                for (auto& filter : filters) {
                    filter->processFrame(block_out.data(), frames_to_read);
                }
            } else {
                // Multi-channel: apply filter to each channel separately
                for (size_t ch = 0; ch < static_cast<size_t>(num_channels); ++ch) {
                    // Extract channel
                    for (size_t i = 0; i < frames_to_read; ++i) {
                        block_in[i] = block_out[i * num_channels + ch];
                    }

                    // Filter channel
                    for (auto& filter : filters) {
                        filter->processFrame(block_in.data(), frames_to_read);
                    }

                    // Put back channel
                    for (size_t i = 0; i < frames_to_read; ++i) {
                        block_out[i * num_channels + ch] = block_in[i];
                    }
                }
            }

            // Write output
            output.writeFrames(block_out.data(), frames_to_read);
            frames_processed += frames_to_read;

            // Progress indicator
            float progress = 100.0f * frames_processed / num_frames;
            std::cout << "\rProcessing audio ... " << std::fixed << std::setprecision(1)
                      << progress << "%   " << std::flush;
        }

        std::cout << "\rProcessing audio ... Done!                 \n\n";

        // ===== Summary =====
        std::cout << "Saved: " << opts.output_file << "\n"
                  << "  Frames written: " << frames_processed << "\n"
                  << "  Duration: " << std::fixed << std::setprecision(2)
                  << (double)frames_processed / sample_rate << " seconds\n\n";

        std::cout << "Success!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
