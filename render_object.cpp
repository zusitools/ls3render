#include "./render_object.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "./scene.hpp"
#include "./shader_parameters.hpp"
#include "./utils.hpp"
#include "./macros.hpp"
#include "./texture.hpp"

#include "zusi_parser/zusi_types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace ls3render {

namespace {
// Faktor, mit dem das zZoom-Attribut von Subsets beim Skalieren multipliziert wird.
constexpr float kZZoomFaktor = 1.0f;
}

GLRenderObject::GLRenderObject() : m_vao(), m_vbos(), m_ebos(), m_texs(), m_initialized(false) {}

bool GLRenderObject::cleanup() {
  TRY(glDeleteBuffers(m_vbos.size(), m_vbos.data()));
  TRY(glDeleteBuffers(m_ebos.size(), m_ebos.data()));
  for (const auto& texs : m_texs) {
    TRY(glDeleteTextures(texs.size(), texs.data()));
  }
  TRY(glDeleteVertexArrays(1, &m_vao));
  m_initialized = false;
  return true;
}

GLRenderObject::~GLRenderObject() {
  if (!m_initialized) {
    return;
  }

  cleanup();
}

Ls3RenderObject::Ls3RenderObject(const Landschaft& ls3_datei, const std::unordered_set<int>& aktive_ani_nrs, const std::unordered_set<int>& overlay_ani_nrs, float verkn_dimmung_alpha) : GLRenderObject(),
    m_ls3_datei(ls3_datei), m_aktive_ani_nrs(aktive_ani_nrs), m_overlay_ani_nrs(overlay_ani_nrs), m_verkn_dimmung_alpha(verkn_dimmung_alpha) {}

bool Ls3RenderObject::init() {
  TRY(glGenVertexArrays(1, &m_vao));
  TRY(glBindVertexArray(m_vao));

  auto n_subsets = m_ls3_datei.children_SubSet.size();
  m_vbos.resize(n_subsets);
  m_ebos.resize(n_subsets);
  m_texs.resize(n_subsets);

  TRY(glGenBuffers(n_subsets, m_vbos.data()));
  TRY(glGenBuffers(n_subsets, m_ebos.data()));

  for (size_t i = 0; i < n_subsets; i++) {
    const auto& mesh_subset = m_ls3_datei.children_SubSet[i];

    // Create Vertex Buffer Object and copy the vertex data into it
    TRY(glBindBuffer(GL_ARRAY_BUFFER, m_vbos[i]));  // Make vbo the active object
    TRY(glBufferData(GL_ARRAY_BUFFER, mesh_subset->children_Vertex.size() * sizeof(Vertex), mesh_subset->children_Vertex.data(), GL_STATIC_DRAW));  // GL_STATIC_DRAW: vertex data will be uploaded once, drawn many times

    // Create Element Buffer Object
    TRY(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebos[i]));
    TRY(glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh_subset->children_Face.size() * sizeof(Face), mesh_subset->children_Face.data(), GL_STATIC_DRAW));

    auto n_texturen = mesh_subset->children_Textur.size();
    m_texs[i].resize(n_texturen);
    TRY(glGenTextures(n_texturen, m_texs[i].data()));
    for (size_t j = 0; j < n_texturen; j++) {
      Texture texture;
#ifndef NDEBUG
      std::cerr << "Loading image " << mesh_subset->children_Textur[j]->Datei.Dateiname << std::endl;
#endif
      TRY(glBindTexture(GL_TEXTURE_2D, m_texs[i][j]));
      if (!texture.load_DDS(mesh_subset->children_Textur[j]->Datei.Dateiname)) {
        std::cerr << "Loading image " << mesh_subset->children_Textur[j]->Datei.Dateiname << " failed" << std::endl;
        return false;
      }
    }
  }

  m_initialized = true;
  return true;
}

void Ls3RenderObject::setTransform(glm::mat4 transform) {
  m_transform = transform;
}

