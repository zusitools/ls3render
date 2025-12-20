#include "ls3render.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  ls3render_Init();
  ls3render_SetMultisampling(4);
#if 0
  ls3render_SetPixelProMeter(20);
#else
  ls3render_SetPixelProMeter(40);
  ls3render_SetAxonometrieParameter(3.141592/4.0f, 0.5f);
#endif

  std::vector<uint8_t> buf;
  for (int i = 1; i < argc; i++) {
    std::string dateiname(argv[i]);
    std::cerr << "Rendering " << i << "/" << (argc-1) << ": " << dateiname << std::endl;
    ls3render_AddFahrzeug(dateiname.c_str(), 0, 0, false,
        // Stromabnehmer
        0, false, false, false, false,
        // Lichter
        false, false, false, false
    );

    int bufsize = ls3render_GetAusgabepufferGroesse();

    if (bufsize == 0) {
      std::cerr << "Nothing to render for " << dateiname << "!\n";
      continue;
    }
    buf.resize(bufsize);

    if (ls3render_Render(buf.data()) == 0) {
      std::cerr << "Failed to render " << dateiname << "!\n";
      continue;
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

#ifdef _WIN32
    std::replace(std::begin(dateiname), std::end(dateiname), '\\', '_');
    std::replace(std::begin(dateiname), std::end(dateiname), ':', '_');
#else
    std::replace(std::begin(dateiname), std::end(dateiname), '/', '_');
#endif

    std::cout << "Writing to " << dateiname << ".tga\n";
    FILE* out = fopen((dateiname + ".tga").c_str(),"wb");
    fwrite(&TGAhead, sizeof(TGAhead), 1, out);
    fwrite(&buf[0], bufsize, 1, out);
    fclose(out);

    ls3render_Reset();
  }

  ls3render_Cleanup();
}
