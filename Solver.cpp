#include "Solver.h"
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

// Returns all binary vectors of length n with exactly r ones,
// using popcount for fast bitmask enumeration. Results are cached per (n, r).
vector<vector<int>> Solver::getCombinations(int n, int r) const {
  uint32_t key = ((uint32_t)n << 16) | (uint32_t)r;
  auto it = combinationCache.find(key);
  if (it != combinationCache.end())
    return it->second;

  vector<vector<int>> out;
  for (int i = 0; i < (1 << n); ++i) {
#if defined(_MSC_VER) && !defined(__clang__)
    int pc = __popcnt(i);
#else
    int pc = __builtin_popcount(i);
#endif
    if (pc == r) {
      vector<int> row;
      int t = i;
      for (int j = 0; j < n; ++j) {
        row.push_back(t & 1);
        t >>= 1;
      }
      out.push_back(row);
    }
  }
  combinationCache[key] = out;
  return out;
}

// Computes n-choose-r (binomial coefficient), clamped to an upper bound to prevent overflow.
static uint64_t bounded_nCr(uint16_t n, uint16_t r, uint64_t bound = -1) {
  uint64_t out = 1;
  for (int i = 1; i <= r; ++i) {
    out = out * (n - i + 1) / i;
    if (out >= bound)
      return bound;
  }
  return out;
}

// Constructs the solver from a 2D board representation. Initializes groups from
// numbered cells, builds the popcount lookup table, and identifies cells with no
// numbered neighbors (used for remaining-mine probability calculations).
Solver::Solver(vector<vector<int>> rd) : board(rd) {
  valid_input = true;
  for (int i = 0; i < board.height; ++i) {
    for (int j = 0; j < board.width; ++j) {
      Cell* c = board.getCellMutable(i, j);
      if (c->value == CELL_SAFE) {
        c->minePerc = 0;
        this->solvedCells.insert(c);
        continue;
      }
      Group* group = new Group(i, j, board);
      if (group->minV == 0 && group->maxV == 0 && group->groupcells.size() == 0)
        continue;
      if (group->maxV < 0 || group->minV > group->groupcells.size())
        valid_input = false;
      if (board.data.size() != (size_t) 0)
        addGroup(group);
    }
  }
  solved = false;
  canEndgame = false;

  noNeighbors = board.noNeighborsCells();

  for (int i = 0; i < board.height; ++i) {
    for (int j = 0; j < board.width; ++j) {
      Cell* c = board.getCellMutable(i, j);
      if (!c->groups.empty())
        groupedCells.push_back(c);
    }
  }
}

// Assigns an ID to the group and appends it to the solver's group list.
void Solver::addGroup(Group* g) {
  g->id = (int) groups.size();
  groups.push_back(g);
}

// For every cell belonging to multiple groups, crosses (splits) overlapping group
// pairs to produce new refined sub-groups with tighter mine-count constraints.
void Solver::crossAllGroups() {
  int maxGroupId = (int) groups.size();

  for (Cell* c : groupedCells) {
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

        vector<Group*> newGroups = c->groups[x]->cross((*c->groups[y]));
        for (Group* g : newGroups)
          addGroup(g);
      }
    }
  }
}

// Disables groups that carry no useful constraint information, i.e. groups
// whose mine range [minV, maxV] spans from 0 to the group size (all assignments valid).
void Solver::filterTrivial() {
  for (Group* g : groups) {
    if (g->disabled)
      continue;

    if (g->maxV == g->groupcells.size() && g->minV == 0)
      g->disabled = true;
  }
}

// Removes disabled groups from both individual cell group-lists and the solver's
// master group list. Disabled groups are swapped to the back and popped off, and
// remaining group IDs are reassigned to stay contiguous.
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

// Marks cells as definitively safe or mined when a group's constraint is fully
// determined (minV == maxV == 0 means all safe, minV == maxV == size means all mines).
// Returns true if any cells were resolved.
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

