const positions = [
  [80, 102], [530, 70], [824, 180], [68, 398], [450, 294], [620, 404],
  [858, 469], [134, 708], [288, 518], [762, 752], [360, 790], [519, 682]
];

const factionBoards = {
  "Marquise de Cat": {
    front: "/assets/marquisdecat_frontboardgame.webp",
    back: "/assets/marquisdecat_backboardgame.webp"
  },
  "Eyrie Dynasties": {
    front: "/assets/eyriedynasties_frontboardgame.webp",
    back: "/assets/eyriedynasties_backboardgame.webp"
  }
};

const app = {
  session: null,
  state: null,
  mode: "move",
  selected: null,
  pollTimer: null,
  busy: false,
  cardArt: [],
  cardArtPromise: null,
  boardSide: "front"
};

const elements = Object.fromEntries([
  "lobby", "game", "create-form", "join-form", "room-code-button", "phase-label",
  "active-label", "actions-label", "players", "event-text", "board", "waiting",
  "waiting-code", "move-form", "battle-form", "move-from", "move-to",
  "battle-clearing", "phase-button", "control-note", "leave-button", "toast",
  "hand-panel", "hand-left", "hand-right", "deck-count", "card-dialog", "card-dialog-close",
  "card-dialog-image", "card-dialog-title", "table-view", "faction-board-view",
  "faction-board-canvas", "faction-board-image", "faction-board-pieces",
  "flip-board-button"
].map(id => [id, document.getElementById(id)]));

async function api(path, method = "GET", values = {}) {
  const parameters = new URLSearchParams(values);
  const target = method === "GET" ? `${path}?${parameters}` : path;
  const response = await fetch(target, {
    method,
    headers: method === "POST" ? { "Content-Type": "application/x-www-form-urlencoded" } : {},
    body: method === "POST" ? parameters : undefined
  });
  const result = await response.json();
  if (!response.ok || !result.ok) throw new Error(result.error || "Request failed");
  return result;
}

function notify(message) {
  elements.toast.textContent = message;
  elements.toast.classList.add("visible");
  window.clearTimeout(notify.timer);
  notify.timer = window.setTimeout(() => elements.toast.classList.remove("visible"), 2600);
}

function saveSession(code, token) {
  app.session = { code, token };
  localStorage.setItem("gout-session", JSON.stringify(app.session));
}

function enterGame(state) {
  app.state = state;
  elements.lobby.classList.add("hidden");
  elements.game.classList.remove("hidden");
  render();
  loadCardArt();
  startPolling();
}

async function loadCardArt() {
  if (app.cardArt.length || app.cardArtPromise) return app.cardArtPromise;
  app.cardArtPromise = fetch("/assets/cards/manifest.json")
    .then(response => {
      if (!response.ok) throw new Error("Card artwork is unavailable");
      return response.json();
    })
    .then(manifest => {
      app.cardArt = manifest.cards.filter(card => card.group === "shared-deck");
      if (app.state) renderHand();
    })
    .catch(error => notify(error.message))
    .finally(() => { app.cardArtPromise = null; });
  return app.cardArtPromise;
}

function leaveGame() {
  window.clearInterval(app.pollTimer);
  localStorage.removeItem("gout-session");
  app.session = null;
  app.state = null;
  elements.game.classList.add("hidden");
  elements.lobby.classList.remove("hidden");
}

async function createRoom(event) {
  event.preventDefault();
  const values = Object.fromEntries(new FormData(event.currentTarget));
  try {
    const result = await api("/api/rooms", "POST", values);
    saveSession(result.state.code, result.token);
    enterGame(result.state);
  } catch (error) { notify(error.message); }
}

async function joinRoom(event) {
  event.preventDefault();
  const values = Object.fromEntries(new FormData(event.currentTarget));
  values.code = values.code.toUpperCase();
  try {
    const result = await api("/api/rooms/join", "POST", values);
    saveSession(result.state.code, result.token);
    enterGame(result.state);
  } catch (error) { notify(error.message); }
}

