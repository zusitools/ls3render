R""(
#version 150  // GLSL 1.50

in vec3 position;
in vec3 normal;
in vec2 uv1;
in vec2 uv2;

out vec2 UV1;
out vec2 UV2;
out vec4 GouraudShadedColor;  // Gouraud-style per-vertex lighting

uniform mat4 nor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 shear;
uniform vec4 ambientColor;
uniform vec4 diffuseColor;
uniform vec4 emissiveColor;

const vec3 globalAmbient = vec3(.9);

const vec3 lightDiffuse  = vec3(.9);
const vec3 lightFromDir  = normalize(vec3(-1, -.5, 1));

void main() {
  vec3 transformedNormal = normalize(vec3(nor * vec4(normal, 0)));
  float ndotl = max(0.0, dot(transformedNormal, lightFromDir));

  GouraudShadedColor = vec4(
       emissiveColor.rgb
     + ambientColor.rgb * globalAmbient
     + diffuseColor.rgb * lightDiffuse * ndotl,
  diffuseColor.a);

  UV1 = uv1;
  UV2 = uv2;
  gl_Position = proj * shear * view * model * vec4(position, 1.0);
}
)""
