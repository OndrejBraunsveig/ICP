// OpenGL Extension Wrangler: allow all multiplatform GL functions
#include <GL/glew.h> 
// WGLEW = Windows GL Extension Wrangler (change for different platform) 
// platform specific functions (in this case Windows)
//#include <GL/wglew.h> 

// GLFW toolkit
// Uses GL calls to open GL context, i.e. GLEW __MUST__ be first.
#include <GLFW/glfw3.h>

// OpenGL math (and other additional GL libraries, at the end)
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>



#include "assets.hpp"

class App {

private:
    
    //new GL stuff
    GLFWwindow* window = nullptr;

    GLuint shader_prog_ID{ 0 };
    GLuint VBO_ID{ 0 };
    GLuint VAO_ID{ 0 };
    
    std::vector<vertex> triangle_vertices =
    {
    	{{0.0f,  0.5f,  0.0f}},
    	{{0.5f, -0.5f,  0.0f}},
    	{{-0.5f, -0.5f,  0.0f}}
    };
    
}

