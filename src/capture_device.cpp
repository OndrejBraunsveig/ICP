// icp.cpp 
// Author: JJ

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <opencv2/opencv.hpp>
#include <opencv2/geometry.hpp>

class App {
public:
    App() = default;
    ~App();

    bool init(void);
    int run(void);

    void draw_cross_normalized(cv::Mat& img, cv::Point2f center_relative, int size);
private:
    std::optional<cv::Point2f> find_object_chroma(const cv::Mat& frame);
    cv::VideoCapture capture;  // global variable, move to app class, protected
};

//=====================================================================================================

bool App::init(void)
{
    //open first available camera, using any API available (autodetect) 
    capture = cv::VideoCapture(0, cv::CAP_ANY);
    
    //open video file
    //capture = cv::VideoCapture("video.mkv");

    if (!capture.isOpened())
    { 
        std::cerr << "no source?" << std::endl;
        return false;
    }
    else
    {
        std::cout << "Source: " << 
            ": width=" << capture.get(cv::CAP_PROP_FRAME_WIDTH) <<
            ", height=" << capture.get(cv::CAP_PROP_FRAME_HEIGHT) << '\n';
    }
    return true;
}

int App::run(void)
{
    cv::Mat frame, scene;
    std::optional<cv::Point2f> last_center_normalized;

    while (1)
    {
        capture.read(frame);
        if (frame.empty())
        {
            std::cerr << "Cam disconnected? End of file?\n";
            break;
        }

        // show grabbed frame
        cv::imshow("grabbed", frame);

        // create copy of the frame
        cv::Mat frame2;
        frame.copyTo(frame2);

        // Find the red object. If this frame has no detection, keep the last one.
        const std::optional<cv::Point2f> detected_center = find_object_chroma(frame2);
        if (detected_center)
            last_center_normalized = detected_center;

        if (last_center_normalized)
        {
            const cv::Point2f center(
                last_center_normalized->x * frame.cols,
                last_center_normalized->y * frame.rows);

            std::cout << "Center absolute: " << center << '\n';
            std::cout << "Center normalized: " << *last_center_normalized;
            if (!detected_center)
                std::cout << " (last known position)";
            std::cout << '\n';

            draw_cross_normalized(frame2, *last_center_normalized, 25);
        }
        else
        {
            std::cout << "No red object detected yet.\n";
        }

        // show me the result
        cv::namedWindow("frame2");
        cv::imshow("frame2", frame2);

        // keep application open until ESC is pressed
        int key = cv::pollKey(); // poll OS events (key press, mouse move, ...)
        if (key == 27) // test for ESC key
            break;
    }

    return EXIT_SUCCESS;
}

App::~App()
{
    // clean-up
    cv::destroyAllWindows();
    std::cout << "Bye...\n";

    if (capture.isOpened())
        capture.release();
}

std::optional<cv::Point2f> App::find_object_chroma(const cv::Mat& frame)
{
    // convert to HSV color space
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // Red wraps around the ends of OpenCV's hue range (0..179), so use two masks.
    cv::Mat lower_red_mask;
    cv::Mat upper_red_mask;
    cv::Mat mask;
    cv::inRange(hsv, cv::Scalar(0, 60, 40), cv::Scalar(15, 255, 255), lower_red_mask);
    cv::inRange(hsv, cv::Scalar(165, 60, 40), cv::Scalar(179, 255, 255), upper_red_mask);
    cv::bitwise_or(lower_red_mask, upper_red_mask, mask);

    // Remove isolated pixels and fill small gaps in the detected red area.
    const cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    // find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // find the largest contour
    double max_area = 0;
    int max_index = -1;
    for (size_t i = 0; i < contours.size(); i++)
    {
        double area = cv::contourArea(contours[i]);
        if (area > max_area)
        {
            max_area = area;
            max_index = i;
        }
    }

    // No detection is different from an object located at the image center.
    if (max_index == -1)
        return std::nullopt;

    // compute the centroid of the largest contour
    cv::Moments m = cv::moments(contours[max_index]);
    if (m.m00 == 0.0)
        return std::nullopt;

    return cv::Point2f(static_cast<float>(m.m10 / m.m00) / frame.cols,
                       static_cast<float>(m.m01 / m.m00) / frame.rows);
}

void App::draw_cross_normalized(cv::Mat& img, cv::Point2f center_normalized, int size)
{
    center_normalized.x = std::clamp(center_normalized.x, 0.0f, 1.0f);
    center_normalized.y = std::clamp(center_normalized.y, 0.0f, 1.0f);
    size = std::clamp(size, 1, std::min(img.cols, img.rows));

    cv::Point2f center_absolute(center_normalized.x * img.cols, center_normalized.y * img.rows);

    cv::Point2f p1(center_absolute.x - size / 2, center_absolute.y);
    cv::Point2f p2(center_absolute.x + size / 2, center_absolute.y);
    cv::Point2f p3(center_absolute.x, center_absolute.y - size / 2);
    cv::Point2f p4(center_absolute.x, center_absolute.y + size / 2);

    cv::line(img, p1, p2, CV_RGB(255, 0, 0), 3);
    cv::line(img, p3, p4, CV_RGB(255, 0, 0), 3);
}

int main()
{
    App app;

    if (!app.init())
        return EXIT_FAILURE;

    return app.run();
}


