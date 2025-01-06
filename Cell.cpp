#include "Cell.h"

Cell::Cell(int rv, int cv, int val) {
  r = rv;
  c = cv;
  value = val;
  minePerc = (val == CELL_FLAG) ? 100.f : (val >= CELL_NUMBER(0) ? 0.f : -1.f);
  for (int i = 0; i < 9; ++i)
    valuePerc[i] = minePerc >= 0 ? 0.f : -1.f;

  if (value >= CELL_NUMBER(0))
    valuePerc[value] = 100.f;

  disabled = false;
}

Cell::Cell(const Cell& origin) : Cell(origin.r, origin.c) {
  this->value = origin.value;
  this->minePerc = origin.minePerc;
  for (int i = 0; i < 9; ++i)
    this->valuePerc[i] = origin.valuePerc[i];
  
  this->disabled = origin.isUnpredicted();
}

bool Cell::isUnpredicted() const {
  return value == CELL_UNDISCOVERED && minePerc < 0;
}

int Cell::getState() const {
  if (this->disabled)
    return CELLSTATE_DISABLED;
  if (this->isUnpredicted())
    return CELLSTATE_UNPREDICTED;
  if (this->value >= CELL_NUMBER(0))
    return CELLSTATE_DETERMINED;

  return CELLSTATE_UNDETERMINED;
}
