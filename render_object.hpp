#pragma once

#include <glm/glm.hpp>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstddef>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

struct Landschaft;

namespace ls3render {

struct ShaderParameters;
struct BlinkOverlay;

class RenderObject {
  public:
    virtual ~RenderObject() {}
    virtual bool init() = 0;
    virtual bool cleanup() = 0;
    virtual void updateBoundingBox(std::pair<glm::vec3, glm::vec3>* boundingBox) = 0;
    virtual void collectBlinkOverlays(std::vector<BlinkOverlay>& overlays) const = 0;
    virtual int getSubsetZOffsetSumme() = 0;
    virtual bool render(const ShaderParameters& shaderParameters) const = 0;
};

class GLRenderObject : public RenderObject {
  public:
    GLRenderObject();
    bool cleanup() override;
    ~GLRenderObject() override;

  protected:
    GLuint m_vao;
    std::vector<GLuint> m_vbos;
    std::vector<GLuint> m_ebos;
    std::vector<std::vector<GLuint>> m_texs;
    bool m_initialized;

};

class Ls3RenderObject : public GLRenderObject {
  public:
    /**
     * @param aktive_ani_nrs  Menge aktiver AniNr-Werte (siehe @ref Scene::LadeLandschaft).
     * @param overlay_ani_nrs  Menge der AniNr-Werte, fuer die ein Blink-Overlay
     *   erzeugt werden soll. Subsets mit MeshAnimation in dieser Menge werden
     *   in @ref collectBlinkOverlays aufgesammelt.
     * @param verkn_dimmung_alpha  Multiplikativer Dimmungs-Faktor [0,1] aus
     *   uebergeordneten Verknuepfungsanimationen.
     */
    Ls3RenderObject(const Landschaft& ls3_datei, const std::unordered_set<int>& aktive_ani_nrs, const std::unordered_set<int>& overlay_ani_nrs, float verkn_dimmung_alpha);
    bool init() override;
    void setTransform(glm::mat4 transform);
    void updateBoundingBox(std::pair<glm::vec3, glm::vec3>* boundingBox) override;
    void collectBlinkOverlays(std::vector<BlinkOverlay>& overlays) const override;
    int getSubsetZOffsetSumme() override;
    bool render(const ShaderParameters& shaderParameters) const override;

  private:
    const Landschaft& m_ls3_datei;
    glm::mat4 m_transform { 1 };
    const std::unordered_set<int> m_aktive_ani_nrs;
    const std::unordered_set<int> m_overlay_ani_nrs;
    const float m_verkn_dimmung_alpha;

    /// Transform fuer das angegebene Subset, beruecksichtigt evtl. vorhandene MeshAnimation.
    glm::mat4 getTransform(size_t subset_index) const;
    /// Multiplikativer Dimmungs-Faktor fuer das angegebene Subset, kombiniert
    /// die Verknuepfungs- und Mesh-Dimmung.
    float getDimmungAlpha(size_t subset_index) const;
    /// Gibt zurück, ob das Subset Nr. i Teil einer Blink-Animation ist.
    bool subsetBlinkt(size_t subset_index) const;
    /// Liefert die Welt-Bounding-Box des angegebenen Subsets, falls dieses
    /// mindestens ein Face hat. Beruecksichtigt @ref getTransform.
    std::optional<std::pair<glm::vec3, glm::vec3>> computeSubsetWorldBBox(size_t subset_index) const;
};

}
