#include "app.hpp"

#include <thread>
#include <optional>
#include <atomic>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <stack>
#include <random>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "fps_meter.hpp"
#include "assets.hpp"

namespace {
GLuint compile_shader(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE)
        return shader;

    char log[1024]{};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::cerr << "Triangle shader compilation failed: " << log << '\n';
    glDeleteShader(shader);
    return 0;
}
}

bool App::init_camera(){
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

void App::run_camera(){
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
}
void App::error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}
void APIENTRY App::gl_debug_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    std::cerr << "OpenGL debug message [" << id << "]: "
              << message << '\n';
}
void App::glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos){
    std::cout << "Mouse moved to: (" << xpos << ", " << ypos << ")\n";
}
void App::glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    std::cout << "Mouse scrolled: (" << xoffset << ", " << yoffset << ")\n";
}
void App::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods){
    if (action == GLFW_PRESS)
        std::cout << "Mouse button pressed: " << button << '\n';
    else if (action == GLFW_RELEASE)
        std::cout << "Mouse button released: " << button << '\n';
}
void App::glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height){
    std::cout << "Framebuffer resized to: " << width << "x" << height << '\n';
}
void App::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    App* this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!this_inst)
    return;
    if ((action == GLFW_PRESS) || (action == GLFW_REPEAT))
    {
        switch (key)
        {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_V:
           this_inst->is_vsync_on = !this_inst->is_vsync_on;
			glfwSwapInterval(this_inst->is_vsync_on);
			std::cout << "VSync: " << this_inst->is_vsync_on << "\n";
            break;  
        default:
            break;
        }
    }
}
void App::print_GLEW_info()
{
    const GLubyte* vendor = (const GLubyte*)glGetString(GL_VENDOR);

    if (vendor == nullptr)
        std::cout << "<Unknown>\n";
    else 
        std::cout << "Vendor:" << vendor << '\n';

    const GLubyte* renderer = (const GLubyte*)glGetString(GL_RENDERER);

    if (renderer == nullptr)
        std::cout << "<Unknown>\n";
    else 
        std::cout << "Renderer:" << renderer << '\n';

    const GLubyte* version = (const GLubyte*)glGetString(GL_VERSION);

    if (version == nullptr)
        std::cout << "<Unknown>\n";
    else 
        std::cout << "Version:" << version << '\n';

    const GLubyte* shading_language_version = (const GLubyte*)glGetString(GL_SHADING_LANGUAGE_VERSION);

    if (shading_language_version == nullptr)
        std::cout << "<Unknown>\n";
    else 
        std::cout << "Shading Language Version:" << shading_language_version << '\n';

    GLint gl_major_version;
    glGetIntegerv(GL_MAJOR_VERSION, &gl_major_version);

    std::cout << "Value of gl_major_version:" << gl_major_version << '\n';

    GLint gl_minor_version;
    glGetIntegerv(GL_MINOR_VERSION, &gl_minor_version);

    std::cout << "Value of gl_minor_version:" << gl_minor_version << '\n';


    GLint gl_context_profile_info;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &gl_context_profile_info);

    if ( gl_context_profile_info & GL_CONTEXT_CORE_PROFILE_BIT ) {
        std::cout << "We are using CORE profile\n";
    } else {
        if ( gl_context_profile_info & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) {    
            std::cout << "We are using COMPATIBILITY profile\n";
        } else {
                throw std::runtime_error("What??");
        }
    }

    GLint context_flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &context_flags);

    if (context_flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT)
        std::cout << "Forward-compatible context\n";

    if (context_flags & GL_CONTEXT_FLAG_DEBUG_BIT)
        std::cout << "Debug context\n";

    if (context_flags & GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT)
        std::cout << "Robust access enabled\n";

    if (context_flags & GL_CONTEXT_FLAG_NO_ERROR_BIT)
        std::cout << "No-error context\n";
}

