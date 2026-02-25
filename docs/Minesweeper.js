const difficulties = {
  easy: { rows: 9, cols: 9, mines: 10 },
  medium: { rows: 16, cols: 16, mines: 40 },
  hard: { rows: 16, cols: 30, mines: 99 },
  custom: { rows: 16, cols: 16, mines: 40 }
};

let board = [];
let revealed = [];
let flagged = [];
let mines = [];
let gameOver = false;
let firstClick = true;
let timer = 0;
let timerInterval = null;
let rows, cols, mineCount;
let mouseButtons = { left: false, right: false };

function beforeAnalyze() {
  if (firstClick) {
    alert("Make at least one move first, you coward!");
    return false;
  }
  if (gameOver) {
    alert("The game is over. Let it go.");
    return false;
  }
  return true;
}

function startGame(config) {
  rows = config.rows;
  cols = config.cols;
  mineCount = config.mines;

  board = Array(rows).fill().map(() => Array(cols).fill(0));
  revealed = Array(rows).fill().map(() => Array(cols).fill(false));
  flagged = Array(rows).fill().map(() => Array(cols).fill(false));
  mines = [];
  gameOver = false;
  firstClick = true;
  timer = 0;
  analysisOverlay = null;
  analyzeMode = false;
  endgameSolvable = false;
  endgameMode = false;
  bestMoveRow = -1;
  bestMoveCol = -1;
  winProbability = null;

  clearInterval(timerInterval);
  document.getElementById('timer').textContent = '0';
  document.getElementById('flags').textContent = mineCount;
  document.getElementById('message').style.display = 'none';
  document.getElementById('legend').style.display = 'none';
  updateAnalyzeButton();
  updateEndgameButton();

  renderBoard();
}

function initGame() {
  const diff = document.getElementById('difficulty').value;
  const config = difficulties[diff];
  startGame(config);
}

function placeMines(excludeR, excludeC) {
  let placed = 0;
  while (placed < mineCount) {
    const r = Math.floor(Math.random() * rows);
    const c = Math.floor(Math.random() * cols);
    if (board[r][c] !== -1 && r != excludeR && c != excludeC) {
      board[r][c] = -1;
      mines.push([r, c]);
      placed++;
    }
  }

  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      if (board[r][c] !== -1) {
        board[r][c] = countAdjacentMines(r, c);
      }
    }
  }
}

function countAdjacentMines(r, c) {
  let count = 0;
  for (let dr = -1; dr <= 1; dr++) {
    for (let dc = -1; dc <= 1; dc++) {
      const nr = r + dr, nc = c + dc;
      if (nr >= 0 && nr < rows && nc >= 0 && nc < cols && board[nr][nc] === -1) {
        count++;
      }
    }
  }
  return count;
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

      if (revealed[r][c]) {
        cell.classList.add('revealed');
        if (board[r][c] === -1) {
          cell.classList.add('mine');
          cell.textContent = '💣';
        } else if (board[r][c] > 0) {
          cell.textContent = board[r][c];
          cell.classList.add(`num-${board[r][c]}`);
        }
      } else if (flagged[r][c]) {
        cell.classList.add('flagged');
        cell.textContent = '🚩';
      }

      if (analysisOverlay && !revealed[r][c] && !flagged[r][c]) {
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
      cell.addEventListener('mouseup', (e) => handleMouseUp(e, r, c));

      boardEl.appendChild(cell);
    }
  }
}

function handleMouseDown(e, r, c) {
  e.preventDefault();
  if (e.button === 0) mouseButtons.left = true;
  if (e.button === 2) mouseButtons.right = true;
}

function handleMouseUp(e, r, c) {
  e.preventDefault();
  const bothPressed = mouseButtons.left && mouseButtons.right;

  if (bothPressed) {
    chord(r, c);
  } else if (e.button === 0 && mouseButtons.left && !mouseButtons.right) {
    handleClick(r, c);
  } else if (e.button === 2 && mouseButtons.right && !mouseButtons.left) {
    handleRightClick(r, c);
  }

  if (e.button === 0) mouseButtons.left = false;
  if (e.button === 2) mouseButtons.right = false;
}

function chord(r, c) {
  if (gameOver || !revealed[r][c] || board[r][c] <= 0) return;

  let adjacentFlags = 0;
  for (let dr = -1; dr <= 1; dr++) {
    for (let dc = -1; dc <= 1; dc++) {
      const nr = r + dr, nc = c + dc;
      if (nr >= 0 && nr < rows && nc >= 0 && nc < cols && flagged[nr][nc]) {
        adjacentFlags++;
      }
    }
  }

  if (adjacentFlags === board[r][c]) {
    for (let dr = -1; dr <= 1; dr++) {
      for (let dc = -1; dc <= 1; dc++) {
        const nr = r + dr, nc = c + dc;
        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols && !flagged[nr][nc] && !revealed[nr][nc]) {
          if (board[nr][nc] === -1) {
            revealAllMines(nr, nc);
            endGame(false);
            return;
          }
          reveal(nr, nc);
        }
      }
    }
    renderBoard();
    checkWin();
    if (analyzeMode) runAnalysis();
  }
}

function handleClick(r, c) {
  if (gameOver || flagged[r][c] || revealed[r][c]) return;

  if (firstClick) {
    firstClick = false;
    placeMines(r, c);
    timerInterval = setInterval(() => {
      timer++;
      document.getElementById('timer').textContent = timer;
    }, 1000);
  }

  if (board[r][c] === -1) {
    revealAllMines(r, c);
    endGame(false);
    return;
  }

  reveal(r, c);
  renderBoard();
  checkWin();
  if (analyzeMode) runAnalysis();
}