function startPolling() {
  window.clearInterval(app.pollTimer);
  app.pollTimer = window.setInterval(refreshState, 800);
}

async function refreshState() {
  if (!app.session || app.busy) return;
  try {
    const result = await api("/api/state", "GET", app.session);
    if (!app.state || result.state.revision !== app.state.revision) {
      app.state = result.state;
      render();
    }
  } catch (error) {
    window.clearInterval(app.pollTimer);
    notify(error.message);
  }
}

async function sendAction(action, values = {}) {
  if (!app.session || !app.state || app.busy) return;
  app.busy = true;
  try {
    const result = await api("/api/action", "POST", {
      ...app.session,
      revision: app.state.revision,
      action,
      ...values
    });
    app.state = result.state;
    render();
  } catch (error) {
    notify(error.message);
    await refreshState();
  } finally {
    app.busy = false;
  }
}

function render() {
  const state = app.state;
  elements["room-code-button"].textContent = `Room ${state.code}`;
  elements["waiting-code"].textContent = state.code;
  elements["event-text"].textContent = state.lastEvent;
  renderPlayers();
  renderHand();
  renderFactionBoard();
  syncTableLayout();

  if (state.status === "waiting") {
    elements["phase-label"].textContent = "Lobby";
    elements["active-label"].textContent = state.you;
    elements["actions-label"].textContent = "";
    elements.board.replaceChildren();
    setControls(false, "Waiting for an opponent");
    elements["phase-button"].disabled = true;
    return;
  }

  elements["phase-label"].textContent = state.phase;
  elements["active-label"].textContent = state.activeFaction;
  elements["actions-label"].textContent = `${state.actionsRemaining} actions`;
  renderBoard();
  renderClearingOptions();
  const yourTurn = state.activeFaction === state.you && !state.winner;
  const canAct = yourTurn && state.phase === "Daylight" && state.actionsRemaining > 0;
  setControls(canAct, state.winner ? `${state.winner} wins` : yourTurn ? "Your turn" : "Opponent's turn");
  elements["phase-button"].disabled = !yourTurn;
}

function syncTableLayout() {
  elements.board.classList.remove("hidden");
  elements["faction-board-view"].classList.remove("hidden");
  elements["hand-panel"].classList.add("with-faction");
  document.querySelector(".game-layout").classList.add("first-person");
  elements.waiting.classList.toggle("hidden", app.state?.status !== "waiting");
}

function boardPiece(source, left, top, label) {
  return `<img class="faction-piece" src="${source}" alt="${label}" style="left:${left}%;top:${top}%">`;
}

function renderFactionBoard() {
  const faction = app.state?.you;
  const board = factionBoards[faction];
  if (!board) return;

  const side = app.boardSide;
  elements["faction-board-image"].src = board[side];
  elements["faction-board-image"].alt = `${faction} board ${side}`;
  elements["flip-board-button"].textContent = side === "front" ? "Show back" : "Show front";

  if (side === "back") {
    elements["faction-board-pieces"].replaceChildren();
    return;
  }

  if (faction === "Marquise de Cat") {
    const slots = [58.4, 66.4, 74.6, 82.8, 90.8];
    const rows = [
      ["/assets/sawmill.png", 43.2, "Sawmill"],
      ["/assets/workshop.png", 53.5, "Workshop"],
      ["/assets/recruiter.png", 64, "Recruiter"]
    ];
    elements["faction-board-pieces"].innerHTML = rows.flatMap(([source, top, label]) =>
      slots.map(left => boardPiece(source, left, top, label))
    ).join("");
  } else {
    elements["faction-board-pieces"].innerHTML = [58, 66, 74, 81.8, 89.4, 95]
      .map(left => boardPiece("/assets/roost.png", left, 45.2, "Roost"))
      .join("");
  }
}

