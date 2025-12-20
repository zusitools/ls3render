#include "./scene.hpp"

#include "./render_object.hpp"
#include "./utils.hpp"

#include "zusi_parser/zusi_types.hpp"
#include "zusi_parser/utils.hpp"

#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ls3render {

bool Scene::LadeLandschaft(const zusixml::ZusiPfad& dateiname, const glm::mat4& transform, const std::unordered_map<int, float>& ani_positionen, const ls3render::LichterSchaltung& lichterSchaltung) {
  const auto& dateinameOsPfad = dateiname.alsOsPfad();
  std::unique_ptr<Zusi> zusi_datei = zusixml::tryParseFile(dateinameOsPfad);
  if (!zusi_datei) {
    std::cerr << "Error parsing " << dateinameOsPfad << "\n";
    return false;
  }
  auto* ls3_datei = zusi_datei->Landschaft.get();
  if (!ls3_datei) {
    std::cerr << "Not an LS3 file: " << dateinameOsPfad << "\n";
    return false;
  }

  if (!ls3_datei->lsb.Dateiname.empty()) {
    std::string lsb_pfad = zusixml::ZusiPfad::vonZusiPfad(ls3_datei->lsb.Dateiname, dateiname).alsOsPfad();

    std::ifstream lsb_stream;
    lsb_stream.exceptions(std::ifstream::failbit | std::ifstream::eofbit | std::ifstream::badbit);
    try {
      lsb_stream.open(lsb_pfad, std::ios::in | std::ios::binary);
    } catch (const std::ifstream::failure& e) {
      std::cerr << lsb_pfad << ": open() failed: " << e.what();
      return false;
    }

    try {
      for (const auto& mesh_subset : ls3_datei->children_SubSet) {
        static_assert(sizeof(Vertex) == 40, "Wrong size of Vertex");
        static_assert(offsetof(Vertex, p) == 0, "Wrong offset of Vertex::p");
        static_assert(offsetof(Vertex, n) == 12, "Wrong offset of Vertex::n");
        static_assert(offsetof(Vertex, U) == 24, "Wrong offset of Vertex::U");
        static_assert(offsetof(Vertex, V) == 28, "Wrong offset of Vertex::V");
        static_assert(offsetof(Vertex, U2) == 32, "Wrong offset of Vertex::U2");
        static_assert(offsetof(Vertex, V2) == 36, "Wrong offset of Vertex::V2");
        mesh_subset->children_Vertex.resize(mesh_subset->MeshV);
        lsb_stream.read(reinterpret_cast<char*>(mesh_subset->children_Vertex.data()), mesh_subset->children_Vertex.size() * sizeof(Vertex));

        static_assert(sizeof(Face) == 6, "Wrong size of Face");
        assert(mesh_subset->MeshI % 3 == 0);
        mesh_subset->children_Face.resize(mesh_subset->MeshI / 3);
        lsb_stream.read(reinterpret_cast<char*>(mesh_subset->children_Face.data()), mesh_subset->children_Face.size() * sizeof(Face));
      }
    } catch (const std::ifstream::failure& e) {
      std::cerr << lsb_pfad << ": read() failed: " << e.what();
      return false;
    }

    lsb_stream.exceptions(std::ios_base::iostate());
    lsb_stream.peek();
    assert(lsb_stream.eof());
  }

  for (auto& mesh_subset : ls3_datei->children_SubSet) {
    for (auto& textur : mesh_subset->children_Textur) {
      if (!textur->Datei.Dateiname.empty()) {
        textur->Datei.Dateiname = zusixml::ZusiPfad::vonZusiPfad(textur->Datei.Dateiname, dateiname).alsOsPfad();
      }
    }
  }

  m_Ls3Dateien.push_back(std::move(zusi_datei));  // keep for later

  auto render_object = std::make_unique<Ls3RenderObject>(*ls3_datei, ani_positionen, lichterSchaltung);
  render_object->setTransform(transform);
  m_RenderObjects.push_back(std::move(render_object));

  for (size_t counter = 0, len = ls3_datei->children_Verknuepfte.size(); counter < len; counter++) {
    // Zusi zeichnet verknuepfte Dateien mit dem gleichen Abstand zur Kamera
    // in der _umgekehrten_ Reihenfolge, wie sie in der LS3-Datei stehen.
    // Jede Datei bekommt einen Abstandsbonus in Hoehe der doppelten Summe der Subset-Z-Offsets.
    // Wir nehmen vereinfachend an, dass alle Objekte denselben Abstand
    // zur Kamera haben. Der Z-Offset wird spaeter vor dem Rendern beruecksichtigt.
    size_t i = len - 1 - counter;
    const auto& verkn = ls3_datei->children_Verknuepfte[i];

    if (verkn->SichtbarAb > 1) {
      continue;
    }

    const auto verkn_dateiname = zusixml::ZusiPfad::vonZusiPfad(verkn->Datei.Dateiname, dateiname);
    if (verkn->Datei.Dateiname.empty()) {
      continue;
    }
    std::optional<AniPunkt> verkn_animation;

    for (const auto& v : ls3_datei->children_VerknAnimation) {
      std::optional<float> t;
      if (v->AniIndex >= 0 && static_cast<decltype(i)>(v->AniIndex) == i) {
        for (const auto& def : ls3_datei->children_Animation) {
          if (std::find_if(
              std::begin(def->children_AniNrs),
              std::end(def->children_AniNrs),
              [&v](const auto& aniNrs) {
                return aniNrs->AniNr == v->AniNr;
              }) != std::end(def->children_AniNrs)) {
            auto it = ani_positionen.find(def->AniID);
            if (it != std::end(ani_positionen)) {
              t = it->second;
            } else {
            }
            verkn_animation = interpoliere(v->children_AniPunkt, t.value_or(0.0f));
            break;
          }
        }
        if (verkn_animation) {
          break;
        }
      }
    }

    glm::mat4 rot_verkn = glm::eulerAngleZYX(verkn->phi.z, verkn->phi.y, verkn->phi.x);
    glm::mat4 transform_verkn = rot_verkn;

    if (verkn_animation) {
      transform_verkn = glm::toMat4(glm::quat(verkn_animation->q.w, verkn_animation->q.x, verkn_animation->q.y, verkn_animation->q.z)) * transform_verkn;
    }

    auto zero_to_one = [](float f) { return f == 0.0 ? 1.0 : f; };
    transform_verkn = glm::scale(glm::mat4 { 1 }, glm::vec3(zero_to_one(verkn->sk.x), zero_to_one(verkn->sk.y), zero_to_one(verkn->sk.z))) * transform_verkn;
    transform_verkn = glm::translate(glm::mat4 { 1 }, glm::vec3(verkn->p.x, verkn->p.y, verkn->p.z)) * transform_verkn;

    if (verkn_animation) {
      // Translation der Verknuepfungsanimation bezieht sich auf das durch die Verknuepfung rotierte Koordinatensystem.
      glm::mat4 translate_verkn_animation =
          rot_verkn * glm::translate(glm::mat4 { 1 }, glm::vec3(verkn_animation->p.x, verkn_animation->p.y, verkn_animation->p.z)) * glm::inverse(rot_verkn);
      transform_verkn = translate_verkn_animation * transform_verkn;
    }

    this->LadeLandschaft(verkn_dateiname, transform * transform_verkn, ani_positionen, lichterSchaltung);
  }

  return true;
}

void Scene::UpdateBoundingBox(std::pair<glm::vec3, glm::vec3>* bbox) {
  // Update bounding box
  for (const auto& ro : m_RenderObjects) {
    ro->updateBoundingBox(bbox);
  }
}

bool Scene::LoadIntoGraphicsCardMemory() {
  // Sortiere nach -zOffsetSumme, damit negativer Z-Offset => spaeter zeichnen
  std::stable_sort(std::begin(m_RenderObjects), std::end(m_RenderObjects), [](const auto& lhs, const auto& rhs) {
    // lhs < rhs
    return -lhs->getSubsetZOffsetSumme() < -rhs->getSubsetZOffsetSumme();
  });

  for (const auto& ro : m_RenderObjects) {
    if (!ro->init()) {
      std::cerr << "Error initializing render object\n";
      return false;
    }
  }
  return true;
}

void Scene::Render(const ShaderParameters& shader_parameters) const {
  for (const auto& render_object : m_RenderObjects) {
    render_object->render(shader_parameters);
  }
}

void Scene::FreeGraphicsCardMemory() {
  for (const auto& ro : m_RenderObjects) {
    ro->cleanup();
  }
}

Scene::Scene() : m_Ls3Dateien(), m_RenderObjects() {}

}
