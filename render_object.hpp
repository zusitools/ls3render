#pragma once

#include <glm/glm.hpp>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstddef>
#include <unordered_map>
#include <vector>

struct Landschaft;

namespace ls3render {

struct ShaderParameters;

class RenderObject {
  public:
    virtual ~RenderObject() {}
    virtual bool init() = 0;
    virtual bool cleanup() = 0;
    virtual void updateBoundingBox(std::pair<glm::vec3, glm::vec3>* boundingBox) = 0;
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

struct LichterSchaltung {
  bool spitzenlichtVorne { false };
  bool spitzenlichtHinten { false };
  bool schlusslichtVorne { false };
  bool schlusslichtHinten { false };
};

class Ls3RenderObject : public GLRenderObject {
  public:
    Ls3RenderObject(const Landschaft& ls3_datei, const std::unordered_map<int, float>& ani_positionen, const LichterSchaltung& lichterSchaltung);
    bool init() override;
    void setTransform(glm::mat4 transform);
    void updateBoundingBox(std::pair<glm::vec3, glm::vec3>* boundingBox) override;
    int getSubsetZOffsetSumme() override;
    bool render(const ShaderParameters& shaderParameters) const override;

  private:
    const Landschaft& m_ls3_datei;
    glm::mat4 m_transform { 1 };
    const std::unordered_map<int, float> m_ani_positionen;
    const LichterSchaltung m_lichter_schaltung;

    glm::mat4 getTransform(size_t subset_index) const;
};

}
