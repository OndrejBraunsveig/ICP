//
// library demonstrator
//

#include <iostream>
#include <fstream>
#include <filesystem>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

//#include <opencv4/opencv2/opencv.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

#include <nlohmann/json.hpp>

int main()
{
    // console output
    {
    std::cout << "Hello!\n";
    }

    // GL Math
    {
        glm::vec3 test{ 0.0 };
        std::cout << "Hello World! " << glm::to_string(test) << "\n";
    }

    // std::filesystem
    {
        std::cout << "Current working directory: " << std::filesystem::current_path().generic_string() << '\n';
        
        if (!std::filesystem::exists("resources"))
            throw std::runtime_error("Directory 'resources' not found. Various media files are expected to be there.");
    }

    // JSON
    {
        std::ifstream sett_file("resources/app_settings.json");
        nlohmann::json settings = nlohmann::json::parse(sett_file);

        std::cout << settings["appname"] << '\n';

        // getting value - safely
        int x = 0;
        if (settings["default_resolution"]["x"].is_number_integer()) {
            // key found and value is proper type, use safe conversion            
            x = settings["default_resolution"]["x"].template get<int>();
        }
        else {
            // key not found or wrong data type, use default
            x = 800;
        }

        // getting value - throws exception in any problems...
        int y = settings["default_resolution"]["y"];

        std::cout << "[x,y] = [" << x << ',' << y << "]\n";
    }

    // OpenCV
    {
        // Disable OpenCV info messages = display only warning and more severe 
        cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

        // try to read image
        cv::Mat image = cv::imread("resources/lightbulb.jpg");
        std::cout << "Image size: " << image.cols << ',' << image.rows << '\n';

    // try to open first camera of any type and grab single image
        cv::Mat x(cv::Size(100,100), CV_8UC3); // RGB-888 image
	    auto cam = cv::VideoCapture(0, cv::CAP_ANY);
	    if (cam.isOpened())
	        cam >> x;

        // demo: use OpenCV
        cv::namedWindow("test_window");
	    cv::imshow("test_window", x);
	    cv::pollKey();
    }

    // GLFW + GLEW
    {
        if (!glfwInit())
            exit(100);

        GLFWwindow* w = glfwCreateWindow(800, 600, "test", nullptr, nullptr);

        if (!w) {
            glfwTerminate();
            exit(111);
        }

        glfwMakeContextCurrent(w);

        auto s = glewInit();
        if (s != GLEW_OK) {
            glfwDestroyWindow(w);
            glfwTerminate();
            exit(123);
        }

        std::cout << glewGetString(GLEW_VERSION) << '\n';

        while (!glfwWindowShouldClose(w)) {
            if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(w, GLFW_TRUE);

            glfwPollEvents();
        }

        glfwDestroyWindow(w);
        glfwTerminate();
    }

    std::cout << "Bye!\n";
    return 0;
}

