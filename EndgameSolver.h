#pragma once
#include "Solver.h"
#include "Macros.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <cstdint>
#include <utility>

using std::vector;
using std::map;
using std::pair;

struct EndgameResult {
  double winProbability;
  int bestRow;
  int bestCol;
  bool valid;
};

class EndgameSolver {
public:
  Solver solver;

  struct ConfigMask {
    uint64_t lo;
    uint64_t hi;

    ConfigMask() : lo(0), hi(0) {}
    ConfigMask(uint64_t l, uint64_t h) : lo(l), hi(h) {}

    void setBit(int i) {
      if (i < 64) lo |= (1ULL << i);
      else hi |= (1ULL << (i - 64));
    }

    bool getBit(int i) const {
      if (i < 64) return (lo >> i) & 1;
      return (hi >> (i - 64)) & 1;
    }

    int popcount() const {
#if defined(_MSC_VER) && !defined(__clang__)
      return (int)__popcnt64(lo) + (int)__popcnt64(hi);
#else
      return __builtin_popcountll(lo) + __builtin_popcountll(hi);
#endif
    }

    bool operator==(const ConfigMask& o) const { return lo == o.lo && hi == o.hi; }
  };

  struct StateKey {
    uint64_t revealedMask;
    uint64_t configLo;
    uint64_t configHi;

    bool operator==(const StateKey& o) const {
      return revealedMask == o.revealedMask && configLo == o.configLo && configHi == o.configHi;
    }
  };

  struct StateKeyHash {
    size_t operator()(const StateKey& k) const {
      size_t h = std::hash<uint64_t>{}(k.revealedMask);
      h ^= std::hash<uint64_t>{}(k.configLo) + 0x9e3779b9 + (h << 6) + (h >> 2);
      h ^= std::hash<uint64_t>{}(k.configHi) + 0x9e3779b9 + (h << 6) + (h >> 2);
      return h;
    }
  };

  struct ObservationKey {
    uint64_t newRevealedMask;
    vector<int> values;

    bool operator<(const ObservationKey& o) const {
      if (newRevealedMask != o.newRevealedMask) return newRevealedMask < o.newRevealedMask;
      return values < o.values;
    }
  };

  int numCells;
  int numConfigs;
  vector<pair<int,int>> cellPos;                   // idx -> (r, c)
  vector<vector<int>> posToIdx;                    // (r, c) -> idx (-1 if not an unrevealed cell)
  vector<vector<bool>> configMine;                 // [config][cell] -> is mine?
  vector<vector<int>> configRevealValue;           // [config][cell] -> number shown if revealed, -1 if mine
  vector<vector<int>> adjacency;                   // [cell] -> list of neighbor cell indices

  std::unordered_map<StateKey, double, StateKeyHash> memo;

  EndgameSolver(vector<vector<int>> rd);

  bool buildConfigurations(int mines, int maxConfigs = MAX_ENDGAME_CONFIGS);
  void precomputeRevealValues();
  void buildAdjacency();
  uint64_t simulateReveal(int cellIdx, int configIdx, uint64_t currentRevealed) const;
  double solve(uint64_t revealedMask, ConfigMask configMask);
  EndgameResult solveEndgame(int mines, int maxConfigs = MAX_ENDGAME_CONFIGS);
};
