const positions = [
  [80, 112], [522, 92], [837, 219], [87, 363], [389, 266], [607, 425],
  [852, 450], [113, 700], [310, 490], [763, 739], [356, 775], [538, 658]
];

const phaseLabels = {
  Setup: "Setup",
  Birdsong: "Sáng sớm",
  Daylight: "Ban ngày",
  Evening: "Buổi tối"
};

const actionLabels = {
  produce_wood: "Produce wood", move: "Move", battle: "Battle", recruit: "Recruit",
  build_sawmill: "Build sawmill", build_workshop: "Build workshop", build_recruiter: "Build recruiter",
  overwork: "Overwork", spend_bird: "Extra action", finish_march: "Finish March", craft: "Craft",
  draw_card: "Draw cards", discard_card: "Discard", choose_eyrie_leader: "Choose leader",
  add_to_decree: "Add to Decree", build_roost: "Build roost", revolt: "Revolt",
  spread_sympathy: "Spread sympathy", mobilize: "Mobilize", train: "Train", organize: "Organize",
  slip: "Slip", explore: "Explore", aid: "Aid", repair: "Repair",
  place_keep: "Place Keep", place_setup_sawmill: "Place sawmill", place_setup_workshop: "Place workshop",
  place_setup_recruiter: "Place recruiter", place_roost: "Place starting roost", new_roost: "A New Roost"
};
const setupLabels = { place_keep: "Đặt Keep", place_buildings: "Đặt building khởi đầu", place_roost: "Đặt Roost khởi đầu", choose_leader: "Chọn leader", choose_character: "Chọn nhân vật", place_pawn: "Đặt pawn vào rừng" };
const setupCatalog = { place_keep: ["place_keep"], place_buildings: ["place_setup_sawmill", "place_setup_workshop", "place_setup_recruiter"], place_roost: ["place_roost"], choose_leader: ["choose_eyrie_leader"] };
const setupMapActions = new Set(["place_keep", "place_roost", "place_setup_sawmill", "place_setup_workshop", "place_setup_recruiter"]);
const phaseCatalog = {
  "Marquise de Cat": { Birdsong: ["produce_wood"], Daylight: ["craft", "move", "battle", "recruit", "build_sawmill", "build_workshop", "build_recruiter", "overwork", "spend_bird", "finish_march"] },
  "Eyrie Dynasties": { Birdsong: ["choose_eyrie_leader", "add_to_decree", "new_roost"], Daylight: ["craft", "recruit", "move", "battle", "build_roost"] },
  "Woodland Alliance": { Birdsong: ["revolt", "spread_sympathy"], Daylight: ["craft", "mobilize", "train"], Evening: ["move", "battle", "recruit", "organize"] },
  Vagabond: { Birdsong: ["slip"], Daylight: ["craft", "move", "battle", "explore", "aid", "repair"] }
};

function mapActionMode() {
  return ["move", "slip", "battle", "build_sawmill", "build_workshop", "build_recruiter", "overwork",
    "build_roost", "new_roost", "revolt", "spread_sympathy", "organize", "explore", "aid", "repair", "recruit",
    "place_keep", "place_roost", "place_setup_sawmill", "place_setup_workshop", "place_setup_recruiter"]
    .includes(app.activeAction) && !(app.activeAction === "recruit" && app.state.you === "Marquise de Cat");
}

function cardActionMode() {
  return ["craft", "spend_bird", "mobilize", "train", "add_to_decree", "discard_card", "overwork"].includes(app.activeAction);
}

function eligibleCard(card) {
  if (!card) return false;
  if (app.activeAction === "overwork") {
    const clearing = app.state.clearings[app.selected];
    return Boolean(clearing && (card.suit === "bird" || card.suit === clearing.suit));
  }
  return (card.actions || []).includes(app.activeAction);
}

function renderPhaseActions() {
  const state = app.state;
  const catalogue = state.phase === "Setup" ? [...(setupCatalog[state.setup.step] || [])]
    : [...(phaseCatalog[state.you]?.[state.phase] || [])];
  if (state.phase === "Evening") catalogue.push("choose_eyrie_leader", "draw_card", "discard_card");
  const actions = [...new Set(catalogue)].filter(action => action !== "choose_eyrie_leader" || state.phase === "Setup" || state.you === "Eyrie Dynasties")
    .filter(action => action !== "finish_march" || state.factionState?.marchMovesRemaining);
  const stage = state.phase === "Setup" ? setupLabels[state.setup.step]
    + (state.setup.step === "place_buildings" ? ` · ${Object.values(state.setup.buildingsPlaced).filter(Boolean).length}/3` : "") : phaseLabels[state.phase];
  elements["phase-actions"].innerHTML = `<strong>${stage}</strong>` + actions.map(action => {
    const available = !app.busy && ((state.phaseActions || []).includes(action) || state.ruleActions?.some(rule => rule.type === action));
    const label = action === "move" && state.you === "Marquise de Cat" ? state.factionState?.marchMovesRemaining ? "March: free move" : "March" : actionLabels[action];
    const cost = state.phase === "Setup" ? undefined : state.factionState?.buildCosts?.[action.replace("build_", "")];
    return `<button type="button" data-phase-action="${action}" aria-pressed="${app.activeAction === action}" ${available ? "" : "disabled"} title="${available ? label : `${label}: không có hành động hợp lệ`}">${label}${cost === undefined ? "" : ` · ${cost} wood`}</button>`;
  }).join("") + (app.activeAction ? '<button type="button" data-phase-action="cancel" aria-label="Cancel action">&times;</button>' : "");
  elements["phase-actions"].querySelectorAll("[data-phase-action]").forEach(button =>
    button.addEventListener("click", () => selectAction(button.dataset.phaseAction)));
}

function selectAction(action) {
  if (app.busy) return;
  if (["aid", "repair", "slip"].includes(action) && app.state.ruleActions?.some(rule => rule.type === action)) {
    openRuleDialog(action);
    return;
  }
  if (action !== "cancel" && !(app.state.phaseActions || []).includes(action)) return;
  app.selected = null;
  app.moveOrigin = null;
  app.selectedCardIndex = null;
  app.activeAction = action === "cancel" || app.activeAction === action ? null : action;
  if (["produce_wood", "draw_card", "finish_march"].includes(action)
      || (action === "recruit" && app.state.you === "Marquise de Cat")) {
    sendAction(action);
    return;
  }
  render();
}

const factionBoards = {
  "Marquise de Cat": {
    front: "/assets/marquisdecat_frontboardgame.webp",
    back: "/assets/marquisdecat_backboardgame.webp"
  },
  "Eyrie Dynasties": {
    front: "/assets/eyriedynasties_frontboardgame.webp",
    back: "/assets/eyriedynasties_backboardgame.webp"
  },
  "Woodland Alliance": {
    front: "/assets/woodlandalliance_frontboardgame.webp",
    back: "/assets/woodlandalliance_backboardgame.webp"
  },
  Vagabond: {
    front: "/assets/vagabond_frontboardgame.webp",
    back: "/assets/vagabond_backboardgame.webp"
  }
};

const factionStyles = {
  "Marquise de Cat": { className: "cat", shortName: "Cats" },
  "Eyrie Dynasties": { className: "eyrie", shortName: "Eyrie" },
  "Woodland Alliance": { className: "alliance", shortName: "Alliance" },
  Vagabond: { className: "vagabond", shortName: "Vagabond" }
};

const pieceStyles = {
  "Marquise de Cat": {
    warrior: "/assets/marquisedecat_warrior.png",
    building: "/assets/sawmill.png",
    token: "/assets/keep.png",
    offset: [-49, -42]
  },
  "Eyrie Dynasties": {
    warrior: "/assets/eyriedynasties_warrior.png",
    building: "/assets/roost.png",
    token: "/assets/roost.png",
    offset: [5, -42]
  },
  "Woodland Alliance": {
    warrior: "/assets/woodlandalliance_warrior.png",
    building: "/assets/sympathy.png",
    token: "/assets/sympathy.png",
    offset: [-49, 8]
  },
  Vagabond: {
    warrior: "/assets/pawn.png",
    building: "/assets/pawn.png",
    token: "/assets/pawn.png",
    offset: [5, 8]
  }
};

