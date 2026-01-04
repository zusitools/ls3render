R""(
#version 150  // GLSL 1.50

in vec3 position;
in vec3 normal;
in vec2 uv1;
in vec2 uv2;

out vec3 Normal;
out vec4 DiffuseColor;
out vec4 EmissiveColor;
out vec2 UV1;
out vec2 UV2;

uniform mat4 nor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 shear;
uniform vec4 diffuseColor;
uniform vec4 emissiveColor;

void main() {
  Normal = vec3(nor * vec4(normal, 0.0));
  DiffuseColor = diffuseColor;
  EmissiveColor = emissiveColor;
  UV1 = uv1;
  UV2 = uv2;
  gl_Position = proj * shear * view * model * vec4(position, 1.0);
}
)""
