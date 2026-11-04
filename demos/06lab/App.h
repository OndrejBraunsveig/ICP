#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

class App {
public:
    struct CompressionResult {
        std::vector<uchar> bytes;
        double final_encode_ms = 0.0;
        double total_encode_ms = 0.0;
        int attempts = 0;
        int jpeg_quality = 0;
        bool within_size_limit = false;
    };

    App();
    bool init();
    int run();
    ~App();

private:
    struct RawFrame {
        cv::Mat image;
        std::uint64_t sequence = 0;
    };

    struct EncodedFrame {
        std::shared_ptr<const RawFrame> source;
        CompressionResult compression;
        std::size_t uncompressed_size = 0;
        std::size_t size_limit = 0;
        double camera_fps = 0.0;
        double bandwidth_limit_mbps = 0.0;
    };

    struct DecodedFrame {
        std::shared_ptr<const EncodedFrame> encoded;
        cv::Mat image;
        double decode_ms = 0.0;
    };

    CompressionResult lossy_bw_limit(
        const cv::Mat& input_img,
        std::size_t size_limit);

    void grabber_loop(std::stop_token stop_token);
    void encoder_loop(std::stop_token stop_token);
    void decoder_loop(std::stop_token stop_token);
    void adjust_bandwidth(double change_mbps);
    void fail_pipeline(const std::string& message);

    cv::VideoCapture capture;

    std::atomic<double> bandwidth_limit_mbps {5.0};
    std::atomic<double> camera_fps {30.0};
    std::atomic<int> jpeg_quality_hint {100};

    std::mutex raw_mutex;
    std::condition_variable_any raw_ready;
    std::shared_ptr<const RawFrame> latest_raw;

    std::mutex encoded_mutex;
    std::condition_variable_any encoded_ready;
    std::shared_ptr<const EncodedFrame> latest_encoded;

    std::mutex decoded_mutex;
    std::shared_ptr<const DecodedFrame> latest_decoded;

    std::atomic<bool> grabber_finished {false};
    std::atomic<bool> encoder_finished {false};
    std::atomic<bool> decoder_finished {false};
    std::atomic<bool> pipeline_failed {false};

    std::mutex error_mutex;
    std::string pipeline_error;
};
