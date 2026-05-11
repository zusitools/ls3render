#define ls3render_EXPORT

#include <stdint.h>

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
 * Setzt die Signalbild-ID, die von nachfolgenden Aufrufen von @ref ls3render_AddSignalFrame
 * konsumiert wird. Die ID kodiert bitweise (von rechts gezaehlt), welche Frames sichtbar
 * sind bzw. wie deren Animationen geschaltet werden:
 *
 * - Enthaelt die LS3-Datei eines Frames keine `<Animation>`-Knoten, verbraucht der Frame
 *   genau ein Bit. Ist das Bit 0, wird der Frame nicht gerendert; ist es 1, wird er
 *   regulaer dargestellt.
 * - Enthaelt die LS3-Datei N>0 `<Animation>`-Knoten, verbraucht der Frame N Bits. Bit i
 *   schaltet die i-te Animation (Dokumentenreihenfolge) auf Position 1 (Bit gesetzt) bzw.
 *   Position 0 (Bit nicht gesetzt). Der Frame wird in jedem Fall gerendert.
 * - Ist der Dateiname leer (oder `nullptr`), wirkt der Frame als Platzhalter: Er
 *   verbraucht genau ein Bit, wird unabhaengig vom Bitwert nicht gerendert und
 *   veraendert die Szene nicht.
 *
 * Default ist `~0ull` (alle Frames sichtbar, alle Animationen auf Position 1). Wird auch
 * durch @ref ls3render_Reset auf diesen Wert zurueckgesetzt.
 */
ls3render_EXPORT void ls3render_SetSignalbild(uint64_t SignalbildID);

/**
 * Fuegt einen Signal-Frame zur aktuellen Szene hinzu. Mehrfachaufrufe sind erlaubt;
 * jeder Frame wird in dieselbe Szene eingefuegt, sodass auch Signale mit mehreren
 * SignalFrame-Eintraegen gemeinsam gerendert werden koennen.
 *
 * Konsumiert die unteren Bits der zuvor mit @ref ls3render_SetSignalbild gesetzten
 * Signalbild-ID (Default `~0ull`) und schiebt diese intern nach rechts. Die Anzahl
 * konsumierter Bits richtet sich nach der LS3-Datei (siehe Doku zu
 * @ref ls3render_SetSignalbild).
 *
 * Macht vorherige Rueckgabewerte von @ref ls3render_GetBildbreite, @ref ls3render_GetBildhoehe und @ref ls3render_GetAusgabepufferGroesse ungueltig.
 *
 * @param Dateiname OS-Pfad der LS3-Datei (entspricht <SignalFrame><Datei Dateiname=...>; kein Zusi-Pfad).
 *                  Ein leerer Dateiname (`""` oder `nullptr`) wird als Platzhalter
 *                  behandelt: Der Frame wird nicht gerendert, konsumiert aber genau
 *                  1 Bit der Signalbild-ID, sodass nachfolgende Frames weiterhin auf
 *                  den korrekten Bits operieren.
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
 * @return 1 bei Erfolg, 0 bei Fehlschlag. Auch bei einem aufgrund des Signalbild-Bits
 *         ausgelassenen Frame wird 1 zurueckgegeben.
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
 * Rendert die Szene und schreibt das Ergebnis als unkomprimierte 32-Bit-Pixeldaten
 * in den angegebenen Ausgabepuffer. Die Byte-Reihenfolge je Pixel ist B, G, R, A
 * (passend zu GL_BGRA bzw. zum TGA-Truecolor-Format). Die Zeilen sind von unten
 * nach oben angeordnet (OpenGL-Konvention).
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
