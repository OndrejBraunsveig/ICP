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

App::CompressionResult App::lossy_bw_limit(cv::Mat& input_img, size_t size_limit)
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

    std::cout << '[';

    //try step-by-step to decrease quality by 5%, until it fits into limit
    for (auto i = 100; i > 0; i -= 5) {
        compression_params = compression_params_template; // reset parameters
        compression_params.push_back(i);                  // set desired quality
        std::cout << i << ',';

        // try to encode
        const auto encode_start = std::chrono::steady_clock::now();
        cv::imencode(suff, input_img, result.bytes, compression_params);
        const auto encode_end = std::chrono::steady_clock::now();

        result.final_encode_ms =
            std::chrono::duration<double, std::milli>(encode_end - encode_start).count();
        result.total_encode_ms += result.final_encode_ms;
        ++result.attempts;
        result.jpeg_quality = i;

        // check the size limit
        if (result.bytes.size() <= size_limit) {
            result.within_size_limit = true;
            break; // ok, done 
        }
    }
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

            // Convert Mbit/s to the maximum number of JPEG bytes per frame:
            // Mbit/s * 1,000,000 bit/Mbit / 8 bit/byte / frames/s.
            auto size_uncompressed = frame.elemSize() * frame.total();
            const auto size_compressed_limit = static_cast<size_t>(
                bandwidth_limit_mbps * 1'000'000.0 / 8.0 / stream_fps);

            //
            // Encode single image with limitation by bandwidth (encoded to original datasize ratio in 0.0 - 1.0)
            //
            const CompressionResult compressed =
                lossy_bw_limit(frame, size_compressed_limit);
            
            // display compression ratio
            auto size_compressed = compressed.bytes.size();
            const double achieved_mbps =
                size_compressed * 8.0 * stream_fps / 1'000'000.0;
            std::cout << "Size: uncompressed = " << size_uncompressed
                      << ", compressed = " << size_compressed
                      << ", limit = " << size_compressed_limit
                      << " bytes/frame, JPEG quality = " << compressed.jpeg_quality
                      << ", bitrate = " << achieved_mbps << " Mbit/s"
                      << (compressed.within_size_limit ? "\n" : " (limit cannot be reached)\n");

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

            std::cout << std::fixed << std::setprecision(3)
                      << "JPEG timing: encode = " << compressed.final_encode_ms
                      << " ms, decode = " << decode_ms
                      << " ms, encode/decode = " << asymmetry_ratio << "x\n"
                      << "Compression search: " << compressed.total_encode_ms
                      << " ms total in " << compressed.attempts << " attempt(s)\n";

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
                std::cout << "Measured processing rate: " << stream_fps
                          << " FPS\n";
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
