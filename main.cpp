
#include <stdio.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define WIDTH  640
#define HEIGHT 480

// NOTE: Uncomment the following line for GL error handling
//#define GL_DEBUG

#ifdef GL_DEBUG
#define GLCALL(function) \
   { \
      GLenum error = GL_INVALID_ENUM; \
      while (error != GL_NO_ERROR) \
      { \
         error = glGetError(); \
      } \
      function; \
      error = glGetError(); \
      if (error != GL_NO_ERROR) \
      { \
         fprintf(stderr, "OpenGL Error: GL_ENUM(%d) at %s:%d\n", error, __FILE__, __LINE__); \
      } \
   }
#else
#define GLCALL(function) function;
#endif

std::string vertex_shader_source = R"(
#version 400
layout (location = 0) in vec2 aPos;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

std::string fragment_shader_source = R"(
#version 400

uniform vec4 uColor = vec4(1.0);

void main()
{
    gl_FragColor = uColor;
}
)";

std::string tess_evaluation_shader_source = R"(
#version 400

layout (isolines) in;

uniform mat4 uMVP;

void main()
{
    float t = gl_TessCoord.x;

    vec3 p0 = gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz;
    vec3 p3 = gl_in[3].gl_Position.xyz;

    float t1 = (1.0 - t);
    float t2 = t * t;

    // Bernstein ploynomials
    float b3 = t2 * t;
    float b2 = 3.0 * t2 * t1;
    float b1 = 3.0 * t * t1 * t1;
    float b0 = t1 * t1 * t1;

    // Cubic Bezier interpolation
    vec3 p = p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;

    gl_Position = uMVP * vec4(p, 1.0);
}
)";

std::string tess_control_shader_source = R"(
#version 400

layout (vertices = 4) out;

uniform int uNumSegments;

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    gl_TessLevelOuter[0] = 1.0;

    gl_TessLevelOuter[1] = float(uNumSegments);
}
)";

void resize(GLFWwindow* window, int width, int height)
{
   GLCALL(glViewport(0, 0, width, height));
}

bool initialize_buffers = true;
GLuint vao;
GLuint vbo;
GLuint program;
GLuint points_program;
int num_segments = 40;
float point_size = 3.0f;

float vertices[4][2] =
{
   { -0.95f, -0.95f },
   { -0.85f,  0.95f },
   {  0.5f,  -0.95f },
   {  0.95f,  0.95f },
};

void render()
{
   if (initialize_buffers)
   {

      printf("Initialize buffers, program %d\n", program);
      GLCALL(glGenVertexArrays(1, &vao));
      GLCALL(glGenBuffers(1, &vbo));

      GLCALL(glBindVertexArray(vao));
      GLCALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
      GLCALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW));
      GLCALL(glEnableVertexAttribArray(0));
      GLCALL(glVertexAttribPointer(0, 2, GL_FLOAT, false, 0, (void*)0));

      GLCALL(glPatchParameteri(GL_PATCH_VERTICES, 4));

      initialize_buffers = false;
   }

   GLCALL(glUseProgram(program));

   // Set uniforms
   int mvp_loc;
   GLCALL(mvp_loc = glGetUniformLocation(program, "uMVP"));
   glm::mat4 mvp = glm::mat4(1.0f);
   GLCALL(glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, &mvp[0][0]));
   int num_segments_loc;
   GLCALL(num_segments_loc = glGetUniformLocation(program, "uNumSegments"));
   GLCALL(glUniform1i(num_segments_loc, num_segments));

   GLCALL(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices));

   GLCALL(glDrawArrays(GL_PATCHES, 0, 4));

   // Draw points
   GLCALL(glUseProgram(points_program));

   // Set uniforms
   int color_loc;
   GLCALL(color_loc = glGetUniformLocation(points_program, "uColor"));
   glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0);
   GLCALL(glUniform4fv(color_loc, 1, &color[0]));

   GLCALL(glDrawArrays(GL_POINTS, 0, 4));
}