function normalizedCardTitle(value) {
  return value.toLowerCase().replace(/[^a-z0-9]/g, "");
}

function cardArtwork(card) {
  const title = normalizedCardTitle(card.title);
  const candidates = app.cardArt.filter(entry => normalizedCardTitle(entry.title) === title);
  const suited = candidates.find(entry => entry.id.startsWith(`${card.suit}-`));
  const match = suited || candidates[0];
  return match ? `/assets/${match.file}` : "";
}

function renderHand() {
  const cards = app.state?.hand;
  const playing = Array.isArray(cards);
  elements["hand-panel"].classList.toggle("hidden", !playing);
  if (!playing) return;

  elements["deck-count"].textContent = `${app.state.deckCount} cards in deck`;
  if (!cards.length) {
    elements["hand-left"].innerHTML = '<span class="card-placeholder">No cards in hand</span>';
    elements["hand-right"].replaceChildren();
    return;
  }

  const cardMarkup = (card, index) => {
    const artwork = cardArtwork(card);
    if (!artwork) {
      return `<button type="button" class="card-thumb card-placeholder" data-card-index="${index}">${escapeHtml(card.title)}</button>`;
    }
    return `<button type="button" class="card-thumb" data-card-index="${index}" title="${escapeHtml(card.title)}">
      <img src="${artwork}" alt="${escapeHtml(card.title)}" draggable="false">
    </button>`;
  };
  elements["hand-left"].innerHTML = cards
    .map((card, index) => index % 2 === 0 ? cardMarkup(card, index) : "").join("");
  elements["hand-right"].innerHTML = cards
    .map((card, index) => index % 2 === 1 ? cardMarkup(card, index) : "").join("");

  elements["hand-panel"].querySelectorAll("[data-card-index]").forEach(button => {
    button.addEventListener("click", () => showCard(cards[Number(button.dataset.cardIndex)]));
  });
}

function showCard(card) {
  const artwork = cardArtwork(card);
  if (!artwork) return;
  elements["card-dialog-image"].src = artwork;
  elements["card-dialog-image"].alt = card.title;
  elements["card-dialog-title"].textContent = card.title;
  elements["card-dialog"].showModal();
}

function renderPlayers() {
  elements.players.innerHTML = app.state.players.map(player => {
    const factionClass = player.faction.startsWith("Marquise") ? "cat" : "eyrie";
    const score = app.state.scores ? app.state.scores[player.faction] : 0;
    return `<div class="player-row ${factionClass}"><b>${score}</b><strong>${escapeHtml(player.name)}</strong><span>${player.faction}</span></div>`;
  }).join("");
}

function renderBoard() {
  const state = app.state;
  const nodes = state.clearings.map(clearing => {
    const [x, y] = positions[clearing.id];
    const catPieces = clearing.pieces["Marquise de Cat"];
    const birdPieces = clearing.pieces["Eyrie Dynasties"];
    const selected = app.selected === clearing.id ? " selected" : "";
    const ruler = clearing.ruler ? clearing.ruler.replace("Marquise de Cat", "Cats").replace("Eyrie Dynasties", "Eyrie") : "No ruler";
    return `<g class="clearing ${clearing.suit}${selected}" data-id="${clearing.id}" transform="translate(${x} ${y})">
      <title>${escapeHtml(clearing.name)} - ${escapeHtml(ruler)}</title>
      <circle class="hit-area" r="48"></circle>
      ${renderPiece("/assets/marquisedecat_warrior.png", catPieces.warriors, -48, -40, "Marquise warriors")}
      ${renderPiece("/assets/eyriedynasties_warrior.png", birdPieces.warriors, 6, -40, "Eyrie warriors")}
      ${renderPiece("/assets/sawmill.png", catPieces.buildings, -48, 8, "Marquise buildings")}
      ${renderPiece("/assets/roost.png", birdPieces.buildings, 6, 8, "Eyrie roosts")}
      ${renderPiece("/assets/keep.png", catPieces.tokens, -21, 8, "Marquise tokens")}
    </g>`;
  });
  elements.board.innerHTML = `<image class="board-map" href="/assets/map_autumn.webp" width="950" height="870"></image><g>${nodes.join("")}</g>`;
  elements.board.querySelectorAll(".clearing").forEach(node => {
    node.addEventListener("click", () => selectClearing(Number(node.dataset.id)));
  });
}

