#include "EndgameSolver.h"
#include <queue>
#include <algorithm>
#include <cstdint>

using std::queue;

#define byte int8_t

void combineAllGroupsConfigs(const vector<Solver::ChainSolution>& chain_sols, vector<vector<byte>>& all_configs,
                             vector<byte>& config, int mines, int id = 0, int arr_idx = 0) {
  if (id == chain_sols.size()) {
    int remaining = config.size() - arr_idx;
    if (mines > remaining)
      return;

    vector<byte> bitmask(remaining, 0);
    for (int i = 0; i < mines; ++i)
      bitmask[i] = -1;

    do {
      for (int i = 0; i < remaining; ++i)
        config[i + arr_idx] = bitmask[i];
      all_configs.push_back(config);
    } while (next_permutation(bitmask.begin(), bitmask.end()));

    return;
  }

  for (const vector<int>& conf : chain_sols[id].all_configs) {
    int nMines = 0;
    for (int i = 0; i < conf.size(); ++i) {
      bool n = conf[i];
      nMines += n;
      config[i + arr_idx] = -n;
    }

    if (nMines > mines)
      continue;
    combineAllGroupsConfigs(chain_sols, all_configs, config, mines - nMines, id + 1, arr_idx + conf.size());
  }
}

EndgameSolver::EndgameSolver(vector<vector<int>> rd) : solver(rd) {
  numCells = 0;
  numConfigs = 0;
}

bool EndgameSolver::buildConfigurations(int mines, int maxConfigs) {
  bool valid = solver.generalSolve(mines);
  if (!valid) return false;

  int remainingMines = mines;
  for (Cell* c : solver.solvedCells)
    remainingMines -= (c->minePerc == 100.f);

  if (remainingMines < 0) return false;

  vector<vector<Group*>> chains = solver.getGroupChains();
  vector<Solver::ChainSolution> chain_sols;
  for (const vector<Group*>& g : chains)
    chain_sols.push_back(solver.solveChain(g));

  vector<Cell*> allCells;
  for (const Solver::ChainSolution& cs : chain_sols)
    allCells.insert(allCells.end(), cs.relatedCells.begin(), cs.relatedCells.end());

  vector<Cell*>& noNeighbors = solver.noNeighbors;
  allCells.insert(allCells.end(), noNeighbors.begin(), noNeighbors.end());

  if ((int)allCells.size() > MAX_ENDGAME_CELLS) return false;

  vector<int8_t> config(allCells.size(), 0);
  vector<vector<int8_t>> all_configs;
  combineAllGroupsConfigs(chain_sols, all_configs, config, remainingMines, 0, 0);

  if (all_configs.empty() || (int)all_configs.size() > maxConfigs) return false;

  numCells = (int)allCells.size();
  numConfigs = (int)all_configs.size();

  cellPos.resize(numCells);
  posToIdx.assign(solver.board.height, vector<int>(solver.board.width, -1));
  for (int i = 0; i < numCells; ++i) {
    cellPos[i] = {allCells[i]->r, allCells[i]->c};
    posToIdx[allCells[i]->r][allCells[i]->c] = i;
  }

  configMine.assign(numConfigs, vector<bool>(numCells, false));
  for (int c = 0; c < numConfigs; ++c) {
    for (int i = 0; i < numCells; ++i) {
      configMine[c][i] = (all_configs[c][i] == -1);
    }
  }

  return true;
}

void EndgameSolver::precomputeRevealValues() {
  configRevealValue.assign(numConfigs, vector<int>(numCells, 0));

  for (int c = 0; c < numConfigs; ++c) {
    for (int i = 0; i < numCells; ++i) {
      if (configMine[c][i]) {
        configRevealValue[c][i] = -1;
        continue;
      }

      int count = 0;
      int r = cellPos[i].first;
      int col = cellPos[i].second;
      for (int nr = r - 1; nr <= r + 1; ++nr) {
        for (int nc = col - 1; nc <= col + 1; ++nc) {
          if (nr == r && nc == col) continue;
          if (!solver.board.isValidCoord(nr, nc)) continue;

          int boardVal = solver.board.getCell(nr, nc)->value;
          if (boardVal == CELL_FLAG) {
            count++;
          } else if (boardVal == CELL_UNDISCOVERED) {
            int idx = posToIdx[nr][nc];
            if (idx >= 0 && configMine[c][idx])
              count++;
          }
        }
      }
      configRevealValue[c][i] = count;
    }
  }
}

void EndgameSolver::buildAdjacency() {
  adjacency.resize(numCells);
  for (int i = 0; i < numCells; ++i) {
    int r = cellPos[i].first;
    int c = cellPos[i].second;
    for (int nr = r - 1; nr <= r + 1; ++nr) {
      for (int nc = c - 1; nc <= c + 1; ++nc) {
        if (nr == r && nc == c) continue;
        if (!solver.board.isValidCoord(nr, nc)) continue;
        int idx = posToIdx[nr][nc];
        if (idx >= 0)
          adjacency[i].push_back(idx);
      }
    }
  }
}

