MinesweeperModule().then(module => {
  window.Module = module
  window.solveBoard = Module.cwrap('solveBoard', 'number', ['number', 'number', 'number', 'number', 'number'], { async: true });      
});

async function solveMinesweeper(board, mines) {
  const nrows = board.length;
  const ncols = board[0].length;
  const board_flat = board.flat()
  const ptr = Module._malloc(board_flat.length * 4);
  Module.HEAP32.set(board_flat, ptr / 4)
  const outputPtr = Module._malloc(board_flat.length * 4)
  let valid = await solveBoard(nrows, ncols, ptr, mines, outputPtr)
  const output = Array.from(
    new Float32Array(Module.HEAP32.buffer, outputPtr, board_flat.length)
  );
  const result2D = [];
  for (let i = 0; i < nrows; i++) {
    result2D.push(output.slice(i * ncols, (i + 1) * ncols));
  }

  Module._free(ptr);
  Module._free(outputPtr);

  return {
    valid: valid, 
    result: valid ? result2D : null
  }
}

const difficulties = {
  easy: { rows: 9, cols: 9, mines: 10 },
  medium: { rows: 16, cols: 16, mines: 40 },
  hard: { rows: 16, cols: 30, mines: 99 },
  custom: { rows: 16, cols: 16, mines: 40 }
};

const energySpawnRate = [
  0.0002, // 0
  0.0005, // 1
  0.0010, // 2
  0.0019, // 3
  0.0039, // 4
  0.0156, // 5
  0.0625, // 6
  0.25, // 7
  1, // 8
]

const energySpawn = [0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32]

let board = [];
let warpEnergies = []
let revealed = [];
let flagged = [];
let gameOver = false;
let firstClick = true;
let timer = 0;
let timerInterval = null;
let rows, cols, mineCount;
let mouseButtons = { left: false, right: false };
let warpEnergyLeft = 0;
let warpMode = false;
let warpStates = []
let warpCell = {
  row: -1,
  col: -1,
  warpTo: 0
}

document.addEventListener('contextmenu', e => e.preventDefault());

function startGame(config) {
  rows = config.rows;
  cols = config.cols;
  mineCount = config.mines;

  board = Array(rows).fill().map(() => Array(cols).fill(0));
  warpEnergies = Array(rows).fill().map(() => Array(cols).fill(0));
  warpCell = {
    row: -1,
    col: -1,
    warpTo: 0
  }
  revealed = Array(rows).fill().map(() => Array(cols).fill(false));
  flagged = Array(rows).fill().map(() => Array(cols).fill(false));
  gameOver = false;
  firstClick = true;
  timer = 0;

  clearInterval(timerInterval);
  document.getElementById('timer').textContent = '0';
  document.getElementById('flags').textContent = mineCount;
  document.getElementById('message').style.display = 'none';
  document.getElementById('legend').style.display = 'none';

  renderBoard();
  document.getElementById('warpBtn').disabled = true
}

function initGame() {
  const diff = document.getElementById('difficulty').value;
  // if (diff === 'custom') {
  //   openModal();
  //   return;
  // }
  const config = difficulties[diff];
  startGame(config);
}

function fillNumbers(board) {
  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      if (board[r][c] !== -1) {
        board[r][c] = countAdjacentMines(board, r, c);
      }
    }
  }
}

function placeMines(excludeR, excludeC) {
  let placed = 0;
  while (placed < mineCount) {
    const r = Math.floor(Math.random() * rows);
    const c = Math.floor(Math.random() * cols);
    if (board[r][c] !== -1 && r != excludeR && c != excludeC) {
      board[r][c] = -1;
      placed++;
    }
  }

  fillNumbers(board)
}

function getAdjancent(r, c) {
  let out = []
  for (let dr = -1; dr <= 1; dr++) {
    for (let dc = -1; dc <= 1; dc++) {
      const nr = r + dr, nc = c + dc;
      if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
        out.push([nr, nc])
      }
    }
  }
  return out
}

