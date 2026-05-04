#include "./ls3render.h"

#ifdef HAVE_RENDERDOC
#include "renderdoc_app.h"
#endif

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>  // glm::to_string()
#include <glm/gtx/quaternion.hpp>

#include "zusi_parser/zusi_types.hpp"
#include "zusi_parser/utils.hpp"

#include "./macros.hpp"
#include "./texture.hpp"
#include "./scene.hpp"
#include "./shader_parameters.hpp"
#include "./render_object.hpp"

#include <cassert>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>

#ifdef HAVE_RENDERDOC
#include <dlfcn.h>
#endif

using namespace ls3render;

namespace {

static const std::string vs_source = 
#include "./assets/vertex_shader.glsl"
;

static const std::string fs_source = 
#include "./assets/fragment_shader.glsl"
;

void glfw_error_callback(int error, const char* description) {
  std::cerr << "GLFW error " << error << ": " << description << std::endl;
}

}

// Sentinel values so the union with geometry yields the geometry's bounds.
static const auto kInvalidBBox = std::make_pair(
    glm::vec3 { std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max() },
    glm::vec3 { std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest() });

static Scene m_Scene {};
static ShaderParameters m_ShaderParameters {};
// Keine vehicle-spezifischen Animationen mehr (Stromabnehmer/Gleiskruemmung/Neigetechnik).
// Eine leere Map laesst alle Animationen mit Defaultposition 0 ablaufen.
static std::unordered_map<int, float> m_AniPositionen {};
static int m_PixelProMeter { 50 };
static int m_Multisampling { 0 };
static auto m_BBox = kInvalidBBox;

// Aus der Bounding Box abgeleitete Modellgrenzen. Werden in ls3render_AddSignalFrame
// gesetzt, sobald die Szene mindestens einen Frame enthaelt.
static float m_ModelBackX { 0 };
static float m_ModelFrontX { 0 };
static float m_ModelTopZ { 0 };
static float m_ModelBottomZ { 0 };
static float m_ModelLeftY { 0 };
static float m_ModelRightY { 0 };

static short m_OutputHeight { 0 };
static short m_OutputWidth { 0 };
static float m_cabinetAngle { glm::radians(45.0f) };  // Typically around 45 degrees
static float m_cabinetScale { 0 }; // Foreshortening factor for the Y axis, typically 0.5
static GLFWwindow* m_Window { nullptr };
#ifdef HAVE_RENDERDOC
RENDERDOC_API_1_3_0 *renderdoc_api { nullptr };
#endif

ls3render_EXPORT int ls3render_Init() {
#ifdef HAVE_RENDERDOC
  if(void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
  {
    auto RENDERDOC_GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(mod, "RENDERDOC_GetAPI"));
    [[maybe_unused]] const int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_3_0, (void **)&renderdoc_api);
    assert(ret == 1);
  }
