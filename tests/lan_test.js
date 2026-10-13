const assert = require('node:assert/strict');

const base = process.argv[2] || 'http://127.0.0.1:8081';
const players = new Map();
let code;
let state;
let battles = 0;
let responses = 0;

async function request(path, values, method = 'POST') {
  const query = new URLSearchParams(values);
  const response = await fetch(`${base}${path}${method === 'GET' ? `?${query}` : ''}`, {
    method, headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: method === 'GET' ? undefined : query
  });
  const result = await response.json();
  return { status: response.status, ...result };
}

async function read(faction) {
  const result = await request('/api/state', { code, token: players.get(faction) }, 'GET');
  assert.equal(result.status, 200);
  return result.state;
}

async function act(faction, action, values = {}) {
  const current = await read(faction);
  const result = await request('/api/action', { code, token: players.get(faction), revision: current.revision, action, ...values });
  assert.equal(result.status, 200, `${faction} / ${action}: ${result.error}`);
  state = result.state;
  return state;
}

async function settle() {
  let safety = 0;
  while (state.response) {
    assert.ok(++safety < 80);
    const actor = state.response.faction;
    const own = await read(actor);
    const other = await read([...players.keys()].find(faction => faction !== actor));
    assert.equal(other.response.choices.length, 0, 'Response choices must remain private');
    const invalid = await request('/api/action', { code, token: players.get(actor), revision: own.revision, action: 'respond', choice: 'invalid-choice' });
    assert.equal(invalid.status, 409);
    assert.equal((await read(actor)).revision, own.revision, 'Invalid responses must not mutate the room');
    const choice = own.response.choices.find(choice => choice.value === 'pass') || own.response.choices[0];
    await act(actor, 'respond', { choice: choice.value });
    ++responses;
  }
}

(async () => {
  const created = await request('/api/rooms', { name: 'LAN Host' });
  assert.equal(created.status, 201);
  code = created.state.code;
  const tokens = [created.token];
  for (let index = 1; index < 4; ++index) {
    const joined = await request('/api/rooms/join', { code, name: `LAN Player ${index + 1}` });
    assert.equal(joined.state.you, null);
    assert.equal(joined.state.status, 'waiting');
    tokens.push(joined.token);
  }
  const started = await request('/api/rooms/start', { code, token: tokens[0] });
  assert.equal(started.status, 200);
  state = started.state;
  for (const token of tokens) {
    const viewer = await request('/api/state', { code, token }, 'GET');
    players.set(viewer.state.you, token);
  }
  assert.equal(players.size, 4);
  await act('Marquise de Cat', 'place_keep', { clearing: 0 });
  await act('Marquise de Cat', 'place_setup_sawmill', { clearing: 0 });
  await act('Marquise de Cat', 'place_setup_workshop', { clearing: 1 });
  await act('Marquise de Cat', 'place_setup_recruiter', { clearing: 3 });
  await act('Eyrie Dynasties', 'place_roost', { clearing: 9 });
  await act('Eyrie Dynasties', 'choose_eyrie_leader', { leader: 'Builder' });
  assert.equal(state.setup.step, 'choose_character');
  let vag = await read('Vagabond');
  await act('Vagabond', 'rule_action', { index: vag.ruleActions.find(rule => rule.type === 'character' && rule.option === 'Ranger').index });
  vag = await read('Vagabond');
  await act('Vagabond', 'rule_action', { index: vag.ruleActions.find(rule => rule.type === 'pawn' && rule.clearing === 12).index });
  assert.equal(state.phase, 'Birdsong');

  const failedRevision = state.revision;
  const rejected = await request('/api/action', { code, token: players.get(state.activeFaction), revision: failedRevision, action: 'move', from: 999, to: 0, warriors: 1 });
  assert.notEqual(rejected.status, 200);
  assert.equal((await read(state.activeFaction)).revision, failedRevision);

  const visited = new Set();
  for (let turn = 0; turn < 16 && !state.winner; ++turn) {
    const faction = state.activeFaction;
    visited.add(faction);
    state = await read(faction);
    if (faction === 'Marquise de Cat') await act(faction, 'produce_wood');
    if (faction === 'Eyrie Dynasties') await act(faction, 'add_to_decree', { card: 0, column: 'Battle' });
    if (faction === 'Vagabond') {
      for (let safety = 0; safety < 20; ++safety) {
        state = await read(faction);
        const refresh = state.ruleActions.find(rule => rule.type === 'refresh');
        if (!refresh) break;
        await act(faction, 'rule_action', { index: refresh.index });
      }
      state = await read(faction);
      const slip = state.ruleActions.find(rule => rule.type === 'slip' && rule.clearing < 12);
      if (slip) await act(faction, 'rule_action', { index: slip.index });
    }
    await act(faction, 'advance_phase');
    state = await read(faction);
    if (faction === 'Marquise de Cat' && battles === 0) {
      const origin = state.clearings[6];
      if (origin.moveTargets.includes(9)) {
        await act(faction, 'move', { from: 6, to: 9, warriors: 1 });
        await act(faction, 'battle', { clearing: 9, defender: 'Eyrie Dynasties' });
        await settle();
        ++battles;
        assert.ok(state.lastBattle);
      }
    }
    if (faction === 'Eyrie Dynasties') {
      for (let safety = 0; safety < 30; ++safety) {
        state = await read(faction);
        if (state.canAdvance) break;
        const next = state.clearings.find(clearing => clearing.actions.some(action => ['recruit', 'move', 'battle', 'build_roost'].includes(action)));
        assert.ok(next, 'The Decree must either have a legal action or allow Turmoil');
        const action = next.actions.find(action => ['recruit', 'move', 'battle', 'build_roost'].includes(action));
        await act(faction, action, action === 'move' ? { from: next.id, to: next.moveTargets[0], warriors: 1 }
          : action === 'battle' ? { clearing: next.id, defender: next.battleTargets[0] } : { clearing: next.id });
        await settle();
      }
    }
    if (state.winner) break;
    await act(faction, 'advance_phase');
    state = await read(faction);
    if (state.factionState.needsLeaderAfterTurmoil) {
      const leader = ['Builder', 'Commander', 'Despot', 'Charismatic'].find(leader => !state.factionState.deposedLeaders.includes(leader));
      await act(faction, 'choose_eyrie_leader', { leader });
    }
    await act(faction, 'draw_card');
    state = await read(faction);
    while (state.hand.length > 5) await act(faction, 'discard_card', { card: 0 });
    await act(faction, 'advance_phase');
  }
  assert.equal(visited.size, 4);
  assert.ok(battles > 0 && responses > 0);
  console.log(`LAN integration passed: four-player setup, 16 turns, ${responses} responses, rollback and private choices.`);
})().catch(error => { console.error(error); process.exitCode = 1; });
