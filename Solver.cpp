#include "Solver.h"

vector<vector<int>> Solver::getCombinations(int n, int r) const {
  vector<vector<int>> out;
  for (int i = 0; i < (1 << n); ++i) if (auxiliary[i] == r) {
    vector<int> r;
    int t = i;
    for (int j = 0; j < n; ++j) {
      r.push_back(t & 1);
      t >>= 1;
    }
    out.push_back(r);
  }
  return out;
}

Solver::Solver(vector<vector<int>> rd) : board(rd) {
  for (int i = 0; i < board.height; ++i) {
    for (int j = 0; j < board.width; ++j) {
      Group* group = new Group(i, j, board);
      if (group->minV == 0 && group->maxV == 0 && group->groupcells.size() == 0)
        continue;
      if (board.data.size() != (size_t) 0)
        addGroup(group);
    }
  }
  solved = false;

  for (int i = 0; i < 256; ++i) {
    auxiliary.push_back(0);
    int t = i;
    while (t > 0) {
      auxiliary[i] += t & 1;
      t >>= 1;
    }
  }
}

void Solver::addGroup(Group* g) {
  g->id = (int) groups.size();
  groups.push_back(g);
}

void Solver::crossAllGroups() {
  int maxGroupId = (int) groups.size();

  for (int i = 0; i < board.height; ++i) {
    for (int j = 0; j < board.width; ++j) {
      Cell* c = board.getCellMutable(i, j);
      if (c->groups.size() <= 1)
        continue;
      int n = (int) c->groups.size();
      for (int x = 0; x < n - 1; ++x) {
        if (c->groups[x]->disabled)
          continue;
        if (c->groups[x]->id >= maxGroupId)
          continue;
        for (int y = x+1; y < n; ++y) {
          if (c->groups[y]->disabled)
            continue;
          if (c->groups[y]->id >= maxGroupId)
            continue;

          if (c->groups[x]->id == 10 && c->groups[y]->id == 11)
            i = i;
          if (c->groups[x]->id == 11 && c->groups[y]->id == 10)
            j = j;

          vector<Group*> newGroups = c->groups[x]->cross((*c->groups[y]));
          for (Group* g : newGroups)
            addGroup(g);
        }
      }
    }
  }
}

void Solver::filterTrivial() {
  for (Group* g : groups) {
    if (g->disabled)
      continue;

    if (g->maxV == g->groupcells.size() && g->minV == 0)
      g->disabled = true;
  }
}

void Solver::cleanDisabled() {
  for (int i = 0; i < board.height; ++i) {
    for (int j = 0; j < board.width; ++j) {
      Cell* c = board.getCellMutable(i, j);
      if (c->value != -1) {
        c->groups.clear();
        continue;
      }

      int n = (int) c->groups.size();
      for (int idx = 0; idx < n; ++idx) {
        if (!c->groups[idx]->disabled)
          continue;

        while (c->groups[n-1]->disabled)
          n -= 1;

        if (n <= idx)
          break;
          
        Group* t = c->groups[idx];
        c->groups[idx] = c->groups[n-1];
        c->groups[n-1] = t;
      }

      while (c->groups.size() > 0 && c->groups.back()->disabled)
        c->groups.pop_back();
    }
  }

  int n = (int) groups.size();
  for (int idx = 0; idx < n; ++idx) {
    if (!groups[idx]->disabled)
      continue;
    while (n > 0 && groups[n-1]->disabled)
      n -= 1;

    if (n <= idx)
      break;

    std::swap(groups[idx], groups[n-1]);
    groups[idx]->id = idx;
  }

  while (groups.size() > 0 && groups.back()->disabled) {
    Group* g = groups.back();
    groups.pop_back();
    delete g;
  }
}

bool Solver::apply() {
  bool reduced = false;
  for (Group* g : groups) {
    if (!(g->minV == g->maxV && (g->maxV == g->groupcells.size() || g->maxV == 0)))
      continue;
    if (g->disabled)
      continue;
    reduced = true;
    for (Cell* c : g->groupcells) {
      c->value = g->minV == 0 ? CELL_SAFE : CELL_FLAG;
      c->minePerc = g->minV == 0 ? 0.f : 100.f;
      solvedCells.insert(c);
    }
  }
  return reduced;
}

