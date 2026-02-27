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
    vector<uint64_t> words;

    ConfigMask() {}
    explicit ConfigMask(int numBits) : words((numBits + 63) / 64, 0) {}

    void setBit(int i) {
      int idx = i / 64;
      if (idx >= (int)words.size()) words.resize(idx + 1, 0);
      words[idx] |= (1ULL << (i % 64));
    }

    bool getBit(int i) const {
      int idx = i / 64;
      if (idx >= (int)words.size()) return false;
      return (words[idx] >> (i % 64)) & 1;
    }

    int popcount() const {
      int count = 0;
      for (uint64_t w : words) {
#if defined(_MSC_VER) && !defined(__clang__)
        count += (int)__popcnt64(w);
#else
        count += __builtin_popcountll(w);
#endif
      }
      return count;
    }

    bool operator==(const ConfigMask& o) const { return words == o.words; }
  };

  struct StateKey {
    uint64_t revealedMask;
    vector<uint64_t> configWords;

    bool operator==(const StateKey& o) const {
      return revealedMask == o.revealedMask && configWords == o.configWords;
    }
  };

  struct StateKeyHash {
    size_t operator()(const StateKey& k) const {
      size_t h = std::hash<uint64_t>{}(k.revealedMask);
      for (uint64_t w : k.configWords)
        h ^= std::hash<uint64_t>{}(w) + 0x9e3779b9 + (h << 6) + (h >> 2);
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