const app = {
  session: null,
  state: null,
  selected: null,
  activeAction: null,
  moveOrigin: null,
  moveAction: "move",
  selectedCardIndex: null,
  inspectedPlayer: null,
  pollTimer: null,
  busy: false,
  polling: false,
  cardArt: [],
  cardArtPromise: null,
  boardSide: "front",
  lastBattleRevision: 0,
  rulesFilter: null
};

const elements = Object.fromEntries([
  "lobby", "game", "create-form", "create-name", "join-form", "join-name", "room-code", "room-code-button", "table-status",
  "active-label", "actions-label", "seat-count", "players", "event-text", "board",
  "waiting", "waiting-code", "waiting-actions", "waiting-note", "start-game-button",
  "phase-button", "leave-button", "toast", "hand-panel", "hand-cards", "hand-count",
  "deck-count", "deck-button", "deck-note", "card-dialog", "card-dialog-close",
  "card-dialog-image", "card-dialog-title", "table-view", "faction-board-view",
  "faction-board-canvas", "faction-board-image", "faction-board-pieces",
  "flip-board-button", "map-action", "map-action-close", "map-action-suit",
  "map-action-title", "map-action-summary", "map-action-buttons",
  "battle-button", "move-controls", "move-hint", "warrior-picker", "warrior-count", "warrior-minus",
  "warrior-plus", "warrior-all", "warrior-selected", "cancel-move", "special-primary-button",
  "faction-status", "card-dialog-actions",
  "decree-card-actions", "battle-dialog",
  "battle-dialog-close", "battle-dialog-title", "attacker-name", "attacker-die",
  "attacker-hits", "defender-name", "defender-die", "defender-hits",
  "battle-roll-button", "battle-defender", "eyrie-decree-board", "phase-actions", "apply-card-action", "crafted-items",
  "rules-panel", "rules-dialog", "rules-dialog-close", "rules-dialog-title", "rules-filters", "rule-choice", "rule-preview", "rule-confirm",
  "response-dialog", "response-title", "response-dice", "response-choices"
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
  localStorage.setItem("mike-root-session", JSON.stringify(app.session));
}

function enterGame(state) {
  app.state = state;
  app.lastBattleRevision = state.lastBattle?.revision || 0;
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
      if (app.state) {
        renderHand();
        renderFactionBoard();
      }
    })
    .catch(error => notify(error.message))
    .finally(() => { app.cardArtPromise = null; });
  return app.cardArtPromise;
}

function leaveGame() {
  window.clearInterval(app.pollTimer);
  localStorage.removeItem("mike-root-session");
  app.session = null;
  app.state = null;
  app.selected = null;
  app.activeAction = null;
  app.moveOrigin = null;
  elements.game.classList.add("hidden");
  elements.lobby.classList.remove("hidden");
}