uint64_t EndgameSolver::simulateReveal(int cellIdx, int configIdx, uint64_t currentRevealed) const {
  uint64_t newRevealed = currentRevealed | (1ULL << cellIdx);

  if (configRevealValue[configIdx][cellIdx] == 0) {
    queue<int> q;
    q.push(cellIdx);
    while (!q.empty()) {
      int curr = q.front();
      q.pop();
      for (int neighbor : adjacency[curr]) {
        if ((newRevealed >> neighbor) & 1) continue;
        if (configMine[configIdx][neighbor]) continue;
        newRevealed |= (1ULL << neighbor);
        if (configRevealValue[configIdx][neighbor] == 0)
          q.push(neighbor);
      }
    }
  }

  return newRevealed;
}

double EndgameSolver::solve(uint64_t revealedMask, ConfigMask configMask) {
  int totalAlive = configMask.popcount();
  if (totalAlive == 0) return 0.0;
  if (totalAlive == 1) return 1.0;

  // Check win: all unrevealed cells are mines in every alive config
  bool needToClick = false;
  for (int i = 0; i < numCells && !needToClick; ++i) {
    if ((revealedMask >> i) & 1) continue;
    for (int c = 0; c < numConfigs && !needToClick; ++c) {
      if (!configMask.getBit(c)) continue;
      if (!configMine[c][i]) needToClick = true;
    }
  }
  if (!needToClick) return 1.0;

  StateKey key = {revealedMask, configMask.lo, configMask.hi};
  auto it = memo.find(key);
  if (it != memo.end()) return it->second;

  double bestProb = 0.0;

  for (int i = 0; i < numCells; ++i) {
    if ((revealedMask >> i) & 1) continue;

    // Skip if mine in all alive configs (guaranteed loss)
    bool anySafe = false;
    for (int c = 0; c < numConfigs; ++c) {
      if (!configMask.getBit(c)) continue;
      if (!configMine[c][i]) { anySafe = true; break; }
    }
    if (!anySafe) continue;

    // Group alive safe configs by observation
    map<ObservationKey, ConfigMask> obsGroups;

    for (int c = 0; c < numConfigs; ++c) {
      if (!configMask.getBit(c)) continue;
      if (configMine[c][i]) continue;

      uint64_t newRevealed = simulateReveal(i, c, revealedMask);
      uint64_t newlyRevealed = newRevealed & ~revealedMask;

      ObservationKey obsKey;
      obsKey.newRevealedMask = newRevealed;
      for (int j = 0; j < numCells; ++j) {
        if ((newlyRevealed >> j) & 1)
          obsKey.values.push_back(configRevealValue[c][j]);
      }

      obsGroups[obsKey].setBit(c);
    }

    double prob = 0.0;
    for (auto& [obsKey, groupMask] : obsGroups) {
      int groupSize = groupMask.popcount();
      prob += (double)groupSize / totalAlive * solve(obsKey.newRevealedMask, groupMask);
    }

    bestProb = std::max(bestProb, prob);
  }

  memo[key] = bestProb;
  return bestProb;
}

EndgameResult EndgameSolver::solveEndgame(int mines, int maxConfigs) {
  EndgameResult result = {0.0, -1, -1, false};

  if (!buildConfigurations(mines, maxConfigs))
    return result;

  if (numCells == 0) {
    result.winProbability = 1.0;
    result.valid = true;
    return result;
  }

  precomputeRevealValues();
  buildAdjacency();
  memo.clear();

  uint64_t initialRevealed = 0;
  ConfigMask allConfigs;
  for (int c = 0; c < numConfigs; ++c)
    allConfigs.setBit(c);

  // Populate memo for all reachable states
  double winProb = solve(initialRevealed, allConfigs);

  // Find best first move by replaying the root decision
  int bestRow = -1, bestCol = -1;
  double bestProb = -1.0;

  for (int i = 0; i < numCells; ++i) {
    bool anySafe = false;
    for (int c = 0; c < numConfigs; ++c) {
      if (allConfigs.getBit(c) && !configMine[c][i]) { anySafe = true; break; }
    }
    if (!anySafe) continue;

    map<ObservationKey, ConfigMask> obsGroups;
    for (int c = 0; c < numConfigs; ++c) {
      if (!allConfigs.getBit(c)) continue;
      if (configMine[c][i]) continue;

      uint64_t newRevealed = simulateReveal(i, c, initialRevealed);

      ObservationKey obsKey;
      obsKey.newRevealedMask = newRevealed;
      for (int j = 0; j < numCells; ++j) {
        if ((newRevealed >> j) & 1)
          obsKey.values.push_back(configRevealValue[c][j]);
      }

      obsGroups[obsKey].setBit(c);
    }

    double prob = 0.0;
    for (auto& [obsKey, groupMask] : obsGroups) {
      int groupSize = groupMask.popcount();
      prob += (double)groupSize / numConfigs * solve(obsKey.newRevealedMask, groupMask);
    }

    if (prob > bestProb) {
      bestProb = prob;
      bestRow = cellPos[i].first;
      bestCol = cellPos[i].second;
    }
  }

  result.winProbability = winProb;
  result.bestRow = bestRow;
  result.bestCol = bestCol;
  result.valid = true;
  return result;
}
