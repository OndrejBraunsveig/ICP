#pragma once

#include <optional>
#include <atomic>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/objdetect/face.hpp>
#include <opencv2/videoio.hpp>

class App {
public:
    App() = default;
    ~App();

    bool init();
    int run();

private:
    std::optional<cv::Point2f> find_face(const cv::Mat& frame);
    void tracker_thread(cv::VideoCapture & capture);
    void draw_cross_normalized(
        cv::Mat& image,
        cv::Point2f center_normalized,
        int size);
    cv::VideoCapture capture;
    std::stop_source tracker_stop_source_;

    std::atomic<bool> no_more_frames{false};
    std::atomic<bool> face_found{false};

    struct NormalizedCoordinates
    {
        float x;
        float y;
    };
    std::atomic<std::shared_ptr<const cv::Mat>> latest_frame{
    nullptr
};

    std::atomic<NormalizedCoordinates> normalized_coordinates{
        NormalizedCoordinates{0.0f, 0.0f}};
    
    cv::Ptr<cv::FaceDetectorYN> face_detector;
};