function handleRightClick(r, c) {
  if (gameOver || revealed[r][c]) return;

  flagged[r][c] = !flagged[r][c];
  const flagCount = flagged.flat().filter(f => f).length;
  document.getElementById('flags').textContent = mineCount - flagCount;
  renderBoard();
  if (analyzeMode) runAnalysis();
}

function reveal(r, c) {
  if (r < 0 || r >= rows || c < 0 || c >= cols) return;
  if (revealed[r][c] || flagged[r][c]) return;

  revealed[r][c] = true;

  if (board[r][c] === 0) {
    for (let dr = -1; dr <= 1; dr++) {
      for (let dc = -1; dc <= 1; dc++) {
        reveal(r + dr, c + dc);
      }
    }
  }
}

function revealAllMines(hitR, hitC) {
  mines.forEach(([r, c]) => {
    revealed[r][c] = true;
  });
  renderBoard();
  const hitCell = document.querySelector(`[data-row="${hitR}"][data-col="${hitC}"]`);
  if (hitCell) hitCell.classList.add('mine-hit');
}

function checkWin() {
  let unrevealedSafe = 0;
  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      if (!revealed[r][c] && board[r][c] !== -1) {
        unrevealedSafe++;
      }
    }
  }
  if (unrevealedSafe === 0) {
    endGame(true);
  }
}

function endGame(won) {
  gameOver = true;
  clearInterval(timerInterval);
  const msg = document.getElementById('message');
  msg.style.display = 'block';
  if (won) {
    msg.className = 'message win';
    msg.textContent = `🎉 You won in ${timer}s! Your trust issues paid off!`;
  } else {
    msg.className = 'message lose';
    msg.textContent = '💀 BOOM! Your luck ran out. Shocking.';
  }
}

initGame();

document.getElementById('difficulty').addEventListener('change', function() {
  if (this.value === 'custom') {
    openModal();
  } else {
    initGame();
  }
  updateCustomDifficultyLabel()
});

document.getElementById('customWidth').addEventListener('input', updateModalStats);
document.getElementById('customHeight').addEventListener('input', updateModalStats);
document.getElementById('customMines').addEventListener('input', updateModalStats);

function openModal() {
  document.getElementById('customModal').classList.add('active');
  updateModalStats();
}

function closeModal() {
  document.getElementById('customModal').classList.remove('active');
  const diff = document.getElementById('difficulty');
  if (difficulties.custom.rows === 16 && difficulties.custom.cols === 16 && difficulties.custom.mines === 40) {
    diff.value = 'medium';
  }
  updateCustomDifficultyLabel()
}

function updateCustomDifficultyLabel() {
  let diff = document.getElementById('difficulty').value
  if (diff != "custom")
    document.getElementById("custom-opt").innerHTML = "Custom size..."
  else {
    let r = difficulties.custom.rows
    let c = difficulties.custom.cols
    let m = difficulties.custom.mines

    document.getElementById("custom-opt").innerHTML = `Custom size (${r}x${c}) ${m} mines`
  }
}

function updateModalStats() {
  const w = parseInt(document.getElementById('customWidth').value) || 5;
  const h = parseInt(document.getElementById('customHeight').value) || 5;
  const m = parseInt(document.getElementById('customMines').value) || 1;
  const total = w * h;
  const percent = ((m / total) * 100).toFixed(1);
  document.getElementById('totalTiles').textContent = `${total} tiles`;
  document.getElementById('minePercent').textContent = `${percent}%`;
}

function setMinePercent(percent) {
  const w = parseInt(document.getElementById('customWidth').value) || 5;
  const h = parseInt(document.getElementById('customHeight').value) || 5;
  const total = w * h;
  const mines = Math.round(total * percent / 100);
  document.getElementById('customMines').value = Math.max(1, Math.min(mines, total - 9));
  updateModalStats();
}

function submitCustom() {
  let w = parseInt(document.getElementById('customWidth').value) || 16;
  let h = parseInt(document.getElementById('customHeight').value) || 16;
  let m = parseInt(document.getElementById('customMines').value) || 40;

  w = Math.max(5, Math.min(50, w));
  h = Math.max(5, Math.min(30, h));
  m = Math.max(1, Math.min(m, w * h - 9));

  difficulties.custom = { rows: h, cols: w, mines: m };
  closeModal();
  startGame(difficulties.custom);
}

startGame(difficulties.easy);

async function runAnalysis() {
  if (!analyzeMode || firstClick || gameOver) return;

  const inputBoard = [];
  for (let r = 0; r < rows; r++) {
    const row = [];
    for (let c = 0; c < cols; c++) {
      if (revealed[r][c]) {
        row.push(board[r][c]);
      } else {
        row.push(-1);
      }
    }
    inputBoard.push(row);
  }

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
    if (valid && analyzeMode) {
      analysisOverlay = result;
      if (endgameMode && canEndgame) {
        await runEndgameAnalysis(inputBoard);
      }
      renderBoard();
      checkAllUncertain(result);
    }
  } catch (e) {
    console.error("Analysis failed:", e);
  }
}

function checkAllUncertain(result) {
  const uncertainties = [];
  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      if (!revealed[r][c] && !flagged[r][c]) {
        uncertainties.push(result[r][c]);
      }
    }
  }
  if (uncertainties.length > 0 && uncertainties.every(p => p === 50)) {
    const messages = [
      "Every tile is 50/50. Math has abandoned you. Good luck! 🎲",
      "It's all coin flips from here. May the odds be ever in your favor! 🍀",
      "Pure chaos. No logic can save you now. Godspeed! 🙏",
      "Schrödinger's minefield: every tile is both safe AND a mine. Good luck! 🐱",
      "The solver shrugs. Pick a tile, any tile. YOLO! 🎰"
    ];
    alert(messages[Math.floor(Math.random() * messages.length)]);
  }
}
