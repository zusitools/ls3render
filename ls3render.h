#if defined _WIN32 || defined __CYGWIN__
  #ifdef ls3render_EXPORTS
    #define ls3render_EXPORT __declspec(dllexport)
  #else
    #define ls3render_EXPORT __declspec(dllimport)
  #endif
  #define DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define ls3render_EXPORT __attribute__ ((visibility ("default")))
  #else
    #define ls3render_EXPORT
  #endif
#endif

extern "C" {

/**
 * Initialisiert die OpenGL-Umgebung.
 *
 * @return 1 bei Erfolg, 0 bei Fehlschlag.
 */
ls3render_EXPORT int ls3render_Init();

/**
 * Raeumt die OpenGL-Umgebung auf.
 *
 * @return 1 bei Erfolg, 0 bei Fehlschlag.
 */
ls3render_EXPORT int ls3render_Cleanup();

/**
 * Setzt die Aufloesung der Ausgabedatei.
 * Macht vorherige Rueckgabewerte von @ref ls3render_GetBildbreite, @ref ls3render_GetBildhoehe und @ref ls3render_GetAusgabepufferGroesse ungueltig.
 * @param PixelProMeter Ein Meter im Modell entspricht so vielen Pixeln in der Ausgabedatei.
 */
ls3render_EXPORT void ls3render_SetPixelProMeter(int PixelProMeter);

/**
 * Aktiviert Multisampling (Multisample Anti Aliasing) mit der angegebenen Anzahl von Samples pro Pixel.
 * @param Samples Anzahl Samples pro Pixel. Ein Wert von 0 deaktiviert Multisampling.
 */
ls3render_EXPORT void ls3render_SetMultisampling(int Samples);

/**
 * Setzt die Parameter für die axonometrische Projektion: https://de.wikipedia.org/wiki/Axonometrie#Kavalierprojektion,_Kabinettprojektion
 * @param Winkel Winkel in Radians für die verzerrte Achse. Empfohlen: Pi/4 = 45 Grad.
 * @param Skalierung Skalierungsfaktor für die verzerrte Achse. 0 = orthographische Projektion (Standard), 0.5 = Kabinettprojektion (empfohlen), 1 = Kavalierprojektion.
 */
ls3render_EXPORT void ls3render_SetAxonometrieParameter(float Winkel, float Skalierung);

/**
 * Fuegt einen Signal-Frame zur aktuellen Szene hinzu. Mehrfachaufrufe sind erlaubt;
 * jeder Frame wird in dieselbe Szene eingefuegt, sodass auch Signale mit mehreren
 * SignalFrame-Eintraegen gemeinsam gerendert werden koennen.
 *
 * Macht vorherige Rueckgabewerte von @ref ls3render_GetBildbreite, @ref ls3render_GetBildhoehe und @ref ls3render_GetAusgabepufferGroesse ungueltig.
 *
 * @param Dateiname OS-Pfad der LS3-Datei (entspricht <SignalFrame><Datei Dateiname=...>; kein Zusi-Pfad).
 *
 * @param PosX X-Koordinate aus <SignalFrame><p ...>.
 * @param PosY Y-Koordinate aus <SignalFrame><p ...>.
 * @param PosZ Z-Koordinate aus <SignalFrame><p ...>.
 *
 * @param PhiX Eulerwinkel X (Bogenmass) aus <SignalFrame><phi ...>.
 * @param PhiY Eulerwinkel Y (Bogenmass) aus <SignalFrame><phi ...>.
 * @param PhiZ Eulerwinkel Z (Bogenmass) aus <SignalFrame><phi ...>.
 *
 * @param SkX Skalierung X aus <SignalFrame><sk ...>; 0 wird als 1 interpretiert (Zusi-Konvention).
 * @param SkY Skalierung Y aus <SignalFrame><sk ...>; 0 wird als 1 interpretiert.
 * @param SkZ Skalierung Z aus <SignalFrame><sk ...>; 0 wird als 1 interpretiert.
 *
 * @return 1 bei Erfolg, 0 bei Fehlschlag.
 */
ls3render_EXPORT int ls3render_AddSignalFrame(const char* Dateiname,
    float PosX, float PosY, float PosZ,
    float PhiX, float PhiY, float PhiZ,
    float SkX, float SkY, float SkZ);

/**
 * @return Die Breite des zu rendernden Bildes in Pixeln.
 */
ls3render_EXPORT int ls3render_GetBildbreite();

/**
 * @return Die Hoehe des zu rendernden Bildes in Pixeln.
 */
ls3render_EXPORT int ls3render_GetBildhoehe();

/**
 * @return Die Groesse des notwendigen Ausgabepuffers in Bytes.
 */
ls3render_EXPORT int ls3render_GetAusgabepufferGroesse();

/**
 * Rendert die Szene und schreibt das Ergebnis als unkomprimierte RGBA-Daten in den angegebenen Ausgabepuffer.
 * @param Ausgabepuffer Ein Zeiger auf den Ausgabepuffer, der mindestens so gross sein muss wie durch @ref ls3render_GetAusgabepufferGroesse angegeben.
 * @return 1 bei Erfolg, 0 bei Fehlschlag.
 */
ls3render_EXPORT int ls3render_Render(void* Ausgabepuffer);

/**
 * Entfernt alle Signal-Frames aus der aktuellen Szene.
 *
 * Macht vorherige Rueckgabewerte von @ref ls3render_GetBildbreite, @ref ls3render_GetBildhoehe und @ref ls3render_GetAusgabepufferGroesse ungueltig.
 */
ls3render_EXPORT void ls3render_Reset();

}