function countAdjacentMines(board, r, c) {
  const rows = board.length
  const cols = board[0].length
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

        if (warpEnergies[r][c] > 0) {
          cell.classList.add('energized')
        }
      } else if (flagged[r][c]) {
        cell.classList.add('flagged');
        cell.textContent = '🚩';
      }

      if (warpMode && r == warpCell.row && c == warpCell.col) {
        if (warpCell.warpTo == 1)
          cell.classList.add('warp-safe')
        if (warpCell.warpTo == -1)
          cell.classList.add('warp-mine')
      }

      cell.addEventListener('mousedown', (e) => handleMouseDown(e, r, c));
      cell.addEventListener('mouseup', (e) => handleMouseUp(e, r, c));

      boardEl.appendChild(cell);
    }
  }
}

function handleWarpSet(e, r, c) {
  const bothPressed = mouseButtons.left && mouseButtons.right;
  if (bothPressed)
    return;

  if (revealed[r][c])
    return;

  let warpId = (mouseButtons.left ? 1 : -1)

  if (warpCell.row == r && warpCell.col == c && warpCell.warpTo == warpId)
    warpCell.warpTo = 0
  else {
    warpCell.row = r
    warpCell.col = c
    warpCell.warpTo = warpId
  }

  renderBoard()
  document.getElementById('commitWarp').disabled = (warpCell.warpTo == 0)
}

function handleMouseDown(e, r, c) {
  e.preventDefault();
  if (e.button === 0) mouseButtons.left = true;
  if (e.button === 2) mouseButtons.right = true;

  if (warpEnergies[r][c] > 0) {
    updateWarpEnergy(warpEnergies[r][c])
    warpEnergies[r][c] = 0;
    renderBoard()
  }

  if (warpMode)
    handleWarpSet(e, r, c)
}

function handleMouseUp(e, r, c) {
  e.preventDefault();

  if (!warpMode) {
    const bothPressed = mouseButtons.left && mouseButtons.right;

    if (bothPressed) {
      chord(r, c);
    } else if (e.button === 0 && mouseButtons.left && !mouseButtons.right) {
      handleClick(r, c);
    } else if (e.button === 2 && mouseButtons.right && !mouseButtons.left) {
      handleRightClick(r, c);
    }
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
    document.getElementById('warpBtn').disabled = false
  }

  if (board[r][c] === -1) {
    revealAllMines(r, c);
    endGame(false);
    return;
  }

  reveal(r, c);
  renderBoard();
  checkWin();
}

function handleRightClick(r, c) {
  if (gameOver || revealed[r][c]) return;

  flagged[r][c] = !flagged[r][c];
  const flagCount = flagged.flat().filter(f => f).length;
  document.getElementById('flags').textContent = mineCount - flagCount;
  renderBoard();
}

function reveal(r, c) {
  if (r < 0 || r >= rows || c < 0 || c >= cols) return;
  if (revealed[r][c] || flagged[r][c]) return;

  revealed[r][c] = true;

  if (Math.random() < energySpawnRate[board[r][c]])
    warpEnergies[r][c] = energySpawn[board[r][c]]

  if (board[r][c] === 0) {
    for (let dr = -1; dr <= 1; dr++) {
      for (let dc = -1; dc <= 1; dc++) {
        reveal(r + dr, c + dc);
      }
    }
  }
}

