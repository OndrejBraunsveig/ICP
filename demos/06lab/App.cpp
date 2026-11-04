#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <utility>

#include <opencv2/opencv.hpp>

#include "App.h"
#include "JpegEncoder.h"
#include "src/fps_meter.hpp"

using namespace std::chrono_literals;

App::App()
{
    std::cout << "Constructed...\n";
}

bool App::init()
{
    try {
        capture.open(0, cv::CAP_ANY);
        if (!capture.isOpened())
            throw std::runtime_error("Can not open camera :(");

        if (!cv::haveImageWriter(".jpg"))
            throw std::runtime_error("Can not compress to JPEG");

        const double reported_fps = capture.get(cv::CAP_PROP_FPS);
        if (reported_fps > 0.0)
            camera_fps.store(reported_fps);

        std::cout << "Cam opened successfully.\n"
                  << "Bandwidth limit: " << bandwidth_limit_mbps.load()
                  << " Mbit/s at " << camera_fps.load() << " FPS.\n"
                  << "Press Q to increase or A to decrease the limit.\n";
    }
    catch (const std::exception& error) {
        std::cerr << "Init failed: " << error.what() << '\n';
        throw;
    }

    std::cout << "Initialized...\n";
    return true;
}

App::CompressionResult App::lossy_bw_limit(
    const cv::Mat& input_img,
    std::size_t size_limit)
{
    CompressionResult result;

    int fallback_quality = 101;
    std::vector<uchar> fallback_bytes;
    double fallback_encode_ms = 0.0;

    const int initial_quality =
        std::clamp(jpeg_quality_hint.load(), 1, 100);
    jpeg::EncodedCandidate initial =
        jpeg::encode_at_quality(input_img, initial_quality);

    result.total_encode_ms += initial.encode_ms;
    ++result.attempts;
    fallback_quality = initial.quality;
    fallback_bytes = initial.bytes;
    fallback_encode_ms = initial.encode_ms;

    if (initial.bytes.size() <= size_limit) {
        result.bytes = std::move(initial.bytes);
        result.final_encode_ms = initial.encode_ms;
        result.jpeg_quality = initial.quality;
        result.within_size_limit = true;
    }
    else {
        int lowest_quality = 1;
        int highest_quality = initial_quality - 1;

        while (lowest_quality <= highest_quality) {
            const int quality = lowest_quality +
                (highest_quality - lowest_quality) / 2;
            jpeg::EncodedCandidate candidate =
                jpeg::encode_at_quality(input_img, quality);

            result.total_encode_ms += candidate.encode_ms;
            ++result.attempts;

            if (quality < fallback_quality) {
                fallback_quality = quality;
                fallback_bytes = candidate.bytes;
                fallback_encode_ms = candidate.encode_ms;
            }

            if (candidate.bytes.size() <= size_limit) {
                result.bytes = std::move(candidate.bytes);
                result.final_encode_ms = candidate.encode_ms;
                result.jpeg_quality = candidate.quality;
                result.within_size_limit = true;
                lowest_quality = quality + 1;
            }
            else {
                highest_quality = quality - 1;
            }
        }
    }

    if (!result.within_size_limit) {
        result.bytes = std::move(fallback_bytes);
        result.final_encode_ms = fallback_encode_ms;
        result.jpeg_quality = fallback_quality;
    }

    jpeg_quality_hint.store(result.jpeg_quality);
    return result;
}

void App::grabber_loop(std::stop_token stop_token)
{
    try {
        fps_meter frame_rate_meter;
        std::uint64_t sequence = 0;

        while (!stop_token.stop_requested()) {
            cv::Mat frame;
            if (!capture.read(frame) || frame.empty())
                break;

            auto packet = std::make_shared<RawFrame>();
            packet->image = std::move(frame);
            packet->sequence = ++sequence;

            {
                std::lock_guard lock(raw_mutex);
                latest_raw = std::move(packet);
            }
            raw_ready.notify_one();

            frame_rate_meter.update();
            if (frame_rate_meter.is_updated())
                camera_fps.store(frame_rate_meter.get());
        }
    }
    catch (const std::exception& error) {
        fail_pipeline(std::string("Grabber failed: ") + error.what());
    }

    grabber_finished.store(true);
    raw_ready.notify_all();
}

