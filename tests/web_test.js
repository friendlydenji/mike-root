const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const html = fs.readFileSync('web/index.html', 'utf8');
const nodes = new Map();
for (const match of html.matchAll(/\bid="([^"]+)"/g)) {
  const classes = new Set();
  nodes.set(match[1], {
    classList: {
      add: name => classes.add(name),
      remove: name => classes.delete(name),
      contains: name => classes.has(name),
      toggle(name, force) {
        const enabled = force ?? !classes.has(name);
        if (enabled) classes.add(name); else classes.delete(name);
      }
    },
    dataset: {}, listeners: {}, textContent: '', innerHTML: '', value: '1', open: false, scrollOptions: null,
    addEventListener(name, handler) { this.listeners[name] = handler; },
    scrollIntoView(options) { this.scrollOptions = options; },
    querySelectorAll() { return []; }, querySelector() { return null; },
    replaceChildren() { this.innerHTML = ''; },
    showModal() { this.open = true; }, close() { this.open = false; }
  });
}

const requests = [];
const viewportListeners = {};
const context = vm.createContext({
  document: { getElementById: id => nodes.get(id), querySelectorAll: () => [], activeElement: null },
  localStorage: { getItem: () => null, setItem() {}, removeItem() {} },
  window: {
    clearTimeout() {}, clearInterval() {}, setInterval() {}, setTimeout: callback => setTimeout(callback, 0),
    requestAnimationFrame: callback => callback(),
    visualViewport: { addEventListener: (name, handler) => { viewportListeners[name] = handler; } }
  },
  URLSearchParams,
  navigator: {},
  fetch: async (url, options) => {
    requests.push(Object.fromEntries(options.body));
    return { ok: true, json: async () => ({ ok: true, state: vm.runInContext('app.state', context) }) };
  }
});
vm.runInContext(fs.readFileSync('web/app.js', 'utf8'), context);

nodes.get('join-name').listeners.focus();
assert.equal(nodes.get('join-name').scrollOptions?.block, 'center',
  'Focusing a lobby field must reveal it above the mobile keyboard');
assert.equal(nodes.get('join-name').scrollOptions?.inline, 'nearest');
context.document.activeElement = nodes.get('room-code');
viewportListeners.resize();
assert.equal(nodes.get('room-code').scrollOptions?.block, 'center',
  'Keyboard viewport changes must keep the active lobby field visible');
assert.equal(nodes.get('room-code').scrollOptions?.inline, 'nearest');

