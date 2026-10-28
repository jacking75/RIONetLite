'use strict';
(function (models) {
  const {check, integer} = models.core;
  function createInitialState(config = {}) {
    check(config && typeof config === 'object' && !Array.isArray(config), '설정은 객체여야 한다.');
    const messageBytes = config.messageBytes ?? 4;
    const generation = config.generation ?? '41';
    check(integer(messageBytes, 1, 256), '메시지는 1~256바이트여야 한다.');
    check(typeof generation === 'string' && /^[1-9][0-9]{0,19}$/.test(generation) &&
      BigInt(generation) <= 18446744073709551615n, '세대는 uint64 범위의 양의 10진 문자열이어야 한다.');
    return {messageBytes, generation, offset: 0, length: messageBytes, remaining: messageBytes,
      outstanding: 1, requestNumber: 1, freed: false, error: null};
  }
  function validateState(s) {
    const errors = [];
    if (!integer(s.offset, 0, s.messageBytes) || s.remaining !== s.messageBytes - s.offset) errors.push('링 읽기 위치 위반');
    if (![0, 1].includes(s.outstanding) || s.outstanding !== Number(s.length > 0)) errors.push('송신 요청 수 위반');
    if (s.length > s.remaining || s.length < 0 || (s.freed && s.outstanding)) errors.push('등록 버퍼 수명 위반');
    return errors;
  }
  function transition(state, action) {
    check(action && ['complete', 'free'].includes(action.type), '완료 또는 해제 시도만 허용한다.');
    let next = {...state};
    const events = [];
    let explanationId;
    if (action.type === 'free') {
      if (next.outstanding) {
        events.push({type: 'blocked-free'});
        explanationId = 'send.free_blocked';
      } else {
        next.freed = true;
        events.push({type: 'send-buffer-released'});
        explanationId = 'send.free_isolated';
      }
    } else {
      check(integer(action.bytes, 0, 257), '완료 크기는 0~257의 정수여야 한다.');
      check(action.repostSucceeds === undefined || typeof action.repostSucceeds === 'boolean', '재게시 결과는 참 또는 거짓이어야 한다.');
      check(!next.freed && next.outstanding === 1 && !next.error, '완료할 송신 요청이 없다.');
      if (action.bytes === 0 || action.bytes > next.length) {
        // CompleteSend rejects the size; Completed calls FailUnlocked and retires the reference.
        next.error = 'invalid-completion';
        next.length = 0;
        next.outstanding = 0;
        events.push({type: 'failed-request'});
        explanationId = 'send.invalid_completion';
      } else {
        next.offset += action.bytes;
        next.remaining -= action.bytes;
        next.length = 0;
        next.outstanding = 0;
        events.push({type: 'retired', bytes: action.bytes});
        if (next.remaining) {
          next.requestNumber++;
          if (action.repostSucceeds === false) {
            next.error = 'kSEND_IO_FAIL';
            events.push({type: 'post-rollback'});
            explanationId = 'send.post_failed';
          } else {
            next.length = next.remaining;
            next.outstanding = 1;
            events.push({type: 'posted', offset: next.offset, length: next.length});
            explanationId = 'send.remainder';
          }
        } else explanationId = 'send.finished';
      }
    }
    check(validateState(next).length === 0, '송신 모델 상태가 유효하지 않다.');
    return {state: next, events, explanationId, blocked: explanationId === 'send.free_blocked',
      ...(explanationId === 'send.free_blocked' ? {message: '차단: 미완료 송신 요청이 등록 메모리를 참조한다. 먼저 완료를 처리해야 한다.'} : {})};
  }
  function view(s) {
    return {visual:{kind:"bytes",bytes:Array.from({length:s.messageBytes},(_,i)=>i),headerBytes:0,arrived:s.messageBytes,consumed:s.offset,marks:["offset="+s.offset+" / posted="+s.length]},values:[{label:'링 읽기 위치',value:s.offset},{label:'게시된 길이',value:s.length},
      {label:'미완료 송신',value:s.outstanding},{label:'링에 남은 바이트',value:s.remaining},
      {label:'게시 준비 횟수',value:s.requestNumber},{label:'오류',value:s.error??'없음'}],
      nodes:[{label:'송신 링',value:`${s.remaining}바이트`},{label:'진행 요청',value:s.length,active:s.outstanding===1},
        {label:'소비한 바이트',value:s.offset,active:s.outstanding===0}],
      message:s.freed?'이 송신 버퍼만 해제했다. 서버 전체의 수신·RQ·CQ·워커 해제 조건은 이 모델에서 확인하지 않는다.':
        s.error?`${s.error}: 요청 참조를 정리했지만 링에 ${s.remaining}바이트가 남았다. 송신 성공을 뜻하지 않는다.`:
        s.outstanding?`현재 ${s.length}바이트 요청 1개가 등록 메모리를 참조한다. 정상 완료 뒤 남은 구간은 다시 게시한다.`:
        '이번 송신의 모든 바이트가 완료됐다. 다른 I/O 자원의 해제 가능 여부는 별도로 확인한다.'};
  }
  const model = {createInitialState, transition, validateState, view,
    serializeScenario(config, actions) { return models.core.serialize(model, 'partial-send', config, actions); },
    parseScenario(text) { return models.core.parse(text, model, 'partial-send'); }};
  models.partialSend = model;
})(typeof module !== 'undefined' && module.exports ? require('./core.js') : BookModels);
if (typeof module !== 'undefined' && module.exports) module.exports = require('./core.js').partialSend;
