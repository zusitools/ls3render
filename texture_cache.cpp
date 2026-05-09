#include "./texture_cache.hpp"

#include "./texture.hpp"

#include <iostream>
#include <utility>
#include <vector>

namespace ls3render {

GLuint TextureCache::getOrLoad(const std::string& filename) {
  if (auto it = m_textures.find(filename); it != m_textures.end()) {
    return it->second;
  }

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  Texture texture;
  if (!texture.load_DDS(filename)) {
    std::cerr << "Loading image " << filename << " failed" << std::endl;
    glDeleteTextures(1, &tex);
    return 0;
  }

  m_textures.emplace(filename, tex);
  return tex;
}

void TextureCache::clear() {
  if (m_textures.empty()) {
    return;
  }
  std::vector<GLuint> handles;
  handles.reserve(m_textures.size());
  for (const auto& entry : m_textures) {
    handles.push_back(entry.second);
  }
  glDeleteTextures(static_cast<GLsizei>(handles.size()), handles.data());
  m_textures.clear();
}

}  // namespace ls3render
