#include "./blink_overlay.hpp"

#include "./macros.hpp"
#include "./scene.hpp"

#define GLEW_STATIC
#include <GL/glew.h>

#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace {

static const std::string blink_vs_source =
#include "./assets/blink_vertex_shader.glsl"
;

static const std::string blink_fs_source =
#include "./assets/blink_fragment_shader.glsl"
;

// Anzahl der Strahlen pro Stern.
constexpr int kNumStrahlen = 4;
// Innen- und Aussenradius der Strahlen, jeweils bezogen auf die Bbox-Diagonale
// des markierten Elements (1.0 = halbe Diagonale = umschriebener Kreis).
constexpr float kInnerRadiusFactor = 0.45f;
constexpr float kOuterRadiusFactor = 1.05f;

bool compileShader(GLenum type, const std::string& source, GLuint& out_shader) {
  out_shader = glCreateShader(type);
  const char* c_str = source.c_str();
  TRY(glShaderSource(out_shader, 1, &c_str, nullptr));
  TRY(glCompileShader(out_shader));

  GLint buflen;
  TRY(glGetShaderiv(out_shader, GL_INFO_LOG_LENGTH, &buflen));
  if (buflen > 1) {
    std::string buf;
    buf.resize(buflen);
    glGetShaderInfoLog(out_shader, buflen, nullptr, buf.data());
    std::cerr << "Compiling blink overlay shader produced warnings or errors: " << buf << "\n";
  }

  GLint status;
  TRY(glGetShaderiv(out_shader, GL_COMPILE_STATUS, &status));
  if (status != GL_TRUE) {
    std::cerr << "Compiling blink overlay shader failed.\n";
    return false;
  }
  return true;
}

}  // namespace

namespace ls3render {

BlinkOverlayRenderer::BlinkOverlayRenderer()
    : m_initialized(false), m_program(0), m_vao(0), m_vbo(0), m_n_vertices(0),
      m_uni_view(-1), m_uni_proj(-1), m_uni_shear(-1),
      m_uni_center(-1), m_uni_radius(-1), m_uni_color(-1) {}

BlinkOverlayRenderer::~BlinkOverlayRenderer() {
  if (m_initialized) {
    cleanup();
  }
}

bool BlinkOverlayRenderer::init() {
  if (m_initialized) {
    return true;
  }

  GLuint vertex_shader = 0;
  if (!compileShader(GL_VERTEX_SHADER, blink_vs_source, vertex_shader)) {
    return false;
  }
  GLuint fragment_shader = 0;
  if (!compileShader(GL_FRAGMENT_SHADER, blink_fs_source, fragment_shader)) {
    glDeleteShader(vertex_shader);
    return false;
  }

  m_program = glCreateProgram();
  TRY(glAttachShader(m_program, vertex_shader));
  TRY(glAttachShader(m_program, fragment_shader));
  TRY(glBindFragDataLocation(m_program, 0, "outColor"));
  TRY(glLinkProgram(m_program));

  GLint link_status = GL_FALSE;
  TRY(glGetProgramiv(m_program, GL_LINK_STATUS, &link_status));
  if (link_status != GL_TRUE) {
    GLint buflen;
    TRY(glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &buflen));
    if (buflen > 1) {
      std::string buf;
      buf.resize(buflen);
      glGetProgramInfoLog(m_program, buflen, nullptr, buf.data());
      std::cerr << "Linking blink overlay program failed: " << buf << "\n";
    } else {
      std::cerr << "Linking blink overlay program failed.\n";
    }
    return false;
  }

  // Shaders koennen nach dem Linken geloescht werden.
  TRY(glDetachShader(m_program, vertex_shader));
  TRY(glDetachShader(m_program, fragment_shader));
  TRY(glDeleteShader(vertex_shader));
  TRY(glDeleteShader(fragment_shader));

  // Uniform/Attribute-Locations
  const GLint attrib_dir = glGetAttribLocation(m_program, "dir");
  m_uni_view   = glGetUniformLocation(m_program, "view");
  m_uni_proj   = glGetUniformLocation(m_program, "proj");
  m_uni_shear  = glGetUniformLocation(m_program, "shear");
  m_uni_center = glGetUniformLocation(m_program, "center");
  m_uni_radius = glGetUniformLocation(m_program, "radius");
  m_uni_color  = glGetUniformLocation(m_program, "color");

  // Stern-Geometrie: Linien-Paare in der Welt-y-z-Ebene (Kamera blickt entlang +x).
  std::vector<glm::vec3> verts;
  verts.reserve(static_cast<size_t>(2 * kNumStrahlen));
  for (int i = 0; i < kNumStrahlen; ++i) {
    const float a = static_cast<float>(M_PI / 4) + 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(kNumStrahlen);
    const glm::vec3 dir { 0.0f, std::cos(a), std::sin(a) };
    verts.push_back(kInnerRadiusFactor * dir);
    verts.push_back(kOuterRadiusFactor * dir);
  }
  m_n_vertices = static_cast<GLsizei>(verts.size());

  TRY(glGenVertexArrays(1, &m_vao));
  TRY(glBindVertexArray(m_vao));

  TRY(glGenBuffers(1, &m_vbo));
  TRY(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
  TRY(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW));

  if (attrib_dir >= 0) {
    TRY(glEnableVertexAttribArray(attrib_dir));
    TRY(glVertexAttribPointer(attrib_dir, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr));
  }

  m_initialized = true;
  return true;
}

void BlinkOverlayRenderer::cleanup() {
  if (!m_initialized) {
    return;
  }
  if (m_vbo) glDeleteBuffers(1, &m_vbo);
  if (m_vao) glDeleteVertexArrays(1, &m_vao);
  if (m_program) glDeleteProgram(m_program);
  m_vbo = 0;
  m_vao = 0;
  m_program = 0;
  m_n_vertices = 0;
  m_initialized = false;
}

bool BlinkOverlayRenderer::render(const glm::mat4& view, const glm::mat4& proj, const glm::mat4& shear,
                                  const std::vector<BlinkOverlay>& overlays) const {
  if (!m_initialized || overlays.empty()) {
    return true;
  }

  TRY(glUseProgram(m_program));
  TRY(glBindVertexArray(m_vao));

  TRY(glUniformMatrix4fv(m_uni_view,  1, GL_FALSE, glm::value_ptr(view)));
  TRY(glUniformMatrix4fv(m_uni_proj,  1, GL_FALSE, glm::value_ptr(proj)));
  TRY(glUniformMatrix4fv(m_uni_shear, 1, GL_FALSE, glm::value_ptr(shear)));
  // Warmes Gelb fuer den Strahlen-Stern.
  TRY(glUniform4f(m_uni_color, 1.0f, 0.85f, 0.2f, 1.0f));

  // Overlays liegen "ueber" der Geometrie -> Tiefentest und Backface-Culling aus.
  TRY(glDisable(GL_DEPTH_TEST));
  TRY(glDisable(GL_CULL_FACE));
  TRY(glDisable(GL_BLEND));
  // Lineweite > 1.0 ist nicht portabel garantiert; viele Treiber unterstuetzen
  // aber 2.0. Fehler ignorieren, falls der Treiber nur 1.0 zulaesst.
  glLineWidth(2.0f);
  glGetError();  // potentiellen GL_INVALID_VALUE verwerfen

  for (const auto& o : overlays) {
    TRY(glUniform3fv(m_uni_center, 1, glm::value_ptr(o.center)));
    TRY(glUniform1f (m_uni_radius, o.radius));
    TRY(glDrawArrays(GL_LINES, 0, m_n_vertices));
  }

  return true;
}

}  // namespace ls3render