#endif

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    std::cerr << "Error initializing GLFW" << std::endl;
    return false;
  }

  // Require the OpenGL context to support at least OpenGL 3.2
  TRY_GLFW(glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3));
  TRY_GLFW(glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2));

  // We want a context that supports only the new core functionality
  TRY_GLFW(glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE));

  TRY_GLFW(glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE));

  TRY_GLFW(glfwWindowHint(GLFW_RESIZABLE, GL_FALSE));
  TRY_GLFW(glfwWindowHint(GLFW_VISIBLE, GL_FALSE));

  // The GLFWwindow object encapsulates both a window and a context
  // (which are inseparably linked).
  m_Window = TRY_GLFW(glfwCreateWindow(1, 1, "ls3render",
          nullptr,  // monitor if fullscreen
          nullptr   // existing OpenGL context to share resources with
          ));
  TRY_GLFW(glfwMakeContextCurrent(m_Window));

  // Init GLEW
  TRY_GLEW(glewInit());
  // std::cerr << "Using GLEW " << glewGetString(GLEW_VERSION) << std::endl;

  if (!glewIsSupported("GL_EXT_texture_filter_anisotropic")) {
    std::cerr << "GLEW extension GL_EXT_texture_filter_anisotropic not supported\n";
  }
  if (!glewIsSupported("GL_EXT_framebuffer_multisample")) {
    std::cerr << "GLEW extension GL_EXT_framebuffer_multisample not supported\n";
  }
  if (!glewIsSupported("GL_EXT_framebuffer_blit")) {
    std::cerr << "GLEW extension GL_EXT_framebuffer_blit not supported\n";
  }

  // Load shaders
  GLuint vertex_shader;
  TRY(vertex_shader = glCreateShader(GL_VERTEX_SHADER));
  const char* c_str = vs_source.c_str();
  TRY(glShaderSource(vertex_shader, 1, &c_str, nullptr));

  GLuint fragment_shader;
  TRY(fragment_shader = glCreateShader(GL_FRAGMENT_SHADER));
  c_str = fs_source.c_str();
  TRY(glShaderSource(fragment_shader, 1, &c_str, nullptr));

  // Compile shaders
  TRY(glCompileShader(vertex_shader));

  GLint buflen;
  TRY(glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &buflen));
  if (buflen > 1) {
    std::string buf;
    buf.resize(buflen);
    glGetShaderInfoLog(vertex_shader, buflen, nullptr, buf.data());
    std::cerr << "Compiling vertex shader produced warnings or errors: " << buf << "\n";
  }

  GLint status;
  TRY(glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status));
  if (status != GL_TRUE) {
    std::cerr << "Compiling vertex shader failed.\n";
    return false;
  }

  TRY(glCompileShader(fragment_shader));

  TRY(glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &buflen));
  if (buflen > 1) {
    std::string buf;
    buf.resize(buflen);
    glGetShaderInfoLog(fragment_shader, buflen, nullptr, buf.data());
    std::cerr << "Compiling fragment shader produced warnings or errors: " << buf << "\n";
  }

  TRY(glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status));
  if (status != GL_TRUE) {
    std::cerr << "Compiling fragment shader failed.\n";
    return false;
  }

  // Combine vertex and fragment shaders into a program
  GLuint shader_program = glCreateProgram();
  TRY(glAttachShader(shader_program, vertex_shader));
  TRY(glAttachShader(shader_program, fragment_shader));

  // Bind output of fragment shader to buffer
  // TODO: only after creating frame-/renderbuffers?
  TRY(glBindFragDataLocation(shader_program, 0, "outColor"));  // not necessary because there is only one output

  // Link and use program
  TRY(glLinkProgram(shader_program));
  TRY(glUseProgram(shader_program));

  // Vertex shader inputs
  m_ShaderParameters.attrib_pos = glGetAttribLocation(shader_program, "position");
  m_ShaderParameters.attrib_nor = glGetAttribLocation(shader_program, "normal");
  m_ShaderParameters.attrib_uv1 = glGetAttribLocation(shader_program, "uv1");
  m_ShaderParameters.attrib_uv2 = glGetAttribLocation(shader_program, "uv2");

  // Bind uniform variables
  // Vertex shader
  m_ShaderParameters.uni_nor = glGetUniformLocation(shader_program, "nor");
  m_ShaderParameters.uni_model = glGetUniformLocation(shader_program, "model");
  m_ShaderParameters.uni_view = glGetUniformLocation(shader_program, "view");
  m_ShaderParameters.uni_proj = glGetUniformLocation(shader_program, "proj");
  m_ShaderParameters.uni_shear = glGetUniformLocation(shader_program, "shear");
  m_ShaderParameters.uni_diffuse_color = glGetUniformLocation(shader_program, "diffuseColor");
  m_ShaderParameters.uni_emissive_color = glGetUniformLocation(shader_program, "emissiveColor");

  // Fragment shader
  m_ShaderParameters.uni_tex.push_back(glGetUniformLocation(shader_program, "tex1"));
  m_ShaderParameters.uni_tex.push_back(glGetUniformLocation(shader_program, "tex2"));
  m_ShaderParameters.uni_numTextures = glGetUniformLocation(shader_program, "numTextures");
  m_ShaderParameters.uni_alphaCutoff = glGetUniformLocation(shader_program, "alphaCutoff");
  m_ShaderParameters.uni_texVoreinstellung = glGetUniformLocation(shader_program, "texVoreinstellung");

  m_ShaderParameters.validate();

  // Enable depth test
  TRY(glEnable(GL_DEPTH_TEST));

  // Enable backface culling
  TRY(glEnable(GL_CULL_FACE));
  TRY(glFrontFace(GL_CW));

  // Enable depth offset
  TRY(glEnable(GL_POLYGON_OFFSET_FILL));

  return true;
}

ls3render_EXPORT int ls3render_Cleanup() {
  TRY_GLFW(glfwTerminate());  // Destroys any remaining windows
  return true;
}

void SetOutputSize() {
  if (m_ModelFrontX < m_ModelBackX || m_ModelTopZ < m_ModelBottomZ || m_ModelRightY < m_ModelLeftY) {
    // Noch kein Frame hinzugefuegt: keine sinnvolle Ausgabegroesse.
    m_OutputWidth = 0;
    m_OutputHeight = 0;
    return;
  }

  m_OutputHeight = (m_ModelTopZ - m_ModelBottomZ) * m_PixelProMeter;
  m_OutputWidth = (m_ModelFrontX - m_ModelBackX) * m_PixelProMeter;

  const float cabinetX = m_cabinetScale * cos(m_cabinetAngle);
  const float cabinetY = m_cabinetScale * sin(m_cabinetAngle);
  const float yExtent = m_ModelRightY - m_ModelLeftY;
  m_OutputWidth += std::abs(cabinetX) * yExtent * m_PixelProMeter;
  m_OutputHeight += std::abs(cabinetY) * yExtent * m_PixelProMeter;
}

