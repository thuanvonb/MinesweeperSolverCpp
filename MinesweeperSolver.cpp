#include <iostream>
#include <fstream>
#include <vector>
#include "Solver.h"
#include "EndgameSolver.h"
#include <map>
#include <algorithm>
#include <numeric>
#include <stdlib.h>

// #define BUILD_EMSDK

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;
using std::map;
using std::pair;
using std::sort;
using std::prev_permutation;

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

#ifdef BUILD_EMSDK
extern "C" {
  bool solveBoard(int nrows, int ncols, int* nums, int mines, float* prob);
  bool solveEndgame(int nrows, int ncols, int* nums, int mines, float* winProb, int* bestRow, int* bestCol);
}
#endif

bool solveBoard(int nrows, int ncols, int* nums, int mines, float* prob) {
  vector<vector<int>> rd(nrows, vector<int>(ncols));
  for (int i = 0; i < nrows; ++i) {
    for (int j = 0; j < ncols; ++j)
      rd[i][j] = nums[i * ncols + j];
  }

  Solver solver(rd);
  bool valid = solver.generalSolve(mines);
  
  if (valid) {
    for (int i = 0; i < nrows; ++i) {
      for (int j = 0; j < ncols; ++j) {
        const Cell* cell = solver.board.getCell(i, j);
        prob[i*ncols + j] = cell->minePerc;
      }
    }
  }

  return valid;
}

bool solveEndgame(int nrows, int ncols, int* nums, int mines, float* winProb, int* bestRow, int* bestCol) {
  vector<vector<int>> rd(nrows, vector<int>(ncols));
  for (int i = 0; i < nrows; ++i) {
    for (int j = 0; j < ncols; ++j)
      rd[i][j] = nums[i * ncols + j];
  }

  EndgameSolver endgame(rd);
  EndgameResult result = endgame.solveEndgame(mines);

  if (result.valid) {
    *winProb = (float)result.winProbability;
    *bestRow = result.bestRow;
    *bestCol = result.bestCol;
  }

  return result.valid;
}

int main() {
#ifndef BUILD_EMSDK
  ifstream inp("minesweeper.inp");
  int h, w, mines;
  inp >> h >> w >> mines;

  vector<vector<int>> rd(h, vector<int>(w));
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; ++j)
      inp >> rd[i][j];
  }

  Solver solver(rd);
  cout << "Start solving\n";
  bool valid = solver.generalSolve(mines);
  cout << "Done solving\n";

  if (valid)
    solver.printProb();

  // Try endgame solver
  cout << "\nEndgame solver:\n";
  EndgameSolver endgame(rd);
  EndgameResult egResult = endgame.solveEndgame(mines);
  if (egResult.valid) {
    printf("Win probability: %.4f%%\n", egResult.winProbability * 100.0);
    printf("Best move: (%d, %d)\n", egResult.bestRow, egResult.bestCol);
    printf("Configs: %d, Cells: %d\n", endgame.numConfigs, endgame.numCells);
  } else {
    cout << "Endgame solver not applicable (too many configs or cells)\n";
  }

#endif

  return 0;
}
