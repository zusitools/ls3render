#include "./render_object.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

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
#include <unordered_map>
#include <vector>

namespace ls3render {

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

Ls3RenderObject::Ls3RenderObject(const Landschaft& ls3_datei, const std::unordered_map<int, float>& ani_positionen, const LichterSchaltung& lichterSchaltung) : GLRenderObject(),
    m_ls3_datei(ls3_datei), m_ani_positionen(ani_positionen), m_lichter_schaltung{lichterSchaltung} {}

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

void Ls3RenderObject::updateBoundingBox(std::pair<glm::vec3, glm::vec3>* boundingBox) {
  for (size_t i = 0, n_subsets = m_ls3_datei.children_SubSet.size(); i < n_subsets; i++) {
    const auto& mesh_subset = m_ls3_datei.children_SubSet[i];
    if (mesh_subset->children_Face.empty()) {
      continue;
    }

    glm::mat4 transform = getTransform(i);

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

    boundingBox->first[0] = std::min({ boundingBox->first.x, v000.x, v001.x, v010.x, v011.x, v100.x, v101.x, v110.x, v111.x });
    boundingBox->first[1] = std::min({ boundingBox->first.y, v000.y, v001.y, v010.y, v011.y, v100.y, v101.y, v110.y, v111.y });
    boundingBox->first[2] = std::min({ boundingBox->first.z, v000.z, v001.z, v010.z, v011.z, v100.z, v101.z, v110.z, v111.z });

    boundingBox->second[0] = std::max({ boundingBox->second.x, v000.x, v001.x, v010.x, v011.x, v100.x, v101.x, v110.x, v111.x });
    boundingBox->second[1] = std::max({ boundingBox->second.y, v000.y, v001.y, v010.y, v011.y, v100.y, v101.y, v110.y, v111.y });
    boundingBox->second[2] = std::max({ boundingBox->second.z, v000.z, v001.z, v010.z, v011.z, v100.z, v101.z, v110.z, v111.z });
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

    if (mesh_subset->TypLs3 == 16) { // Lichter
      continue;
    }
    if (mesh_subset->TypLs3 == 17 && !m_lichter_schaltung.spitzenlichtVorne) {
      continue;
    }
    if (mesh_subset->TypLs3 == 18 && !m_lichter_schaltung.schlusslichtVorne) {
      continue;
    }
    if (mesh_subset->TypLs3 == 19 && !m_lichter_schaltung.spitzenlichtHinten) {
      continue;
    }
    if (mesh_subset->TypLs3 == 20 && !m_lichter_schaltung.schlusslichtHinten) {
      continue;
    }

    auto texVoreinstellung = mesh_subset->RenderFlags->TexVoreinstellung == 5 ? mesh_subset->NachtEinstellung /* sollte heissen: TagVoreinstellung */ : mesh_subset->RenderFlags->TexVoreinstellung;

    if (texVoreinstellung == 9 || texVoreinstellung == 10 || texVoreinstellung == 11) {
      // Nachtfenster, Nebelwand
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

    TRY(glUniform4f(shaderParameters.uni_color,
          (mesh_subset->Cd.r + mesh_subset->Ce.r) / 255.0,
          (mesh_subset->Cd.g + mesh_subset->Ce.g) / 255.0,
          (mesh_subset->Cd.b + mesh_subset->Ce.b) / 255.0,
          mesh_subset->Cd.a / 255.0));

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
    TRY(glPolygonOffset(0, 275 * bias));

    if (texVoreinstellung == 4) {
        // Enable alpha blending
        TRY(glEnable(GL_BLEND));
        TRY(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    } else {
        TRY(glDisable(GL_BLEND));
    }

    // Alpha cutoff
    if (texVoreinstellung == 8) {
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

glm::mat4 Ls3RenderObject::getTransform(size_t subset_index) const {
  std::optional<AniPunkt> subset_animation;

  for (const auto& a : m_ls3_datei.children_MeshAnimation) {
    std::optional<float> t;
    if (a->AniIndex >= 0 && static_cast<decltype(subset_index)>(a->AniIndex) == subset_index) {
      for (const auto& def : m_ls3_datei.children_Animation) {
        if (std::find_if(
            std::begin(def->children_AniNrs),
            std::end(def->children_AniNrs),
            [&a](const auto& aniNrs) {
              return aniNrs->AniNr == a->AniNr;
            }) != std::end(def->children_AniNrs)) {
          auto it = m_ani_positionen.find(def->AniID);
          if (it != std::end(m_ani_positionen)) {
            t = it->second;
          }
          subset_animation = interpoliere(a->children_AniPunkt, t.value_or(0.0f));
          break;
        }
      }
      if (subset_animation) {
        break;
      }
    }
  }

  glm::mat4 transform { 1 };

  if (subset_animation) {
    transform = glm::toMat4(glm::quat { subset_animation->q.w, subset_animation->q.x, subset_animation->q.y, subset_animation->q.z });
    transform = glm::translate(glm::mat4 { 1 }, glm::vec3(subset_animation->p.x, subset_animation->p.y, subset_animation->p.z)) * transform;
  }

  return m_transform * transform;
}

}