ls3render_EXPORT void ls3render_SetPixelProMeter(int PixelProMeter) {
  assert(PixelProMeter > 0);
  m_PixelProMeter = PixelProMeter;
  SetOutputSize();
}

ls3render_EXPORT void ls3render_SetMultisampling(int Samples) {
  assert(Samples >= 0);
  m_Multisampling = Samples;
}

ls3render_EXPORT void ls3render_SetAxonometrieParameter(float Winkel, float Skalierung) {
  m_cabinetAngle = Winkel;
  m_cabinetScale = Skalierung;
  SetOutputSize();
}

ls3render_EXPORT int ls3render_AddSignalFrame(const char* Dateiname,
    float PosX, float PosY, float PosZ,
    float PhiX, float PhiY, float PhiZ,
    float SkX, float SkY, float SkZ) {
  try {
    // Transform-Reihenfolge analog zu <Verknuepfte> in scene.cpp: erst Rotation,
    // dann Skalierung, dann Translation. Eine Skalierungs-Komponente von 0
    // wird wie in Zusi als 1 interpretiert.
    const auto zeroToOne = [](float f) { return f == 0.0f ? 1.0f : f; };
    glm::mat4 transform = glm::eulerAngleZYX(PhiZ, PhiY, PhiX);
    transform = glm::scale(glm::mat4 { 1 },
        glm::vec3(zeroToOne(SkX), zeroToOne(SkY), zeroToOne(SkZ))) * transform;
    transform = glm::translate(glm::mat4 { 1 },
        glm::vec3(PosX, PosY, PosZ)) * transform;

    if (!m_Scene.LadeLandschaft(zusixml::ZusiPfad::vonOsPfad(Dateiname), transform, m_AniPositionen)) {
      return false;
    }

    m_Scene.UpdateBoundingBox(&m_BBox);

    m_ModelBackX = m_BBox.first.x;
    m_ModelFrontX = m_BBox.second.x;
    m_ModelLeftY = m_BBox.first.y;
    m_ModelRightY = m_BBox.second.y;
    m_ModelBottomZ = m_BBox.first.z;
    m_ModelTopZ = m_BBox.second.z;
    SetOutputSize();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return false;
  } catch (...) {
    return false;
  }

  return true;
}

ls3render_EXPORT int ls3render_GetBildbreite() {
  return m_OutputWidth;
}

ls3render_EXPORT int ls3render_GetBildhoehe() {
  return m_OutputHeight;
}

ls3render_EXPORT int ls3render_GetAusgabepufferGroesse() {
  return m_OutputWidth * m_OutputHeight * 4;
}