// Propagates constraints between overlapping groups sharing cells by merging
// their min/max bounds. Returns false if a contradiction is detected.
bool Solver::syncAllGroups() {
  for (Cell* c : groupedCells) {
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

  return true;
}

// Returns true if all unsolved cells have been resolved.
bool Solver::isDone() const {
  return board.unsolved == solvedCells.size();
}

// Repeatedly crosses groups, syncs constraints, and applies deterministic deductions
// until no more progress can be made. Returns false if a contradiction is found.
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

// Recursively combines mine-count distributions from independent chains via a
// Cartesian product. Builds a 2D table (mines[totalMines][chainSlot]) tracking
// the weighted frequency of each total mine count across all chain combinations.
static vector<int> s_chain_index;
static vector<int> s_local_index;

void combineChainMineCount(const vector<Solver::ChainSolution>& chain_sols, vector<vector<int>>& mines,
                           vector<int>& offset, vector<int>& config, int& minMines, int id = 0) {
  if (id == 0) {
    mines.clear();
    offset.clear();
    config.clear();
    int maxMines = 0;
    int c = 0;
    minMines = 0;
    for (const Solver::ChainSolution& cs : chain_sols) {
      maxMines += cs.no_mines.back();
      minMines += cs.no_mines.front();
      offset.push_back(c);
      c += (int) cs.no_mines.size();
    }
    mines = vector<vector<int>>(maxMines + 1 - minMines, vector<int>(c, 0));
    config = vector<int>(c, 0);

    s_chain_index.resize(c);
    s_local_index.resize(c);
    for (int ci = 0; ci < (int) chain_sols.size(); ++ci) {
      int len = (int) chain_sols[ci].no_mines.size();
      for (int li = 0; li < len; ++li) {
        s_chain_index[offset[ci] + li] = ci;
        s_local_index[offset[ci] + li] = li;
      }
    }
  }

  if (id == (int) chain_sols.size()) {
    int sumMines = 0;
    int weight = 1;
    int n = (int) config.size();
    for (int i = 0; i < n; ++i) {
      if (config[i] == 0)
        continue;
      weight *= config[i];
      sumMines += chain_sols[s_chain_index[i]].no_mines[s_local_index[i]];
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

// Main entry point: runs deterministic deduction first, then if the total mine count
// is known, enumerates valid configurations per chain and computes per-cell mine
// probabilities using Bayesian weighting over remaining unassigned mines.
bool Solver::generalSolve(int mines) { // number of unsolved mines (flags in the input do not count)
  if (!valid_input)
    return false;

  bool valid = iterativeSolve();
  if (!valid)
    return false;

  if (mines == -1)
    return true;

  for (Cell* solved_cell : solvedCells)
    mines -= (solved_cell->minePerc == 100.);

  if (mines < 0)
    return false;

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
  int l = offset.size() <= 1 ? (int) config.size() : offset[1];
  for (int i = 0; i < cmines.size(); ++i) {
    for (int j = 0; j < l; ++j)
      weight[i] += cmines[i][j];
    //
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

  if (sumW == 0)
    weight[0] = 1;
  else {
    vector<int> remaining_mines(weight.size(), 0);
    for (int i = low; i <= high; ++i) {
      int no_mines = i + minMines;
      remaining_mines[i] = mines - no_mines;
    }

    vector<double> p = computeNormalizedBinomials(noNeighbors.size(), remaining_mines, weight);
    for (int i = low; i <= high; ++i)
      noMinesProb[i-low] = p[i-low];

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
  }

  double expectedExposedMines = 0;
  for (int i = low; i <= high; ++i)
    expectedExposedMines += (i + minMines) * noMinesProb[i-low];
  double expectedRemain = mines - expectedExposedMines;
  for (Cell* c : noNeighbors)
    c->minePerc = expectedRemain / noNeighbors.size() * 100;

  // Check endgame eligibility
  int totalUnrevealedCells = 0;
  for (const Solver::ChainSolution& cs : chain_sols)
    totalUnrevealedCells += (int)cs.relatedCells.size();
  totalUnrevealedCells += (int)noNeighbors.size();

  canEndgame = false;
  if (totalUnrevealedCells <= MAX_ENDGAME_CELLS) {
    const uint64_t bound = MAX_ENDGAME_CONFIGS + 1;
    uint64_t numberOfConfiguration = 0;
    for (int numMines = low; numMines <= high; ++numMines) {
      uint64_t nConfig = weight[numMines] * bounded_nCr(noNeighbors.size(), mines - (numMines + minMines), bound);
      if (nConfig >= bound) {
        numberOfConfiguration = bound;
        break;
      }
      numberOfConfiguration += nConfig;
      if (numberOfConfiguration >= bound) {
        numberOfConfiguration = bound;
        break;
      }
    }
    canEndgame = (numberOfConfiguration > 0 && numberOfConfiguration <= MAX_ENDGAME_CONFIGS);
  }

  return true;
}

// Prints the board state to stdout: numbers for revealed cells, '-' for undiscovered,
// 'F' for flagged, and 'S' for safe cells.
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

// Prints the board with mine probabilities: numbers for revealed cells, percentage
// for undiscovered cells, 100% for flags, and 0% for safe cells.
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

// Partitions groups into independent chains (connected components) using BFS.
// Two groups are connected if they share at least one cell. Each chain can be
// solved independently, reducing the combinatorial search space.
vector<vector<Group*>> Solver::getGroupChains() const {
  vector<vector<Group*>> out;
  int n = (int) groups.size();
  vector<bool> added(n, false);

  int idx = 0;
  while (1) {
    while (idx < n && added[idx])
      idx += 1;
    if (idx >= n)
      break;

    vector<Group*> chain;
    queue<Group*> process;
    process.push(groups[idx]);
    while (!process.empty()) {
      Group* next = process.front();
      process.pop();
      if (added[next->id])
        continue;
      added[next->id] = true;
      if (next->groupcells.size() == 0)
        continue;
      chain.push_back(next);
      for (Cell* c : next->groupcells) {
        for (Group* g : c->groups)
          process.push(g);
      }
    }

    if (chain.size() > 0)
      out.push_back(chain);
  }

  return out;
}

// Recursively enumerates all valid mine assignments for a chain of groups. Processes
// groups in the given order, trying each valid combination for unassigned cells and
// pruning branches that violate group constraints. Accumulates configuration counts
// and per-cell mine frequencies indexed by total mine count.
void Solver::solveRec(const vector<Group*>& chain, const vector<int>& order,
                      const vector<vector<int>>& group_cells_id, vector<int>& freq_no_mines,
                      vector<vector<int>>& freq_mines_pos, vector<int>& sol, vector<vector<int>>& all_configs, int id) const {
  if (id == order.size()) {
    int sumMines = 0;
    for (int v : sol)
      sumMines += v;
    all_configs.push_back(sol);
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
  mn = max(mn, 0);
  for (int v = mn; v <= mx; ++v) {
    const vector<vector<int>>& tries = getCombinations(c, v);
    for (const vector<int>& a_try : tries) {
      int i = 0;
      for (int j : to_assign)
        sol[j] = a_try[i++];
      solveRec(chain, order, group_cells_id, freq_no_mines, freq_mines_pos, sol, all_configs, id+1);
    }
  }

  for (int idx : to_assign)
    sol[idx] = -1;
}

// Solves a single chain by determining an optimal group processing order (most
// overlapping groups first for better pruning), mapping cells to indices, and
// running the recursive enumerator. Returns per-cell mine frequencies grouped
// by total mine count, along with all valid configurations.
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
    sort(overlaps[i].begin(), overlaps[i].end(), [&overlaps, &gid2i](Group* g1, Group* g2) {
      return overlaps[gid2i[g1->id]].size() > overlaps[gid2i[g2->id]].size();
    });
  }
  sort(id.begin(), id.end(), [&overlaps](int a, int b) {
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
  unordered_map<Cell*, int> c2i;
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
  vector<vector<int>> all_configs;
  solveRec(chain, processQ, groups_cell_id, freq_no_mines, freq_mines_pos, sol, all_configs);

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
    freq_mines_pos_out,
    all_configs
  };
}

// Updates groups to account for newly solved cells: disables groups fully covered
// by solved cells and creates reduced sub-groups for partially covered ones, adjusting
// mine counts based on whether solved cells were mines or safe. Returns false on contradiction.
bool Solver::filter() {
  int maxId = (int) groups.size();
  for (int i = 0; i < maxId; ++i) {
    Group* g = groups[i];

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

void Solver::sampleConfiguration(const Solver& solver, int mines,
                                 vector<vector<int>>& mineConf, std::mt19937& rng) {
  int h = solver.board.height;
  int w = solver.board.width;
  mineConf.assign(h, vector<int>(w, -1));

  // Fill deterministic cells
  int adjustedMines = mines;
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      const Cell* cell = solver.board.getCell(r, c);
      if (cell->value >= 0) {
        mineConf[r][c] = 0;
      } else if (cell->minePerc == 0.f) {
        mineConf[r][c] = 0;
      } else if (cell->minePerc == 100.f) {
        mineConf[r][c] = 1;
        adjustedMines -= 1;
      }
    }
  }

  // Recompute chain data
  vector<vector<Group*>> chains = solver.getGroupChains();
  vector<Solver::ChainSolution> chain_sols;
  for (const vector<Group*>& g : chains)
    chain_sols.push_back(solver.solveChain(g));

  int C = (int) chain_sols.size();

  vector<vector<int>> cmines;
  vector<int> offset;
  vector<int> config;
  int minMines;
  combineChainMineCount(chain_sols, cmines, offset, config, minMines);

  vector<int> weight(cmines.size(), 0);
  int l = offset.size() <= 1 ? (int) config.size() : offset[1];
  for (int i = 0; i < (int) cmines.size(); ++i) {
    for (int j = 0; j < l; ++j)
      weight[i] += cmines[i][j];
  }

  int low = 0, high = (int) cmines.size() - 1;
  if (high + minMines > adjustedMines)
    high = adjustedMines - minMines;
  if (adjustedMines - (low + minMines) > (int) solver.noNeighbors.size())
    low = adjustedMines - (int) solver.noNeighbors.size() - minMines;

  if (low > high || low < 0 || high < 0) {
    mineConf.assign(h, vector<int>(w, -1));
    return;
  }

  // Compute noMinesProb (same as generalSolve)
  vector<double> noMinesProb(high - low + 1, 0.);
  int sumW = 0;
  for (int i = low; i <= high; ++i)
    sumW += weight[i];

  if (sumW == 0) {
    noMinesProb[0] = 1.0;
  } else {
    vector<int> remaining_mines(weight.size(), 0);
    for (int i = low; i <= high; ++i) {
      int no_mines = i + minMines;
      remaining_mines[i] = adjustedMines - no_mines;
    }
    vector<double> p = computeNormalizedBinomials(solver.noNeighbors.size(), remaining_mines, weight);
    for (int i = low; i <= high; ++i)
      noMinesProb[i - low] = p[i - low];
  }

  // Step 5b: Sample total chain mine count from noMinesProb
  std::discrete_distribution<int> totalDist(noMinesProb.begin(), noMinesProb.end());
  int kIdx = totalDist(rng);
  int totalChainMines = (kIdx + low) + minMines;
  int remainingForNoNeighbors = adjustedMines - totalChainMines;

  // Step 5c: Sample per-chain mine counts via backward DP
  // dp[i][s] = weighted number of ways chains i..C-1 can sum to s mines
  int maxBudget = totalChainMines + 1;
  vector<vector<double>> dp(C + 1, vector<double>(maxBudget, 0.0));
  dp[C][0] = 1.0;

  for (int i = C - 1; i >= 0; --i) {
    const ChainSolution& cs = chain_sols[i];
    for (int s = 0; s < maxBudget; ++s) {
      for (int j = 0; j < (int) cs.no_mines.size(); ++j) {
        int m = cs.no_mines[j];
        int rem = s - m;
        if (rem >= 0 && rem < maxBudget)
          dp[i][s] += (double) cs.freq_no_mines[j] * dp[i + 1][rem];
      }
    }
  }

  // Sample sequentially
  vector<int> chosenMines(C);
  int remaining = totalChainMines;
  for (int i = 0; i < C; ++i) {
    const ChainSolution& cs = chain_sols[i];
    vector<double> weights;
    vector<int> candidates;
    for (int j = 0; j < (int) cs.no_mines.size(); ++j) {
      int m = cs.no_mines[j];
      int rem = remaining - m;
      if (rem >= 0 && rem < maxBudget) {
        double w = (double) cs.freq_no_mines[j] * dp[i + 1][rem];
        if (w > 0) {
          weights.push_back(w);
          candidates.push_back(j);
        }
      }
    }
    std::discrete_distribution<int> chainDist(weights.begin(), weights.end());
    int chosen = chainDist(rng);
    int jIdx = candidates[chosen];
    chosenMines[i] = chain_sols[i].no_mines[jIdx];
    remaining -= chosenMines[i];
  }

  // Step 5d: Sample per-chain configuration
  for (int i = 0; i < C; ++i) {
    const ChainSolution& cs = chain_sols[i];
    int targetMines = chosenMines[i];

    // Filter all_configs to those matching targetMines
    vector<int> matching;
    for (int ci = 0; ci < (int) cs.all_configs.size(); ++ci) {
      int sum = 0;
      for (int v : cs.all_configs[ci])
        sum += v;
      if (sum == targetMines)
        matching.push_back(ci);
    }

    // Pick one uniformly at random
    std::uniform_int_distribution<int> configDist(0, (int) matching.size() - 1);
    int pickedIdx = matching[configDist(rng)];
    const vector<int>& pickedConfig = cs.all_configs[pickedIdx];

    // Map to board positions using relatedCells iteration order
    int idx = 0;
    for (Cell* cell : cs.relatedCells) {
      mineConf[cell->r][cell->c] = pickedConfig[idx];
      idx++;
    }
  }

  // Step 5e: Sample noNeighbor mines
  if (!solver.noNeighbors.empty()) {
    int n = (int) solver.noNeighbors.size();
    vector<bool> isMineVec(n, false);
    for (int i = 0; i < remainingForNoNeighbors && i < n; ++i)
      isMineVec[i] = true;
    std::shuffle(isMineVec.begin(), isMineVec.end(), rng);
    for (int i = 0; i < n; ++i) {
      Cell* cell = solver.noNeighbors[i];
      mineConf[cell->r][cell->c] = isMineVec[i] ? 1 : 0;
    }
  }
}

float Solver::tryWarp(int mines, int row, int col, bool isMine, vector<vector<int>>& mineConf) {
  int h = board.height;
  int w = board.width;

  // Step 1: Snapshot raw board before generalSolve mutates it
  vector<vector<int>> rawBoard(h, vector<int>(w));
  for (int r = 0; r < h; ++r)
    for (int c = 0; c < w; ++c)
      rawBoard[r][c] = board.getCell(r, c)->value;

  // Solve current board
  bool ok = generalSolve(mines);
  if (!ok) {
    mineConf.assign(h, vector<int>(w, -1));
    return -1;
  }

  // Step 2: Compute warp point
  Cell* cell = board.getCellMutable(row, col);
  float warpPoint = isMine ? cell->minePerc : (100.f - cell->minePerc);

  // Step 3: Early exit
  if (warpPoint <= 0 || warpPoint >= 100) {
    mineConf.assign(h, vector<int>(w, -1));
    return warpPoint;
  }

  // Step 4: Create warped board and re-solve
  vector<vector<int>> warpedBoard = rawBoard;
  warpedBoard[row][col] = isMine ? CELL_FLAG : CELL_SAFE;
  int warpedMines = isMine ? mines - 1 : mines;

  Solver warpedSolver(warpedBoard);
  bool warpedOk = warpedSolver.generalSolve(warpedMines);
  if (!warpedOk) {
    mineConf.assign(h, vector<int>(w, -1));
    return warpPoint;
  }

  // Step 5: Sample mine configuration
  std::mt19937 rng(std::random_device{}());
  sampleConfiguration(warpedSolver, warpedMines, mineConf, rng);

  // Ensure warped cell is set correctly
  mineConf[row][col] = isMine ? 1 : 0;

  // Step 6: Return warp point
  return warpPoint;
}
