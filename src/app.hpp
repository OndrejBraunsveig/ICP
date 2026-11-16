#pragma once

#include <optional>
#include <atomic>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/objdetect/face.hpp>
#include <opencv2/videoio.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

class App {
    public:
    App() = default;
    ~App();
    
    bool init();
    int run();
    
    private:
    bool init_camera();
    void run_camera();
    void print_GLEW_info();
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
    bool is_vsync_on{true};
        
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
   
    //Callbacks
    static void error_callback(int error, const char* description);
    static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
    static void APIENTRY gl_debug_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam);
};