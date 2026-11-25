#include <iostream>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "App.hpp"

void App::glfw_error_callback(int error, const char* description)
{
	std::cerr << "GLFW error: " << description << std::endl;
}

void App::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
	if ((action == GLFW_PRESS) || (action == GLFW_REPEAT)) {
		switch (key) {
		case GLFW_KEY_ESCAPE:
			// Exit The App
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			break;
		case GLFW_KEY_V:
			// Vsync on/off
			this_inst->is_vsync_on = !this_inst->is_vsync_on;
			glfwSwapInterval(this_inst->is_vsync_on);
			std::cout << "VSync: " << this_inst->is_vsync_on << "\n";
			break;
		case GLFW_KEY_D:
			this_inst->show_imgui = !this_inst->show_imgui;
			break;
		default:
			break;
		}
	}
}

void App::glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (yoffset > 0.0) {
        std::cout << "wheel up...\n";
    }
}

void App::glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height) {
}

void App::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (action == GLFW_PRESS) {
		switch (button) {
		case GLFW_MOUSE_BUTTON_LEFT: {
			int mode = glfwGetInputMode(window, GLFW_CURSOR);
			if (mode == GLFW_CURSOR_NORMAL) {
				// we are outside of application, catch the cursor
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			}
			else {
				// we are already inside our game: shoot, click, etc.
				std::cout << "Bang!\n";
			}
			break;
		}
		case GLFW_MOUSE_BUTTON_RIGHT:
            // release the cursor
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		default:
			break;
		}
	}
}
