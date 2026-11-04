#pragma once
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

    bool init(void);
    
    CompressionResult lossy_bw_limit(
        cv::Mat& input_img,
        size_t size_limit,
        bool print_progress);

    int run(void);

    ~App();
private:
    void adjust_bandwidth(double change_mbps);

    cv::VideoCapture capture;
    double bandwidth_limit_mbps = 5.0;
    double stream_fps = 30.0;
};

