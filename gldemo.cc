#include "ls3render.h"

#include <cstdint>
#include <cstdio>
#include <vector>

int main(int /*argc*/, char** argv) {
  struct Cleanup {
    ~Cleanup() { ls3render_Cleanup(); }
  } cleanup;

  if (!ls3render_Init()) {
    return 1;
  }

  ls3render_SetSignalbild(~uint64_t{0});
  if (!ls3render_AddSignalFrame(argv[1],
          /*Pos=*/0.0f, 0.0f, 0.0f,
          /*Phi=*/0.0f, 0.0f, 0.0f,
          /*Sk=*/0.0f, 0.0f, 0.0f)) {
    return 1;
  }

  std::vector<uint8_t> buf(ls3render_GetAusgabepufferGroesse());
  if (!ls3render_Render(buf.data())) {
    return 1;
  }

  short TGAhead[] = {
    0,  // image id length 0, color map type 0
    2,  // data type code 2 [uncompressed RGB], color map origin 0 (first half)
    0,  // color map origin 0 (second half), color map length 0 (first half)
    0,  // color map length 0 (second half), color map depth 0
    0,  // x origin
    0,  // y origin
    static_cast<short>(ls3render_GetBildbreite()),  // width
    static_cast<short>(ls3render_GetBildhoehe()), // height
    32  // bits per pixel 32, image descriptor 0
  };
  FILE* out = fopen("screenshot.tga","wb");
  fwrite(&TGAhead, sizeof(TGAhead), 1, out);
  fwrite(&buf[0], buf.size(), 1, out);
  fclose(out);
}
