#pragma once

#include "Board.h"
#include "Group.h"
#include "Utils.h"
#include <iostream>
#include <queue>
#include <stack>
#include <map>
#include <unordered_map>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <random>
using std::cout;
using std::queue;
using std::stack;
using std::map;
using std::unordered_map;

class Solver {
private:
  mutable unordered_map<uint32_t, vector<vector<int>>> combinationCache;
  vector<vector<int>> getCombinations(int n, int r) const;
  void solveRec(const vector<Group*>& chain, const vector<int>& order, const vector<vector<int>>& group_cells_id,
                vector<int>& freq_no_mines, vector<vector<int>>& freq_mines_pos,
                vector<int>& sol, vector<vector<int>>& all_configs, int id = 0) const;
  static void sampleConfiguration(const Solver& solver, int mines,
                                  vector<vector<int>>& mineConf, std::mt19937& rng);

public:
  struct ChainSolution {
    set<Cell*> relatedCells;
    vector<int> no_mines;
    vector<int> freq_no_mines;
    vector<vector<int>> freq_mines_pos;
    vector<vector<int>> all_configs;
  };

  Board board;
  vector<Group*> groups;
  set<Cell*> solvedCells;
  bool solved;
  bool valid_input;
  bool canEndgame;
  vector<Cell*> noNeighbors;
  vector<Cell*> groupedCells;
  
  Solver(vector<vector<int>> rd);
  void addGroup(Group* g);
  void crossAllGroups();
  void filterTrivial();
  void cleanDisabled();
  bool filter();

  bool apply();
  bool syncAllGroups();
  bool isDone() const;
  bool iterativeSolve();
  bool generalSolve(int = -1);

  void printBoard() const;
  void printProb() const;
  vector<vector<Group*>> getGroupChains() const;
  ChainSolution solveChain(const vector<Group*>&) const;
  float tryWarp(int mines, int row, int col, bool isMine, vector<vector<int>>& mineConf);
};

