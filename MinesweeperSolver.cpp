#include <iostream>
#include <fstream>
#include <vector>
#include "Solver.h"
#include <map>
#include <algorithm>
#include <numeric>

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
  /*cout << "Start solving\n";
  bool valid = solver.generalSolve(mines);
  cout << "Done solving\n";
  
  solver.printProb();*/

  vector<vector<Group*>> chains = solver.getGroupChains();
  vector<Solver::ChainSolution> chain_sols;
  for (const vector<Group*>& g : chains)
    chain_sols.push_back(solver.solveChain(g));

  vector<Cell*> allCells;
  for (const Solver::ChainSolution& cs : chain_sols) {
    allCells.insert(allCells.end(), cs.relatedCells.begin(), cs.relatedCells.end());
  }

  vector<Cell*> noNeighbors = solver.board.noNeighborsCells();
  allCells.insert(allCells.end(), noNeighbors.begin(), noNeighbors.end());

  vector<byte> config(allCells.size(), false);
  vector<vector<byte>> all_configs;
  combineAllGroupsConfigs(chain_sols, all_configs, config, mines);



  return 0;
}
