#pragma once

#include "Cell.h"
#include <vector>
using std::vector;

class Board {
public:
  vector<vector<Cell>> data;
  int width;
  int height;
  int unsolved = 0;

  Board(vector<vector<int>> raw_data);
  
  const Cell* getCell(int r, int c) const;
  Cell* getCellMutable(int r, int c);
  bool isValidCoord(int r, int c) const;
  vector<Cell*> noNeighborsCells();
};

