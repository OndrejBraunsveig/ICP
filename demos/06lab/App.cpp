#include <algorithm>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <vector>
#include <array>
#include <thread>

#include <opencv2/opencv.hpp>

#include "App.h"
#include "src/fps_meter.hpp"

App::App()
{
    // default constructor
    // nothing to do here (so far...)
    std::cout << "Constructed...\n";
}

bool App::init()
{
    try {
        capture = cv::VideoCapture(0, cv::CAP_ANY);
        if (!capture.isOpened())
            throw std::runtime_error("Can not open camera :(");

        std::cout << "Cam opened successfully.\n";

        const double reported_fps = capture.get(cv::CAP_PROP_FPS);
        if (reported_fps > 0.0)
            stream_fps = reported_fps;

        std::cout << "Bandwidth limit: " << bandwidth_limit_mbps
                  << " Mbit/s at " << stream_fps << " FPS.\n"
                  << "Press Q to increase or A to decrease the limit.\n";

    }
    catch (std::exception const& e) {
        std::cerr << "Init failed : " << e.what() << std::endl;
        throw;
    }
    std::cout << "Initialized...\n";
    return true;
}

App::CompressionResult App::lossy_bw_limit(
    cv::Mat& input_img,
    size_t size_limit,
    bool print_progress)
{
    std::string suff(".jpg"); // target format
    if (!cv::haveImageWriter(suff))
        throw std::runtime_error("Can not compress to format:" + suff);

    CompressionResult result;
    std::vector<int> compression_params;

    // prepare parameters for JPEG compressor
    // we use only quality, but other parameters are available (progressive, optimization...)
    std::vector<int> compression_params_template;
    compression_params_template.push_back(cv::IMWRITE_JPEG_QUALITY); 

    if (print_progress)
        std::cout << '[';

    // JPEG size generally grows with quality, so binary search can find the
    // highest quality that fits in about seven attempts instead of up to 20.
    int lowest_quality = 1;
    int highest_quality = 100;
    int fallback_quality = 101;
    std::vector<uchar> fallback_bytes;
    double fallback_encode_ms = 0.0;

    while (lowest_quality <= highest_quality) {
        const int quality = lowest_quality +
            (highest_quality - lowest_quality) / 2;

        compression_params = compression_params_template; // reset parameters
        compression_params.push_back(quality);             // set desired quality
        if (print_progress)
            std::cout << quality << ',';

        // try to encode
        std::vector<uchar> candidate_bytes;
        const auto encode_start = std::chrono::steady_clock::now();
        cv::imencode(suff, input_img, candidate_bytes, compression_params);
        const auto encode_end = std::chrono::steady_clock::now();

        const double candidate_encode_ms =
            std::chrono::duration<double, std::milli>(encode_end - encode_start).count();
        result.total_encode_ms += candidate_encode_ms;
        ++result.attempts;

        // Retain the lowest-quality attempt as a fallback for limits that
        // cannot be reached even at JPEG quality 1.
        if (quality < fallback_quality) {
            fallback_quality = quality;
            fallback_bytes = candidate_bytes;
            fallback_encode_ms = candidate_encode_ms;
        }

        if (candidate_bytes.size() <= size_limit) {
            // This candidate fits. Save it and try a higher quality.
            result.bytes = std::move(candidate_bytes);
            result.final_encode_ms = candidate_encode_ms;
            result.jpeg_quality = quality;
            result.within_size_limit = true;
            lowest_quality = quality + 1;
        }
        else {
            // The candidate is too large, so only lower qualities can fit.
            highest_quality = quality - 1;
        }
    }

    if (!result.within_size_limit) {
        result.bytes = std::move(fallback_bytes);
        result.final_encode_ms = fallback_encode_ms;
        result.jpeg_quality = fallback_quality;
    }

    if (print_progress)
        std::cout << "]\n";

    return result;
}

void App::adjust_bandwidth(double change_mbps)
{
    bandwidth_limit_mbps =
        std::clamp(bandwidth_limit_mbps + change_mbps, 0.1, 100.0);

    std::cout << "Bandwidth limit changed to " << bandwidth_limit_mbps
              << " Mbit/s\n";
}