async function createRoom(event) {
  event.preventDefault();
  try {
    const result = await api("/api/rooms", "POST", Object.fromEntries(new FormData(event.currentTarget)));
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

async function startGame() {
  if (!app.session || app.busy) return;
  app.busy = true;
  try {
    const result = await api("/api/rooms/start", "POST", app.session);
    app.state = result.state;
    app.boardSide = "front";
  } catch (error) {
    notify(error.message);
    await refreshState();
  } finally {
    app.busy = false;
    if (app.state) render();
  }
}

function startPolling() {
  window.clearInterval(app.pollTimer);
  app.pollTimer = window.setInterval(refreshState, 800);
}

async function refreshState() {
  if (!app.session || app.busy || app.polling) return;
  app.polling = true;
  const session = app.session;
  try {
    const result = await api("/api/state", "GET", app.session);
    if (app.session !== session || app.busy) return;
    if (!app.state || result.state.revision > app.state.revision) {
      const started = app.state?.status === "waiting" && result.state.status !== "waiting";
      if (started) app.boardSide = "front";
      app.busy = true;
      try {
        await presentState(result.state);
      } finally {
        app.busy = false;
        if (app.state) render();
      }
    }
  } catch (error) {
    window.clearInterval(app.pollTimer);
    notify(error.message);
  } finally {
    app.polling = false;
  }
}

async function sendAction(action, values = {}) {
  if (!app.session || !app.state) return;
  if (app.busy) {
    notify("Đang đồng bộ action trước đó");
    return;
  }
  app.busy = true;
  elements["phase-button"].disabled = true;
  elements["deck-button"].disabled = true;
  renderPhaseActions();
  try {
    const result = await api("/api/action", "POST", {
      ...app.session,
      revision: app.state.revision,
      action,
      ...values
    });
    app.selected = null;
    app.moveOrigin = null;
    app.moveAction = "move";
    app.activeAction = action === "move" && result.state.factionState?.marchMovesRemaining > 0 ? "move" : null;
    await presentState(result.state);
  } catch (error) {
    notify(error.message);
    app.busy = false;
    app.moveOrigin = null;
    app.moveAction = "move";
    await refreshState();
    if (app.state) render();
  } finally {
    app.busy = false;
    if (app.state) render();
  }
}

function render() {
  const state = app.state;
  elements["room-code-button"].textContent = `Room ${state.code}`;
  elements["waiting-code"].textContent = state.code;
  elements["event-text"].textContent = state.lastEvent;
  elements["seat-count"].textContent = `${state.players.length}/${state.maxPlayers}`;
  renderPlayers();
  renderFactionStatus();
  const crafted = Object.entries(state.craftedItems || {}).filter(([, count]) => count > 0);
  elements["crafted-items"].classList.toggle("hidden", !crafted.length || state.status === "waiting");
  elements["crafted-items"].innerHTML = '<strong>Crafted items</strong>' + crafted.map(([item, count]) =>
    `<span>${item === "coins" ? "Coins" : `<img src="/assets/${escapeHtml(item)}.png" alt="${escapeHtml(item)}">`}<b>${count}</b></span>`).join("");

  const waiting = state.status === "waiting";
  elements.waiting.classList.toggle("hidden", !waiting);
  elements["waiting-actions"].classList.toggle("hidden", !waiting);
  elements["start-game-button"].classList.toggle("hidden", !state.isHost);
  elements["start-game-button"].disabled = state.players.length < 2;
  elements["waiting-note"].textContent = state.isHost
    ? state.players.length < 2 ? "Cần ít nhất 2 người chơi" : "Sẵn sàng random faction base"
    : "Đang đợi host bắt đầu";

  if (waiting) {
    elements["rules-panel"].classList.add("hidden");
    if (elements["response-dialog"].open) elements["response-dialog"].close();
    elements["phase-actions"].replaceChildren();
    elements["table-status"].textContent = "Waiting room";
    elements["active-label"].textContent = "Chưa bắt đầu";
    elements["actions-label"].textContent = `${state.players.length} người trong phòng`;
    document.querySelectorAll("[data-phase]").forEach(marker => marker.classList.remove("active"));
    elements["phase-button"].classList.add("hidden");
    elements["hand-panel"].classList.add("hidden");
    elements["faction-board-view"].classList.add("hidden");
    elements["faction-status"].classList.add("hidden");
    renderWaitingBoard();
    closeMapAction();
    return;
  }

  const activePlayer = state.players.find(player => player.faction === state.activeFaction);
  const yourTurn = state.activeFaction === state.you && !state.winner;
  const phaseLabel = phaseLabels[state.phase] || state.phase;
  elements["table-status"].textContent = `${phaseLabel} · ${activePlayer?.name || state.activeFaction}`;
  elements["active-label"].textContent = activePlayer?.name || state.activeFaction;
  const actionText = state.phase === "Setup"
    ? setupLabels[state.setup.step]
    : state.you === "Marquise de Cat" && state.phase === "Birdsong"
    ? state.factionState.woodProduced ? "Đã tạo wood" : "Wood chưa được tạo"
    : state.phase === "Evening" && state.factionState?.needsLeaderAfterTurmoil
      ? "Chưa chọn leader mới"
    : state.phase === "Evening" && state.you !== "Woodland Alliance"
      ? state.canDraw ? "Chưa rút bài" : state.hand.length > 5 ? `${state.hand.length - 5} lá cần bỏ` : "Hoàn tất buổi tối"
    : state.you === "Vagabond" && state.phase === "Daylight"
    ? "Dùng item để hành động"
    : state.you === "Vagabond" && state.phase === "Birdsong"
      ? state.ruleActions?.some(rule => rule.type === "refresh") ? "Refresh item"
        : state.ruleActions?.some(rule => rule.type === "slip") ? "Slip hoặc sang ban ngày" : "Sẵn sàng sang ban ngày"
    : state.you === "Eyrie Dynasties" && state.phase === "Birdsong"
      ? "Chọn leader · xếp Decree"
    : state.you === "Woodland Alliance" && state.phase === "Birdsong"
      ? "Supporters · Revolt · Sympathy"
      : `${state.actionsRemaining} action còn lại${state.factionState?.marchMovesRemaining ? " · 1 move miễn phí" : ""}`;
  elements["actions-label"].textContent = state.winner
    ? `${state.winner}${state.coalition === state.winner ? " + Vagabond" : ""} thắng`
    : yourTurn ? `${actionText} · ${state.phase === "Setup" ? "Setup của bạn" : "Lượt của bạn"}`
      : state.phase === "Setup" ? `${setupLabels[state.setup.step]} · ${activePlayer?.name || state.activeFaction}` : state.activeFaction;
  document.querySelectorAll("[data-phase]").forEach(marker => {
    marker.classList.toggle("active", marker.dataset.phase === state.phase);
  });

  elements["phase-button"].classList.toggle("hidden", state.phase === "Setup");
  elements["phase-button"].disabled = app.busy || !yourTurn || !state.canAdvance;
  const mustTurmoil = state.you === "Eyrie Dynasties" && state.phase === "Daylight"
    && state.actionsRemaining > 0 && state.canAdvance;
  elements["phase-button"].textContent = mustTurmoil ? "Turmoil"
    : state.phase === "Evening" ? "End turn" : "Next phase";
  elements["hand-panel"].classList.remove("hidden");
  elements["faction-board-view"].classList.remove("hidden");
  if (!(state.phaseActions || []).includes(app.activeAction)
      && !state.ruleActions?.some(rule => rule.type === app.activeAction)) app.activeAction = null;
  if (state.phase === "Setup" && yourTurn && !app.busy && !app.activeAction) {
    const availableSetupActions = (state.phaseActions || []).filter(action => setupMapActions.has(action));
    if (availableSetupActions.length === 1) app.activeAction = availableSetupActions[0];
  }
  if (!mapActionMode() || (app.selected !== null
      && !clearingActions(state.clearings[app.selected]).includes(app.activeAction))) {
    app.selected = null;
    app.moveOrigin = null;
  }
  renderPhaseActions();
  renderFactionStatus();
  renderBoard();
  renderMapAction();
  renderHand();
  renderFactionBoard();
  renderRules();
  renderResponse();
}

const ruleLabels = { character: "Character", pawn: "Start in forest", refresh: "Refresh items", slip: "Slip",
  forest_move: "Move from forest", aid: "Aid", repair: "Repair", strike: "Strike", quest: "Quests",
  special: "Character action", discard_item: "Item capacity", effect: "Crafted effects", dominance: "Dominance",
  take_dominance: "Take Dominance", bonus_battle: "Command Warren", bonus_move: "Cobbler", allied_move: "Move with Ally" };

function renderRules() {
  const state = app.state;
  const rules = state.ruleActions || [];
  const types = [...new Set(rules.map(rule => rule.type))];
  const inventory = state.you === "Vagabond" ? state.inventory || [] : [];
  const cards = state.persistentCards || [];
  elements["rules-panel"].classList.toggle("hidden", !types.length && !inventory.length && !cards.length);
  elements["rules-panel"].innerHTML = `<div class="rule-tools">${types.map(type =>
    `<button type="button" data-rule-type="${type}">${ruleLabels[type] || type}</button>`).join("")}</div>`
    + (state.character && state.you === "Vagabond" ? `<button class="character-art" data-character-art="${state.character.toLowerCase()}" title="${state.character}"><img src="/assets/cards/vagabond/${state.character.toLowerCase()}.png" alt="${state.character}"></button>` : "")
    + `<div class="inventory">${inventory.map(item => `<button type="button" class="inventory-item${item.damaged ? " damaged" : ""}${item.exhausted ? " exhausted" : ""}" data-item-id="${item.id}" title="${item.type} #${item.id}: ${item.damaged ? "damaged" : item.exhausted ? "exhausted" : "ready"}">
      ${item.type === "coins" ? '<span>Coins</span>' : `<img src="/assets/${item.type}.png" alt="${item.type}">`}<small>#${item.id} ${item.damaged ? "Damaged" : item.exhausted ? "Exhausted" : "Ready"}</small></button>`).join("")}</div>`
    + `<div class="persistent-cards">${cards.map((card, index) => `<button type="button" data-effect-art="${index}">${escapeHtml(card.title)}</button>`).join("")}</div>`;
  elements["rules-panel"].querySelectorAll("[data-rule-type]").forEach(button => button.addEventListener("click", () => openRuleDialog(button.dataset.ruleType)));
  elements["rules-panel"].querySelectorAll("[data-item-id]").forEach(button => button.addEventListener("click", () => {
    const item = Number(button.dataset.itemId);
    const action = rules.find(rule => rule.item === item && ["refresh", "repair", "discard_item"].includes(rule.type));
    if (action) openRuleDialog(action.type, null, item);
  }));
  elements["rules-panel"].querySelectorAll("[data-effect-art]").forEach(button => button.addEventListener("click", () => {
    const card = cards[Number(button.dataset.effectArt)];
    showArtifact(cardArtwork(card) || "/assets/card_back.webp", card.title);
  }));
  elements["rules-panel"].querySelectorAll("[data-character-art]").forEach(button => button.addEventListener("click", () =>
    showArtifact(`/assets/cards/vagabond/${button.dataset.characterArt}.png`, state.character)));
  if (elements["rules-dialog"].open) renderRuleOptions();
}

function openRuleDialog(type, clearing = null, item = null) {
  app.rulesFilter = { type, clearing, item };
  const rules = (app.state.ruleActions || []).filter(rule => rule.type === type && (clearing === null || rule.clearing === clearing));
  if (!rules.length) return;
  if (type === "slip") app.activeAction = "slip";
  elements["rules-dialog-title"].textContent = ruleLabels[type] || type;
  elements["rules-filters"].innerHTML = type === "aid" ? ["target", "card", "item", "option"].map(key => {
    const values = [...new Set(rules.map(rule => String(rule[key])))];
    return `<label>${{ target: "Faction", card: "Card", item: "Exhaust item", option: "Take item" }[key]}<select data-rule-filter="${key}">${values.map(value => {
      const label = key === "card" ? app.state.hand[Number(value)]?.title : key === "item" ? `${app.state.inventory.find(unit => unit.id === Number(value))?.type} #${value}` : value || "None";
      return `<option value="${escapeHtml(value)}">${escapeHtml(label || value)}</option>`;
    }).join("")}</select></label>`;
  }).join("") : "";
  elements["rules-filters"].querySelectorAll("select").forEach(select => select.addEventListener("change", renderRuleOptions));
  elements["rule-choice"].value = "";
  renderRuleOptions();
  if (type === "pawn" || type === "slip") renderBoard();
  if (elements["battle-dialog"].open) elements["battle-dialog"].close();
  if (!elements["rules-dialog"].open) elements["rules-dialog"].showModal();
}

function renderRuleOptions() {
  if (!app.rulesFilter) return;
  const filter = app.rulesFilter;
  const fields = Array.from(elements["rules-filters"].querySelectorAll("select"));
  const rules = (app.state.ruleActions || []).filter(rule => rule.type === filter.type
    && (filter.clearing === null || rule.clearing === filter.clearing)
    && (filter.item === null || rule.item === filter.item)
    && fields.every(select => String(rule[select.dataset.ruleFilter]) === select.value));
  const previous = elements["rule-choice"].value;
  elements["rule-choice"].innerHTML = rules.map(rule => `<option value="${rule.index}">${escapeHtml(rule.label)}</option>`).join("");
  if (rules.some(rule => String(rule.index) === previous)) elements["rule-choice"].value = previous;
  else elements["rule-choice"].value = rules.length ? String(rules[0].index) : "";
  elements["rule-confirm"].disabled = !rules.length || app.busy;
  renderRulePreview();
}

function renderRulePreview() {
  const rule = (app.state.ruleActions || []).find(action => String(action.index) === elements["rule-choice"].value);
  let art = "";
  if (rule?.type === "character") art = `/assets/cards/vagabond/${rule.option.toLowerCase()}.png`;
  if (rule?.type === "quest") {
    const quest = app.state.quests.find(entry => Number(entry.id) === rule.card);
    if (quest) art = `/assets/cards/quests/${quest.art}.png`;
  }
  if (rule?.type === "aid") art = cardArtwork(app.state.hand[rule.card]);
  elements["rule-preview"].innerHTML = (art ? `<img src="${art}" alt="">` : "") + (rule ? `<p>${escapeHtml(rule.label)}</p>` : "");
}

function renderResponse() {
  const response = app.state.response;
  if (!response || response.faction !== app.state.you) {
    if (elements["response-dialog"].open) elements["response-dialog"].close();
    if (response) elements["actions-label"].textContent = `${response.faction} đang phản hồi`;
    return;
  }
  for (const id of ["rules-dialog", "card-dialog", "battle-dialog"]) if (elements[id].open) elements[id].close();
  elements["response-title"].textContent = response.title;
  const battle = app.state.pendingBattle;
  elements["response-dice"].innerHTML = battle && battle.attackerRoll >= 0
    ? `<span>${escapeHtml(battle.attacker)} <b>${battle.attackerRoll}</b></span><span>${escapeHtml(battle.defender)} <b>${battle.defenderRoll}</b></span>` : "";
  elements["response-choices"].innerHTML = response.choices.map(choice =>
    `<button type="button" data-response="${escapeHtml(choice.value)}" ${app.busy ? "disabled" : ""}>${escapeHtml(choice.label)}</button>`).join("");
  elements["response-choices"].querySelectorAll("button").forEach(button => button.addEventListener("click", () => sendAction("respond", { choice: button.dataset.response })));
  if (!elements["response-dialog"].open) elements["response-dialog"].showModal();
}

function renderWaitingBoard() {
  elements.board.innerHTML = '<image class="board-map waiting-map" href="/assets/map_autumn.webp" width="950" height="870"></image>';
}

function boardPiece(source, left, top, label) {
  return `<img class="faction-piece" src="${source}" alt="${label}" style="left:${left}%;top:${top}%">`;
}

function renderFactionBoard() {
  const faction = app.state?.you;
  const board = factionBoards[faction];
  if (!board) {
    elements["faction-board-view"].classList.add("hidden");
    return;
  }

  const side = app.boardSide;
  elements["faction-board-image"].src = board[side];
  elements["faction-board-image"].alt = `${faction} board ${side}`;
  elements["flip-board-button"].textContent = side === "front" ? "Show back" : "Show front";
  elements["eyrie-decree-board"].classList.toggle("hidden", side === "back" || faction !== "Eyrie Dynasties");

  if (side === "back") {
    elements["faction-board-pieces"].replaceChildren();
    return;
  }

  if (faction === "Marquise de Cat") {
    const slots = [50.4, 58.4, 66.4, 74.6, 82.8, 90.8];
    const rows = [
      ["/assets/sawmill.png", 43.2, "Sawmill"],
      ["/assets/workshop.png", 53.5, "Workshop"],
      ["/assets/recruiter.png", 64, "Recruiter"]
    ];
    elements["faction-board-pieces"].innerHTML = rows.flatMap(([source, top, label]) =>
      slots.slice(app.state.factionState?.buildingCounts?.[label.toLowerCase()] || 0)
        .map(left => boardPiece(source, left, top, label))
    ).join("");
  } else if (faction === "Eyrie Dynasties") {
    const eyrie = app.state.factionState;
    const roostsOnMap = app.state.clearings.reduce((sum, clearing) =>
      sum + clearing.pieces[faction].buildings, 0);
    const roostSlots = [50.2, 57.2, 64.2, 71.2, 78.4, 85.4, 92.8];
    const leader = eyrie.leader ? `<button type="button" class="leader-board-card" title="${eyrie.leader}">
      <img src="/assets/cards/eyrie/leader-${eyrie.leader.toLowerCase()}.png" alt="${eyrie.leader} leader">
    </button>` : "";
    elements["faction-board-pieces"].innerHTML = roostSlots.slice(roostsOnMap)
      .map(left => boardPiece("/assets/roost.png", left, 45.2, "Roost")).join("") + leader;
    elements["faction-board-pieces"].querySelector(".leader-board-card")?.addEventListener("click", () =>
      showArtifact(`/assets/cards/eyrie/leader-${eyrie.leader.toLowerCase()}.png`, `${eyrie.leader} leader`));
    elements["eyrie-decree-board"].innerHTML = Object.entries(eyrie.decree).map(([column, cards]) =>
      `<div class="board-decree-column${eyrie.currentColumn === column && app.state.phase === "Daylight" ? " current" : ""}">
        <strong>${column}</strong><div>${cards.map((card, index) => {
          const source = card.title === "Loyal Vizier" ? "/assets/cards/eyrie/loyal-vizier-1.png" : cardArtwork(card);
          return `<button type="button" class="decree-art-card${card.resolved ? " resolved" : ""}" data-column="${column}" data-index="${index}" title="${escapeHtml(card.title)}">
            ${source ? `<img src="${source}" alt="${escapeHtml(card.title)}">` : `<span>${escapeHtml(card.title)}</span>`}
            <small class="${card.suit}">${card.suit}${card.resolved ? " - done" : ""}</small>
          </button>`;
        }).join("")}</div></div>`).join("");
    elements["eyrie-decree-board"].querySelectorAll("[data-column]").forEach(button => {
      button.addEventListener("click", () => {
        const card = eyrie.decree[button.dataset.column][Number(button.dataset.index)];
        const source = card.title === "Loyal Vizier" ? "/assets/cards/eyrie/loyal-vizier-1.png" : cardArtwork(card);
        showArtifact(source || "/assets/card_back.webp", card.title);
      });
    });
  } else if (faction === "Woodland Alliance") {
    const alliance = app.state.factionState;
    const bases = ["fox", "mouse", "rabbit"].flatMap((suit, index) => {
      const placed = app.state.clearings.some(clearing => clearing.suit === suit && clearing.pieces[faction].buildings > 0);
      return placed ? [] : [boardPiece(`/assets/${suit}_base.png`, 44 + index * 7, 60.3, `${suit} base`)];
    });
    const sympathy = Array.from({length:10}, (_, index) => index).slice(alliance.sympathyPlaced)
      .map(index => boardPiece("/assets/sympathy.png", 31.5 + index * 7, index % 2 ? 83 : 87.5, "Sympathy"));
    const officers = Array.from({length:alliance.officers}, (_, index) =>
      boardPiece("/assets/woodlandalliance_warrior.png", 74 + (index % 4) * 5, 59 + Math.floor(index / 4) * 3.5, "Officer"));
    elements["faction-board-pieces"].innerHTML = [...bases, ...sympathy, ...officers].join("");
  } else if (faction === "Vagabond") {
    let satchel = 0;
    let damaged = 0;
    const tracks = {tea:0,coins:0,bag:0};
    const items = (app.state.inventory || []).map(item => {
      let left;
      let top;
      if (!item.damaged && !item.exhausted && item.type in tracks && tracks[item.type] < 3) {
        left = 23.5 + tracks[item.type]++ * 5.8;
        top = {tea:30,coins:82,bag:91}[item.type];
      } else {
        const index = item.damaged ? damaged++ : satchel++;
        left = item.damaged ? 44.3 + (index % 10) * 2.4 : 43.5 + (index % 7) * 3.8;
        top = item.damaged ? 90 + Math.floor(index / 10) * 3.2 : 74 + Math.floor(index / 7) * 3.5;
      }
      return `<span class="vagabond-board-item${item.exhausted ? " exhausted" : ""}${item.damaged ? " damaged" : ""}" style="left:${left}%;top:${top}%" title="${item.type} #${item.id}">
        ${item.type === "coins" ? '<b>Coins</b>' : `<img src="/assets/${item.type}.png" alt="${item.type}${item.damaged ? " damaged" : item.exhausted ? " exhausted" : " ready"}">`}</span>`;
    });
    const character = app.state.character ? `<button type="button" class="character-board-card" title="${app.state.character}">
      <img src="/assets/cards/vagabond/${app.state.character.toLowerCase()}.png" alt="${app.state.character}"></button>` : "";
    elements["faction-board-pieces"].innerHTML = items.join("") + character;
    elements["faction-board-pieces"].querySelector(".character-board-card")?.addEventListener("click", () =>
      showArtifact(`/assets/cards/vagabond/${app.state.character.toLowerCase()}.png`, app.state.character));
  } else {
    elements["faction-board-pieces"].replaceChildren();
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
  if (!Array.isArray(cards)) return;
  elements["hand-count"].textContent = `${cards.length} card${cards.length === 1 ? "" : "s"}`;
  elements["deck-count"].textContent = app.state.deckCount;
  elements["deck-button"].disabled = app.busy || !app.state.canDraw;
  elements["deck-button"].classList.toggle("ready", app.state.canDraw);
  elements["deck-note"].textContent = app.state.canDraw ? "Rút bài" : `${app.state.deckCount} lá còn lại`;

  if (!cards.length) {
    elements["hand-cards"].innerHTML = '<span class="card-placeholder">No cards in hand</span>';
    return;
  }

  elements["hand-cards"].innerHTML = cards.map((card, index) => {
    const artwork = cardArtwork(card);
    const selectable = !cardActionMode() || eligibleCard(card);
    const attributes = `${selectable ? "" : "disabled"} class="card-thumb${cardActionMode() && selectable ? " action-ready" : ""}"`;
    if (!artwork) {
      return `<button type="button" ${attributes} data-card-index="${index}">${escapeHtml(card.title)}</button>`;
    }
    return `<button type="button" ${attributes} data-card-index="${index}" title="${escapeHtml(card.title)}">
      <img src="${artwork}" alt="${escapeHtml(card.title)}" draggable="false">
    </button>`;
  }).join("");

  elements["hand-cards"].querySelectorAll("[data-card-index]").forEach(button => {
    button.addEventListener("click", () => {
      const index = Number(button.dataset.cardIndex);
      showCard(cards[index], index);
    });
  });
}

function showCard(card, index) {
  if (cardActionMode() && !eligibleCard(card)) return;
  const artwork = cardArtwork(card);
  app.selectedCardIndex = index;
  elements["card-dialog-image"].src = artwork || "/assets/card_back.webp";
  elements["card-dialog-image"].alt = card.title;
  elements["card-dialog-title"].textContent = card.title;
  const eyrieBirdsong = app.activeAction === "add_to_decree" && eligibleCard(card);
  const applyCard = cardActionMode() && eligibleCard(card) && !eyrieBirdsong;
  elements["decree-card-actions"].classList.toggle("hidden", !eyrieBirdsong);
  elements["apply-card-action"].classList.toggle("hidden", !applyCard);
  elements["apply-card-action"].textContent = actionLabels[app.activeAction] || "";
  elements["card-dialog-actions"].classList.toggle("hidden", !applyCard && !eyrieBirdsong);
  const birdBlocked = card.suit === "bird" && app.state.factionState?.birdAdded;
  elements["decree-card-actions"].querySelectorAll("button").forEach(button => {
    button.disabled = Boolean(birdBlocked);
  });
  elements["card-dialog"].showModal();
}

function showArtifact(source, title) {
  app.selectedCardIndex = null;
  elements["card-dialog-image"].src = source;
  elements["card-dialog-image"].alt = title;
  elements["card-dialog-title"].textContent = title;
  elements["card-dialog-actions"].classList.add("hidden");
  if (!elements["card-dialog"].open) elements["card-dialog"].showModal();
}

function renderPlayers() {
  const state = app.state;
  elements.players.innerHTML = state.players.map((player, index) => {
    const style = factionStyles[player.faction] || { className: "unassigned", shortName: "Chưa chia faction" };
    const active = player.faction && player.faction === state.activeFaction;
    const inspected = app.inspectedPlayer === index;
    const score = player.faction && state.scores ? state.scores[player.faction] : "-";
    return `<button type="button" class="player-row ${style.className}${active ? " active" : ""}${inspected ? " expanded" : ""}" data-player-index="${index}" aria-pressed="${inspected}">
      <b>${score}</b>
      <strong>${escapeHtml(player.name)}${player.host ? '<em>Host</em>' : ""}</strong>
      <span>${escapeHtml(player.faction || "Chưa chia faction")}</span>
      <small>${state.victoryConditions?.[player.faction] ? player.faction === "Vagabond" ? `Coalition: ${escapeHtml(state.coalition)}` : `${state.victoryConditions[player.faction]} Dominance` : active ? `${phaseLabels[state.phase]} · ${state.phase === "Setup" ? "đang setup" : "đang đi lượt"}` : player.faction ? style.shortName : "Đang chờ"}</small>
    </button>`;
  }).join("");
  elements.players.querySelectorAll("[data-player-index]").forEach(button => {
    button.addEventListener("click", () => {
      const index = Number(button.dataset.playerIndex);
      app.inspectedPlayer = app.inspectedPlayer === index ? null : index;
      renderPlayers();
    });
  });
}

function renderFactionStatus() {
  const status = app.state?.factionState;
  if (!status || status.type === "standard") {
    elements["faction-status"].classList.add("hidden");
    return;
  }
  elements["faction-status"].classList.remove("hidden");
  if (status.type === "cat") {
    elements["faction-status"].innerHTML = `
      <div class="faction-stat-heading"><strong>Marquise de Cat</strong><span>${status.wood}/8 wood</span></div>
      ${Object.entries(status.buildingCounts).map(([type, count]) => `<div class="faction-stat-row"><span>${escapeHtml(type)}</span><b>${count}/6</b></div>`).join("")}
      <div class="faction-stat-row"><span>${app.state.phase === "Setup" ? "Setup" : "Birdsong"}</span><b>${app.state.phase === "Setup" ? `${Object.values(status.buildingCounts).reduce((sum, count) => sum + count, 0)}/3 buildings` : status.woodProduced ? "Wood produced" : "Produce wood"}</b></div>
      ${status.marchMovesRemaining ? '<div class="faction-stat-row"><span>March</span><b>1 free move</b></div>' : ""}`;
    return;
  }
  if (status.type === "eyrie") {
    if (!status.leader) {
      if (app.state.phase === "Setup" && app.state.setup.step !== "choose_leader") {
        elements["faction-status"].innerHTML = '<div class="faction-stat-heading"><strong>Eyrie Dynasties</strong><span>Setup</span></div><div class="faction-stat-row"><span>Starting roost</span><b>Chưa đặt</b></div>';
        return;
      }
      elements["faction-status"].innerHTML = `
        <div class="faction-stat-heading"><strong>Choose leader</strong><span>${phaseLabels[app.state.phase]}</span></div>
        <div class="leader-options">
          ${["Commander", "Despot", "Builder", "Charismatic"].map(leader =>
            `<button type="button" data-eyrie-leader="${leader}" ${app.activeAction !== "choose_eyrie_leader" || status.deposedLeaders.includes(leader) ? "disabled" : ""}>
              <img src="/assets/cards/eyrie/leader-${leader.toLowerCase()}.png" alt="">${leader}
            </button>`).join("")}
        </div>`;
      elements["faction-status"].querySelectorAll("[data-eyrie-leader]").forEach(button => {
        button.addEventListener("click", () => sendAction("choose_eyrie_leader", {
          leader: button.dataset.eyrieLeader
        }));
      });
      return;
    }
    const columns = ["Recruit", "Move", "Battle", "Build"];
    elements["faction-status"].innerHTML = `
      <div class="faction-stat-heading"><strong>${escapeHtml(status.leader)}</strong><span>${status.cardsAdded}/2 added</span></div>
      <div class="decree-grid">
        ${columns.map(column => `<div class="decree-column"><strong>${column}</strong><div class="decree-cards">
          ${status.decree[column].map(card => `<span class="decree-chip ${card.suit}${card.resolved ? " resolved" : ""}" title="${escapeHtml(card.title)}">${card.suit}</span>`).join("")}
        </div></div>`).join("")}
      </div>
      <p class="decree-note">${app.state.phase === "Daylight" ? `Resolve: ${status.currentColumn || "Complete"}` : `${status.cardsAdded}/2 cards added · ${status.birdAdded ? "Bird used" : "Bird available"}`}</p>`;
    return;
  }
  if (status.type === "alliance") {
    const supporters = status.supporters.length
      ? status.supporters.map(card => `<span class="supporter ${card.suit}" title="${escapeHtml(card.title)}">${card.suit}</span>`).join("")
      : '<span class="empty-status">No supporters</span>';
    elements["faction-status"].innerHTML = `
      <div class="faction-stat-heading"><strong>Alliance network</strong><span>${status.sympathyPlaced}/10 sympathy</span></div>
      <div class="supporter-list">${supporters}</div>
      <div class="faction-stat-row"><span>Supporters</span><b>${status.supporters.length}</b></div>
      <div class="faction-stat-row"><span>Officers</span><b>${status.officers}</b></div>`;
    return;
  }

  const items = status.items;
  const itemRows = [
    ["Boot", items.boots, items.exhaustedBoots],
    ["Sword", items.swords, items.exhaustedSwords],
    ["Torch", items.torches, items.exhaustedTorches],
    ["Hammer", items.hammers, items.exhaustedHammers],
    ["Tea", items.tea, items.exhaustedTea]
  ].filter(([, total]) => total > 0).map(([name, total, exhausted]) =>
    `<div class="item-row"><span>${name}</span><b>${total - exhausted}/${total}</b></div>`
  ).join("");
  const relationships = Object.entries(status.relationships).map(([faction, level]) =>
    `<span>${factionStyles[faction]?.shortName || faction}: ${level < 0 ? "Hostile" : level}</span>`
  ).join("");
  elements["faction-status"].innerHTML = `
    <div class="faction-stat-heading"><strong>Satchel</strong><span>${items.damaged} damaged</span></div>
    <div class="item-list">${itemRows}</div>
    <div class="relationship-list">${relationships || "Indifferent"}</div>`;
}

function renderBoard() {
  const state = app.state;
  const origin = app.moveOrigin === null ? null : state.clearings[app.moveOrigin];
  const nodes = state.clearings.map(clearing => {
    const [x, y] = positions[clearing.id];
    const selected = app.selected === clearing.id ? " selected" : "";
    const from = app.moveOrigin === clearing.id ? " origin" : "";
    const available = origin
      ? clearing.id === origin.id || isLegalMoveDestination(origin, clearing, app.moveAction)
      : mapActionMode() && (clearingActions(clearing).includes(app.activeAction)
        || (state.you === "Vagabond" && app.activeAction === "slip"
          && state.ruleActions?.some(rule => rule.type === "slip" && rule.clearing === clearing.id)));
    const legal = origin && clearing.id !== origin.id && available ? " legal" : "";
    const availability = available ? " actionable" : " unavailable";
    const ruler = clearing.ruler ? factionStyles[clearing.ruler]?.shortName || clearing.ruler : "No ruler";
    const pieces = Object.entries(pieceStyles).flatMap(([faction, style]) => {
      const counts = clearing.pieces[faction];
      if (!counts) return [];
      const [xOffset, yOffset] = style.offset;
      if (faction === "Marquise de Cat") {
        return [renderPiece(style.warrior, counts.warriors, xOffset, yOffset, "Cats warriors", true),
          ...Object.entries(clearing.catBuildings || {}).map(([type, count], index) =>
            renderPiece(`/assets/${type}.png`, count, xOffset + index * 27, yOffset + 36, type)),
          renderPiece("/assets/wood.png", clearing.wood, xOffset, yOffset + 68, "Wood"),
          renderPiece("/assets/keep.png", clearing.keep, xOffset + 28, yOffset + 68, "Keep")];
      }
      return [
        renderPiece(style.warrior, counts.warriors, xOffset, yOffset, `${faction} warriors`, true),
        renderPiece(faction === "Woodland Alliance" ? `/assets/${clearing.suit}_base.png` : style.building, counts.buildings, xOffset, yOffset + 39, `${faction} buildings`),
        renderPiece(style.token, counts.tokens, xOffset + 28, yOffset + 39, `${faction} tokens`)
      ];
    }).join("");
    const actions = origin ? [] : clearingActions(clearing);
    return `<g class="clearing ${clearing.suit}${selected}${from}${legal}${availability}" data-id="${clearing.id}" aria-disabled="${!available}" transform="translate(${x} ${y})">
      <title>${escapeHtml(clearing.name)} - ${escapeHtml(ruler)}${actions.length ? ` - ${actions.join(", ")}` : ""}</title>
      <circle class="hit-area" r="50"></circle>
      ${pieces}
    </g>`;
  });
  const forestNodes = (app.state.forests || []).map(forest => {
    const points = forest.clearings.map(id => positions[id]);
    const x = points.reduce((sum, point) => sum + point[0], 0) / points.length;
    const y = points.reduce((sum, point) => sum + point[1], 0) / points.length;
    const rule = (app.state.ruleActions || []).find(action => action.clearing === forest.id
      && ((action.type === "pawn" && app.rulesFilter?.type === "pawn") || (action.type === "slip" && app.activeAction === "slip")));
    const pawn = app.state.vagabondLocation === forest.id ? renderPiece("/assets/pawn.png", 1, -20, -20, "Vagabond") : "";
    return `<g class="forest${rule ? " available" : ""}" transform="translate(${x} ${y})" ${rule ? `data-rule-index="${rule.index}"` : ""}>
      <title>Forest ${forest.id - 11}</title><circle r="30"></circle><text y="42">F${forest.id - 11}</text>${pawn}</g>`;
  }).join("");
  elements.board.innerHTML = `<image class="board-map" href="/assets/map_autumn.webp" width="950" height="870"></image><g>${forestNodes}${nodes.join("")}</g>`;
}

function renderPiece(source, count, x, y, label, warrior = false) {
  if (!count) return "";
  const countMarker = warrior
    ? `<rect class="warrior-count-bg" x="17" y="-10" width="44" height="23" rx="5"></rect>
      <text class="warrior-count" x="39" y="7">&#215;${count}</text>`
    : `<circle class="piece-count-bg" cx="34" cy="5" r="10"></circle>
      <text class="piece-count" x="34" y="9">${count}</text>`;
  return `<g class="piece${warrior ? " warrior-piece" : ""}" transform="translate(${x} ${y})">
    <image class="piece-art" href="${source}" width="40" height="40"></image>
    ${countMarker}
    <title>${label}: ${count}</title>
  </g>`;
}

function enemyPieceCount(clearing) {
  return Object.keys(clearing.pieces)
    .filter(faction => faction !== app.state.you)
    .reduce((sum, faction) => sum + totalPieces(clearing, faction), 0);
}

function isLegalMoveDestination(origin, target, action = "move") {
  const targets = action === "slip" ? origin.slipTargets : origin.moveTargets;
  return (targets || []).includes(target.id);
}

function clearingActions(clearing) {
  return clearing.actions || [];
}

function selectClearing(id) {
  if (app.busy || !mapActionMode() || !app.state.clearings[id]) return;
  if (app.state.you === "Vagabond" && app.activeAction === "slip") {
    const rule = (app.state.ruleActions || []).find(action => action.type === "slip" && action.clearing === id);
    if (rule) sendAction("rule_action", { index: rule.index });
    else notify("Clearing không hợp lệ cho Slip");
    return;
  }
  if (app.moveOrigin !== null && id !== app.moveOrigin) {
    const origin = app.state.clearings[app.moveOrigin];
    if (!isLegalMoveDestination(origin, app.state.clearings[id], app.moveAction)) {
      notify("Clearing này không phải đích di chuyển hợp lệ");
      return;
    }
    if (app.moveAction === "slip") sendAction("slip", { to: id });
    else {
      sendAction("move", {
        from: app.moveOrigin,
        to: id,
        warriors: app.state.you === "Vagabond" ? 1 : elements["warrior-count"].value
      });
    }
    return;
  }
  if (app.moveOrigin === id) {
    cancelMove();
    return;
  }
  const actions = clearingActions(app.state.clearings[id]);
  if (!actions.includes(app.activeAction)) {
    notify(`Clearing không hợp lệ cho ${actionLabels[app.activeAction]}`);
    return;
  }
  if (app.state.phase === "Setup" && setupMapActions.has(app.activeAction)) {
    sendAction(app.activeAction, { clearing: id });
    return;
  }
  app.selected = app.selected === id ? null : id;
  if (app.selected !== null && ["move", "slip"].includes(app.activeAction)) {
    beginMove(app.activeAction);
    return;
  }
  renderBoard();
  renderMapAction();
  renderHand();
}

function totalPieces(clearing, faction) {
  const pieces = clearing.pieces[faction];
  return pieces ? pieces.warriors + pieces.buildings + pieces.tokens : 0;
}

function configureSpecialButton(button, action, label, enabled = true) {
  button.classList.remove("hidden");
  button.dataset.action = action;
  button.textContent = label;
  button.disabled = !enabled;
}

function renderMapAction() {
  if (app.selected === null && app.moveOrigin === null) {
    elements["map-action"].classList.add("hidden");
    return;
  }

  const id = app.moveOrigin ?? app.selected;
  const clearing = app.state.clearings[id];
  const ownPieces = clearing.pieces[app.state.you] || { warriors: 0, buildings: 0, tokens: 0 };
  const enemyPieces = enemyPieceCount(clearing);
  const actions = clearingActions(clearing);
  const defenders = clearing.battleTargets || [];
  elements["battle-defender"].classList.toggle("hidden", defenders.length < 2);
  elements["battle-defender"].innerHTML = defenders.map(faction =>
    `<option value="${escapeHtml(faction)}">${escapeHtml(faction)}</option>`).join("");
  elements["map-action"].classList.remove("hidden");
  elements["map-action-suit"].textContent = `${clearing.suit} clearing · ${clearing.ruler || "No ruler"}`;
  elements["map-action-title"].textContent = clearing.name;
  const ownPieceName = app.state.you === "Vagabond" ? "pawn" : "warrior";
  elements["map-action-summary"].textContent = `${ownPieces.warriors} ${ownPieceName} · ${enemyPieces} enemy piece`;
  const primary = elements["special-primary-button"];
  primary.classList.add("hidden");
  delete primary.dataset.action;

  const faction = app.state.you;
  elements["battle-button"].classList.toggle("hidden", app.activeAction !== "battle");
  elements["battle-button"].disabled = false;

  if (mapActionMode() && actions.includes(app.activeAction)
      && !["move", "slip", "battle", "overwork"].includes(app.activeAction)) {
    configureSpecialButton(primary, app.activeAction, actionLabels[app.activeAction]);
  }
  if (app.activeAction === "overwork") {
    elements["map-action-summary"].textContent = `Overwork · ${clearing.suit} · ${clearing.wood || 0} wood`;
  }

  elements["map-action-buttons"].classList.toggle("hidden", app.moveOrigin !== null);
  elements["move-controls"].classList.toggle("hidden", app.moveOrigin === null);

  if (app.moveOrigin !== null) {
    const fixedPawnMove = faction === "Vagabond";
    elements["warrior-picker"].classList.toggle("hidden", fixedPawnMove);
    elements["warrior-count"].max = fixedPawnMove ? 1 : ownPieces.warriors;
    const selectedWarriors = normalizeWarriorCount();
    elements["warrior-selected"].textContent = `${selectedWarriors} / ${elements["warrior-count"].max}`;
    const marchMove = faction === "Marquise de Cat"
      ? app.state.factionState?.marchMovesRemaining ? "March · move 2/2" : "March · move 1/2"
      : actionLabels[app.moveAction];
    elements["move-hint"].textContent = fixedPawnMove
      ? "Slip · clearing đích được đánh dấu"
      : `${marchMove} · clearing đích được đánh dấu`;
    if (!fixedPawnMove) {
      elements["map-action-summary"].textContent = `${selectedWarriors} / ${ownPieces.warriors} warrior đã chọn`;
    }
  }
}

function beginMove(action = "move") {
  if (app.selected === null) return;
  app.moveOrigin = app.selected;
  app.moveAction = action;
  elements["warrior-count"].value = "1";
  renderBoard();
  renderMapAction();
}

function cancelMove() {
  app.selected = null;
  app.moveOrigin = null;
  app.moveAction = "move";
  renderBoard();
  renderMapAction();
  renderHand();
}

function closeMapAction() {
  app.selected = null;
  app.moveOrigin = null;
  app.moveAction = "move";
  elements["map-action"].classList.add("hidden");
  if (app.state && app.state.status !== "waiting") { renderBoard(); renderHand(); }
}

function stepWarriors(delta) {
  const input = elements["warrior-count"];
  input.value = normalizeWarriorCount() + delta;
  renderMapAction();
}

function normalizeWarriorCount() {
  const input = elements["warrior-count"];
  const minimum = Number(input.min) || 1;
  const maximum = Number(input.max) || 25;
  const requested = Number.isFinite(Number(input.value)) ? Math.floor(Number(input.value)) : minimum;
  input.value = Math.min(maximum, Math.max(minimum, requested));
  return Number(input.value);
}

function selectAllWarriors() {
  elements["warrior-count"].value = elements["warrior-count"].max;
  renderMapAction();
}

function openBattle() {
  if (app.selected === null || app.busy) return;
  const clearing = app.state.clearings[app.selected];
  if (!clearingActions(clearing).includes("battle")) return;
  elements["battle-dialog"].dataset.clearing = clearing.id;
  elements["battle-dialog"].dataset.defender = elements["battle-defender"].value || clearing.battleTargets[0];
  fillBattleDialog({ clearing: clearing.id, attacker: app.state.you,
    defender: elements["battle-dialog"].dataset.defender });
  elements["battle-roll-button"].classList.remove("hidden");
  elements["battle-roll-button"].disabled = false;
  elements["battle-dialog-close"].disabled = false;
  elements["battle-dialog"].showModal();
}

function fillBattleDialog(battle, state = app.state) {
  const clearing = state.clearings.find(entry => entry.id === battle.clearing);
  elements["battle-dialog-title"].textContent = clearing?.name || `Clearing ${battle.clearing}`;
  elements["attacker-name"].textContent = battle.attacker;
  elements["defender-name"].textContent = battle.defender;
  elements["attacker-die"].textContent = battle.rolled === false ? "-" : battle.attackerRoll ?? "?";
  elements["defender-die"].textContent = battle.rolled === false ? "-" : battle.defenderRoll ?? "?";
  elements["attacker-hits"].textContent = battle.hitsToDefender === undefined ? "Attacker" : `${battle.hitsToDefender} hit dealt`;
  elements["defender-hits"].textContent = battle.hitsToAttacker === undefined ? "Defender" : `${battle.hitsToAttacker + (battle.ambushHits || 0)} hit dealt${battle.ambushHits ? " (Ambush)" : ""}`;
}

async function presentState(state) {
  const battle = state.lastBattle;
  if (battle && battle.revision > app.lastBattleRevision) {
    app.lastBattleRevision = battle.revision;
    fillBattleDialog({ ...battle, attackerRoll: undefined, defenderRoll: undefined,
      hitsToDefender: undefined, hitsToAttacker: undefined }, state);
    elements["battle-roll-button"].classList.add("hidden");
    elements["battle-dialog-close"].disabled = true;
    if (elements["card-dialog"].open) elements["card-dialog"].close();
    if (!elements["battle-dialog"].open) elements["battle-dialog"].showModal();
    const dice = [elements["attacker-die"], elements["defender-die"]];
    if (battle.rolled !== false) {
      dice.forEach(die => die.classList.add("rolling"));
      await new Promise(resolve => window.setTimeout(resolve, 750));
    }
    dice.forEach(die => die.classList.remove("rolling"));
    fillBattleDialog(battle, state);
    elements["battle-dialog-close"].disabled = false;
  }
  const changedTurn = app.state?.phase !== state.phase || app.state?.activeFaction !== state.activeFaction;
  if (changedTurn) {
    app.activeAction = null;
    app.selected = null;
    app.moveOrigin = null;
    app.moveAction = "move";
  }
  app.state = state;
  render();
}

function escapeHtml(value) {
  return String(value).replace(/[&<>'"]/g, character => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", "'": "&#39;", '"': "&quot;"
  })[character]);
}

