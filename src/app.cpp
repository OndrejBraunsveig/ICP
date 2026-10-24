#include <thread>
#include <optional>
#include <atomic>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <iostream>

#include <opencv2/opencv.hpp>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "fps_meter.hpp"
#include "app.hpp"

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
void App::tracker_thread(cv::VideoCapture & capture)
{
    cv::Mat frame;
    while (!tracker_stop_source_.stop_requested())
    {
        if (!capture.read(frame) || frame.empty())
        {
            std::cerr << "Camera disconnected or returned an empty frame.\n";
            no_more_frames = true;
            break;
        }

        const std::optional<cv::Point2f> center = find_face(frame);
        if (center)
        {
          face_found = true; 
          latest_frame = std::make_shared<const cv::Mat>(frame.clone());
          normalized_coordinates = NormalizedCoordinates{center->x, center->y};
        }
        else
        {
            face_found = false;
        }
    }
}

int App::run()
{
  fps_meter FPS;
  std::jthread tracker_thread(&App::tracker_thread, this, std::ref(capture));
  while(true){
    
    std::optional<cv::Point2f> center;
    const auto frame = latest_frame.load();
    cv::Mat result = frame ? *frame : cv::Mat::zeros(480, 640, CV_8UC3);
    if (face_found)
    {
      std::cout << "Face found at normalized coordinates: "
      << normalized_coordinates.load().x << ", "
      << normalized_coordinates.load().y << '\n';
      center = cv::Point2f(normalized_coordinates.load().x, normalized_coordinates.load().y);
      draw_cross_normalized(result, *center, 30);
    }
    else
    {
      std::cout << "No face detected.\n";
    }
    cv::imshow("Face detection", result);
    
    if (FPS.is_updated()) // display new value only once per interval (default = 1.0s)
			std::cout << "FPS: " << FPS.get() << std::endl;
            
		FPS.update();

    if (cv::pollKey() == 27)
      break;
  }   

  tracker_stop_source_.request_stop();
  return EXIT_SUCCESS;
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

App::~App()
{
    if (capture.isOpened())
        capture.release();

    cv::destroyAllWindows();
}