bool Solver::syncAllGroups() {
  for (int i = 0; i < board.height; ++i) {
    for (int j = 0; j < board.width; ++j) {
      Cell* c = board.getCellMutable(i, j);
      if (c->groups.size() <= 1)
        continue;
      int n = (int) c->groups.size();
      for (int i = 0; i < n-1; ++i) {
        if (c->groups[i]->disabled)
          continue;
        for (int j = i + 1; j < n; ++j) {
          if (c->groups[j]->disabled)
            continue;

          bool valid = c->groups[i]->merge(*(c->groups[j]));
          if (!valid)
            return false;
        }
      }
    }
  }

  return true;
}

bool Solver::isDone() const {
  return board.unsolved == solvedCells.size();
}

bool Solver::iterativeSolve() {
  while (!isDone()) {
    crossAllGroups();
    int iter = 0;
    while (1) {
      bool valid = syncAllGroups();
      if (!valid)
        return false;
      bool thingsHappened = apply();
      if (!thingsHappened)
        break;
      iter += 1;
      valid = filter();
      if (!valid)
        return false;
    }
    if (iter == 0)
      break;
  }
  filterTrivial();
  cleanDisabled();
  return true;
}

void combineChainMineCount(const vector<Solver::ChainSolution>& chain_sols, vector<vector<int>>& mines, 
                           vector<int>& offset, vector<int>& config, int& minMines, int id = 0) {
  if (id == 0) {
    mines.clear();
    offset.clear();
    config.clear();
    int r = 0;
    int c = 0;
    minMines = 0;
    for (const Solver::ChainSolution& cs : chain_sols) {
      r += cs.no_mines.back();
      minMines += cs.no_mines.front();
      offset.push_back(c);
      c += (int) cs.no_mines.size();
    }
    mines = vector<vector<int>>(r + 1 - minMines, vector<int>(c, 0));
    config = vector<int>(c, 0);
  }

  if (id == chain_sols.size()) {
    int sumMines = 0;
    int weight = 1;
    int n = (int) config.size();
    int c = 0;
    int t = 0;
    for (int i = 0; i < n; ++i) {
      int l = (int) chain_sols[c].no_mines.size();
      if (i - t >= l) {
        t += l;
        c += 1;
      }

      if (config[i] == 0)
        continue;

      weight *= config[i];
      sumMines += chain_sols[c].no_mines[i - t];
    }

    for (int i = 0; i < n; ++i) {
      if (config[i] == 0)
        continue;
      mines[sumMines - minMines][i] += weight;
    }
    return;
  }

  const Solver::ChainSolution& cs = chain_sols[id];
  int n = (int) cs.no_mines.size();
  for (int i = 0; i < n; ++i) {
    config[i + offset[id]] = cs.freq_no_mines[i];
    combineChainMineCount(chain_sols, mines, offset, config, minMines, id + 1);
    config[i + offset[id]] = 0;
  }
}

