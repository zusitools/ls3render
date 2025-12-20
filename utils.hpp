#pragma once

#include "zusi_parser/zusi_types.hpp"

#include <glm/glm.hpp>

#include <cassert>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

namespace ls3render {

inline std::optional<AniPunkt> interpoliere(const std::vector<std::unique_ptr<AniPunkt>>& ani_punkte, float t) {
  if (ani_punkte.size() == 0) {
    return std::nullopt;
  }
  auto rechts = std::upper_bound(std::begin(ani_punkte), std::end(ani_punkte), t, [](float t, const auto& a) { return t < a->AniZeit; });
  if (rechts == std::begin(ani_punkte)) {
    return AniPunkt(**std::begin(ani_punkte));
  } else {
    auto links = std::prev(rechts);
    if (rechts == std::end(ani_punkte) || (*rechts)->AniZeit == (*links)->AniZeit) {
      return AniPunkt(**links);
    } else {
      assert((*rechts)->AniZeit > (*links)->AniZeit);
      // std::cerr << "Interpoliere links = " << links->AniZeit << " t = " << t << " rechts = " << rechts->t << std::endl;
      float factor = (t - (*links)->AniZeit) / ((*rechts)->AniZeit - (*links)->AniZeit);
      glm::quat rot = glm::slerp(
          glm::quat((*links)->q.w, (*links)->q.x, (*links)->q.y, (*links)->q.z),
          glm::quat((*rechts)->q.w, (*rechts)->q.x, (*rechts)->q.y, (*rechts)->q.z),
          factor);
      // std::cerr << "  slerp r1 = ("
      //   << links->q.w << ", " << links->q.x << ", " << links->q.y << ", " << links->q.z << ") r2 = ("
      //   << rechts->q.w << ", " << rechts->q.x << ", " << rechts->q.y << ", " << rechts->q.z << ") factor = " << factor << std::endl;

      AniPunkt result;
      result.AniZeit = t;
      result.p = Vec3 {
        (1 - factor) * (*links)->p.x + factor * (*rechts)->p.x,
        (1 - factor) * (*links)->p.y + factor * (*rechts)->p.y,
        (1 - factor) * (*links)->p.z + factor * (*rechts)->p.z
      };
      result.q = Quaternion {
        rot.w,
        rot.x,
        rot.y,
        rot.z
      };
      return result;
    }
  }
}

}
