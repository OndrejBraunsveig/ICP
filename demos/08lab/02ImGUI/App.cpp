#include <thread>
#include <chrono>
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <random>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <opencv2/opencv.hpp>
#include <GL/glew.h> 
#include <GL/wglew.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// To add ImGUI to your project:
// vcpkg add port imgui[glfw-binding,opengl3-binding]

// ImGUI headers
#include <imgui.h>               // main ImGUI header
#include <imgui_impl_glfw.h>     // GLFW bindings
#include <imgui_impl_opengl3.h>  // OpenGL bindings

#include "App.hpp"

App::App() {
	// default constructor
}

void App::init_glfw(void)
{

	/* Initialize the library */
	glfwSetErrorCallback(glfw_error_callback);

	if (!glfwInit()) {
		throw std::runtime_error("GLFW can not be initialized.");
	}

	// try to open OpenGL
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// open window, but hidden - it will be enabled later, after asset initialization
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(800, 600, "ICP", nullptr, nullptr);
	if (!window) {
		throw std::runtime_error("GLFW window can not be created.");
	}

	glfwSetWindowUserPointer(window, this);

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	// disable mouse cursor
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// GLFW callbacks registration
	glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);
	glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
	glfwSetKeyCallback(window, glfw_key_callback);
	glfwSetScrollCallback(window, glfw_scroll_callback);
}

bool App::init()
{
	try {
		std::cout << "Current working directory: " << std::filesystem::current_path().generic_string() << '\n';

		if (!std::filesystem::exists("resources"))
			throw std::runtime_error("Directory 'resources' not found. Various media files are expected to be there.");
		
		init_opencv();
		init_glfw();
		init_glew();
		init_gl_debug();

		print_opencv_info();
		print_glfw_info();
		print_gl_info();
		print_glm_info();

		glfwSwapInterval(is_vsync_on?1:0); // vsync

		// init assets (models, sounds, textures, level map, ...)
		// (this may take a while, app window is hidden in the meantime)...
		init_assets();

		// Initialize ImGUI (see https://github.com/ocornut/imgui/wiki/Getting-Started)
		init_imgui();

		// When all is loaded, show the window.
		glfwShowWindow(window);

	}
	catch (std::exception const& e) {
		std::cerr << "Init failed : " << e.what() << std::endl;
		throw;
	}

	return true;
}

void App::init_imgui()
{
	// see https://github.com/ocornut/imgui/wiki/Getting-Started
	
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
	std::cout << "ImGUI version: " << ImGui::GetVersion() << "\n";
}

void App::init_assets(void) {
	// load and initialize all objects and resources (including initial positions)
}

int App::run(void)
{
	/*
	* Typical game loop:

			// INIT: Initial positions and state
			while (application_should_not_close)
			{
				// UPDATE: Update game state
				// RENDER: Render content
				// SWAP: Swap back/front buffer
				// VSYNC: Wait for vertical retrace (e.g. 1/60 of a second)
				// POLL: Poll events, dispatch
			}
	*/
	try {
		double now = glfwGetTime();
		// FPS related
		double fps_last_displayed = now;
		int fps_counter_frames = 0;
		double FPS = 0.0;

		// animation related
		double frame_begin_timepoint = now;
		double previous_frame_render_time{};

		// Clear color saved to OpenGL state machine: no need to set repeatedly in game loop
		glClearColor(0, 0, 0, 0);

		while (!glfwWindowShouldClose(window))
		{
			// ImGui prepare render (only if required)
			if (show_imgui) {
				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();
				//ImGui::ShowDemoWindow(); // Enable mouse when using Demo!
				ImGui::SetNextWindowPos(ImVec2(10, 10));
				ImGui::SetNextWindowSize(ImVec2(250, 100));

				ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
				ImGui::Text("V-Sync: %s", is_vsync_on ? "ON" : "OFF");
				ImGui::Text("FPS: %.1f", FPS);
				ImGui::Text("(press RMB to release mouse)");
				ImGui::Text("(hit D to show/hide info)");
				ImGui::End();
			}

			//
			// UPDATE: recompute objects state, players position etc.
            //         nothing here so far... 
			//
            		
            
			//
			// RENDER: GL drawCalls
			// 

			// Clear OpenGL canvas, both color buffer and Z-buffer
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
            // drawCalls to render whole scene
			//for (auto& [name, model] : scene) {
				// example: set uniform for specific model
				//if (name == "triangle") {
				//	model.shader.setUniform("uniform_Color", glm::vec4(glm::sin((float(glfwGetTime()))), g, b, a));
				//}

			//	model.draw();
			//}


			// ImGui display
			if (show_imgui) {
				ImGui::Render();
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			}

			//
			// SWAP + VSYNC
			//
			glfwSwapBuffers(window);

			//
			// POLL
			//
			glfwPollEvents();

			// Time/FPS measurement
			now = glfwGetTime();
			previous_frame_render_time = now - frame_begin_timepoint; //compute delta_t
			frame_begin_timepoint = now; // set new start

			fps_counter_frames++;
			if (now - fps_last_displayed >= 1) {
				FPS = fps_counter_frames / (now - fps_last_displayed);
				fps_last_displayed = now;
				fps_counter_frames = 0;
				std::cout << "\r[FPS]" << FPS << "     "; // Compare: FPS with/without ImGUI
			}
		}
	}
	catch (std::exception const& e) {
		std::cerr << "App failed : " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void App::destroy(void)
{
	// clean up ImGUI
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	// clean up OpenCV
	cv::destroyAllWindows();
	
	// clean-up GLFW
	if (window) {
		glfwDestroyWindow(window);
		window = nullptr;
	}
	glfwTerminate();
}

App::~App()
{
	destroy();

	std::cout << "Bye...\n";
}
