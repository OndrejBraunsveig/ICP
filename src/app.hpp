#pragma once

#include <optional>

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
    void draw_cross_normalized(
        cv::Mat& image,
        cv::Point2f center_normalized,
        int size);

    cv::VideoCapture capture;
    cv::Ptr<cv::FaceDetectorYN> face_detector;
};