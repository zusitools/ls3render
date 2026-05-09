#pragma once

#include "./render_object.hpp"

#include "zusi_parser/zusi_types.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace zusixml {
  class ZusiPfad;
}

namespace ls3render {

struct ShaderParameters;
class TextureCache;

/**
 * Beschreibt einen radialen Strahlen-Stern, der ueber einem aktuell blinkenden
 * Mesh oder einer aktuell blinkenden Verknuepfung gezeichnet wird.
 */
struct BlinkOverlay {
  glm::vec3 center;  ///< Mittelpunkt der Welt-Bounding-Box.
  float radius;      ///< Halbe Diagonale der Welt-Bounding-Box (Skala fuer den Stern).
};

class Scene {
 public:
  Scene();

  /**
   * @param aktive_ani_namen  Menge der Animationsnamen (XML-Attribut
   *   `AniBeschreibung`), die in der obersten LS3-Datei anhand der
   *   Signalbild-ID aktiviert wurden. In dieser und allen rekursiv geladenen
   *   verknuepften LS3-Dateien werden diejenigen `<Animation>`-Knoten als
   *   aktiv betrachtet, deren `AniBeschreibung` in dieser Menge enthalten
   *   ist. Die zugehoerigen `AniNrs` der jeweiligen Datei steuern die
   *   `MeshAnimation`- und `VerknAnimation`-Auswertung (t=1 vs. t=0).
   * @param blink_ani_namen  Teilmenge von @p aktive_ani_namen, fuer die ein
   *   Blink-Overlay (Strahlen-Stern) erzeugt werden soll. Praktisch:
   *   Animationsnamen aus der obersten LS3-Datei, deren `<Animation>`-Knoten
   *   `AniLoopen="1"` haben und deren Bit im Signalbild gesetzt ist. Fuer
   *   eine als blinkend erkannte Verknuepfung wird beim rekursiven Aufruf
   *   eine leere Menge weitergereicht, damit innerhalb der Verknuepfung
   *   keine zusaetzlichen Sterne entstehen.
   * @param verkn_dimmung_alpha  Faktor [0,1], mit dem die Transparenz der gesamten
   *   verknuepften Datei multipliziert wird (entsteht durch
   *   `AniDimmung`-Werte uebergeordneter Verknuepfungsanimationen).
   *   1 = vollstaendig undurchsichtig, 0 = vollstaendig durchsichtig.
   */
  bool LadeLandschaft(const zusixml::ZusiPfad& dateiname, const glm::mat4& transform, const std::unordered_set<std::string>& aktive_ani_namen, const std::unordered_set<std::string>& blink_ani_namen, float dimmung_alpha = 1.0f);

  /**
   * Wie @ref LadeLandschaft, aber mit einer bereits geparsten Zusi-Datei. Der Aufrufer
   * uebergibt das Eigentum; die Datei wird in der Scene aufbewahrt. Verknuepfte Dateien
   * werden weiterhin per Pfad nachgeladen.
   */
  bool LadeLandschaft(std::unique_ptr<Zusi> zusi_datei, const zusixml::ZusiPfad& dateiname, const glm::mat4& transform, const std::unordered_set<std::string>& aktive_ani_namen, const std::unordered_set<std::string>& blink_ani_namen, float dimmung_alpha = 1.0f);
  void UpdateBoundingBox(std::pair<glm::vec3, glm::vec3>* bbox);
  /**
   * Laedt Geometrie und Texturen der Szene in den Grafikkartenspeicher.
   * Texturen werden ueber den uebergebenen @p textureCache geladen, sodass
   * eine Datei nur beim ersten Aufruf vom Datentraeger geholt wird; Folgeauf-
   * rufe (auch ueber mehrere @ref Render-Zyklen hinweg) verwenden den vor-
   * handenen Eintrag.
   */
  bool LoadIntoGraphicsCardMemory(TextureCache& textureCache);
  void Render(const ShaderParameters& shader_parameters) const;
  void FreeGraphicsCardMemory();

  const std::vector<BlinkOverlay>& getBlinkOverlays() const { return m_BlinkOverlays; }

 private:
  std::vector<std::unique_ptr<Zusi>> m_Ls3Dateien;
  std::vector<std::unique_ptr<RenderObject>> m_RenderObjects;
  std::vector<BlinkOverlay> m_BlinkOverlays;
};

}
