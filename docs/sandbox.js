// Board state
let rows = 9, cols = 9, mineCount = 10;
// cellValues[r][c]: the number displayed on a revealed cell (0-8)
let cellValues = [];
// cellState[r][c]: 'revealed' | 'unrevealed' | 'flagged'
let cellState = [];

let hoveredCell = null;
document.addEventListener('keydown', (e) => {
  if (!hoveredCell) return;
  const { r, c } = hoveredCell;
  if (cellState[r][c] !== 'revealed') return;
  if (e.key === '-') {
    cellValues[r][c] = (cellValues[r][c] === -3) ? 0 : -3;
  } else {
    const num = parseInt(e.key);
    if (isNaN(num) || num < 0 || num > 8) return;
    cellValues[r][c] = (cellValues[r][c] === num) ? 0 : num;
  }
  renderBoard();
  if (analyzeMode) runAnalysis();
});

function beforeAnalyze() {
  return true;
}

function onAnalyzeOff() {
  document.getElementById('message').style.display = 'none';
}

function initBoard() {
  cellValues = Array(rows).fill().map(() => Array(cols).fill(0));
  cellState = Array(rows).fill().map(() => Array(cols).fill('revealed'));
  analysisOverlay = null;
  endgameSolvable = false;
  endgameMode = false;
  bestMoveRow = -1;
  bestMoveCol = -1;
  winProbability = null;

  if (analyzeMode) {
    analyzeMode = false;
    updateAnalyzeButton();
    document.getElementById('legend').style.display = 'none';
  }
  document.getElementById('message').style.display = 'none';
  updateEndgameButton();
  updateWinProbLabel();
  renderBoard();
}

function resizeBoard() {
  const w = Math.max(2, Math.min(50, parseInt(document.getElementById('paramWidth').value) || 9));
  const h = Math.max(2, Math.min(30, parseInt(document.getElementById('paramHeight').value) || 9));
  const m = Math.max(0, parseInt(document.getElementById('paramMines').value) || 0);

  document.getElementById('paramWidth').value = w;
  document.getElementById('paramHeight').value = h;
  document.getElementById('paramMines').value = m;

  rows = h;
  cols = w;
  mineCount = m;
  initBoard();
}

function renderBoard() {
  const boardEl = document.getElementById('board');
  boardEl.style.gridTemplateColumns = `repeat(${cols}, 32px)`;
  boardEl.innerHTML = '';

  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      const cell = document.createElement('div');
      cell.className = 'cell';
      cell.dataset.row = r;
      cell.dataset.col = c;

      const state = cellState[r][c];

      if (state === 'revealed') {
        cell.classList.add('revealed');
        if (cellValues[r][c] === -3) {
          cell.textContent = '?';
          cell.classList.add('dont-care');
        } else if (cellValues[r][c] > 0) {
          cell.textContent = cellValues[r][c];
          cell.classList.add(`num-${cellValues[r][c]}`);
        }
      } else if (state === 'flagged') {
        cell.classList.add('flagged');
        cell.textContent = '🚩';
      }

      // Analysis overlay on unrevealed (non-flagged) cells
      if (analysisOverlay && state === 'unrevealed') {
        const prob = analysisOverlay[r][c];
        const overlay = document.createElement('div');
        overlay.className = 'overlay';
        overlay.style.background = getProbabilityColor(prob);
        overlay.textContent = prob.toFixed(0) + '%';
        cell.appendChild(overlay);
      }

      if (endgameMode && r === bestMoveRow && c === bestMoveCol) {
        cell.classList.add('best-move');
      }

      cell.addEventListener('mousedown', (e) => handleMouseDown(e, r, c));
      cell.addEventListener('wheel', (e) => handleWheel(e, r, c));
      cell.addEventListener('mouseenter', () => { hoveredCell = { r, c }; });
      cell.addEventListener('mouseleave', () => { hoveredCell = null; });

      boardEl.appendChild(cell);
    }
  }
}

function handleMouseDown(e, r, c) {
  e.preventDefault();
  if (e.button === 0) {
    handleLeftClick(r, c);
  } else if (e.button === 2) {
    handleRightClick(r, c);
  }
}

function handleLeftClick(r, c) {
  const state = cellState[r][c];
  if (state === 'revealed') {
    // Toggle to unrevealed
    cellState[r][c] = 'unrevealed';
  } else if (state === 'unrevealed') {
    // Toggle to revealed
    cellState[r][c] = 'revealed';
  } else if (state === 'flagged') {
    // Unflag first, then reveal — remove flag effects
    removeFlag(r, c);
    cellState[r][c] = 'revealed';
  }
  renderBoard();
  if (analyzeMode) runAnalysis();
}

