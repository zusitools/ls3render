#pragma once

#define GLEW_STATIC
#include <GL/glew.h>

#include <string>
#include <unordered_map>

namespace ls3render {

/**
 * Cache fuer geladene Texturen.
 *
 * Eigentuemer aller hier verwalteten GL-Texturobjekte. Eine Textur wird beim
 * ersten Aufruf von @ref getOrLoad fuer einen Dateinamen geladen und
 * anschliessend wiederverwendet, bis @ref clear aufgerufen wird (z.B. aus
 * `ls3render_Reset`).
 *
 * Alle GL-Aufrufe erfordern einen aktuellen GL-Kontext. Der Destruktor ruft
 * bewusst keine GL-Funktionen auf, da er typischerweise nach Zerstoerung des
 * GL-Kontextes (am Programmende) ausgefuehrt wird; @ref clear muss explizit
 * vor `glfwTerminate` aufgerufen werden.
 */
class TextureCache {
 public:
  TextureCache() = default;
  ~TextureCache() = default;

  TextureCache(const TextureCache&) = delete;
  TextureCache& operator=(const TextureCache&) = delete;

  /**
   * Liefert das GL-Texturhandle fuer die angegebene Datei. Beim ersten
   * erfolgreichen Aufruf fuer einen Dateinamen wird die Textur geladen und
   * intern gespeichert; bei Folgeaufrufen wird das gespeicherte Handle
   * zurueckgegeben.
   *
   * Fehlgeschlagene Ladeversuche werden nicht gecached, sodass ein erneuter
   * Aufruf fuer dieselbe Datei das Laden erneut versucht.
   *
   * @return GL-Texturhandle, oder 0 bei Lade-Fehlern.
   */
  GLuint getOrLoad(const std::string& filename);

  /**
   * Loescht alle gecachten GL-Texturen und leert den Cache. Erfordert einen
   * aktuellen GL-Kontext.
   */
  void clear();

 private:
  std::unordered_map<std::string, GLuint> m_textures;
};

}  // namespace ls3render