const lobbyInputs = [elements["create-name"], elements["join-name"], elements["room-code"]];
const joinInputs = [elements["join-name"], elements["room-code"]];
let lobbyEditingInput = null;
let lobbyViewportHeight = window.visualViewport?.height || window.innerHeight || 0;
let lobbyKeyboardOpen = false;

function keepLobbyInputVisible(input) {
  if (!input?.scrollIntoView) return;
  const scroll = () => {
    if (document.activeElement !== input || lobbyEditingInput !== input || elements.lobby.classList.contains("hidden")) return;
    input.scrollIntoView({ block: "center", inline: "nearest" });
    const viewport = window.visualViewport;
    if (!viewport || !input.getBoundingClientRect || !window.scrollBy) return;
    const bounds = input.getBoundingClientRect();
    const visibleTop = viewport.offsetTop + 16;
    const visibleBottom = viewport.offsetTop + viewport.height - 24;
    const adjustment = bounds.bottom > visibleBottom ? bounds.bottom - visibleBottom
      : bounds.top < visibleTop ? bounds.top - visibleTop : 0;
    if (adjustment) window.scrollBy({ top: adjustment, left: 0, behavior: "auto" });
  };
  const schedule = callback => window.requestAnimationFrame ? window.requestAnimationFrame(callback) : callback();
  schedule(scroll);
  for (const delay of [120, 320]) window.setTimeout(() => schedule(scroll), delay);
}

