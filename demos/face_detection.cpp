#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect/face.hpp>

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

bool App::init()
{
    constexpr const char* model_path =
        "resources/face_detection_yunet_2023mar.onnx";

    try
    {
        face_detector = cv::FaceDetectorYN::create(
            model_path,
            "",
            cv::Size(320, 320),
            0.7f,
            0.3f,
            5000);
    }
    catch (const cv::Exception& error)
    {
        std::cerr << "Could not load face detector: " << error.what() << '\n';
        return false;
    }

    if (!face_detector)
    {
        std::cerr << "Could not load face detector model: " << model_path << '\n';
        return false;
    }

    capture.open(0, cv::CAP_ANY);
    if (!capture.isOpened())
    {
        std::cerr << "Could not open camera.\n";
        return false;
    }

    std::cout << "Camera: width=" << capture.get(cv::CAP_PROP_FRAME_WIDTH)
              << ", height=" << capture.get(cv::CAP_PROP_FRAME_HEIGHT) << '\n';
    return true;
}

int App::run()
{
    cv::Mat frame;

    while (true)
    {
        if (!capture.read(frame) || frame.empty())
        {
            std::cerr << "Camera disconnected or returned an empty frame.\n";
            return EXIT_FAILURE;
        }

        const std::optional<cv::Point2f> center = find_face(frame);

        cv::Mat result = frame.clone();
        if (center)
        {
            std::cout << "Found normalized center: " << *center << '\n';
            draw_cross_normalized(result, *center, 30);
        }
        else
        {
            std::cout << "No face detected.\n";
        }

        cv::imshow("Face detection", result);

        if (cv::pollKey() == 27)
            break;
    }

    return EXIT_SUCCESS;
}

std::optional<cv::Point2f> App::find_face(const cv::Mat& frame)
{
    face_detector->setInputSize(frame.size());

    cv::Mat faces;
    face_detector->detect(frame, faces);

    if (faces.empty())
        return std::nullopt;

    // Each row contains x, y, width, height, facial landmarks, and confidence.
    int largest_face_index = 0;
    float largest_area = 0.0f;

    for (int row = 0; row < faces.rows; ++row)
    {
        const float width = faces.at<float>(row, 2);
        const float height = faces.at<float>(row, 3);
        const float area = width * height;

        if (area > largest_area)
        {
            largest_area = area;
            largest_face_index = row;
        }
    }

    const float x = faces.at<float>(largest_face_index, 0);
    const float y = faces.at<float>(largest_face_index, 1);
    const float width = faces.at<float>(largest_face_index, 2);
    const float height = faces.at<float>(largest_face_index, 3);

    return cv::Point2f(
        (x + width / 2.0f) / static_cast<float>(frame.cols),
        (y + height / 2.0f) / static_cast<float>(frame.rows));
}

void App::draw_cross_normalized(
    cv::Mat& image,
    cv::Point2f center_normalized,
    int size)
{
    center_normalized.x = std::clamp(center_normalized.x, 0.0f, 1.0f);
    center_normalized.y = std::clamp(center_normalized.y, 0.0f, 1.0f);
    size = std::clamp(size, 1, std::min(image.cols, image.rows));

    cv::Point2f center_absolute(center_normalized.x * image.cols, center_normalized.y * image.rows);

    cv::Point2f p1(center_absolute.x - size / 2, center_absolute.y);
    cv::Point2f p2(center_absolute.x + size / 2, center_absolute.y);
    cv::Point2f p3(center_absolute.x, center_absolute.y - size / 2);
    cv::Point2f p4(center_absolute.x, center_absolute.y + size / 2);

    cv::line(image, p1, p2, CV_RGB(255, 0, 0), 3);
    cv::line(image, p3, p4, CV_RGB(255, 0, 0), 3);
}

App::~App()
{
    if (capture.isOpened())
        capture.release();

    cv::destroyAllWindows();
}

int main()
{
    App app;

    if (!app.init())
        return EXIT_FAILURE;

    return app.run();
}