int App::run(void)
{
    cv::Mat frame;
    fps_meter frame_rate_meter;
    auto last_status_report =
        std::chrono::steady_clock::now() - std::chrono::seconds(10);

    // OpenCV supplies key codes through pollKey(). The map dispatches each
    // supported key to its callback.
    const std::unordered_map<int, std::function<void()>> key_callbacks {
        {'q', [this] { adjust_bandwidth(+0.5); }},
        {'Q', [this] { adjust_bandwidth(+0.5); }},
        {'a', [this] { adjust_bandwidth(-0.5); }},
        {'A', [this] { adjust_bandwidth(-0.5); }}
    };

    try {
        while (capture.isOpened())
        {
            capture >> frame;

            if (frame.empty()) {
                std::cerr << "device closed (or video at the end)" << '\n';
                capture.release();
                break;
            }

            const auto now = std::chrono::steady_clock::now();
            const bool print_status =
                now - last_status_report >= std::chrono::seconds(10);

            // Convert Mbit/s to the maximum number of JPEG bytes per frame:
            // Mbit/s * 1,000,000 bit/Mbit / 8 bit/byte / frames/s.
            auto size_uncompressed = frame.elemSize() * frame.total();
            const auto size_compressed_limit = static_cast<size_t>(
                bandwidth_limit_mbps * 1'000'000.0 / 8.0 / stream_fps);

            //
            // Encode single image with limitation by bandwidth (encoded to original datasize ratio in 0.0 - 1.0)
            //
            const CompressionResult compressed =
                lossy_bw_limit(frame, size_compressed_limit, print_status);
            
            // display compression ratio
            auto size_compressed = compressed.bytes.size();
            const double achieved_mbps =
                size_compressed * 8.0 * stream_fps / 1'000'000.0;

            //
            // decode and display compressed and original data
            //  
            const auto decode_start = std::chrono::steady_clock::now();
            cv::Mat decoded_frame = cv::imdecode(compressed.bytes, cv::IMREAD_ANYCOLOR);
            const auto decode_end = std::chrono::steady_clock::now();

            const double decode_ms =
                std::chrono::duration<double, std::milli>(decode_end - decode_start).count();
            const double asymmetry_ratio =
                decode_ms > 0.0 ? compressed.final_encode_ms / decode_ms : 0.0;

            const std::string bandwidth_text =
                cv::format("Limit %.1f Mbit/s | %.1f FPS | JPEG quality %d | %.2f Mbit/s",
                           bandwidth_limit_mbps,
                           stream_fps,
                           compressed.jpeg_quality,
                           achieved_mbps);
            cv::putText(decoded_frame,
                        bandwidth_text,
                        cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.65,
                        cv::Scalar(255, 0, 0),
                        2,
                        cv::LINE_AA);

            if (print_status) {
                std::cout << std::fixed << std::setprecision(3)
                          << "Size: uncompressed = " << size_uncompressed
                          << ", compressed = " << size_compressed
                          << ", limit = " << size_compressed_limit
                          << " bytes/frame, JPEG quality = " << compressed.jpeg_quality
                          << ", bitrate = " << achieved_mbps << " Mbit/s"
                          << (compressed.within_size_limit
                                  ? "\n"
                                  : " (limit cannot be reached)\n")
                          << "JPEG timing: encode = " << compressed.final_encode_ms
                          << " ms, decode = " << decode_ms
                          << " ms, encode/decode = " << asymmetry_ratio << "x\n"
                          << "Compression search: " << compressed.total_encode_ms
                          << " ms total in " << compressed.attempts << " attempt(s)\n";
                last_status_report = now;
            }

            cv::namedWindow("original");
            cv::imshow("original", frame);

            cv::namedWindow("decoded");
            cv::imshow("decoded", decoded_frame);

            // key handling
            int c = cv::pollKey();
            if (c == 27)
                return EXIT_SUCCESS;

            if (const auto callback = key_callbacks.find(c);
                callback != key_callbacks.end())
                callback->second();

            frame_rate_meter.update();
            if (frame_rate_meter.is_updated()) {
                stream_fps = frame_rate_meter.get();
            }
        }
    }
    catch (std::exception const& e) {
        std::cerr << "App failed : " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Finished OK...\n";
    return EXIT_SUCCESS;
}

App::~App()
{
    // clean-up
    cv::destroyAllWindows();
    std::cout << "Bye...\n";
}