bool Solver::generalSolve(int mines) { // number of unsolved mines (flags in the input do not count)
  bool valid = iterativeSolve();
  if (!valid)
    return false;

  if (mines == -1)
    return true;

  for (Cell* solved_cell : solvedCells)
    mines -= (solved_cell->minePerc == 100.);

  if (mines < 0)
    return false;

  vector<Cell*> noNeighbors = board.noNeighborsCells();

  vector<vector<Group*>> chains = getGroupChains();
  vector<Solver::ChainSolution> chain_sols;
  for (const vector<Group*>& g : chains)
    chain_sols.push_back(solveChain(g));

  vector<vector<int>> cmines;
  vector<int> offset;
  vector<int> config;
  int minMines;
  combineChainMineCount(chain_sols, cmines, offset, config, minMines);
  vector<int> weight(cmines.size(), 0);
  int l = offset.size() == 1 ? (int) config.size() : offset[1];
  for (int i = 0; i < cmines.size(); ++i) {
    for (int j = 0; j < l; ++j)
      weight[i] += cmines[i][j];
    
  }

  int low = 0, high = (int) cmines.size()-1;
  if (high + minMines > mines)
    high = mines - minMines;

  if (mines - (low + minMines) > noNeighbors.size())
    low = mines - noNeighbors.size() - minMines;

  if (low > high || low < 0 || high < 0)
    return false;

  vector<double> noMinesProb(high - low + 1, 0.);
  int sumW = 0;
  for (int i = low; i <= high; ++i)
    sumW += weight[i];
  for (int i = low; i <= high; ++i)
    noMinesProb[i-low] = 1. * weight[i] / sumW;

  int idx = 0;
  for (const Solver::ChainSolution& cs : chain_sols) {
    int nCells = cs.relatedCells.size();
    int nV = cs.freq_mines_pos.size();
    int i = 0;
    for (Cell* c : cs.relatedCells) {
      double prob = 0;
      for (int k = low; k <= high; ++k) {
        double imProb = 0;
        for (int j = 0; j < nV; ++j) {
          double p1 = 1.*cs.freq_mines_pos[j][i] / cs.freq_no_mines[j];
          double p2 = 1.*cmines[k][j+offset[idx]] / weight[k];
          imProb += p1*p2;
        }
        prob += imProb * noMinesProb[k-low];
      }
      c->minePerc = prob*100;
      i += 1;
    }
    idx += 1;
  }

  double expectedExposedMines = 0;
  for (int i = low; i <= high; ++i)
    expectedExposedMines += (i + minMines) * noMinesProb[i-low];
  double expectedRemain = mines - expectedExposedMines;
  for (Cell* c : noNeighbors)
    c->minePerc = expectedRemain / noNeighbors.size() * 100;

  return true;
}

void Solver::printBoard() const {
  for (int i = 0; i < board.height; ++i) {
    for (int j = 0; j < board.width; ++j) {
      int v = board.getCell(i, j)->value;
      if (v >= 0)
        cout << v << " ";
      else if (v == CELL_UNDISCOVERED)
        cout << "- ";
      else if (v == CELL_FLAG)
        cout << "F ";
      else if (v == CELL_SAFE)
        cout << "S ";
    }
    cout << "\n";
  }
}

void Solver::printProb() const {
  for (int i = 0; i < board.height; ++i) {
    for (int j = 0; j < board.width; ++j) {
      const Cell* c = board.getCell(i, j);
      int v = c->value;
      if (v >= 0)
        cout << "    " << v << "    ";
      else if (v == CELL_UNDISCOVERED)
        printf("[%5.1f%%] ", c->minePerc);
      else if (v == CELL_FLAG)
        cout << "[100.0%] ";
      else if (v == CELL_SAFE)
        cout << "[  0.0%] ";
    }
    cout << "\n";
  }
}

vector<vector<Group*>> Solver::getGroupChains() const {
  vector<vector<Group*>> out;
  set<int> added;

  int n = (int) groups.size();
  int idx = 0;
  while (1) {
    while (added.find(idx) != added.end())
      idx += 1;
    if (idx >= n)
      break;

    vector<Group*> chain;
    queue<Group*> process;
    process.push(groups[idx]);
    while (!process.empty()) {
      Group* next = process.front();
      process.pop();
      if (added.find(next->id) != added.end())
        continue;
      chain.push_back(next);
      added.insert(next->id);
      for (Cell* c : next->groupcells) {
        for (Group* g : c->groups)
          process.push(g);
      }
    }
    out.push_back(chain);
  }

  return out;
}

void Solver::solveRec(const vector<Group*>& chain, const vector<int>& order, 
                      const vector<vector<int>>& group_cells_id, vector<int>& freq_no_mines, 
                      vector<vector<int>>& freq_mines_pos, vector<int>& sol, int id) const {
  if (id == order.size()) {
    int sumMines = 0;
    for (int v : sol)
      sumMines += v;
    freq_no_mines[sumMines] += 1;
    int i = 0;
    for (int v : sol)
      freq_mines_pos[sumMines][i++] += v;
    return;
  }

  Group* g = chain[order[id]];
  const vector<int>& cells_id = group_cells_id[order[id]];

  vector<int> to_assign;
  int c = (int) cells_id.size();
  int mx = g->maxV;
  int mn = g->minV;
  for (int idx : cells_id) {
    if (sol[idx] == -1) {
      to_assign.push_back(idx);
      continue;
    }
    c -= 1;
    mx -= sol[idx];
    mn -= sol[idx];
  }

  if (mx < 0)
    return;
  if (mn > c)
    return;
  for (int v = mn; v <= mx; ++v) {
    const vector<vector<int>>& tries = getCombinations(c, v);
    for (vector<int> a_try : tries) {
      int i = 0;
      for (int j : to_assign)
        sol[j] = a_try[i++];
      solveRec(chain, order, group_cells_id, freq_no_mines, freq_mines_pos, sol, id+1);
    }
  }

  for (int idx : to_assign)
    sol[idx] = -1;
}