function handleRightClick(r, c) {
  const state = cellState[r][c];
  if (state === 'flagged') {
    // Remove flag
    removeFlag(r, c);
    cellState[r][c] = 'unrevealed';
  } else if (state === 'unrevealed') {
    // Place flag
    cellState[r][c] = 'flagged';
    addFlag(r, c);
  } else if (state === 'revealed') {
    // Flag directly from revealed
    cellState[r][c] = 'flagged';
    addFlag(r, c);
  }
  renderBoard();
  if (analyzeMode) runAnalysis();
}

function addFlag(r, c) {
  // Increase neighbor numbers by 1
  for (let dr = -1; dr <= 1; dr++) {
    for (let dc = -1; dc <= 1; dc++) {
      if (dr === 0 && dc === 0) continue;
      const nr = r + dr, nc = c + dc;
      if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
        if (cellState[nr][nc] === 'revealed' && cellValues[nr][nc] !== -3) {
          cellValues[nr][nc] = Math.min(8, cellValues[nr][nc] + 1);
        }
      }
    }
  }
}

function removeFlag(r, c) {
  // Decrease neighbor numbers by 1 (capped at 0)
  for (let dr = -1; dr <= 1; dr++) {
    for (let dc = -1; dc <= 1; dc++) {
      if (dr === 0 && dc === 0) continue;
      const nr = r + dr, nc = c + dc;
      if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
        if (cellState[nr][nc] === 'revealed' && cellValues[nr][nc] !== -3) {
          cellValues[nr][nc] = Math.max(0, cellValues[nr][nc] - 1);
        }
      }
    }
  }
}

function handleWheel(e, r, c) {
  e.preventDefault();
  if (cellState[r][c] !== 'revealed') return;

  if (e.deltaY < 0) {
    // Scroll up — increase number; from -3 (don't-care) wrap to 0
    if (cellValues[r][c] === -3) {
      cellValues[r][c] = 0;
    } else {
      cellValues[r][c] = Math.min(8, cellValues[r][c] + 1);
    }
  } else if (e.deltaY > 0) {
    // Scroll down — decrease number; from 0 wrap to -3 (don't-care)
    if (cellValues[r][c] === 0) {
      cellValues[r][c] = -3;
    } else if (cellValues[r][c] !== -3) {
      cellValues[r][c] = Math.max(0, cellValues[r][c] - 1);
    }
  }
  renderBoard();
  if (analyzeMode) runAnalysis();
}

// Build solver input board
function getInputBoard() {
  const inputBoard = [];
  for (let r = 0; r < rows; r++) {
    const row = [];
    for (let c = 0; c < cols; c++) {
      const state = cellState[r][c];
      if (state === 'revealed') {
        row.push(cellValues[r][c]);
      } else if (state === 'flagged') {
        row.push(-2); // CELL_FLAG
      } else {
        row.push(-1); // CELL_UNDISCOVERED
      }
    }
    inputBoard.push(row);
  }
  return inputBoard;
}

async function runAnalysis() {
  if (!analyzeMode) return;

  // Check if there are any unrevealed/flagged cells — need something to analyze
  let hasUnrevealed = false;
  for (let r = 0; r < rows && !hasUnrevealed; r++) {
    for (let c = 0; c < cols && !hasUnrevealed; c++) {
      if (cellState[r][c] !== 'revealed') hasUnrevealed = true;
    }
  }
  if (!hasUnrevealed) {
    analysisOverlay = null;
    endgameSolvable = false;
    if (endgameMode) {
      endgameMode = false;
      bestMoveRow = -1;
      bestMoveCol = -1;
      winProbability = null;
    }
    updateEndgameButton();
    updateWinProbLabel();
    renderBoard();
    return;
  }

  mineCount = Math.max(0, parseInt(document.getElementById('paramMines').value) || 0);
  const inputBoard = getInputBoard();

  try {
    const { valid, result, canEndgame } = await solveMinesweeper(inputBoard, mineCount);
    endgameSolvable = canEndgame;
    if (!canEndgame && endgameMode) {
      endgameMode = false;
      bestMoveRow = -1;
      bestMoveCol = -1;
      winProbability = null;
    }
    updateEndgameButton();
    updateWinProbLabel();

    const msg = document.getElementById('message');
    if (!valid) {
      analysisOverlay = null;
      msg.style.display = 'block';
      msg.className = 'message lose';
      msg.textContent = 'Impossible board state — no valid mine configuration exists.';
      renderBoard();
      return;
    }

    msg.style.display = 'none';
    analysisOverlay = result;

    if (endgameMode && canEndgame) {
      await runEndgameAnalysis(inputBoard);
    }

    renderBoard();
  } catch (e) {
    console.error("Analysis failed:", e);
  }
}

// Initialize
initBoard();