std::optional<std::pair<glm::vec3, glm::vec3>>
Ls3RenderObject::computeSubsetWorldBBox(size_t subset_index) const {
  const auto& mesh_subset = m_ls3_datei.children_SubSet[subset_index];
  if (mesh_subset->children_Face.empty()) {
    return std::nullopt;
  }

  glm::mat4 transform = getTransform(subset_index);

  // Berechne Bounding Box zuerst lokal und transformiere dann
  glm::vec4 min { std::numeric_limits<glm::vec4::value_type>::max(), std::numeric_limits<glm::vec4::value_type>::max(), std::numeric_limits<glm::vec4::value_type>::max(), 1 };
  glm::vec4 max { std::numeric_limits<glm::vec4::value_type>::lowest(), std::numeric_limits<glm::vec4::value_type>::lowest(), std::numeric_limits<glm::vec4::value_type>::lowest(), 1 };

  // Zaehle nur Vertices, die wirklich verwendet werden
  std::vector<bool> used_vertices(mesh_subset->children_Vertex.size(), false);
  for (const auto& dreieck : mesh_subset->children_Face) {
    for (size_t j = 0; j < 3; j++) {
      used_vertices[dreieck.i[j]] = true;
    }
  }

  auto updateBoundingBoxForVertex = [&](size_t i) {
    const auto& vertex = mesh_subset->children_Vertex[i];
    min.x = std::min(min.x, vertex.p.x);
    max.x = std::max(max.x, vertex.p.x);
    min.y = std::min(min.y, vertex.p.y);
    max.y = std::max(max.y, vertex.p.y);
    min.z = std::min(min.z, vertex.p.z);
    max.z = std::max(max.z, vertex.p.z);
  };

  if (std::find(std::cbegin(used_vertices), std::cend(used_vertices), false) == std::cend(used_vertices)) {
    // all vertices used
    for (size_t i = 0; i < mesh_subset->children_Vertex.size(); ++i) {
      updateBoundingBoxForVertex(i);
    }
  } else {
    for (size_t i = 0; i < mesh_subset->children_Vertex.size(); ++i) {
      if (__builtin_expect(used_vertices[i] == true, 1)) {
        updateBoundingBoxForVertex(i);
      }
    }
  }

  // Transformiere gesamte Bounding Box (alle 8 Eckpunkte)
  glm::vec4 v000 = transform * min;
  glm::vec4 v001 = transform * glm::vec4 { min.x, min.y, max.z, 1 };
  glm::vec4 v010 = transform * glm::vec4 { min.x, max.y, min.z, 1 };
  glm::vec4 v011 = transform * glm::vec4 { min.x, max.y, max.z, 1 };
  glm::vec4 v100 = transform * glm::vec4 { max.x, min.y, min.z, 1 };
  glm::vec4 v101 = transform * glm::vec4 { max.x, min.y, max.z, 1 };
  glm::vec4 v110 = transform * glm::vec4 { max.x, max.y, min.z, 1 };
  glm::vec4 v111 = transform * max;

  glm::vec3 worldMin {
    std::min({ v000.x, v001.x, v010.x, v011.x, v100.x, v101.x, v110.x, v111.x }),
    std::min({ v000.y, v001.y, v010.y, v011.y, v100.y, v101.y, v110.y, v111.y }),
    std::min({ v000.z, v001.z, v010.z, v011.z, v100.z, v101.z, v110.z, v111.z }),
  };
  glm::vec3 worldMax {
    std::max({ v000.x, v001.x, v010.x, v011.x, v100.x, v101.x, v110.x, v111.x }),
    std::max({ v000.y, v001.y, v010.y, v011.y, v100.y, v101.y, v110.y, v111.y }),
    std::max({ v000.z, v001.z, v010.z, v011.z, v100.z, v101.z, v110.z, v111.z }),
  };
  return std::make_pair(worldMin, worldMax);
}

void Ls3RenderObject::updateBoundingBox(std::pair<glm::vec3, glm::vec3>* boundingBox) {
  for (size_t i = 0, n_subsets = m_ls3_datei.children_SubSet.size(); i < n_subsets; i++) {
    const auto subsetBBox = computeSubsetWorldBBox(i);
    if (!subsetBBox) {
      continue;
    }
    boundingBox->first  = glm::min(boundingBox->first,  subsetBBox->first);
    boundingBox->second = glm::max(boundingBox->second, subsetBBox->second);
  }
}

void Ls3RenderObject::collectBlinkOverlays(std::vector<BlinkOverlay>& overlays) const {
  if (m_overlay_ani_nrs.empty()) {
    return;
  }
  for (size_t i = 0, n_subsets = m_ls3_datei.children_SubSet.size(); i < n_subsets; i++) {
    if (!subsetBlinkt(i)) {
      continue;
    }
    const auto subsetBBox = computeSubsetWorldBBox(i);
    if (!subsetBBox) {
      continue;
    }
    const glm::vec3 center = 0.5f * (subsetBBox->first + subsetBBox->second);
    const float radius = 0.5f * glm::length(subsetBBox->second - subsetBBox->first);
    overlays.push_back(BlinkOverlay { center, radius });
  }
}