function revealAllMines(hitR, hitC) {
  board.forEach((row, i) => {
    row.forEach((cell, j) => {
      if (cell == -1)
        revealed[i][j] = true;
    })
  })
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

  document.getElementById('warpBtn').disabled = true
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

function getProbabilityColor(prob) {
  if (prob <= 0) return 'rgba(39, 174, 96, 0.85)';
  if (prob >= 100) return 'rgba(231, 76, 60, 0.85)';
  if (prob < 50) {
    const ratio = prob / 50;
    return `rgba(${Math.round(39 + (243 - 39) * ratio)}, ${Math.round(174 + (156 - 174) * ratio)}, ${Math.round(96 + (18 - 96) * ratio)}, 0.85)`;
  } else {
    const ratio = (prob - 50) / 50;
    return `rgba(${Math.round(243 + (231 - 243) * ratio)}, ${Math.round(156 + (76 - 156) * ratio)}, ${Math.round(18 + (60 - 18) * ratio)}, 0.85)`;
  }
}

async function analyzeBoard(apply_warp = false) {
  if (firstClick || gameOver) return null;

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

  if (apply_warp) {
    inputBoard[warpCell.row][warpCell.col] = (warpCell.warpTo == 1 ? -3 : -2)
  }

  try {
    const { valid, result } = await solveMinesweeper(inputBoard, mineCount);
    console.log(valid, result)
    return result;
  } catch (e) {
    console.error("Analysis failed:", e);
    return null
  }
}

// async function analyzeBoard() {
//   analyzeMode = !analyzeMode;
//   updateAnalyzeButton();

//   if (analyzeMode) {
//     if (firstClick) {
//       alert("Make at least one move first, you coward!");
//       analyzeMode = false;
//       updateAnalyzeButton();
//       return;
//     }
//     if (gameOver) {
//       alert("The game is over. Let it go.");
//       analyzeMode = false;
//       updateAnalyzeButton();
//       return;
//     }
//     document.getElementById('legend').style.display = 'flex';
//     await runAnalysis();
//   } else {
//     analysisOverlay = null;
//     document.getElementById('legend').style.display = 'none';
//     renderBoard();
//   }
// }

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

function updateWarpEnergy(change) {
  warpEnergyLeft += change
  document.getElementById('warp-energy').innerHTML = warpEnergyLeft
}

function updateWarpButton() {
  const btn = document.getElementById('warpBtn');
  if (warpMode) {
    btn.innerHTML = "Cancel warp"
    btn.style.background = 'linear-gradient(135deg, rgb(174 39 39) 0%, rgb(204 46 46) 100%)';
  } else {
    btn.innerHTML = "Enter warp mode"
    btn.style.background = 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)';
  }
}

function toggleWarpMode() {
  warpMode = !warpMode
  if (!warpMode) {
    let warpCell = {
      row: -1,
      col: -1,
      warpTo: 0
    }
    renderBoard()
  }

  updateWarpButton()
  document.getElementById('warp-overlay').style.display = warpMode ? 'block' : 'none'
  document.getElementById('commitWarp').style.display = warpMode ? 'block' : 'none'
}

function calculateEnergy(prob) {
  return -Math.log2(prob)
}

async function warpMines(useEnergy) {
  const prob = await analyzeBoard();
  if (prob == null)
    return;

  const p = prob[warpCell.row][warpCell.col] / 100
  let energyUsed = calculateEnergy(warpCell.warpTo == 1 ? (1 - p) : p);
  let success = energyUsed < Infinity;
  if (success && energyUsed > 0) {
    const prob2 = await analyzeBoard(true)
    let board2 = Array(rows).fill().map(() => Array(cols).fill(0));
    let toFix = []
    for (let i = 0; i < rows; ++i) {
      for (let j = 0; j < cols; ++j) {
        if (revealed[i][j]) {
          board2[i][j] = board[i][j]
          getAdjancent(i, j).forEach(([r, c]) => {
            if (revealed[r][c])
              return;

            if (prob2[r][c] == 0 || prob2[r][c] == 100)
              board2[r][c] = (prob2[r][c] == 0 ? 0 : -1)
            else if (board[r][c] == -1) {
              board2[r][c] = -1
            }
          })

          if (countAdjacentMines(board2, i, j) != board2[i][j])
            toFix.push([i, j])
        }
      }
    }

    console.log(board2, toFix)
    

    // revealed.forEach((row, i) => {
    //   row.forEach((r, j) => {
    //     if (r)
    //       return;

    //     if (prob2[i][j] != 0 && prob2[i][j] != 100)
    //       return;
    //   })
    // })
  }

  // toggleWarpMode()
  // if (success) {
  //   if (warpCell.warpTo == 1)
  //     revealed[warpCell.row][warpCell.col] = true
  //   else
  //     flagged[warpCell.row][warpCell.col] = true


  //   renderBoard()
  // } else {
  //   revealAllMines()
  //   endGame(false)
  //   return;
  // }
}

document.getElementById("commitWarp").onclick = async() => {
  await warpMines(false)
};