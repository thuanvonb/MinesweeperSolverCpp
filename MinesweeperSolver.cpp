#include <iostream>
#include <fstream>
#include <vector>
#include "Solver.h"
#include "EndgameSolver.h"

#define BUILD_EMSDK

using std::ifstream;
using std::cout;
using std::vector;

#ifdef BUILD_EMSDK
extern "C" {
  bool solveBoard(int nrows, int ncols, int* nums, int mines, float* prob, bool* canEndgame);
  bool solveEndgame(int nrows, int ncols, int* nums, int mines, float* winProb, int* bestRow, int* bestCol);
}
#endif

bool solveBoard(int nrows, int ncols, int* nums, int mines, float* prob, bool* canEndgame) {
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

  *canEndgame = solver.canEndgame;
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
