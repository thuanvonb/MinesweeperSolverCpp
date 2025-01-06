#pragma once

class Group;

#include <vector>
#include <set>
using std::set;
using std::vector;

#include "Macros.h"

class Cell {
public:
  int r, c;
  int value; // -4: floating, -3: dont care, -2: flag, -1: undiscovered, 0-9: value
  float minePerc;
  float valuePerc[9];

  vector<Group*> groups;
  bool disabled;

  Cell(int = -1, int = -1, int = CELL_FLOATING);
  Cell(const Cell&);

  bool isUnpredicted() const;
  int getState() const;
};

