#include <iostream>
#include <fstream>
#include <vector>
#include "Solver.h"
#include <map>
#include <algorithm>

using std::ifstream;
using std::cout;
using std::endl;
using std::vector;
using std::map;
using std::pair;
using std::sort;

void combineChainMineCount(const vector<Solver::ChainSolution>& chain_sols, vector<vector<int>>& mines, vector<int>& offset,
                           vector<int>& config, int id = 0) {
  if (id == 0) {
    mines.clear();
    offset.clear();
    config.clear();
    int r = 0;
    int c = 0;
    for (const Solver::ChainSolution& cs : chain_sols) {
      r += cs.no_mines.back();
      offset.push_back(c);
      c += cs.no_mines.size();
    }
    mines = vector<vector<int>>(r+1, vector<int>(c, 0));
    config = vector<int>(c, 0);
  }

  if (id == chain_sols.size()) {
    int sumMines = 0;
    int weight = 1;
    int n = config.size();
    int c = 0;
    int t = 0;
    for (int i = 0; i < n; ++i) {
      int l = chain_sols[c].no_mines.size();
      if (i-t >= l) {
        t += l;
        c += 1;
      }

      if (config[i] == 0)
        continue;

      weight *= config[i];
      sumMines += chain_sols[c].no_mines[i-t];
    }
    
    for (int i = 0; i < n; ++i) {
      if (config[i] == 0)
        continue;
      mines[sumMines][i] += weight;
    }
    return;
  }

  const Solver::ChainSolution& cs = chain_sols[id];
  int n = cs.no_mines.size();
  for (int i = 0; i < n; ++i) {
    config[i + offset[id]] = cs.freq_no_mines[i];
    combineChainMineCount(chain_sols, mines, offset, config, id+1);
    config[i + offset[id]] = 0;
  }
}

int main() {
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
  
  solver.printProb();

  return 0;
}