function renderPiece(source, count, x, y, label) {
  if (!count) return "";
  return `<g class="piece" transform="translate(${x} ${y})">
    <image class="piece-art" href="${source}" width="42" height="42"></image>
    <circle class="piece-count-bg" cx="36" cy="5" r="11"></circle>
    <text class="piece-count" x="36" y="9">${count}</text>
    <title>${label}: ${count}</title>
  </g>`;
}

function selectClearing(id) {
  app.selected = id;
  if (app.mode === "battle") elements["battle-clearing"].value = String(id);
  else if (!elements["move-from"].value || elements["move-from"].value === elements["move-to"].value) {
    elements["move-from"].value = String(id);
  } else {
    elements["move-to"].value = String(id);
  }
  renderBoard();
}

function renderClearingOptions() {
  const options = app.state.clearings.map(clearing => `<option value="${clearing.id}">${clearing.name}</option>`).join("");
  for (const id of ["move-from", "move-to", "battle-clearing"]) {
    const current = elements[id].value;
    elements[id].innerHTML = options;
    if (current) elements[id].value = current;
  }
  if (elements["move-to"].value === elements["move-from"].value) elements["move-to"].value = "1";
}

function setControls(enabled, note) {
  elements["move-form"].querySelectorAll("input, select, button").forEach(element => element.disabled = !enabled);
  elements["battle-form"].querySelectorAll("select, button").forEach(element => element.disabled = !enabled);
  elements["control-note"].textContent = note;
}

function setMode(mode) {
  app.mode = mode;
  document.querySelectorAll(".mode").forEach(button => button.classList.toggle("active", button.dataset.mode === mode));
  elements["move-form"].classList.toggle("hidden", mode !== "move");
  elements["battle-form"].classList.toggle("hidden", mode !== "battle");
}

function escapeHtml(value) {
  return String(value).replace(/[&<>'"]/g, character => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", "'": "&#39;", '"': "&quot;"
  })[character]);
}

elements["create-form"].addEventListener("submit", createRoom);
elements["join-form"].addEventListener("submit", joinRoom);
elements["move-form"].addEventListener("submit", event => {
  event.preventDefault();
  sendAction("move", Object.fromEntries(new FormData(event.currentTarget)));
});
elements["battle-form"].addEventListener("submit", event => {
  event.preventDefault();
  sendAction("battle", Object.fromEntries(new FormData(event.currentTarget)));
});
elements["phase-button"].addEventListener("click", () => sendAction("advance_phase"));
elements["leave-button"].addEventListener("click", leaveGame);
elements["room-code-button"].addEventListener("click", async () => {
  await navigator.clipboard?.writeText(app.state.code);
  notify("Room code copied");
});
elements["card-dialog-close"].addEventListener("click", () => elements["card-dialog"].close());
elements["card-dialog"].addEventListener("click", event => {
  if (event.target === elements["card-dialog"]) elements["card-dialog"].close();
});
document.querySelectorAll(".mode").forEach(button => button.addEventListener("click", () => setMode(button.dataset.mode)));
elements["flip-board-button"].addEventListener("click", () => {
  app.boardSide = app.boardSide === "front" ? "back" : "front";
  renderFactionBoard();
});

(async function restoreSession() {
  try {
    const saved = JSON.parse(localStorage.getItem("gout-session"));
    if (!saved?.code || !saved?.token) return;
    app.session = saved;
    const result = await api("/api/state", "GET", saved);
    enterGame(result.state);
  } catch (_) {
    localStorage.removeItem("gout-session");
  }
})();
