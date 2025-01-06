#pragma once

#include "Cell.h"
#include "Board.h"
#include "Macros.h"
#include <vector>
#include <set>
#include <algorithm>
using std::vector;
using std::set;
using std::set_intersection;
using std::set_difference;

#include <cmath>
using std::min;
using std::max;

class Group {
public:
  set<Cell*> groupcells;
  int minV;
  int maxV;
  int id;
  bool disabled;

  Group();
  Group(int, int, Board&);
  Group(const Group*, Board*);
  Group(const set<Cell*>&, int=-1);

  set<Cell*> intersect(const set<Cell*>&) const;
  set<Cell*> subtract(const set<Cell*>&) const;

  void cellSync();
  bool isDisjoint(const Group&) const;
  int groupRelation(const Group&) const;
  int sync(Group&, int = -1, int = -1);
  bool merge(Group&);
  vector<Group*> subcross(const Group&);
  vector<Group*> cross(Group&);
  vector<Group*> getOverlaps() const;
};