void App::encoder_loop(std::stop_token stop_token)
{
    try {
        std::uint64_t last_sequence = 0;
        auto last_quality_refresh = std::chrono::steady_clock::now();

        while (!stop_token.stop_requested()) {
            std::shared_ptr<const RawFrame> raw;
            {
                std::unique_lock lock(raw_mutex);
                raw_ready.wait(lock, stop_token, [&] {
                    return pipeline_failed.load()
                        || grabber_finished.load()
                        || (latest_raw && latest_raw->sequence > last_sequence);
                });

                if (stop_token.stop_requested() || pipeline_failed.load())
                    break;
                if (!latest_raw || latest_raw->sequence <= last_sequence) {
                    if (grabber_finished.load())
                        break;
                    continue;
                }

                raw = latest_raw;
                last_sequence = raw->sequence;
            }

            const auto now = std::chrono::steady_clock::now();
            if (now - last_quality_refresh >= 3s) {
                jpeg_quality_hint.store(100);
                last_quality_refresh = now;
            }

            const double fps = std::max(camera_fps.load(), 1.0);
            const double limit_mbps = bandwidth_limit_mbps.load();
            const auto byte_limit = static_cast<std::size_t>(
                limit_mbps * 1'000'000.0 / 8.0 / fps);

            auto encoded = std::make_shared<EncodedFrame>();
            encoded->source = std::move(raw);
            encoded->uncompressed_size =
                encoded->source->image.elemSize() * encoded->source->image.total();
            encoded->size_limit = byte_limit;
            encoded->camera_fps = fps;
            encoded->bandwidth_limit_mbps = limit_mbps;
            encoded->compression =
                lossy_bw_limit(encoded->source->image, byte_limit);

            {
                std::lock_guard lock(encoded_mutex);
                latest_encoded = std::move(encoded);
            }
            encoded_ready.notify_one();
        }
    }
    catch (const std::exception& error) {
        fail_pipeline(std::string("Encoder failed: ") + error.what());
    }

    encoder_finished.store(true);
    encoded_ready.notify_all();
}

void App::decoder_loop(std::stop_token stop_token)
{
    try {
        std::uint64_t last_sequence = 0;

        while (!stop_token.stop_requested()) {
            std::shared_ptr<const EncodedFrame> encoded;
            {
                std::unique_lock lock(encoded_mutex);
                encoded_ready.wait(lock, stop_token, [&] {
                    return pipeline_failed.load()
                        || encoder_finished.load()
                        || (latest_encoded
                            && latest_encoded->source->sequence > last_sequence);
                });

                if (stop_token.stop_requested() || pipeline_failed.load())
                    break;
                if (!latest_encoded
                    || latest_encoded->source->sequence <= last_sequence) {
                    if (encoder_finished.load())
                        break;
                    continue;
                }

                encoded = latest_encoded;
                last_sequence = encoded->source->sequence;
            }

            auto decoded = std::make_shared<DecodedFrame>();
            decoded->encoded = std::move(encoded);

            const auto decode_start = std::chrono::steady_clock::now();
            decoded->image = cv::imdecode(
                decoded->encoded->compression.bytes,
                cv::IMREAD_ANYCOLOR);
            const auto decode_end = std::chrono::steady_clock::now();
            decoded->decode_ms =
                std::chrono::duration<double, std::milli>(
                    decode_end - decode_start).count();

            {
                std::lock_guard lock(decoded_mutex);
                latest_decoded = std::move(decoded);
            }
        }
    }
    catch (const std::exception& error) {
        fail_pipeline(std::string("Decoder failed: ") + error.what());
    }

    decoder_finished.store(true);
}

void App::adjust_bandwidth(double change_mbps)
{
    const double updated = std::clamp(
        bandwidth_limit_mbps.load() + change_mbps,
        0.1,
        100.0);
    bandwidth_limit_mbps.store(updated);
    jpeg_quality_hint.store(100);

    std::cout << "Bandwidth limit changed to " << updated << " Mbit/s\n";
}

void App::fail_pipeline(const std::string& message)
{
    {
        std::lock_guard lock(error_mutex);
        if (pipeline_error.empty())
            pipeline_error = message;
    }
    pipeline_failed.store(true);
    raw_ready.notify_all();
    encoded_ready.notify_all();
}

