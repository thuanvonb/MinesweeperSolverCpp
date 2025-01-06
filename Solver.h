#pragma once

#include "Board.h"
#include "Group.h"
#include <iostream>
#include <queue>
#include <stack>
#include <map>
#include <cassert>
#include <cstdio>
using std::cout;
using std::queue;
using std::stack;
using std::map;

class Solver {
private:
  vector<int> auxiliary;
  vector<vector<int>> getCombinations(int n, int r) const;
  void solveRec(const vector<Group*>& chain, const vector<int>& order, const vector<vector<int>>& group_cells_id,
                vector<int>& freq_no_mines, vector<vector<int>>& freq_mines_pos, vector<int>& sol, int id = 0) const;

public:
  struct ChainSolution {
    set<Cell*> relatedCells;
    vector<int> no_mines;
    vector<int> freq_no_mines;
    vector<vector<int>> freq_mines_pos;
  };

  Board board;
  vector<Group*> groups;
  set<Cell*> solvedCells;
  bool solved;
  
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
};

