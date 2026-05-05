#pragma once

#include "./render_object.hpp"

#include "zusi_parser/zusi_types.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <unordered_set>
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

  /**
   * @param aktive_ani_nrs  Menge der AniNr-Werte, die aufgrund aktiver
   *   `<Animation>`-Knoten in der obersten LS3-Datei aktiv sind. Mesh- und
   *   Verknuepfungsanimationen mit einem dieser AniNr-Werte werden auf t=1
   *   ausgewertet, alle anderen auf t=0.
   * @param verkn_dimmung_alpha  Faktor [0,1], mit dem die Transparenz der gesamten
   *   verknuepften Datei multipliziert wird (entsteht durch
   *   `AniDimmung`-Werte uebergeordneter Verknuepfungsanimationen).
   *   1 = vollstaendig undurchsichtig, 0 = vollstaendig durchsichtig.
   */
  bool LadeLandschaft(const zusixml::ZusiPfad& dateiname, const glm::mat4& transform, const std::unordered_set<int>& aktive_ani_nrs, float dimmung_alpha = 1.0f);

  /**
   * Wie @ref LadeLandschaft, aber mit einer bereits geparsten Zusi-Datei. Der Aufrufer
   * uebergibt das Eigentum; die Datei wird in der Scene aufbewahrt. Verknuepfte Dateien
   * werden weiterhin per Pfad nachgeladen.
   */
  bool LadeLandschaft(std::unique_ptr<Zusi> zusi_datei, const zusixml::ZusiPfad& dateiname, const glm::mat4& transform, const std::unordered_set<int>& aktive_ani_nrs, float dimmung_alpha = 1.0f);
  void UpdateBoundingBox(std::pair<glm::vec3, glm::vec3>* bbox);
  bool LoadIntoGraphicsCardMemory();
  void Render(const ShaderParameters& shader_parameters) const;
  void FreeGraphicsCardMemory();

 private:
  std::vector<std::unique_ptr<Zusi>> m_Ls3Dateien;
  std::vector<std::unique_ptr<RenderObject>> m_RenderObjects;
};

}
