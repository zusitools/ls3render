R""(
#version 150

in vec4 GouraudShadedColor;
in vec2 UV1;
in vec2 UV2;

out vec4 outColor;

uniform float alphaCutoff;
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform int numTextures;
uniform int texVoreinstellung;
uniform float dimmungAlpha;

void main() {
  vec4 texColor1 = (numTextures > 0) ? texture(tex1, UV1) : vec4(1,1,1,1);

  // Alpha test (FFP D3DCMP_GREATEREQUAL with alpha_ref/255 thresholds)
  if (texColor1.a < alphaCutoff) {
    discard;
  }

  vec3 outRGB;
  if (texVoreinstellung == 3) {
    // Tex 1 Standard, Tex 2 transparent
    vec4 texColor2 = (numTextures > 1) ? texture(tex2, UV2) : vec4(1,1,1,1);
    outRGB = GouraudShadedColor.rgb * mix(texColor1.rgb, texColor2.rgb, texColor2.a);
  } else {
    outRGB = texColor1.rgb * GouraudShadedColor.rgb;
  }

  if (texVoreinstellung == 4 || (texVoreinstellung >= 6 && texVoreinstellung <= 9)) {
    // Halbtransparenz
    if (numTextures == 0) {
      // Nur wenn keine Textur angegeben ist, wird vom Farbwert auch die Alpha-Komponente verwendet.
      // Die wird wiederum nur gebraucht, wenn Alpha-Blending aktiv ist.
      outColor = vec4(outRGB, GouraudShadedColor.a);
    } else {
      outColor = vec4(outRGB, texColor1.a);
    }
  } else {
    outColor = vec4(outRGB, 1.0);
  }

  outColor.a *= dimmungAlpha;
}
)""