const run = code => vm.runInContext(code, context);
const plain = value => JSON.parse(JSON.stringify(value));
const faction = 'Marquise de Cat';
const clearings = [0, 1, 2].map(id => ({
  id, name: `Clearing ${id}`, suit: 'rabbit', ruler: faction, neighbors: [0, 1, 2].filter(other => other !== id),
  pieces: { [faction]: { warriors: id === 0 ? 2 : 0, buildings: 0, tokens: 0 } },
  actions: id === 0 ? ['move'] : [], moveTargets: id === 0 ? [1] : [], slipTargets: [], battleTargets: []
}));
context.stateFixture = {
  code: 'TEST12', revision: 3, status: 'playing', isHost: true, maxPlayers: 4,
  you: faction, activeFaction: faction, phase: 'Daylight', actionsRemaining: 3,
  players: [{ name: 'Host', faction, host: true }], factionState: { type: 'standard' },
  hand: [], deckCount: 48, scores: { [faction]: 0 }, clearings, canAdvance: true, lastEvent: '', winner: null,
  phaseActions: ['move']
};
run('app.state = stateFixture; app.session = { code: "TEST12", token: "test" }; render();');
assert.ok(nodes.get('special-primary-button').listeners.click, 'Map confirmation must be registered');
assert.ok(nodes.get('flip-board-button').listeners.click, 'A missing action ID must not prevent the flip listener');
assert.match(nodes.get('board').innerHTML, /warrior-piece/, 'Warrior stacks need a distinct count marker');
assert.match(nodes.get('board').innerHTML, /&#215;2/, 'The map must show the full warrior count');

nodes.get('flip-board-button').listeners.click();
assert.match(nodes.get('faction-board-image').src, /backboardgame/);
nodes.get('flip-board-button').listeners.click();
assert.match(nodes.get('faction-board-image').src, /frontboardgame/);

run('selectClearing(2)');
assert.equal(run('app.selected'), null, 'An unavailable clearing must not become selected');
run('selectClearing(0)');
assert.equal(run('app.selected'), null, 'Map must stay inactive until an action is chosen');
run('selectAction("move"); selectClearing(0); selectClearing(2)');
assert.equal(run('app.moveOrigin'), 0, 'An invalid destination must preserve the current move');
assert.equal(requests.length, 0, 'An invalid destination must not reach the server');
run('selectClearing(0)');
assert.equal(run('app.moveOrigin'), null, 'Clicking the origin must cancel the move');

(async () => {
  run('selectClearing(0)');
  assert.equal(nodes.get('warrior-count').max, 2, 'Move quantity must match warriors at the origin');
  assert.equal(nodes.get('warrior-selected').textContent, '1 / 2');
  nodes.get('warrior-all').listeners.click();
  assert.equal(nodes.get('warrior-count').value, 2, 'All must select every warrior at the origin');
  assert.equal(nodes.get('warrior-selected').textContent, '2 / 2');
  assert.match(nodes.get('move-hint').textContent, /move 1\/2/, 'Cats must see which March move they are choosing');
  run('selectClearing(1)');
  await new Promise(resolve => setTimeout(resolve, 10));
  assert.equal(requests.length, 1);
  assert.equal(requests[0].action, 'move');
  assert.equal(requests[0].to, '1');
  assert.equal(requests[0].warriors, '2', 'The selected warrior quantity must reach the server');

  run('app.state.phaseActions = ["move", "build_sawmill", "overwork", "spend_bird"]; app.state.clearings[1].actions = ["build_sawmill"];');
  run('selectAction("move"); selectClearing(0); selectAction("build_sawmill")');
  assert.equal(run('app.moveOrigin'), null, 'Switching actions must exit movement');
  run('selectClearing(0)');
  assert.equal(run('app.selected'), null, 'A move clearing must not be selectable for Build');
  run('selectClearing(1)');
  assert.equal(run('app.selected'), 1);
  assert.equal(nodes.get('special-primary-button').dataset.action, 'build_sawmill');
  run('selectAction("cancel"); selectClearing(1)');
  assert.equal(run('app.selected'), null, 'Cancel must deactivate the map');
  run('app.state.hand = [{ title: "Bird", suit: "bird", actions: ["spend_bird"] }, { title: "Mouse", suit: "mouse", actions: [] }]; selectAction("spend_bird")');
  assert.match(nodes.get('hand-cards').innerHTML, /action-ready/);
  assert.equal(run('eligibleCard(app.state.hand[0])'), true);
  assert.equal(run('eligibleCard(app.state.hand[1])'), false);
  run('showCard(app.state.hand[1], 1)');
  assert.equal(nodes.get('card-dialog').open, false, 'Ineligible cards must not open an action dialog');
  run('app.state.clearings[0].actions.push("overwork"); selectAction("overwork")');
  assert.equal(run('eligibleCard(app.state.hand[0])'), false, 'Overwork first needs a sawmill clearing');
  run('selectClearing(0)');
  assert.equal(run('eligibleCard(app.state.hand[0])'), true);
  assert.equal(run('eligibleCard(app.state.hand[1])'), false, 'Overwork filters cards to the clearing suit or bird');
  run('selectAction("cancel")');

  context.battleFixture = { revision: 4, clearing: 1, attacker: faction, defender: 'Vagabond',
    attackerRoll: 3, defenderRoll: 1, hitsToDefender: 2, hitsToAttacker: 1 };
  await run('presentState({ ...app.state, revision: 4, lastBattle: battleFixture })');
  assert.equal(nodes.get('battle-dialog').open, true);
  assert.equal(nodes.get('attacker-die').textContent, 3);
  assert.equal(nodes.get('defender-die').textContent, 1);
  assert.equal(run('app.lastBattleRevision'), 4);
  nodes.get('battle-dialog').close();
  await run('presentState({ ...app.state })');
  assert.equal(nodes.get('battle-dialog').open, false, 'Polling must not reopen the same battle');
  assert.equal(plain(run('app.state')).revision, 4);

  await run('presentState({ ...app.state, revision: 5, lastBattle: { ...battleFixture, revision: 5, rolled: false, ambushHits: 2, hitsToDefender: 0, hitsToAttacker: 0 } })');
  assert.equal(nodes.get('attacker-die').textContent, '-', 'An Ambush-ended battle must not show invented dice');
  assert.match(nodes.get('defender-hits').textContent, /2 hit dealt.*Ambush/);
  nodes.get('battle-dialog').close();

  run('app.state.ruleActions = [{ index: 0, type: "character", label: "Thief", option: "Thief", clearing: -1, item: -1 }, { index: 1, type: "character", label: "Ranger", option: "Ranger", clearing: -1, item: -1 }]; openRuleDialog("character");');
  assert.match(nodes.get('rule-preview').innerHTML, /vagabond\/thief.png/);
  run('elements["rule-choice"].value = "1"; renderRulePreview();');
  assert.match(nodes.get('rule-preview').innerHTML, /vagabond\/ranger.png/);
  nodes.get('rules-dialog').close();
  run('app.state.ruleActions = []; app.state.response = { faction: app.state.you, title: "Ambush?", choices: [{value:"pass",label:"Pass"}] }; app.busy = true; renderResponse();');
  assert.match(nodes.get('response-choices').innerHTML, /disabled/);
  run('app.busy = false; renderResponse();');
  assert.doesNotMatch(nodes.get('response-choices').innerHTML, /disabled/, 'Response controls must unlock after synchronization');
  run('app.state.response = null; renderResponse();');
  assert.equal(nodes.get('response-dialog').open, false);

  context.fetch = async (_url, options) => options.method === 'POST'
    ? { ok: false, json: async () => ({ ok: false, error: 'Game state changed; refresh and try again' }) }
    : { ok: true, json: async () => ({ ok: true, state: { ...context.stateFixture, revision: 5 } }) };
  run('app.moveOrigin = 0; app.selected = 0');
  await run('sendAction("move", { from: 0, to: 1, warriors: 1 })');
  assert.equal(run('app.state.revision'), 5, 'An action conflict must fetch the latest server state');
  assert.equal(run('app.busy'), false, 'A failed action must not lock later interactions');
  assert.equal(run('app.moveOrigin'), null, 'A failed move must exit destination selection');

  context.setupFixture = { ...context.stateFixture, status: 'setup', phase: 'Setup',
    phaseActions: ['place_keep'], canAdvance: false,
    setup: { step: 'place_keep', keep: null, buildingsPlaced: { sawmill: false, workshop: false, recruiter: false } },
    clearings: context.stateFixture.clearings.map(clearing => ({ ...clearing,
      actions: clearing.id === 0 || clearing.id === 2 ? ['place_keep'] : [] })) };
  context.fetch = async (_url, options) => {
    requests.push(Object.fromEntries(options.body));
    return { ok: true, json: async () => ({ ok: true, state: { ...context.setupFixture, revision: 6 } }) };
  };
  run('app.state = { ...stateFixture, status: "waiting" }; app.activeAction = null; app.busy = false;');
  await run('startGame()');
  assert.equal(run('app.busy'), false, 'Starting the game must release the synchronization lock');
  assert.equal(run('app.activeAction'), 'place_keep', 'Starting setup must immediately enable Keep placement');
  assert.doesNotMatch(nodes.get('phase-actions').innerHTML, /disabled/, 'Keep placement must not stay disabled after Start game');
  run('app.state = setupFixture; app.activeAction = null; app.selected = null; render();');
  assert.equal(run('app.activeAction'), 'place_keep', 'A sole setup placement must be selected automatically');
  assert.equal(nodes.get('phase-button').classList.contains('hidden'), true, 'Setup cannot be skipped with Next phase');
  assert.match(nodes.get('phase-actions').innerHTML, /Place Keep/);
  run('selectClearing(1)');
  assert.equal(run('app.selected'), null, 'Setup must reject a non-corner before selection');
  run('selectClearing(2)');
  await new Promise(resolve => setTimeout(resolve, 10));
  assert.equal(requests.at(-1).action, 'place_keep', 'Clicking a legal corner must place the Keep directly');
  assert.equal(requests.at(-1).clearing, '2');
  run('app.state = { ...setupFixture, activeFaction: "Eyrie Dynasties", phaseActions: [] }; app.activeAction = null; render(); selectClearing(0)');
  assert.equal(run('app.activeAction'), null, 'A player who is waiting for Cats must not enter Keep placement mode');
  assert.equal(requests.at(-1).clearing, '2', 'A waiting player must not send a setup action');

  context.roostFixture = { ...context.setupFixture, revision: 7, you: 'Eyrie Dynasties',
    activeFaction: 'Eyrie Dynasties', phaseActions: ['place_roost'],
    setup: { ...context.setupFixture.setup, step: 'place_roost', keep: 0 },
    factionState: { type: 'eyrie', leader: '', deposedLeaders: [],
      decree: { Recruit: [], Move: [], Battle: [], Build: [] } },
    clearings: context.setupFixture.clearings.map(clearing => ({ ...clearing,
      pieces: { ...clearing.pieces, 'Eyrie Dynasties': { warriors: 0, buildings: 0, tokens: 0 } },
      actions: clearing.id === 2 ? ['place_roost'] : [] })) };
  context.fetch = async (_url, options) => {
    if (options.method === 'GET') {
      return { ok: true, json: async () => ({ ok: true, state: context.roostFixture }) };
    }
    requests.push(Object.fromEntries(options.body));
    return { ok: true, json: async () => ({ ok: true, state: context.roostFixture }) };
  };
  run('app.state = { ...roostFixture, revision: 6, activeFaction: "Marquise de Cat", phaseActions: [] }; app.activeAction = null; app.busy = false; app.polling = false;');
  await run('refreshState()');
  assert.equal(run('app.busy'), false, 'Polling a setup handoff must release the synchronization lock');
  assert.equal(run('app.activeAction'), 'place_roost', 'Eyrie must enter Roost placement after Cats finish setup');
  assert.doesNotMatch(nodes.get('phase-actions').innerHTML, /disabled/, 'Roost placement must not stay disabled after polling');
  const requestsBeforeInvalidRoost = requests.length;
  run('selectClearing(1)');
  assert.equal(requests.length, requestsBeforeInvalidRoost, 'A non-opposite clearing must not send Roost placement');
  run('selectClearing(2)');
  await new Promise(resolve => setTimeout(resolve, 10));
  assert.equal(requests.at(-1).action, 'place_roost', 'Clicking the opposite corner must place the Roost directly');
  assert.equal(requests.at(-1).clearing, '2');
  await run('presentState({ ...app.state, phase: "Birdsong", status: "playing", phaseActions: ["produce_wood"], canAdvance: false })');
  assert.equal(run('app.activeAction'), null, 'Finishing setup clears the setup action mode');
  assert.equal(run('app.selected'), null);
  assert.equal(nodes.get('phase-button').classList.contains('hidden'), false);
  console.log('All web interaction regression tests passed.');
})().catch(error => { console.error(error); process.exitCode = 1; });