function syncKeyboardViewport() {
  const viewport = window.visualViewport;
  const viewportHeight = viewport?.height || window.innerHeight || lobbyViewportHeight;
  const input = lobbyInputs.includes(document.activeElement) ? document.activeElement : null;
  // Some mobile browsers keep the input focused after the keyboard is dismissed.
  const keyboardOpen = Boolean(input && lobbyViewportHeight - viewportHeight > 100);
  const keyboardClosed = lobbyKeyboardOpen && !keyboardOpen;
  lobbyKeyboardOpen = keyboardOpen;
  if (!input || keyboardClosed || elements.lobby.classList.contains("hidden")) {
    lobbyEditingInput = null;
    elements.lobby.classList.remove("joining");
    document.documentElement?.style?.setProperty("--keyboard-inset", "0px");
    lobbyViewportHeight = viewportHeight;
    return;
  }
  const keyboardInset = viewport
    ? Math.max(0, (window.innerHeight || viewport.height) - viewport.height - viewport.offsetTop) : 0;
  document.documentElement?.style?.setProperty("--keyboard-inset", `${Math.round(keyboardInset)}px`);
  if (lobbyEditingInput === input) keepLobbyInputVisible(input);
}

lobbyInputs.forEach(input => {
  input.addEventListener("focus", () => {
    if (!lobbyEditingInput) lobbyViewportHeight = window.visualViewport?.height || window.innerHeight || 0;
    lobbyEditingInput = input;
    elements.lobby.classList.toggle("joining", joinInputs.includes(input));
    syncKeyboardViewport();
  });
  input.addEventListener("blur", () => window.setTimeout(syncKeyboardViewport, 0));
});
window.visualViewport?.addEventListener("resize", syncKeyboardViewport);
window.addEventListener?.("resize", syncKeyboardViewport);

