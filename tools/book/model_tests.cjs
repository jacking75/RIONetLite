'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const packet = require('../../docs/interactive_book/models/packet_framing.js');
const send = require('../../docs/interactive_book/models/partial_send.js');
const core = require('../../docs/interactive_book/models/core.js').core;

test('L09: 2+3+2 consumes 0/0/7 and does not mutate the previous state', () => {
  let state = packet.createInitialState({mode: 'split'});
  for (const expected of [0, 0, 7]) {
    const before = JSON.stringify(state);
    const result = packet.transition(state, {type: 'receive'});
    assert.equal(JSON.stringify(state), before);
    state = result.state;
    assert.equal(state.lastConsumed, expected);
    assert.deepEqual(packet.validateState(state), []);
  }
  assert.equal(state.packetCount, 1);
});
for (const [mode, count, consumed, error] of [
  ['coalesced', 2, 12, null], ['invalid', 0, 0, 'kInvalidLength'],
  ['large', 0, 0, 'kMessageTooLarge'], ['empty', 1, 4, null]
]) test(`L09: ${mode}`, () => {
  const result = packet.transition(packet.createInitialState({mode}), {type: 'receive'}).state;
  assert.equal(result.packetCount, count);
  assert.equal(result.consumed, consumed);
  assert.equal(result.error, error);
});
test('L09: a valid frame followed by invalid length preserves the consumed prefix', () => {
  const config = {bytes: [0,0,0,4,0,0,0,3], chunks: [8]};
  const state = packet.transition(packet.createInitialState(config), {type: 'receive'}).state;
  assert.equal(state.consumed, 4);
  assert.equal(state.packetCount, 1);
  assert.equal(state.error, 'kInvalidLength');
});
test('L09: maximum length is inclusive and incomplete payload stays unconsumed', () => {
  let state = packet.createInitialState({bytes: [0,0,0,5,9], chunks: [4,1], maxPacketBytes: 5});
  state = packet.transition(state, {type: 'receive'}).state;
  assert.equal(state.consumed, 0);
  state = packet.transition(state, {type: 'receive'}).state;
  assert.equal(state.consumed, 5);
});
test('L11: 4 bytes completed as 2+2 changes offset, length, and outstanding', () => {
  const initial = send.createInitialState();
  const partial = send.transition(initial, {type: 'complete', bytes: 2});
  assert.deepEqual([partial.state.offset, partial.state.length, partial.state.outstanding], [2,2,1]);
  assert.equal(initial.offset, 0);
  assert.deepEqual(partial.events.map(e => e.type), ['retired', 'posted']);
  const final = send.transition(partial.state, {type: 'complete', bytes: 2}).state;
  assert.deepEqual([final.offset, final.length, final.outstanding], [4,0,0]);
  assert.deepEqual(send.validateState(final), []);
});
for (const size of [0, 5]) test(`L11: invalid completion ${size} retires the reference without consuming bytes`, () => {
  const state = send.transition(send.createInitialState(), {type: 'complete', bytes: size}).state;
  assert.equal(state.offset, 0);
  assert.equal(state.outstanding, 0);
  assert.equal(state.remaining, 4);
  assert.equal(state.error, 'invalid-completion');
});
test('L11: failed repost rolls back outstanding and retains unsent ring bytes', () => {
  const result = send.transition(send.createInitialState(), {type: 'complete', bytes: 2, repostSucceeds: false});
  assert.deepEqual([result.state.offset, result.state.remaining, result.state.length, result.state.outstanding], [2,2,0,0]);
  assert.equal(result.state.error, 'kSEND_IO_FAIL');
  assert.deepEqual(result.events.map(e => e.type), ['retired', 'post-rollback']);
});
test('L11: buffer release is blocked until the isolated send reference is retired', () => {
  const blocked = send.transition(send.createInitialState(), {type: 'free'});
  assert.equal(blocked.state.freed, false);
  const finished = send.transition(blocked.state, {type: 'complete', bytes: 4});
  const released = send.transition(finished.state, {type: 'free'});
  assert.equal(released.state.freed, true);
  assert.throws(() => send.transition(released.state, {type: 'complete', bytes: 1}));
});
for (const [name, model, config, actions] of [
  ['L09', packet, {mode: 'split'}, [{type:'receive'}, {type:'receive'}, {type:'receive'}]],
  ['L11', send, {generation: '18446744073709551615'}, [{type:'complete',bytes:2}, {type:'complete',bytes:2}]]
]) test(`${name}: bounded JSON export/import and replay are deterministic`, () => {
  const json = model.serializeScenario(config, actions);
  assert.deepEqual(model.parseScenario(json).state, core.replay(model, config, actions));
  assert.deepEqual(core.replay(model, config, actions.slice(0, 1)), model.transition(model.createInitialState(config), actions[0]).state);
  assert.throws(() => model.parseScenario('{invalid'));
  assert.throws(() => model.parseScenario(JSON.stringify({schemaVersion:99})));
  assert.throws(() => model.parseScenario('x'.repeat(32769)));
  assert.throws(() => model.serializeScenario(config, Array(129).fill(actions[0])));
  assert.throws(() => model.serializeScenario(config, [{type:'<script>'}]));
});
test('untrusted inputs reject invalid ranges and lossy generation values', () => {
  for (const config of [{mode:'unknown'}, {bytes:[256],chunks:[1]}, {bytes:[0],chunks:[2]}, {maxPacketBytes:3}]) {
    assert.throws(() => packet.createInitialState(config));
  }
  for (const generation of [9007199254740992, '0', '-1', '18446744073709551616', '<img>']) {
    assert.throws(() => send.createInitialState({generation}));
  }
  assert.throws(() => send.transition(send.createInitialState(), {type:'complete',bytes:1.5}));
});
