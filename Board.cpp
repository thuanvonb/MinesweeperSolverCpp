#include "Board.h"

Board::Board(vector<vector<int>> raw_data) {
  height = (int) raw_data.size();
  width = (int) raw_data[0].size();

  data = vector<vector<Cell>>(height);

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      data[i].push_back(Cell(i, j, raw_data[i][j]));
      unsolved += (int) (raw_data[i][j] == -1);
    }
  }
}

const Cell* Board::getCell(int r, int c) const {
  return &data[r][c];
}

Cell* Board::getCellMutable(int r, int c) {
  return &data[r][c];
}

bool Board::isValidCoord(int r, int c) const {
  return 0 <= r && r < height && 0 <= c && c < width;
}

vector<Cell*> Board::noNeighborsCells() {
  vector<Cell*> out;

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      if (data[i][j].value != CELL_UNDISCOVERED)
        continue;

      bool hasNeighbor = false;
      for (int ni = i-1; ni < i+2 && !hasNeighbor; ++ni) if (isValidCoord(ni, 0)) {
        for (int nj = j - 1; nj < j + 2 && !hasNeighbor; ++nj) if (isValidCoord(ni, nj)) {
          if (data[ni][nj].value >= 0)
            hasNeighbor = true;
        }
      }

      if (!hasNeighbor)
        out.push_back(getCellMutable(i, j));
    }
  }

  return out;
}