Solver::ChainSolution Solver::solveChain(const vector<Group*>& chain) const {
  vector<vector<Group*>> overlaps;
  int n = (int) chain.size();
  int mx = -1;
  int mv = -1;
  int mId = -1;
  for (int i = 0; i < n; ++i) {
    overlaps.push_back(chain[i]->getOverlaps());
    int l = (int)overlaps.back().size();
    if (mv < l) {
      mv = l;
      mx = i;
    }
    mId = max(mId, chain[i]->id);
  }

  vector<int> gid2i(mId + 1);
  vector<int> id(n);
  for (int i = 0; i < n; ++i) {
    gid2i[chain[i]->id] = i;
    id[i] = i;
  }

  for (int i = 0; i < n; ++i) {
    sort(overlaps[i].begin(), overlaps[i].end(), [overlaps, gid2i](Group* g1, Group* g2) {
      return overlaps[gid2i[g1->id]].size() > overlaps[gid2i[g2->id]].size();
    });
  }
  sort(id.begin(), id.end(), [overlaps](int a, int b) {
    return overlaps[a].size() > overlaps[b].size();
  });

  vector<bool> existed(mId+1, false);
  vector<int> processQ;
  existed[mx] = true;
  processQ.push_back(mx);
  for (int i = 0; i < n; ++i) {
    for (Group* g : overlaps[id[i]]) {
      int idx = gid2i[g->id];
      if (existed[idx])
        continue;
      existed[idx] = true;
      processQ.push_back(idx);
    }
  }

  set<Cell*> relatedCells;
  for (Group* g : chain) {
    for (Cell* c: g->groupcells)
      relatedCells.insert(c);
  }

  int nCells = (int) relatedCells.size();
  map<Cell*, int> c2i;
  int idx = 0;
  for (Cell* c : relatedCells)
    c2i[c] = idx++;

  vector<int> freq_no_mines(nCells, 0);
  vector<vector<int>> freq_mines_pos(nCells, vector<int>(nCells, 0));
  vector<vector<int>> groups_cell_id(chain.size());
  for (int i = 0; i < n; ++i) {
    for (Cell* c : chain[i]->groupcells)
      groups_cell_id[i].push_back(c2i.find(c)->second);
  }

  vector<int> sol(nCells, -1);
  solveRec(chain, processQ, groups_cell_id, freq_no_mines, freq_mines_pos, sol);

  vector<int> no_mines;
  vector<int> freq_no_mines_out;
  vector<vector<int>> freq_mines_pos_out;
  for (int i = 0; i < nCells; ++i) {
    if (freq_no_mines[i] == 0)
      continue;
    no_mines.push_back(i);
    freq_no_mines_out.push_back(freq_no_mines[i]);
    freq_mines_pos_out.push_back(freq_mines_pos[i]);
  }
  
  return {
    relatedCells, 
    no_mines, 
    freq_no_mines_out,
    freq_mines_pos_out
  };
}

bool Solver::filter() {
  int maxId = (int) groups.size();
  for (int i = 0; i < maxId; ++i) {
    Group* g = groups[i];
    if (i == 9)
      i = i;
    //cout << i << "\n";
    if (g->disabled)
      continue;
    set<Cell*> intersect = g->intersect(solvedCells);
    if (intersect.size() == 0)
      continue;

    g->disabled = true;
    if (intersect.size() == g->groupcells.size())
      continue;

    Group* newG = new Group(g->subtract(solvedCells));
    newG->minV = g->minV;
    newG->maxV = g->maxV;
    for (Cell* c : intersect) {
      if (c->minePerc == 100.f) {
        newG->minV -= 1;
        newG->maxV -= 1;
        
        if (newG->maxV < 0)
          return false;

        if (newG->minV < 0)
          newG->minV = 0;
      }
    }

    addGroup(newG);
  }

  filterTrivial();
  return true;
}