int main(int argc, char* argv[])
{
   GLFWwindow* window = nullptr;

   // initialize glfw
   if (!glfwInit())
      return 0;

   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
   glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

   // Create window
   window = glfwCreateWindow(WIDTH, HEIGHT, "Bezier Curve Line", NULL, NULL);

   if (!window)
   {
      glfwTerminate();
      return 0;
   }

   // make the window's context current
   glfwMakeContextCurrent(window);

   // use glad to load OpenGL function pointers
   if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
   {
      printf("Error: Failed to initialize GLAD.\n");
      glfwTerminate();
      return 0;
   }

   glfwSetWindowSize(window, WIDTH, HEIGHT);

   // Setup Dear ImGui
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGui::StyleColorsDark();

   // Setup Platform/Render backends
   ImGui_ImplGlfw_InitForOpenGL(window, true);
   ImGui_ImplOpenGL3_Init();

   // Compile shaders and link program
   GLuint vertex_shader;
   GLCALL(vertex_shader = glCreateShader(GL_VERTEX_SHADER));

   const char* shader_code = vertex_shader_source.c_str();
   GLCALL(glShaderSource(vertex_shader, 1, &shader_code, nullptr));
   GLCALL(glCompileShader(vertex_shader));

   // check vertex shader
   GLint result = GL_FALSE;
   int info_log_length;
   GLCALL(glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &result));
   GLCALL(glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &info_log_length));
   if (info_log_length > 0)
   {
      std::vector<char> error_msg(info_log_length+1);
      GLCALL(glGetShaderInfoLog(vertex_shader, info_log_length, NULL, &error_msg[0]));
      printf("Error in vertex shader\n");
      printf("%s\n", &error_msg[0]);
   }

   GLuint fragment_shader;
   GLCALL(fragment_shader = glCreateShader(GL_FRAGMENT_SHADER));

   shader_code = fragment_shader_source.c_str();
   GLCALL(glShaderSource(fragment_shader, 1, &shader_code, nullptr));
   GLCALL(glCompileShader(fragment_shader));

   // check fragment shader
   GLCALL(glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &result));
   GLCALL(glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &info_log_length));
   if (info_log_length > 0)
   {
      std::vector<char> error_msg(info_log_length+1);
      GLCALL(glGetShaderInfoLog(fragment_shader, info_log_length, NULL, &error_msg[0]));
      printf("Error in fragment shader\n");
      printf("%s\n", &error_msg[0]);
   }

   GLuint tess_evaluation_shader;
   GLCALL(tess_evaluation_shader = glCreateShader(GL_TESS_EVALUATION_SHADER));

   shader_code = tess_evaluation_shader_source.c_str();
   GLCALL(glShaderSource(tess_evaluation_shader, 1, &shader_code, nullptr));
   GLCALL(glCompileShader(tess_evaluation_shader));

   // check tess evaluation shader
   GLCALL(glGetShaderiv(tess_evaluation_shader, GL_COMPILE_STATUS, &result));
   GLCALL(glGetShaderiv(tess_evaluation_shader, GL_INFO_LOG_LENGTH, &info_log_length));
   if (info_log_length > 0)
   {
      std::vector<char> error_msg(info_log_length+1);
      GLCALL(glGetShaderInfoLog(tess_evaluation_shader, info_log_length, NULL, &error_msg[0]));
      printf("Error in tess evaluation shader\n");
      printf("%s\n", &error_msg[0]);
   }

   GLuint tess_control_shader;
   GLCALL(tess_control_shader = glCreateShader(GL_TESS_CONTROL_SHADER));

   shader_code = tess_control_shader_source.c_str();
   GLCALL(glShaderSource(tess_control_shader, 1, &shader_code, nullptr));
   GLCALL(glCompileShader(tess_control_shader));

   // check tess control shader
   GLCALL(glGetShaderiv(tess_control_shader, GL_COMPILE_STATUS, &result));
   GLCALL(glGetShaderiv(tess_control_shader, GL_INFO_LOG_LENGTH, &info_log_length));
   if (info_log_length > 0)
   {
      std::vector<char> error_msg(info_log_length+1);
      GLCALL(glGetShaderInfoLog(tess_control_shader, info_log_length, NULL, &error_msg[0]));
      printf("Error in tess control shader\n");
      printf("%s\n", &error_msg[0]);
   }

   GLCALL(program = glCreateProgram());
   GLCALL(glAttachShader(program, vertex_shader));
   GLCALL(glAttachShader(program, fragment_shader));
   GLCALL(glAttachShader(program, tess_evaluation_shader));
   GLCALL(glAttachShader(program, tess_control_shader));
   GLCALL(glLinkProgram(program));

   // check the program
   GLCALL(glGetProgramiv(program, GL_LINK_STATUS, &result));
   GLCALL(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length));
   if (info_log_length > 0)
   {
      std::vector<char> error_msg(info_log_length+1);
      GLCALL(glGetProgramInfoLog(program, info_log_length, NULL, &error_msg[0]));
      printf("Error linking program\n");
      printf("%s\n", &error_msg[0]);
   }

   GLCALL(glDetachShader(program, vertex_shader));
   GLCALL(glDetachShader(program, fragment_shader));
   GLCALL(glDetachShader(program, tess_evaluation_shader));
   GLCALL(glDetachShader(program, tess_control_shader));

   // Create program for drawing points
   GLCALL(points_program = glCreateProgram());
   GLCALL(glAttachShader(points_program, vertex_shader));
   GLCALL(glAttachShader(points_program, fragment_shader));
   GLCALL(glLinkProgram(points_program));

   // check the program
   GLCALL(glGetProgramiv(points_program, GL_LINK_STATUS, &result));
   GLCALL(glGetProgramiv(points_program, GL_INFO_LOG_LENGTH, &info_log_length));
   if (info_log_length > 0)
   {
      std::vector<char> error_msg(info_log_length+1);
      GLCALL(glGetProgramInfoLog(points_program, info_log_length, NULL, &error_msg[0]));
      printf("Error linking program\n");
      printf("%s\n", &error_msg[0]);
   }

   GLCALL(glDetachShader(points_program, vertex_shader));
   GLCALL(glDetachShader(points_program, fragment_shader));

   GLCALL(glDeleteShader(vertex_shader));
   GLCALL(glDeleteShader(fragment_shader));
   GLCALL(glDeleteShader(tess_evaluation_shader));
   GLCALL(glDeleteShader(tess_control_shader));

   // Make the window visible
   glfwShowWindow(window);

   // Initialize opengl
   GLCALL(glClearColor(0.5, 0.5, 0.5, 1.0));

   // enable blending
   GLCALL(glEnable(GL_BLEND));
   GLCALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

   GLCALL(glPointSize(point_size));
   GLCALL(glLineWidth(2.0f));

   glfwSetWindowSizeCallback(window, resize);

   // set frame rate to 60 Hz
   using framerate = std::chrono::duration<double, std::ratio<1, 60>>;
   auto frame_time = std::chrono::high_resolution_clock::now() + framerate{1};

   while (window)
   {
      // Poll events
      glfwPollEvents();

      if (glfwWindowShouldClose(window))
      {
         glfwTerminate();
         window = nullptr;
         break;
      }

      GLCALL(glClear(GL_COLOR_BUFFER_BIT));

      render();

      // Start the Dear ImGui frame
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      ImGui::Begin("Debug");
      ImGui::SliderFloat2("Point 1", (float*)&vertices[0], -1.0f, 1.0f);
      ImGui::SliderFloat2("Point 2", (float*)&vertices[1], -1.0f, 1.0f);
      ImGui::SliderFloat2("Point 3", (float*)&vertices[2], -1.0f, 1.0f);
      ImGui::SliderFloat2("Point 4", (float*)&vertices[3], -1.0f, 1.0f);
      ImGui::SliderInt("Line Segments", &num_segments, 1, 40);

      if (ImGui::SliderFloat("Point Size", &point_size, 1.0f, 10.0f))
      {
         GLCALL(glPointSize(point_size));
      }

      ImGui::End();

      // Render ImGui
      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);

      // wait until next frame
      std::this_thread::sleep_until(frame_time);
      frame_time += framerate{1};
   }

   return 0;
}
