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
 * Fuegt ein neues Fahrzeug hinzu.
 *
 * Macht vorherige Rueckgabewerte von @ref ls3render_GetBildbreite, @ref ls3render_GetBildhoehe und @ref ls3render_GetAusgabepufferGroesse ungueltig.
 *
 * @param Dateiname Der Dateiname des Fahrzeugs (Dateisystempfad, kein Zusi-Pfad)
 *
 * @param OffsetX Die horizontale Position des Fahrzeugnullpunktes in Metern. Hoehere Werte -> weiter rechts.
 *
 * @param Fahrzeuglaenge Die Fahrzeuglaenge in Metern (angegeben in den Fahrzeug-Grunddaten).
 * @param Gedreht Ob das Fahrzeug gedreht eingereiht ist.
 *
 * @param StromabnehmerHoehe Die Einbauhoehe des Stromabnehmers in Metern (angegeben in den Fahrzeug-Grunddaten).
 * @param Stromabnehmer1Oben 1, wenn Stromabnehmer 1 gehoben ist, sonst 0.
 * @param Stromabnehmer2Oben 1, wenn Stromabnehmer 2 gehoben ist, sonst 0.
 * @param Stromabnehmer3Oben 1, wenn Stromabnehmer 3 gehoben ist, sonst 0.
 * @param Stromabnehmer4Oben 1, wenn Stromabnehmer 4 gehoben ist, sonst 0.
 *
 * @param SpitzenlichtVorneAn 1, wenn Mesh-Subsets vom Typ "Spitzenlicht vorne" angezeigt werden sollen, sonst 0.
 * @param SpitzenlichtHintenAn 1, wenn Mesh-Subsets vom Typ "Spitzenlicht hinten" angezeigt werden sollen, sonst 0.
 * @param SchlusslichtVorneAn 1, wenn Mesh-Subsets vom Typ "Schlusslicht vorne" angezeigt werden sollen, sonst 0.
 * @param SchlusslichtHintenAn 1, wenn Mesh-Subsets vom Typ "Schlusslicht hinten" angezeigt werden sollen, sonst 0.
 *
 * @return 1 bei Erfolg, 0 bei Fehlschlag.
 */
ls3render_EXPORT int ls3render_AddFahrzeug(const char* Dateiname, float OffsetX, float Fahrzeuglaenge, int Gedreht, float StromabnehmerHoehe, int Stromabnehmer1Oben, int Stromabnehmer2Oben, int Stromabnehmer3Oben, int Stromabnehmer4Oben, int SpitzenlichtVorneAn, int SpitzenlichtHintenAn, int SchlusslichtVorneAn, int SchlusslichtHintenAn);

/**
 * Fuegt die 3D-Datei einer Fahrzeugbeladung zum zuletzt hinzugefügten Fahrzeug hinzu.
 *
 * Macht vorherige Rueckgabewerte von @ref ls3render_GetBildbreite, @ref ls3render_GetBildhoehe und @ref ls3render_GetAusgabepufferGroesse ungueltig.
 *
 * @param Dateiname Der Dateiname des Fahrzeugs (Dateisystempfad, kein Zusi-Pfad)
 *
 * @param OffsetX Die X-Koordinate der Beladung, wie in der Beladungs-Baugruppe im Fahrzeug angegeben
 * @param OffsetY Die Y-Koordinate der Beladung, wie in der Beladungs-Baugruppe im Fahrzeug angegeben
 * @param OffsetZ Die Z-Koordinate der Beladung, wie in der Beladungs-Baugruppe im Fahrzeug angegeben
 * @param PhiX Die X-Drehung der Beladung im Bogenmaß, wie in der Beladungs-Baugruppe im Fahrzeug angegeben
 * @param PhiY Die Y-Drehung der Beladung im Bogenmaß, wie in der Beladungs-Baugruppe im Fahrzeug angegeben
 * @param PhiZ Die Z-Drehung der Beladung im Bogenmaß, wie in der Beladungs-Baugruppe im Fahrzeug angegeben
 *
 * @return 1 bei Erfolg, 0 bei Fehlschlag.
 */
ls3render_EXPORT int ls3render_AddBeladung(const char* Dateiname, float OffsetX, float OffsetY, float OffsetZ, float PhiX, float PhiY, float PhiZ);

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
 * Entfernt alle Fahrzeuge.
 *
 * Macht vorherige Rueckgabewerte von @ref ls3render_GetBildbreite, @ref ls3render_GetBildhoehe und @ref ls3render_GetAusgabepufferGroesse ungueltig.
 */
ls3render_EXPORT void ls3render_Reset();

}
