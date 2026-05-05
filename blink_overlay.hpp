#pragma once

#define GLEW_STATIC
#include <GL/glew.h>

#include <glm/glm.hpp>

#include <vector>

namespace ls3render {

struct BlinkOverlay;

/**
 * Zeichnet radial nach aussen weisende Strahlen-Sterne ueber blinkende Meshes
 * bzw. Verknuepfungen. Verwaltet ein eigenes Shader-Programm sowie einen VBO
 * mit der einmalig erzeugten Stern-Geometrie.
 *
 * Die Sterne liegen in der Ebene senkrecht zur Blickrichtung der Kamera in
 * @ref ls3render_Render (die Kamera schaut in +x), also in der Welt-y-z-Ebene.
 */
class BlinkOverlayRenderer {
 public:
  BlinkOverlayRenderer();
  ~BlinkOverlayRenderer();

  BlinkOverlayRenderer(const BlinkOverlayRenderer&) = delete;
  BlinkOverlayRenderer& operator=(const BlinkOverlayRenderer&) = delete;

  /**
   * Compiliert die Shader und legt den VBO mit der Stern-Geometrie an.
   * Muss innerhalb eines aktiven OpenGL-Kontexts aufgerufen werden, bevor
   * @ref render verwendet werden kann.
   *
   * @return true bei Erfolg, false bei Fehlschlag (Details werden auf stderr ausgegeben).
   */
  bool init();

  /**
   * Loescht alle GL-Ressourcen.
   */
  void cleanup();

  /**
   * Zeichnet die uebergebenen Overlays. Der vorher aktive GL-State (Tiefentest,
   * Backface-Culling, gebundenes Shader-Programm, gebundener VAO) wird nicht
   * wiederhergestellt.
   *
   * @param view  Camera-View-Matrix (analog zum Hauptshader).
   * @param proj  Orthographische Projektionsmatrix (analog zum Hauptshader).
   * @param shear Scher-Matrix der axonometrischen Projektion.
   */
  bool render(const glm::mat4& view, const glm::mat4& proj, const glm::mat4& shear,
              const std::vector<BlinkOverlay>& overlays) const;

 private:
  bool m_initialized;
  GLuint m_program;
  GLuint m_vao;
  GLuint m_vbo;
  GLsizei m_n_vertices;

  GLint m_uni_view;
  GLint m_uni_proj;
  GLint m_uni_shear;
  GLint m_uni_center;
  GLint m_uni_radius;
  GLint m_uni_color;
};

}  // namespace ls3render
