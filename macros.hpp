#pragma once

#define GLEW_STATIC
#include <GL/glew.h>

#include <iostream>

#define TRY(glDoSomething) { \
  do { \
    glDoSomething; \
    GLenum error = glGetError(); \
    if (error == GL_NO_ERROR) { \
      break; \
    } \
    do { \
      std::cerr << __FILE__ ":" << std::dec << __LINE__ << ": Error executing " #glDoSomething ": 0x" << std::hex << error << std::endl; \
      error = glGetError(); \
    } while (error != GL_NO_ERROR); \
    return false; \
  } while (0); }

#define TRY_GLEW(glewDoSomething) { \
  do { \
    GLenum error = glewDoSomething; \
    if (error != GLEW_OK) { \
      std::cerr << __FILE__ ":" << std::dec << __LINE__ << ": Error executing " #glewDoSomething ": " << glewGetErrorString(error) << std::endl; \
      return false; \
    } \
  } while (0); }

#define TRY_GLFW(glfwDoSomething) glfwDoSomething

