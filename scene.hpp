#pragma once

#include "./render_object.hpp"

#include "zusi_parser/zusi_types.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zusixml {
  class ZusiPfad;
}

namespace ls3render {

struct ShaderParameters;

class Scene {
 public:
  Scene();

  bool LadeLandschaft(const zusixml::ZusiPfad& dateiname, const glm::mat4& transform, const std::unordered_map<int, float>& ani_positionen, const LichterSchaltung& lichterSchaltung);
  void UpdateBoundingBox(std::pair<glm::vec3, glm::vec3>* bbox);
  bool LoadIntoGraphicsCardMemory();
  void Render(const ShaderParameters& shader_parameters) const;
  void FreeGraphicsCardMemory();

 private:
  std::vector<std::unique_ptr<Zusi>> m_Ls3Dateien;
  std::vector<std::unique_ptr<RenderObject>> m_RenderObjects;
};

}