ls3render_EXPORT int ls3render_Render(void* Ausgabepuffer) {
  if (m_OutputWidth <= 0 || m_OutputHeight <= 0) {
    std::cerr << "Output width and height must both be > 0" << std::endl;
    return false;
  }

#ifdef HAVE_RENDERDOC
  if (renderdoc_api) {
    renderdoc_api->StartFrameCapture(nullptr, nullptr);
  }
#endif

  // Create and bind framebuffer & renderbuffer
  GLuint framebuffer;
  TRY(glGenFramebuffers(1, &framebuffer));
  TRY(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));

  GLuint color_renderbuffer;
  TRY(glGenRenderbuffers(1, &color_renderbuffer));
  TRY(glBindRenderbuffer(GL_RENDERBUFFER, color_renderbuffer));

  TRY(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, m_OutputWidth, m_OutputHeight));
  TRY(glBindRenderbuffer(GL_RENDERBUFFER, 0));  // don't know why that is necessary

  TRY(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_renderbuffer));

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Non-multisampling framebuffer is not complete" << std::endl;
    return false;
  }

  // Create and bind multisampling framebuffer & renderbuffers
  GLuint framebuffer_multisample;
  TRY(glGenFramebuffers(1, &framebuffer_multisample));
  TRY(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_multisample));

  GLuint color_renderbuffer_multisample;
  TRY(glGenRenderbuffers(1, &color_renderbuffer_multisample));
  TRY(glBindRenderbuffer(GL_RENDERBUFFER, color_renderbuffer_multisample));

  TRY(glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_Multisampling, GL_RGBA, m_OutputWidth, m_OutputHeight));
  TRY(glBindRenderbuffer(GL_RENDERBUFFER, 0));  // don't know why that is necessary

  TRY(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_renderbuffer_multisample));

  GLuint depth_renderbuffer_multisample;
  TRY(glGenRenderbuffers(1, &depth_renderbuffer_multisample));
  TRY(glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer_multisample));

  TRY(glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_Multisampling, GL_DEPTH_COMPONENT, m_OutputWidth, m_OutputHeight));
  TRY(glBindRenderbuffer(GL_RENDERBUFFER, 0));

  TRY(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_renderbuffer_multisample));

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Multisampling framebuffer is not complete" << std::endl;
    return false;
  }

  // Load data into graphics card memory
  if (!m_Scene.LoadIntoGraphicsCardMemory()) {
    std::cerr << "Loading data into graphics card memory failed\n";
    return false;
  }

  // Right-side view.
  const glm::mat4 view = glm::lookAt(
      glm::vec3(0.0f,  0.0f, 0.0f),  // position
      glm::vec3(0.0f, -1.0f, 0.0f),  // lookat
      glm::vec3(0.0f,  0.0f, 1.0f));  // up
  TRY(glUniformMatrix4fv(m_ShaderParameters.uni_view, 1, GL_FALSE, glm::value_ptr(view)));

  // Create an oblique projection matrix (cabinet effect) by shearing the model along the Z-axis.
  // After the view transform, we are in camera space: camera at origin facing -z, y up, x right.
  glm::mat4 shear{1};
  const float cabinetX = m_cabinetScale * cos(m_cabinetAngle);
  const float cabinetY = m_cabinetScale * sin(m_cabinetAngle);
  shear[2][0] = -cabinetX; // X axis distortion: x += scale * -z * cos(alpha).
  shear[2][1] = -cabinetY; // Y axis distortion: y += scale * -z * sin(alpha).

  TRY(glUniformMatrix4fv(m_ShaderParameters.uni_shear, 1, GL_FALSE, glm::value_ptr(shear)));

  // World space x coordinates are in the range (-oo, 0).
  // Camera space x coordinates are in the range (0, oo).
  const float left = -m_ModelFrontX - std::abs(cabinetX) * m_ModelRightY; // left
  const float right = -m_ModelBackX - std::abs(cabinetX) * m_ModelLeftY; // right

  // Camera space y coordinates correspond to world space z coordinates.
  const float bottom = m_ModelBottomZ - std::abs(cabinetY) * m_ModelRightY; // bottom
  const float top = m_ModelTopZ - std::abs(cabinetY) * m_ModelLeftY; // top

  // Camera space z coordinates correspond to world space y coordinates.
  const float zNear = m_BBox.first.y - .01f; // zNear
  const float zFar = m_BBox.second.y + .01f; // zFar

  const glm::mat4 proj = glm::ortho(left, right, bottom, top, zNear, zFar);
  TRY(glUniformMatrix4fv(m_ShaderParameters.uni_proj, 1, GL_FALSE, glm::value_ptr(proj)));

  TRY(glViewport(0, 0, m_OutputWidth, m_OutputHeight));
  TRY(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
  TRY(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

  m_Scene.Render(m_ShaderParameters);
  m_Scene.FreeGraphicsCardMemory();

  TRY(glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_multisample));
  TRY(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer));
  TRY(glBlitFramebuffer(0, 0, m_OutputWidth, m_OutputHeight, 0, 0, m_OutputWidth, m_OutputHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR));

  TRY(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));
  TRY(glReadBuffer(GL_COLOR_ATTACHMENT0));
  TRY(glReadPixels(0, 0, m_OutputWidth, m_OutputHeight, GL_BGRA, GL_UNSIGNED_BYTE, Ausgabepuffer));

  TRY(glDeleteFramebuffers(1, &framebuffer));
  TRY(glDeleteRenderbuffers(1, &color_renderbuffer));
  TRY(glDeleteFramebuffers(1, &framebuffer_multisample));
  TRY(glDeleteRenderbuffers(1, &color_renderbuffer_multisample));
  TRY(glDeleteRenderbuffers(1, &depth_renderbuffer_multisample));

#ifdef HAVE_RENDERDOC
  if (renderdoc_api) {
    renderdoc_api->EndFrameCapture(nullptr, nullptr);
  }
#endif

  return true;
}

ls3render_EXPORT void ls3render_Reset() {
  m_Scene = Scene {};
  m_BBox = kInvalidBBox;
  m_ModelBackX = 0;
  m_ModelFrontX = 0;
  m_ModelLeftY = 0;
  m_ModelRightY = 0;
  m_ModelBottomZ = 0;
  m_ModelTopZ = 0;
  m_OutputWidth = 0;
  m_OutputHeight = 0;
}