elements["create-form"].addEventListener("submit", createRoom);
elements["join-form"].addEventListener("submit", joinRoom);
elements["start-game-button"].addEventListener("click", startGame);
elements["phase-button"].addEventListener("click", () => sendAction("advance_phase"));
elements["deck-button"].addEventListener("click", () => sendAction("draw_card"));
elements.board.addEventListener("click", event => {
  const forest = event.target.closest?.(".forest[data-rule-index]");
  if (forest && !app.busy) { sendAction("rule_action", { index: forest.dataset.ruleIndex }); return; }
  const clearing = event.target.closest?.(".clearing");
  if (clearing) selectClearing(Number(clearing.dataset.id));
});
elements["battle-button"].addEventListener("click", openBattle);
elements["battle-roll-button"].addEventListener("click", async () => {
  elements["battle-roll-button"].disabled = true;
  await sendAction("battle", {
    clearing: elements["battle-dialog"].dataset.clearing,
    defender: elements["battle-dialog"].dataset.defender
  });
  elements["battle-roll-button"].disabled = false;
});
elements["special-primary-button"].addEventListener("click", () => {
  const action = elements["special-primary-button"].dataset.action;
  if (!action || action !== app.activeAction || app.selected === null) return;
  if (["aid", "repair"].includes(action)) { openRuleDialog(action, app.selected); return; }
  sendAction(action, { clearing: app.selected });
});
elements["cancel-move"].addEventListener("click", cancelMove);
elements["map-action-close"].addEventListener("click", closeMapAction);
elements["warrior-minus"].addEventListener("click", () => stepWarriors(-1));
elements["warrior-plus"].addEventListener("click", () => stepWarriors(1));
elements["warrior-all"].addEventListener("click", selectAllWarriors);
elements["warrior-count"].addEventListener("input", renderMapAction);
elements["leave-button"].addEventListener("click", leaveGame);
elements["room-code-button"].addEventListener("click", async () => {
  await navigator.clipboard?.writeText(app.state.code);
  notify("Room code copied");
});
elements["card-dialog-close"].addEventListener("click", () => elements["card-dialog"].close());
elements["apply-card-action"].addEventListener("click", () => {
  const card = app.state.hand[app.selectedCardIndex];
  if (!card || !cardActionMode() || !eligibleCard(card)) return;
  elements["card-dialog"].close();
  sendAction(app.activeAction, { card: app.selectedCardIndex,
    ...(app.activeAction === "overwork" ? { clearing: app.selected } : {}) });
});
elements["card-dialog"].addEventListener("click", event => {
  if (event.target === elements["card-dialog"]) elements["card-dialog"].close();
});
elements["decree-card-actions"].querySelectorAll("[data-decree-column]").forEach(button => {
  button.addEventListener("click", () => {
    if (app.activeAction !== "add_to_decree" || app.selectedCardIndex === null
        || !eligibleCard(app.state.hand[app.selectedCardIndex])) return;
    elements["card-dialog"].close();
    sendAction("add_to_decree", {
      card: app.selectedCardIndex,
      column: button.dataset.decreeColumn
    });
  });
});
elements["battle-dialog-close"].addEventListener("click", () => elements["battle-dialog"].close());
elements["battle-dialog"].addEventListener("click", event => {
  if (event.target === elements["battle-dialog"] && !app.busy) elements["battle-dialog"].close();
});
elements["battle-dialog"].addEventListener("cancel", event => {
  if (app.busy) event.preventDefault();
});
elements["flip-board-button"].addEventListener("click", () => {
  app.boardSide = app.boardSide === "front" ? "back" : "front";
  renderFactionBoard();
});
elements["rules-dialog-close"].addEventListener("click", () => elements["rules-dialog"].close());
elements["rule-choice"].addEventListener("change", renderRulePreview);
elements["rule-confirm"].addEventListener("click", () => {
  if (elements["rule-confirm"].disabled || elements["rule-choice"].value === "") return;
  const index = elements["rule-choice"].value;
  elements["rules-dialog"].close();
  sendAction("rule_action", { index });
});
elements["response-dialog"].addEventListener("cancel", event => event.preventDefault());

(async function restoreSession() {
  try {
    const saved = JSON.parse(localStorage.getItem("mike-root-session"));
    if (!saved?.code || !saved?.token) return;
    app.session = saved;
    const result = await api("/api/state", "GET", saved);
    enterGame(result.state);
  } catch (_) {
    localStorage.removeItem("mike-root-session");
  }
})();
