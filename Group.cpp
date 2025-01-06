#include "Group.h"

Group::Group() {
  minV = maxV = 0;
  id = -1;
  disabled = false;
}

Group::Group(int row, int col, Board& board) : Group() {
  int mines = board.getCell(row, col)->value;
  if (mines < 0)
    return;

  for (int nr = row - 1; nr < row + 2; ++nr) {
    for (int nc = col - 1; nc < col + 2; ++nc) {
      if (!board.isValidCoord(nr, nc))
        continue;
      
      int v = board.getCell(nr, nc)->value;
      if (v >= 0)
        continue;
      if (v == CELL_UNDISCOVERED)
        groupcells.insert(board.getCellMutable(nr, nc));
      else
        mines -= 1;
    }
  }

  minV = maxV = mines;
  cellSync();
}

Group::Group(const Group* group, Board* board) : Group() {
  minV = group->minV;
  maxV = group->maxV;
  id = group->id;

  for (const Cell* cell : group->groupcells) {
    groupcells.insert(board->getCellMutable(cell->r, cell->c));
    board->getCellMutable(cell->r, cell->c)->disabled = false;
  }

  cellSync();
}

Group::Group(const set<Cell*>& groupcells, int maxMines) : Group() {
  this->groupcells = groupcells;
  minV = 0;
  maxV = (int) groupcells.size();
  if (maxMines != -1)
    maxV = min(maxV, maxMines);

  cellSync();
}

set<Cell*> Group::intersect(const set<Cell*>& other) const {
  vector<Cell*> out(min(groupcells.size(), other.size()));
  vector<Cell*>::iterator it = set_intersection(groupcells.begin(), groupcells.end(), 
                                                other.begin(), other.end(), out.begin());
  return set<Cell*>(out.begin(), it);
}

set<Cell*> Group::subtract(const set<Cell*>& other) const {
  vector<Cell*> out(groupcells.size());
  vector<Cell*>::iterator it = set_difference(groupcells.begin(), groupcells.end(),
                                              other.begin(), other.end(), out.begin());
  return set<Cell*>(out.begin(), it);
}

void Group::cellSync() {
  for (Cell* cell : groupcells)
    cell->groups.push_back(this);
}

bool Group::isDisjoint(const Group& other) const {
  return intersect(other.groupcells).size() == 0;
}

int Group::groupRelation(const Group& other) const {
  int ilen = (int) intersect(other.groupcells).size();
  int len1 = (int) this->groupcells.size();
  int len2 = (int) other.groupcells.size();
  
  if (ilen == len1 && len1 == len2)
    return RELATION_EQUAL;
  if (ilen == len1)
    return RELATION_SUBSET;
  if (ilen == len2)
    return RELATION_SUPERSET;
  if (ilen != 0)
    return RELATION_JOINT;

  return RELATION_DISJOINT;
}

int Group::sync(Group& other, int minV, int maxV) {
  if (!isDisjoint(other))
    return 0;

  int t1 = this->minV + other.maxV;
  int t2 = this->maxV + other.minV;
  int out = 0;

  if (minV != -1) {
    if (t1 < minV) {
      this->minV += minV - t1;
      out |= 1;
    }
    if (t2 < minV) {
      other.minV += minV - t2;
      out |= 2;
    }
  }

  if (maxV != -1) {
    if (t1 > maxV) {
      other.maxV -= t1 - maxV;
      out |= 2;
    }
    if (t2 > maxV) {
      this->maxV -= t2 - maxV;
      out |= 1;
    }
  }

  return out;
}

bool Group::merge(Group& other) {
  if (groupRelation(other) != RELATION_EQUAL)
    return true;

  minV = max(minV, other.minV);
  maxV = min(maxV, other.maxV);

  if (minV > maxV)
    return false;

  other.disabled = true;
  return true;
}

vector<Group*> Group::subcross(const Group& other) {
  set<Cell*> diff = other.subtract(groupcells);
  Group* newGroup = new Group(diff);
  sync(*newGroup, other.minV, other.maxV);
  vector<Group*> out;
  out.push_back(newGroup);
  return out;
}

vector<Group*> Group::cross(Group& other) {
  int rel = groupRelation(other);
  if (rel == RELATION_DISJOINT)
    return vector<Group*>();
  if (rel == RELATION_SUBSET)
    return this->subcross(other);
  if (rel == RELATION_SUPERSET)
    return other.subcross(*this);

  set<Cell*> intercells = intersect(other.groupcells);
  set<Cell*> left = subtract(intercells);
  set<Cell*> right = other.subtract(intercells);

  Group* g1 = new Group(left);
  Group* g2 = new Group(intercells);
  Group* g3 = new Group(right);

  g1->sync(*g2, minV, maxV);
  g3->sync(*g2, other.minV, other.maxV);
  g1->sync(*g2, minV, maxV);

  vector<Group*> out;
  out.push_back(g1);
  out.push_back(g2);
  out.push_back(g3);
  return out;
}

vector<Group*> Group::getOverlaps() const {
  vector<Group*> out;
  set<int> ids;
  ids.insert(this->id);
  for (Cell* c : groupcells) {
    for (Group* g : c->groups) {
      if (ids.count(g->id) == 1)
        continue;
      out.push_back(g);
      ids.insert(g->id);
    }
  }
  return out;
}