int Ls3RenderObject::getSubsetZOffsetSumme() {
  int result { 0 };
  for (size_t i = 0, n_subsets = m_ls3_datei.children_SubSet.size(); i < n_subsets; i++) {
    result += m_ls3_datei.children_SubSet[i]->zBias;
  }
  return result;
}

bool Ls3RenderObject::render(const ShaderParameters& shaderParameters) const {
  TRY(glBindVertexArray(m_vao));

  for (size_t i = 0, n_subsets = m_ls3_datei.children_SubSet.size(); i < n_subsets; i++) {
    const auto& mesh_subset = m_ls3_datei.children_SubSet[i];

    if (mesh_subset->TypLs3 == 16) { // Dummy
      continue;
    }
    // Fahrzeug-spezifische Subset-Typen (Spitzen-/Schlusslicht): in einem reinen
    // Signal-Renderer sind diese stets aus.
    if (mesh_subset->TypLs3 >= 17 && mesh_subset->TypLs3 <= 20) {
      continue;
    }

    auto texVoreinstellung = mesh_subset->RenderFlags->TexVoreinstellung == 5 ? mesh_subset->NachtEinstellung /* sollte heissen: TagVoreinstellung */ : mesh_subset->RenderFlags->TexVoreinstellung;

    if (texVoreinstellung == 9 || texVoreinstellung == 10 || texVoreinstellung == 11) {
      // Nachtfenster, Nebelwand
      continue;
    }

    const float dimmungAlpha = getDimmungAlpha(i);
    if (dimmungAlpha <= 0.0f) {
      // Komplett durchsichtig: nicht zeichnen.
      continue;
    }

    glm::mat4 transform = getTransform(i);
    TRY(glUniformMatrix4fv(shaderParameters.uni_model, 1, GL_FALSE, glm::value_ptr(transform)));

    glm::mat4 transform_nor = glm::transpose(glm::inverse(transform));
    TRY(glUniformMatrix4fv(shaderParameters.uni_nor, 1, GL_FALSE, glm::value_ptr(transform_nor)));

    TRY(glBindBuffer(GL_ARRAY_BUFFER, m_vbos[i]));
    TRY(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebos[i]));

    const size_t numTextures = std::min(m_texs[i].size(), shaderParameters.uni_tex.size());
    TRY(glUniform1i(shaderParameters.uni_numTextures, numTextures));
    for (size_t j = 0; j < numTextures; j++) {
      TRY(glActiveTexture(GL_TEXTURE0 + j));
      TRY(glBindTexture(GL_TEXTURE_2D, m_texs[i][j]));

      // https://gamedev.stackexchange.com/a/69397
      float aniso = 0.0f;
      glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);

      TRY(glUniform1i(shaderParameters.uni_tex[j], j));
    }

    TRY(glUniform4f(shaderParameters.uni_diffuse_color,
          mesh_subset->Cd.r / 255.0,
          mesh_subset->Cd.g / 255.0,
          mesh_subset->Cd.b / 255.0,
          mesh_subset->Cd.a / 255.0));
    TRY(glUniform4f(shaderParameters.uni_emissive_color,
          mesh_subset->Ce.r / 255.0,
          mesh_subset->Ce.g / 255.0,
          mesh_subset->Ce.b / 255.0,
          mesh_subset->Ce.a / 255.0));

    TRY(glUniform1i(shaderParameters.uni_texVoreinstellung, texVoreinstellung));

    static_assert(std::is_standard_layout<Vertex>::value, "Vertex must conform to StandardLayoutType");  // for offsetof

    static_assert(sizeof(Vec3::x) == sizeof(GL_FLOAT), "Wrong size of Vec3::X");
    static_assert(sizeof(Vec3::y) == sizeof(GL_FLOAT), "Wrong size of Vec3::Y");
    static_assert(sizeof(Vec3::z) == sizeof(GL_FLOAT), "Wrong size of Vec3::Z");
    static_assert(offsetof(Vec3, x) == 0 * sizeof(GL_FLOAT), "Wrong offset of Vec3::X");
    static_assert(offsetof(Vec3, y) == 1 * sizeof(GL_FLOAT), "Wrong offset of Vec3::Y");
    static_assert(offsetof(Vec3, z) == 2 * sizeof(GL_FLOAT), "Wrong offset of Vec3::Z");

    TRY(glEnableVertexAttribArray(shaderParameters.attrib_pos));
    // This also stores the VBO that is currently bound to GL_ARRAY_BUFFER
    TRY(glVertexAttribPointer(
        shaderParameters.attrib_pos,  // Input contains ...
        3,           // ... three values ..
        GL_FLOAT,    // ... of type GL_FLOAT ...
        GL_FALSE,    // ... which should not be normalized.
        sizeof(Vertex),  // stride
        reinterpret_cast<const GLvoid*>(offsetof(Vertex, p))));         // offset

    TRY(glEnableVertexAttribArray(shaderParameters.attrib_nor));
    TRY(glVertexAttribPointer(
        shaderParameters.attrib_nor,  // Input contains ...
        3,           // ... three values ..
        GL_FLOAT,    // ... of type GL_FLOAT ...
        GL_FALSE,    // ... which should not be normalized.
        sizeof(Vertex),  // stride
        reinterpret_cast<const GLvoid*>(offsetof(Vertex, n))));         // offset

    static_assert(sizeof(Vertex::U) == sizeof(GL_FLOAT), "Wrong size of Vertex::U");
    static_assert(sizeof(Vertex::V) == sizeof(GL_FLOAT), "Wrong size of Vertex::V");
    static_assert(offsetof(Vertex, V) - offsetof(Vertex, U) == sizeof(GL_FLOAT), "Wrong offset of Vertex::V");

    TRY(glEnableVertexAttribArray(shaderParameters.attrib_uv1));
    // This also stores the VBO that is currently bound to GL_ARRAY_BUFFER
    TRY(glVertexAttribPointer(
        shaderParameters.attrib_uv1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const GLvoid*>(offsetof(Vertex, U))));

    static_assert(sizeof(Vertex::U2) == sizeof(GL_FLOAT), "Wrong size of Vertex::U2");
    static_assert(sizeof(Vertex::V2) == sizeof(GL_FLOAT), "Wrong size of Vertex::V2");
    static_assert(offsetof(Vertex, V2) - offsetof(Vertex, U2) == sizeof(GL_FLOAT), "Wrong offset of Vertex::V2");

    TRY(glEnableVertexAttribArray(shaderParameters.attrib_uv2));
    TRY(glVertexAttribPointer(
        shaderParameters.attrib_uv2,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const GLvoid*>(offsetof(Vertex, U2))));

    auto bias = mesh_subset->zBias;

    // Faktor fuer Depth-Bias bei Zusi: 0.000033 - bezieht sich aber auf Direct3D

    // Wine-Source-Code:
    /* The Direct3D depth bias is specified in normalized depth coordinates. In
     * OpenGL the bias is specified in units of "the smallest value that is
     * guaranteed to produce a resolvable offset for a given implementation". To
     * convert from D3D to GL we need to divide the D3D depth bias by that value.
     * We try to detect the value from GL with test draws. On most drivers (r300g,
     * 600g, Nvidia, i965 on Mesa) the value is 2^23 for fixed point depth buffers,
     * for r200 and i965 on OSX it is 2^24, for r500 on OSX it is 2^22. For floating
     * point buffers it is 2^22, 2^23 or 2^24 depending on the GPU. The value does
     * not depend on the depth buffer precision on any driver.
     * [...]
     * Note that SLOPESCALEDEPTHBIAS is a scaling factor for the depth slope, and
     * doesn't need to be scaled to account for GL vs D3D differences. */

    // Auf meinem Computer: scale 2^23 = 8388608, ergibt zusammen mit dem Zusi-Faktor
    // einen Faktor von ~275 fuer den in der LS3-Datei angegebenen Bias.
    // FIXME: Scheint fürs Signal-Rendern nicht zu reichen, wenn auch kleine Tiefenunterschiede
    // überbrückt werden sollen (also nicht nur zwischen Polygonen in exakt gleicher Tiefe).
    // Multipliziere den Faktor mit 10.
    TRY(glPolygonOffset(0, 2750 * bias));

    if ((texVoreinstellung == 4) || ((texVoreinstellung >= 6) && (texVoreinstellung <= 9)) || (texVoreinstellung == 12) || (dimmungAlpha < 1.0f)) {
        // Enable alpha blending
        TRY(glEnable(GL_BLEND));
        TRY(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    } else {
        TRY(glDisable(GL_BLEND));
    }

    TRY(glUniform1f(shaderParameters.uni_dimmungAlpha, dimmungAlpha));

    // Alpha cutoff
    if (numTextures == 0) {
        TRY(glUniform1f(shaderParameters.uni_alphaCutoff, 0));
    } else if (texVoreinstellung == 8) {
        // Laubaehnliche Strukturen, Alpharef 100
        TRY(glUniform1f(shaderParameters.uni_alphaCutoff, 100.0 / 255.0));
    } else if (texVoreinstellung == 12) {
        // Laubaehnliche Strukturen, Alpharef 150
        TRY(glUniform1f(shaderParameters.uni_alphaCutoff, 150.0 / 255.0));
    } else if (texVoreinstellung == 1 || texVoreinstellung == 3) {
        TRY(glUniform1f(shaderParameters.uni_alphaCutoff, 0));
    } else {
        TRY(glUniform1f(shaderParameters.uni_alphaCutoff, 1.0 / 255.0));
    }

#ifndef NDEBUG
    std::cerr << "Drawing " << mesh_subset->children_Face.size() << " triangles" << std::endl;
#endif
    TRY(glDrawElements(GL_TRIANGLES, mesh_subset->children_Face.size() * 3, GL_UNSIGNED_SHORT, 0));
  }

  return true;
}

