// Shared state
let analysisOverlay = null;
let analyzeMode = false;
let endgameSolvable = false;
let endgameMode = false;
let bestMoveRow = -1;
let bestMoveCol = -1;
let winProbability = null;

// WASM module init
MinesweeperModule().then(module => {
  window.Module = module;
  window.solveBoard = Module.cwrap('solveBoard', 'number', ['number', 'number', 'number', 'number', 'number', 'number'], { async: true });
  window.solveEndgameWasm = Module.cwrap('solveEndgame', 'number', ['number', 'number', 'number', 'number', 'number', 'number', 'number'], { async: true });
  document.getElementById('analyzeBtn').disabled = false;
});

document.addEventListener('contextmenu', e => e.preventDefault());

async function solveMinesweeper(board, mines) {
  const nrows = board.length;
  const ncols = board[0].length;
  const board_flat = board.flat();
  const ptr = Module._malloc(board_flat.length * 4);
  Module.HEAP32.set(board_flat, ptr / 4);
  const outputPtr = Module._malloc(board_flat.length * 4);
  const canEndgamePtr = Module._malloc(1);
  let valid = await solveBoard(nrows, ncols, ptr, mines, outputPtr, canEndgamePtr);
  const output = Array.from(
    new Float32Array(Module.HEAP32.buffer, outputPtr, board_flat.length)
  );
  const canEndgame = Module.HEAPU8[canEndgamePtr] !== 0;
  const result2D = [];
  for (let i = 0; i < nrows; i++) {
    result2D.push(output.slice(i * ncols, (i + 1) * ncols));
  }

  Module._free(ptr);
  Module._free(outputPtr);
  Module._free(canEndgamePtr);

  return {
    valid: valid,
    result: valid ? result2D : null,
    canEndgame: canEndgame
  };
}

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

function updateAnalyzeButton() {
  const btn = document.getElementById('analyzeBtn');
  if (analyzeMode) {
    btn.textContent = 'Analyzing';
    btn.style.background = 'linear-gradient(135deg, #27ae60 0%, #2ecc71 100%)';
  } else {
    btn.textContent = 'Analyze';
    btn.style.background = 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)';
  }
  updateEndgameButton();
}

function updateEndgameButton() {
  const btn = document.getElementById('endgameBtn');
  btn.disabled = !analyzeMode || !endgameSolvable;
  if (endgameMode) {
    btn.textContent = 'Endgame';
    btn.style.background = 'linear-gradient(135deg, #f39c12 0%, #f1c40f 100%)';
  } else {
    btn.textContent = 'Endgame';
    btn.style.background = 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)';
  }
}

function updateWinProbLabel() {
  const label = document.getElementById('winProbLabel');
  const value = document.getElementById('winProbValue');
  if (!analyzeMode) {
    label.style.display = 'none';
    return;
  }
  label.style.display = 'flex';
  if (endgameMode && winProbability !== null) {
    value.textContent = (winProbability * 100).toFixed(2) + '%';
  } else {
    value.textContent = 'N/A';
  }
}

async function runEndgameAnalysis(inputBoard) {
  const board_flat = inputBoard.flat();
  const nrows = inputBoard.length;
  const ncols = inputBoard[0].length;

  const ptr = Module._malloc(board_flat.length * 4);
  Module.HEAP32.set(board_flat, ptr / 4);
  const winProbPtr = Module._malloc(4);
  const bestRowPtr = Module._malloc(4);
  const bestColPtr = Module._malloc(4);

  try {
    const valid = await solveEndgameWasm(nrows, ncols, ptr, mineCount, winProbPtr, bestRowPtr, bestColPtr);
    if (valid) {
      winProbability = Module.getValue(winProbPtr, 'float');
      bestMoveRow = Module.getValue(bestRowPtr, 'i32');
      bestMoveCol = Module.getValue(bestColPtr, 'i32');
    } else {
      winProbability = null;
      bestMoveRow = -1;
      bestMoveCol = -1;
    }
  } catch (e) {
    console.error("Endgame analysis failed:", e);
    winProbability = null;
    bestMoveRow = -1;
    bestMoveCol = -1;
  } finally {
    Module._free(ptr);
    Module._free(winProbPtr);
    Module._free(bestRowPtr);
    Module._free(bestColPtr);
  }
  updateWinProbLabel();
}

async function analyzeBoard() {
  analyzeMode = !analyzeMode;
  updateAnalyzeButton();

  if (analyzeMode) {
    if (typeof beforeAnalyze === 'function' && !beforeAnalyze()) {
      analyzeMode = false;
      updateAnalyzeButton();
      return;
    }
    document.getElementById('legend').style.display = 'flex';
    await runAnalysis();
  } else {
    analysisOverlay = null;
    endgameMode = false;
    bestMoveRow = -1;
    bestMoveCol = -1;
    winProbability = null;
    endgameSolvable = false;
    updateEndgameButton();
    updateWinProbLabel();
    document.getElementById('legend').style.display = 'none';
    if (typeof onAnalyzeOff === 'function') onAnalyzeOff();
    renderBoard();
  }
}

async function analyzeEndgame() {
  if (!analyzeMode || !endgameSolvable) return;

  endgameMode = !endgameMode;
  updateEndgameButton();

  if (endgameMode) {
    await runAnalysis();
  } else {
    bestMoveRow = -1;
    bestMoveCol = -1;
    winProbability = null;
    updateWinProbLabel();
    renderBoard();
  }
}
