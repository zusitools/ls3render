R""(
#version 150

in vec3 dir;

uniform mat4 view;
uniform mat4 proj;
uniform mat4 shear;
uniform vec3 center;
uniform float radius;

void main() {
  vec3 world = center + dir * radius;
  gl_Position = proj * shear * view * vec4(world, 1.0);
}
)""
