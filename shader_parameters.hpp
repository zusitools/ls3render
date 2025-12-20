#pragma once

#define GLEW_STATIC
#include <GL/glew.h>

#include <cstddef>
#include <iostream>
#include <vector>

namespace ls3render {

struct ShaderParameters {
  GLint attrib_pos;
  GLint attrib_nor;
  GLint attrib_uv1;
  GLint attrib_uv2;
  GLint uni_nor;
  GLint uni_model;
  GLint uni_view;
  GLint uni_proj;
  GLint uni_shear;
  std::vector<GLint> uni_tex;
  GLint uni_numTextures;
  GLint uni_color;
  GLint uni_alphaCutoff;
  GLint uni_texVoreinstellung;

  ShaderParameters() : uni_tex() {}

  void validate() {
#define CHECK_MINUS_ONE(a) do { if ((a) == -1) { \
      std::cerr << "Warning: Shader parameter " #a " is -1" << std::endl; \
    } } while (0);

    CHECK_MINUS_ONE(attrib_pos);
    CHECK_MINUS_ONE(attrib_nor);
    CHECK_MINUS_ONE(attrib_uv1);
    CHECK_MINUS_ONE(attrib_uv2);
    CHECK_MINUS_ONE(uni_nor);
    CHECK_MINUS_ONE(uni_model);
    CHECK_MINUS_ONE(uni_view);
    CHECK_MINUS_ONE(uni_proj);
    CHECK_MINUS_ONE(uni_shear);
    for (size_t i = 0; i < uni_tex.size(); i++) {
      CHECK_MINUS_ONE(uni_tex[i]);
    }
    CHECK_MINUS_ONE(uni_numTextures);
    CHECK_MINUS_ONE(uni_color);
    CHECK_MINUS_ONE(uni_alphaCutoff);
    CHECK_MINUS_ONE(uni_texVoreinstellung);
  }

#undef CHECK_MINUS_ONE
};

}
