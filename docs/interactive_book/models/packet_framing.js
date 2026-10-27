'use strict';
(function (models) {
  const {check, integer} = models.core;
  const scenarios = {
    split: {bytes: [0, 0, 0, 7, 114, 105, 111], chunks: [2, 3, 2]},
    coalesced: {bytes: [0, 0, 0, 7, 114, 105, 111, 0, 0, 0, 5, 33], chunks: [12]},
    invalid: {bytes: [0, 0, 0, 3], chunks: [4]},
    large: {bytes: [0, 0, 0, 17], chunks: [4]},
    empty: {bytes: [0, 0, 0, 4], chunks: [4]}
  };
  function createInitialState(config = {}) {
    check(config && typeof config === 'object' && !Array.isArray(config), '설정은 객체여야 한다.');
    const scenario = config.mode ? scenarios[config.mode] : scenarios.split;
    check(scenario, '알 수 없는 수신 시나리오다.');
    const bytes = config.bytes ?? scenario.bytes;
    const chunks = config.chunks ?? scenario.chunks;
    const maxPacketBytes = config.maxPacketBytes ?? 16;
    check(integer(maxPacketBytes, 4, 4096), '허용 패킷 길이는 4~4096이어야 한다.');
    check(Array.isArray(bytes) && bytes.length > 0 && bytes.length <= 256 &&
      bytes.every(b => integer(b, 0, 255)), '바이트 배열은 1~256개의 0~255 정수여야 한다.');
    check(Array.isArray(chunks) && chunks.length > 0 && chunks.length <= 128 &&
      chunks.every(n => integer(n, 1, 256)) && chunks.reduce((a, b) => a + b, 0) === bytes.length,
      '수신 조각의 합은 바이트 배열 길이와 같아야 한다.');
    return {bytes: [...bytes], chunks: [...chunks], maxPacketBytes, arrived: 0,
      consumed: 0, packetCount: 0, chunk: 0, lastConsumed: 0, error: null};
  }
  function validateState(s) {
    const problems = [];
    if (!integer(s.consumed, 0, s.arrived) || !integer(s.arrived, 0, s.bytes.length)) problems.push('수신·소비 범위 위반');
    if (!integer(s.chunk, 0, s.chunks.length) || !integer(s.packetCount, 0, 64)) problems.push('진행 위치 위반');
    return problems;
  }
  function transition(state, action) {
    check(action && action.type === 'receive', '수신 실험은 receive 동작만 허용한다.');
    if (state.error || state.chunk === state.chunks.length) return {state, events: [], explanationId: 'framing.finished'};
    const next = {...state, arrived: state.arrived + state.chunks[state.chunk], chunk: state.chunk + 1, lastConsumed: 0};
    const events = [];
    while (next.arrived - next.consumed >= 4) {
      const p = next.consumed;
      const size = next.bytes[p] * 16777216 + next.bytes[p + 1] * 65536 + next.bytes[p + 2] * 256 + next.bytes[p + 3];
      if (size < 4) { next.error = 'kInvalidLength'; break; }
      if (size > next.maxPacketBytes) { next.error = 'kMessageTooLarge'; break; }
      if (size > next.arrived - p) break;
      next.consumed += size;
      next.packetCount++;
      events.push({type: 'packet', offset: p, length: size});
    }
    next.lastConsumed = next.consumed - state.consumed;
    if (next.error) events.push({type: 'decode-error', error: next.error});
    check(validateState(next).length === 0, '수신 모델 상태가 유효하지 않다.');
    return {state: next, events, explanationId: next.error ? 'framing.invalid' : next.lastConsumed ? 'framing.packet' : 'framing.wait'};
  }
  function view(s) {
    return {values:[{label:'누적 수신 바이트',value:s.arrived},{label:'누적 소비 바이트',value:s.consumed},
      {label:'완성된 패킷',value:s.packetCount},{label:'이번 소비 바이트',value:s.lastConsumed},
      {label:'길이 오류',value:s.error??'없음'}],
      nodes:[{label:'수신 조각',value:`${s.chunk}/${s.chunks.length}`,active:!s.error&&s.chunk<s.chunks.length},
        {label:'수신 링',value:`${s.arrived-s.consumed}바이트 남음`},{label:'완전한 패킷',value:s.packetCount,active:s.lastConsumed>0}],
      message:s.error?`${s.error}: 길이 검증에 실패했다. 이미 소비한 앞 패킷은 ${s.consumed}바이트다. EchoHandler는 종료를 요청한다.`:
        s.chunk===0?'아직 바이트가 도착하지 않았다. 다음 수신을 전달한다.':
        s.lastConsumed?`이번 소비량은 ${s.lastConsumed}바이트다. 디코더가 보관하지 않은 꼬리는 수신 링에 남는다.`:
        '이번 소비량은 0이다. 완전한 헤더와 본문이 모일 때까지 수신 링의 바이트를 남긴다.'};
  }
  const model = {createInitialState, transition, validateState, view,
    serializeScenario(config, actions) { return models.core.serialize(model, 'packet-framing', config, actions); },
    parseScenario(text) { return models.core.parse(text, model, 'packet-framing'); }};
  models.packetFraming = model;
})(typeof module !== 'undefined' && module.exports ? require('./core.js') : BookModels);
if (typeof module !== 'undefined' && module.exports) module.exports = require('./core.js').packetFraming;