namespace {

// Findet die zum angegebenen Subset gehoerende MeshAnimation und liefert den
// interpolierten AniPunkt zurueck. t=1, wenn die AniNr der MeshAnimation in der
// Menge der aktiven AniNrs enthalten ist, sonst t=0. Liefert std::nullopt, wenn
// keine passende MeshAnimation existiert.
std::optional<AniPunkt> meshAnimationFuerSubset(const Landschaft& ls3_datei,
    const std::unordered_set<int>& aktive_ani_nrs, size_t subset_index) {
  for (const auto& a : ls3_datei.children_MeshAnimation) {
    if (a->AniIndex >= 0 && static_cast<size_t>(a->AniIndex) == subset_index) {
      const float t = (aktive_ani_nrs.count(a->AniNr) > 0) ? 1.0f : 0.0f;
      return interpoliere(a->children_AniPunkt, t);
    }
  }
  return std::nullopt;
}

}  // namespace

glm::mat4 Ls3RenderObject::getTransform(size_t subset_index) const {
  const auto subset_animation = meshAnimationFuerSubset(m_ls3_datei, m_aktive_ani_nrs, subset_index);

  glm::mat4 transform { 1 };

  if (subset_animation) {
    transform = glm::toMat4(glm::quat { subset_animation->q.w, subset_animation->q.x, subset_animation->q.y, subset_animation->q.z });
    transform = glm::translate(glm::mat4 { 1 }, glm::vec3(subset_animation->p.x, subset_animation->p.y, subset_animation->p.z)) * transform;
  }

  const float zZoom = m_ls3_datei.children_SubSet[subset_index]->zZoom;
  if (zZoom != 0.0f) {
    transform = transform * glm::scale(glm::mat4 { 1 }, glm::vec3((1 + zZoom) * kZZoomFaktor));
  }

  return m_transform * transform;
}

float Ls3RenderObject::getDimmungAlpha(size_t subset_index) const {
  if (subsetBlinkt(subset_index)) {
    return 1.0f;
  }
  const auto subset_animation = meshAnimationFuerSubset(m_ls3_datei, m_aktive_ani_nrs, subset_index);
  const float mesh_dimmung_alpha = subset_animation ? (1.0f - subset_animation->AniDimmung) : 1.0f;
  return m_verkn_dimmung_alpha * mesh_dimmung_alpha;
}

bool Ls3RenderObject::subsetBlinkt(size_t subset_index) const {
  return std::any_of(
    m_ls3_datei.children_MeshAnimation.cbegin(),
    m_ls3_datei.children_MeshAnimation.cend(),
    [&](const auto& a) { return (a->AniIndex >= 0 && static_cast<size_t>(a->AniIndex) == subset_index && m_overlay_ani_nrs.count(a->AniNr) > 0); }
  );
}

}
