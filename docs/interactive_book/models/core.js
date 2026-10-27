'use strict';
var BookModels = typeof BookModels === 'undefined' ? {} : BookModels;
(function (models) {
  const maxActions = 128;
  function integer(value, low, high) {
    return Number.isSafeInteger(value) && value >= low && value <= high;
  }
  function check(condition, message) {
    if (!condition) throw new Error(message);
  }
  function replay(model, config, actions) {
    check(Array.isArray(actions) && actions.length <= maxActions, '동작은 128개 이하여야 한다.');
    let state = model.createInitialState(config);
    for (const action of actions) state = model.transition(state, action).state;
    return state;
  }
  function serialize(model, labId, config, actions) {
    replay(model, config, actions);
    const text = JSON.stringify({schemaVersion: 1, labId, config, actions});
    check(text.length <= 32768, '실험 자료는 32,768자 이하여야 한다.');
    return text;
  }
  function parse(text, model, labId) {
    check(typeof text === 'string' && text.length <= 32768, '실험 자료는 32,768자 이하여야 한다.');
    const value = JSON.parse(text);
    check(value && value.schemaVersion === 1 && value.labId === labId, '지원하지 않는 실험 자료 버전 또는 실험 ID다.');
    const state = replay(model, value.config, value.actions);
    return {config: value.config, actions: value.actions, state};
  }
  models.core = {integer, check, replay, serialize, parse};
})(BookModels);
if (typeof module !== 'undefined' && module.exports) module.exports = BookModels;