int App::run()
{
    grabber_finished.store(false);
    encoder_finished.store(false);
    decoder_finished.store(false);
    pipeline_failed.store(false);

    cv::namedWindow("original");
    cv::namedWindow("decoded");

    std::jthread grabber([this](std::stop_token stop_token) {
        grabber_loop(stop_token);
    });
    std::jthread encoder([this](std::stop_token stop_token) {
        encoder_loop(stop_token);
    });
    std::jthread decoder([this](std::stop_token stop_token) {
        decoder_loop(stop_token);
    });

    const std::unordered_map<int, std::function<void()>> key_callbacks {
        {'q', [this] { adjust_bandwidth(+0.5); }},
        {'Q', [this] { adjust_bandwidth(+0.5); }},
        {'a', [this] { adjust_bandwidth(-0.5); }},
        {'A', [this] { adjust_bandwidth(-0.5); }}
    };

    fps_meter output_rate_meter;
    double output_fps = 0.0;
    std::uint64_t displayed_sequence = 0;
    std::uint64_t dropped_frames = 0;
    auto last_status_report =
        std::chrono::steady_clock::now() - 10s;

    while (!pipeline_failed.load()) {
        std::shared_ptr<const DecodedFrame> decoded;
        {
            std::lock_guard lock(decoded_mutex);
            decoded = latest_decoded;
        }

        if (decoded
            && decoded->encoded->source->sequence > displayed_sequence) {
            const std::uint64_t sequence =
                decoded->encoded->source->sequence;
            if (displayed_sequence == 0)
                dropped_frames += sequence - 1;
            else
                dropped_frames += sequence - displayed_sequence - 1;
            displayed_sequence = sequence;

            output_rate_meter.update();
            if (output_rate_meter.is_updated())
                output_fps = output_rate_meter.get();

            const EncodedFrame& encoded = *decoded->encoded;
            const CompressionResult& compressed = encoded.compression;
            const double achieved_mbps =
                compressed.bytes.size() * 8.0 * encoded.camera_fps / 1'000'000.0;
            const double frame_budget_ms = 1'000.0 / encoded.camera_fps;
            const bool realtime = compressed.total_encode_ms < frame_budget_ms;
            const double asymmetry_ratio = decoded->decode_ms > 0.0
                ? compressed.final_encode_ms / decoded->decode_ms
                : 0.0;

            cv::Mat decoded_display = decoded->image.clone();
            const std::string bandwidth_text = cv::format(
                "Limit %.1f Mbit/s | Camera %.1f FPS | JPEG Q %d | %.2f Mbit/s",
                encoded.bandwidth_limit_mbps,
                encoded.camera_fps,
                compressed.jpeg_quality,
                achieved_mbps);
            const std::string realtime_text = cv::format(
                "Encode %.2f / %.2f ms | Output %.1f FPS | %s",
                compressed.total_encode_ms,
                frame_budget_ms,
                output_fps,
                realtime ? "REALTIME" : "TOO SLOW");

            cv::putText(decoded_display, bandwidth_text, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6,
                        cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
            cv::putText(decoded_display, realtime_text, cv::Point(10, 60),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6,
                        realtime ? cv::Scalar(0, 255, 0)
                                 : cv::Scalar(0, 0, 255),
                        2, cv::LINE_AA);

            cv::imshow("original", decoded->encoded->source->image);
            cv::imshow("decoded", decoded_display);

            const auto now = std::chrono::steady_clock::now();
            if (now - last_status_report >= 5s) {
                std::cout << std::fixed << std::setprecision(3)
                          << "Frame " << sequence
                          << ": camera = " << encoded.camera_fps
                          << " FPS, output = " << output_fps
                          << " FPS, dropped = " << dropped_frames << '\n'
                          << "Size: uncompressed = " << encoded.uncompressed_size
                          << ", compressed = " << compressed.bytes.size()
                          << ", limit = " << encoded.size_limit
                          << " bytes/frame, JPEG quality = "
                          << compressed.jpeg_quality
                          << ", bitrate = " << achieved_mbps << " Mbit/s"
                          << (compressed.within_size_limit
                                  ? "\n"
                                  : " (limit cannot be reached)\n")
                          << "JPEG timing: selected encode = "
                          << compressed.final_encode_ms
                          << " ms, search total = " << compressed.total_encode_ms
                          << " ms, decode = " << decoded->decode_ms
                          << " ms, encode/decode = " << asymmetry_ratio << "x\n"
                          << "Realtime budget: " << frame_budget_ms << " ms -> "
                          << (realtime ? "PASS" : "FAIL")
                          << ", attempts = " << compressed.attempts << "\n";
                last_status_report = now;
            }
        }

        const int key = cv::pollKey();
        if (key == 27)
            break;
        if (const auto callback = key_callbacks.find(key);
            callback != key_callbacks.end())
            callback->second();

        if (decoder_finished.load()
            && (!decoded
                || decoded->encoded->source->sequence <= displayed_sequence))
            break;

        std::this_thread::sleep_for(1ms);
    }

    grabber.request_stop();
    encoder.request_stop();
    decoder.request_stop();
    raw_ready.notify_all();
    encoded_ready.notify_all();

    if (pipeline_failed.load()) {
        std::lock_guard lock(error_mutex);
        std::cerr << pipeline_error << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Finished OK...\n";
    return EXIT_SUCCESS;
}

App::~App()
{
    if (capture.isOpened())
        capture.release();
    cv::destroyAllWindows();
    std::cout << "Bye...\n";
}
