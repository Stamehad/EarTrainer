#pragma once
#include <vector>
#include <map>
#include <algorithm>

template <typename Impl>
struct CatalogBase {
  // Table entry: level and pointer/reference to a drills vector.
  struct Row { int level; const std::vector<DrillSpec>* drills; };

  // Impl must define: static const std::vector<Row>& table();
  static const std::vector<int>& known_levels() {
    static const std::vector<int> levels = []{
      std::vector<int> v;
      for (const auto& r : Impl::table()) v.push_back(r.level);
      std::sort(v.begin(), v.end());
      v.erase(std::unique(v.begin(), v.end()), v.end());
      return v;
    }();
    return levels;
  }

  static const std::map<int, std::vector<int>>& phases() {
    static const std::map<int, std::vector<int>> p = []{
      std::map<int, std::vector<int>> m;
      for (int L : known_levels()) {
        int phase = (L < 10) ? 0 : (L / 10) % 10;
        m[phase].push_back(L);
      }
      for (auto& kv : m) std::sort(kv.second.begin(), kv.second.end());
      return m;
    }();
    return p;
  }

  static const std::vector<DrillSpec>& drills_for_level(int level) {
    for (const auto& r : Impl::table()) if (r.level == level) return *r.drills;
    static const std::vector<DrillSpec> empty;
    return empty;
  }
};