bool App::init()
{
    
    /* Initialize the library */
    if (!glfwInit())
        return false;
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "OpenGL - Window created", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return false;
    }
    glfwSetWindowUserPointer(window, this);
    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    
    // initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Could not initialize GLEW.\n";
        return false;
    }
    //Allow OpenGL debug output
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_debug_callback, nullptr);
    //Register callbacks
    glfwSetErrorCallback(error_callback);
    glfwSetKeyCallback(window, glfw_key_callback);
    
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
    glfwSetCursorPosCallback(window, glfw_cursor_position_callback);
    glfwSetScrollCallback(window, glfw_scroll_callback);
    
    // vsync
    glfwSwapInterval(is_vsync_on?1:0);

    
    //print glew info of the current window
    print_GLEW_info();
    
    return init_assets();
}

bool App::init_assets()
{
    constexpr auto vertex_source = R"(#version 460 core
layout(location = 0) in vec3 position;
uniform mat4 transform;
void main()
{
    gl_Position = transform * vec4(position, 1.0);
}
)";
    constexpr auto fragment_source = R"(#version 460 core
out vec4 color;
void main()
{
    color = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

    const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex_shader || !fragment_shader)
    {
        if (vertex_shader) glDeleteShader(vertex_shader);
        if (fragment_shader) glDeleteShader(fragment_shader);
        return false;
    }

    shader_prog_ID = glCreateProgram();
    glAttachShader(shader_prog_ID, vertex_shader);
    glAttachShader(shader_prog_ID, fragment_shader);
    glLinkProgram(shader_prog_ID);
    glDetachShader(shader_prog_ID, vertex_shader);
    glDetachShader(shader_prog_ID, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(shader_prog_ID, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE)
    {
        char log[1024]{};
        glGetProgramInfoLog(shader_prog_ID, sizeof(log), nullptr, log);
        std::cerr << "Triangle shader program linking failed: " << log << '\n';
        return false;
    }

    glCreateBuffers(1, &VBO_ID);
    glNamedBufferData(VBO_ID, triangle_vertices.size() * sizeof(vertex),
                      triangle_vertices.data(), GL_STATIC_DRAW);

    glCreateVertexArrays(1, &VAO_ID);
    glVertexArrayVertexBuffer(VAO_ID, 0, VBO_ID, 0, sizeof(vertex));
    glVertexArrayAttribFormat(VAO_ID, 0, 3, GL_FLOAT, GL_FALSE, offsetof(vertex, position));
    glVertexArrayAttribBinding(VAO_ID, 0, 0);
    glEnableVertexArrayAttrib(VAO_ID, 0);

    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    return true;
}
int App::run()
{
    fps_meter FPS;
    const GLint transform_location = glGetUniformLocation(shader_prog_ID, "transform");
    const auto model_view = glm::rotate(
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f)),
        glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0)
        {
            glfwWaitEvents();
            FPS.reset();
            continue;
        }

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const glm::mat4 transform = glm::perspective(
            glm::radians(45.0f), static_cast<float>(width) / height, 0.1f, 10.0f)
            * model_view;
        glUseProgram(shader_prog_ID);
        glUniformMatrix4fv(transform_location, 1, GL_FALSE, glm::value_ptr(transform));
        glBindVertexArray(VAO_ID);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triangle_vertices.size()));
        
        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();

        //Measure FPS
        FPS.update();
        if (FPS.is_updated())
        {
            std::string title = "OpenGL - FPS: " + std::to_string(static_cast<int>(FPS.get()));
            glfwSetWindowTitle(window, title.c_str());
        }
    }

    return 0;
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
    if (window)
    {
        glfwMakeContextCurrent(window);
        if (VAO_ID) glDeleteVertexArrays(1, &VAO_ID);
        if (VBO_ID) glDeleteBuffers(1, &VBO_ID);
        if (shader_prog_ID) glDeleteProgram(shader_prog_ID);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    if (capture.isOpened())
        capture.release();

    cv::destroyAllWindows();
}
