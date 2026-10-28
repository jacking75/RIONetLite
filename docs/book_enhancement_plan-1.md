# RIONetLite 인터랙티브 책 보완·확장 구현 계획

## 태스크 목록

- [x] [T1. 컴포넌트 선언 규약과 빌드 검증 확장](#t1-컴포넌트-선언-규약과-빌드-검증-확장) 쨌 ?꾨즺 2026-10-28 18:25:06 KST
- [x] [T2. 보관 자료 분리와 기본 숨김](#t2-보관-자료-분리와-기본-숨김) 쨌 ?꾨즺 2026-10-28 18:30:37 KST
- [x] [T3. 용어 툴팁과 입문 용어집 확장](#t3-용어-툴팁과-입문-용어집-확장) 쨌 ?꾨즺 2026-10-28 18:33:11 KST
- [x] [T4. 그림 엔진 1 시퀀스와 상태도](#t4-그림-엔진-1-시퀀스와-상태도) 쨌 ?꾨즺 2026-10-28 18:36:10 KST
- [x] [T5. 그림 엔진 2 링 슬롯 큐 바이트 의존 그래프](#t5-그림-엔진-2-링-슬롯-큐-바이트-의존-그래프) 쨌 ?꾨즺 2026-10-28 18:38:45 KST
- [x] [T6. 실험 20개의 전용 시각화 전환](#t6-실험-20개의-전용-시각화-전환) 쨌 ?꾨즺 2026-10-28 18:41:21 KST
- [x] [T7. 코드 따라가기 컴포넌트와 Echo 워크스루](#t7-코드-따라가기-컴포넌트와-echo-워크스루) 쨌 ?꾨즺 2026-10-28 18:47:29 KST
- [x] [T8. 핵심 경로 워크스루 10개 집필](#t8-핵심-경로-워크스루-10개-집필) 쨌 ?꾨즺 2026-10-28 18:51:22 KST
- [x] [T9. 인터랙티브 시스템 지도와 장별 위치 표시](#t9-인터랙티브-시스템-지도와-장별-위치-표시) 쨌 ?꾨즺 2026-10-28 18:54:12 KST
- [x] [T10. 학습 경로 선택과 진도 표시](#t10-학습-경로-선택과-진도-표시) 쨌 ?꾨즺 2026-10-28 18:57:54 KST
- [x] [T11. 확인 퀴즈와 예측 질문 컴포넌트](#t11-확인-퀴즈와-예측-질문-컴포넌트) 쨌 ?꾨즺 2026-10-28 19:00:51 KST
- [x] [T12. 0부 1부 입문 장 원고 보강](#t12-0부-1부-입문-장-원고-보강) 쨌 ?꾨즺 2026-10-28 19:04:39 KST
- [x] [T13. 2부에서 4부 핵심 장 원고 보강](#t13-2부에서-4부-핵심-장-원고-보강) 쨌 ?꾨즺 2026-10-28 19:07:57 KST
- [x] [T14. 활용 트랙 Echo에서 Chat으로 그리고 확장 과제](#t14-활용-트랙-echo에서-chat으로-그리고-확장-과제) 쨌 ?꾨즺 2026-10-28 19:11:20 KST
- [x] [T15. 심화 장과 부록 최소 보강](#t15-심화-장과-부록-최소-보강) 쨌 ?꾨즺 2026-10-28 19:19:26 KST
- [ ] [T16. 접근성 인쇄 성능 회귀 검증과 문서 갱신](#t16-접근성-인쇄-성능-회귀-검증과-문서-갱신)


작성일: 2026-10-28 (KST) · 조사 기준 커밋: `59fb614` (`master`) · 문서 번호: 1

대상 독자: 게임 서버 개발자 지망생과 신입 게임 서버 개발자. 책의 목표는 "큰 그림을 먼저 보고 세부로 내려가며, 주요 코드를 이해하고, 실제 서버에 바르게 활용하는 것"이다. 이 문서는 다음 세션부터 그 목표에 맞게 `docs/interactive_book`을 보완·확장하기 위한 작업 명세다.

이 문서의 사용 규칙은 다음과 같다.

- 아래 태스크 목록의 항목 하나를 끝낼 때마다 **즉시** `- [ ]`를 `- [x]`로 바꾸고 완료 일시(KST, `Get-Date` 등 시스템 명령으로 확인)를 덧붙인다. 여러 개를 몰아서 체크하지 않는다.
- 태스크는 서로 독립적으로 완료 판정이 가능하도록 나누었다. 의존 관계가 있는 경우 상세 절의 "선행"에 적었다. 선행 태스크가 끝나지 않았으면 그 태스크를 먼저 끝낸다.
- 모든 태스크는 끝에 [공통 검증 명령](#45-공통-검증-명령)을 모두 통과해야 완료다. 빌드 산출물 `docs/interactive_book/index.html`은 매 태스크마다 재생성해 커밋한다.
- 라이브러리 C++ 코드는 바꾸지 않는다. 책을 쓰다 구현 결함을 발견하면 이 계획 문서의 검증 기록에 근거와 영향을 적고 책에는 현재 동작을 정확히 설명한다.

---


권장 진행 순서와 1차 완료 세트는 [6절](#6-권장-진행-순서)에 있다.

---

## 1. 현황 진단

조사 기준 커밋에서 측정한 수치다. 측정 방법은 각 항목 끝에 적었다.

### 1.1 구조

- 장 36개(C01, P01~P03, C02~C28, A01~A05), 실험 20개(L01~L20), 포함 소스 57개 파일, 출처(evidence) 64건, 용어 25개, 읽기 경로 4개. (`manifest.json`, `evidence.json`, `glossary.json`)
- 단일 HTML `index.html`은 1.9MB다. 생성기 `build_book.py`가 원고·출처·모델·스타일·스크립트를 한 파일로 합친다.
- 원고 HTML에는 `script`·`iframe`·`on*` 속성을 넣을 수 없다(`Fragment` 파서가 거부). 모든 상호작용은 `data-*` 속성을 `book.js`가 해석하는 방식이어야 한다. 이 제약은 유지한다.
- 검사 도구: `build_book.py --check`, `tools/book/content_checks.py`(20개), `model_tests.cjs`·`network_tests.cjs`·`scenario_tests.cjs`, `verify_reader.cjs`(Playwright + Edge), `verify_documents.py`, `tools/check-docs.ps1`.

### 1.2 본문과 보관 자료의 비율

장 파일 바이트의 **78%가 과거 문서 보관 블록**(`<details class="archive">`)이다. 보관 블록을 제외한 본문 분량은 장당 500~1,400단어다. (정규식으로 `details.archive`를 제거한 뒤 길이와 단어 수를 셈)

| 구분 | 예 | 본문 단어 | 보관 비율 |
|---|---|---|---|
| 입문 P장 | P02 | 1,419 | 0% |
| 핵심 C장 | C12 수신 경로 | 560 | 83% |
| 핵심 C장 | C14 송신 | 619 | 53% |
| 심화 C장 | C05 RIO 요청 | 532 | 95% |
| 예제 장 | C19 ChatServer | 1,111 | 91% |

모든 C장과 A01·A02의 마지막 절(`*-prior-notes`)은 보관 블록 모음이다. 검색 색인은 보관 블록 안의 문장까지 포함하므로 입문 독자의 검색 결과가 과거 문구로 채워진다. 인쇄 시 모든 `details`를 펼치므로 PDF의 대부분이 보관 자료가 된다.

### 1.3 그림

보관 블록을 제외하면 **C02~C28 본문에 SVG 그림이 C06의 1개뿐**이다. P01~P03에만 1~2개씩 있다. 본문에 `figure.diagram.animated`나 `.exercise` 블록은 하나도 없고, 보관 블록 안의 애니메이션 그림(`data-step`, `.motion-controls`)은 이를 구동하는 JavaScript가 `book.js`에 없어 정지 그림으로만 보인다. (`<svg`, `class="exercise"`, `diagram animated` 출현 횟수를 본문·보관 블록으로 나누어 셈)

### 1.4 실험

실험 20개는 순수 상태 전이 모델(`models/*.js`)과 범용 렌더러(`book.js`의 `visualState`)로 동작한다. 범용 렌더러는 "상자 한 줄 + 값 목록"만 그린다. L09(바이트 띠)와 L11(메모리 띠)만 전용 그림이 있다. 링 버퍼 실험(L05)은 링이 아니라 상자 세 개로, 슬롯 배정(L06)은 상자 여섯 개로, 종료 의존 관계(L13)는 값 목록으로만 보인다. 모델 설계(불변 상태, 재실행, JSON 내보내기, 128동작 상한)는 좋으므로 유지하고 **보이는 쪽만 교체**한다.

### 1.5 코드

코드 탐험기(`#sourceDialog`)는 포함 소스의 원문을 줄 번호와 함께 보여 주고, 출처 64건으로 함수 구간을 연다. 그러나 "이 함수의 1행부터 차례로 무슨 일이 일어나는지"를 단계별로 설명하는 장치가 없다. 신입 독자는 500줄짜리 `server_net_io_worker.h`를 열고 어디를 봐야 할지 모른다. 본문은 `RecvFailed`, `BeginPeerClose`, `kRECV_BUFFER_FULL` 같은 내부 이름을 소개 없이 쓴다(C12 첫 절).

### 1.6 학습 장치

- 장마다 "이해했다" 체크박스 1개뿐이다. 퀴즈, 예측 질문, 연습 과제, 자주 하는 실수 비교가 본문에 없다.
- 용어는 A02 표와 검색으로만 찾는다. 본문에서 바로 뜻을 보는 장치가 없다.
- P02의 지도는 정지 그림이다. 부품을 눌러 역할·스레드·관련 장·코드로 들어가는 길이 없다. C장 머리말에 "큰 그림에서의 위치" 링크 문장은 있지만 그림으로 표시되지 않는다.
- 읽기 경로 4개가 manifest에 있지만 독자가 경로를 고르거나 진도를 볼 수 없다.

### 1.7 결론

생성기·출처 검증·모델 구조는 그대로 두고, (1) 그림과 상호작용 컴포넌트를 선언형으로 추가할 기반을 만들고, (2) 그 위에 그림·코드 따라가기·퀴즈·지도를 얹고, (3) 입문·핵심 장의 원고를 신입 독자 기준으로 다시 쓰고, (4) 보관 자료를 기본 숨김으로 돌리는 것이 이번 작업이다.

---

## 2. 목표 독자 경험

### 2.1 세 단계 줌

책 전체를 "지도 → 장면 → 코드" 세 단계로 내려가게 만든다.

| 단계 | 독자가 보는 것 | 구현 |
|---|---|---|
| 지도 | 애플리케이션·공개 Server·accept·세션 풀·링·I/O 워커·RIO·로직 큐·로직 워커가 어떻게 놓이고 어느 스레드가 움직이는가 | T9 시스템 지도(P02, C06), 장마다 "지도에서의 위치" 미니 지도 |
| 장면 | 연결 하나의 여섯 장면(준비·연결·수신·송신·종료·정지)에서 누가 무엇을 하는가 | T4 시퀀스 그림, T6 실험 전용 그림 |
| 코드 | 그 장면을 실제로 수행하는 함수의 어느 줄이 무엇을 하는가 | T7·T8 코드 따라가기(워크스루), 기존 코드 탐험기 |

세 단계는 서로 연결된다. 지도의 부품을 누르면 관련 장과 코드로, 시퀀스 그림의 단계를 누르면 워크스루의 같은 단계로, 워크스루의 단계에서 "그림에서 보기"를 누르면 그림의 같은 단계로 이동한다.

### 2.2 장의 표준 구성

입문·중급 장(P01~P03, C02~C04, C06, C07, C10, C12~C14, C16, C18~C23)은 다음 순서를 따른다. 심화 장은 1·2·5·8을 필수로 하고 나머지는 가능한 범위에서 넣는다.

1. **지도에서의 위치**: 미니 지도와 한 문장. "이 장은 ③ 수신 장면의 I/O 워커와 수신 링을 다룬다."
2. **쉽게 말하면**: 기존 `aside.primer`. 비유 한 개(`.analogy`)를 붙일 수 있다.
3. **그림으로 보기**: 시퀀스 또는 상태도 한 개 이상. 재생 가능.
4. **실험**: 기존 실험(전용 그림으로 교체).
5. **코드 따라가기**: 워크스루 한 개 이상. 단계마다 그림의 단계와 연결.
6. **자주 하는 실수**: 잘못된 사용(학습용 예시)과 실제 코드를 나란히 보여 주는 비교 블록 한 개 이상.
7. **확인 퀴즈**: 2~4문항. 틀려도 진행에 제한이 없다.
8. **핵심 세 줄과 다음 장**: 세 문장 요약, 기존 체크박스, 경로상 다음 장 링크.

### 2.3 신입 독자를 위한 집필 규칙

기존 계획의 문체 규칙(평서문 `~다`, 추상어 대신 구체 동작, 성능 문제와 안전성 오류 구분)은 그대로 유지하고 다음을 추가한다.

- **처음 나오는 코드 이름은 "하는 일(이름)" 형태로 소개한다.** 예: "수신 요청 하나가 실패로 끝났음을 기록하는 `RecvFailed`". 같은 장 안에서 두 번째부터는 이름만 쓴다.
- **그림 먼저, 코드 나중.** 코드 블록이나 워크스루 앞에는 "이 코드는 위 그림의 ④ 단계다"처럼 위치를 적는다.
- **한 절은 질문 하나에 답한다.** 절의 첫 문단에 답을 먼저 쓴다.
- **내부 enum 상수와 함수 이름을 나열로 끝내지 않는다.** 표로 정리할 때는 "독자가 겪는 상황" 열을 함께 둔다.
- **"반드시·항상·절대"는 근거 코드나 테스트를 버튼으로 붙인다.**
- 코드는 세 가지로 구분해 라벨을 단다: `실제 코드`(포함 소스에서 추출), `학습용 예시`(컴파일 검증하지 않음), `의사 코드`. 학습용 예시는 실제 코드처럼 보이게 꾸미지 않는다.
- 각 장을 쓴 뒤 다음 질문으로 자체 검수한다: "C++ 문법은 알지만 소켓 프로그래밍을 해 본 적 없는 독자가 이 절의 첫 문단만 읽고 다음 문단으로 갈 수 있는가?" 아니라면 앞에 한 문장을 더 쓴다.

---

## 3. 전체 파일 구조

새로 만들거나 크게 바꾸는 파일이다. 나머지는 각 태스크에 적었다.

```
docs/interactive_book/
  build_book.py          컴포넌트 정의 파일 검증과 데이터 포함 추가
  book.js                컴포넌트 레지스트리, 그림 엔진 호출, 워크스루·퀴즈·지도·용어 UI
  book.css               컴포넌트 스타일(토큰은 기존 변수만 사용)
  book_template.html     #figureDialog, #termPopover 추가
  manifest.json          chapters[].mapFocus, sections[].archive, readingRoutes[].description
  glossary.json          입문 용어 추가
  figures.json           [신규] 그림 정의(시퀀스·상태도·링·슬롯·큐·바이트·그래프)
  walkthroughs.json      [신규] 코드 따라가기와 비교 블록 정의
  quizzes.json           [신규] 퀴즈 정의
  map.json               [신규] 시스템 지도 정의
  models/figure_engine.js  [신규] 순수 SVG 생성 함수(BookFigures). 실험과 그림이 공유
  chapters/*.html        원고
tools/book/
  content_checks.py      컴포넌트 검증 실패 사례 테스트 추가
  figure_tests.cjs       [신규] 그림 엔진 순수 함수 테스트(필요 최소)
  scenario_tests.cjs     view.visual 종류 검사 한 줄 추가
  verify_reader.cjs      새 컴포넌트 조작 시나리오 추가
```

`figure_engine.js`는 `manifest.modelFiles`에 넣어 실험 모델과 같은 방식으로 포함한다. 브라우저와 Node 양쪽에서 로드되도록 `models/core.js`와 같은 패턴(`BookModels` 전역 + `module.exports`)을 쓴다.

---

## 4. 공통 기술 규약

### 4.1 컴포넌트 선언 규약

원고 HTML은 다음 선언만 쓴다. 각 선언은 `book.js`의 레지스트리가 장이 활성화될 때 초기화한다(실험 `initLab`과 같은 시점).

| 선언 | 정의 위치 | 역할 |
|---|---|---|
| `<figure data-figure="F-C12-1"></figure>` | `figures.json` | 재생 가능한 그림 |
| `<div data-walkthrough="W05"></div>` | `walkthroughs.json` | 코드 따라가기 |
| `<div data-compare="X-C04-1"></div>` | `walkthroughs.json`의 `compares` | 잘못된 사용과 실제 코드 비교 |
| `<div data-quiz="Q-C13-1"></div>` | `quizzes.json` | 확인 퀴즈 |
| `<dfn data-term="post">게시</dfn>` | `glossary.json` | 용어 툴팁 |
| `<div data-map="system" data-focus="io-worker,recv-ring"></div>` | `map.json` | 시스템 지도(전체) |
| `<div data-map="system" data-mini data-focus="..."></div>` | `map.json` | 장 머리의 미니 지도 |
| `<details class="predict">` | 원고 | 예측 질문(JS 불필요) |

규칙:

- 선언 요소는 빈 요소로 두고 내용은 JS가 만든다. JS가 없을 때 보일 대체 문장은 `data-fallback` 속성이 아니라 요소 안의 `<p class="no-script">`로 둔다. `noscript` 환경에서는 이 문단이 보이고, JS는 초기화 때 지운다.
- 실행 시 만드는 DOM id는 `rt-` 접두사를 쓴다. 원고 id와 충돌하지 않게 한다. 생성기의 `unique(parsed.ids)` 검사는 원고 id만 본다.
- 인쇄 대체물: 모든 컴포넌트는 `preparePrint`에서 정적 표현으로 바뀐다. 그림은 마지막 단계 프레임 + 단계 목록, 워크스루는 코드 + 단계 번호 여백 표시 + 단계 설명 목록, 퀴즈는 문항 + 정답과 해설, 지도는 정지 그림 + 부품 표다.
- 키보드: 모든 조작은 Tab으로 도달하고 Enter/Space로 실행한다. 단계 이동은 포커스가 컴포넌트 안에 있을 때 ←/→로도 된다. 열리는 패널은 Esc로 닫고 포커스를 연 버튼으로 돌린다.
- 움직임: 자동 재생은 독자가 시작한다. 장 이동·탭 비활성화 때 멈춘다(기존 `stopPlayback` 확장). `prefers-reduced-motion`에서는 전환 효과 없이 상태만 바뀐다.
- 설명 텍스트는 `role="status"` 영역 하나에만 쓴다. 재생 중 매 프레임 알림을 반복하지 않는다(현재 실험의 `.lab-output`과 같은 방식).

### 4.2 정의 파일의 공통 규칙

- 모든 정의 파일은 `{"schemaVersion": 1, ...}`로 시작한다(`read_json`이 검사).
- id는 `접두사-장ID-번호` 형식이다: 그림 `F-C12-1`, 워크스루 `W05`(장 독립, 2자리), 비교 `X-C04-1`, 퀴즈 `Q-C13-1`.
- 코드 참조는 항상 `evidence.json`의 출처 id를 통한다. 새 코드 구간이 필요하면 출처를 먼저 추가한다(해시·테스트 선언 검토 포함). 정의 파일에 파일 경로와 줄 번호를 직접 쓰지 않는다.
- 출처 구간 안의 더 작은 범위는 `focus: {"start": "...", "end": "..."}`로 지정한다. 생성기는 두 문자열이 출처 텍스트 안에 **각각 정확히 한 번** 나오는지 검사하고 줄 번호를 계산해 포함한다(출처 선택자와 같은 규칙).
- 생성기는 다음을 검사해 실패시킨다: 모르는 id 참조, 정의했지만 원고에서 쓰지 않는 id, 워크스루·퀴즈·비교의 중복 사용, 그림 종류 오타, 퀴즈 정답 개수 ≠ 1, 보기 해설 누락, 용어 id 누락, 지도 부품의 장·출처 참조 오류.
- 정의 파일은 `BOOK` 객체에 `figures`, `walkthroughs`, `compares`, `quizzes`, `map` 키로 포함한다. `<` 이스케이프는 기존 방식(`<`)을 따른다.

### 4.3 그림 엔진 API

`models/figure_engine.js`는 DOM을 만지지 않는 순수 함수 모음이다.

```js
BookFigures.render(kind, spec, step) // → {svg: string, width, height, activeIds: [...]}
BookFigures.kinds                    // ['sequence','state','ring','slots','queue','bytes','graph','layers']
BookFigures.stepCount(kind, spec)    // 단계 수
```

- `spec`은 `figures.json`의 그림 한 개이거나 실험 모델 `view()`가 돌려준 `visual` 객체다. 같은 함수로 그림과 실험을 그린다.
- 출력 SVG는 `viewBox`만 쓰고 고정 픽셀 폭을 쓰지 않는다. 글자 크기는 14px 기준, 최소 12px. 좁은 화면에서는 `.diagram-scroll`로 가로 스크롤하고 "확대" 버튼이 `#figureDialog`에 같은 SVG를 크게 연다.
- 색은 기존 CSS 변수만 쓴다. 의미 구분은 색과 함께 선 모양·아이콘·라벨로도 한다: 호출(실선 화살표), 콜백(점선 화살표, `--gold`), 완료 이벤트(이중선), 데이터 이동(굵은 화살표), 차단(빨간 X, `--danger`).
- 텍스트는 `escapeHtml`을 거친다. spec의 문자열을 그대로 SVG에 넣지 않는다.

### 4.4 상태 저장

`persisted`(localStorage `rionetlite.book.reader.v2`)의 `schemaVersion`을 3으로 올리고 다음을 추가한다: `quiz: {id: 선택한 보기 번호}`, `route: 경로 id 또는 null`, `showArchive: false`. 2에서 3으로는 제자리 이관한다(기존 `checks`·`position`·`theme` 유지). `edition`은 장 id가 바뀌지 않으므로 유지한다.

### 4.5 공통 검증 명령

모든 태스크의 마지막에 다음을 저장소 루트에서 실행한다. 하나라도 실패하면 완료가 아니다.

```powershell
python docs/interactive_book/build_book.py
python docs/interactive_book/build_book.py --check
npm.cmd --prefix tools/book run test:content
npm.cmd --prefix tools/book run test:models
npm.cmd --prefix tools/book run test:browser
tools\check-docs.ps1
git diff --check
```

`test:browser`는 설치된 Edge가 필요하고 수 분이 걸린다. 원고만 바꾼 태스크라도 실행한다(모든 장을 순회하며 JS 오류와 가로 넘침을 잡는 유일한 검사다). `verify_documents.py`는 `complete=True`로 생성하므로 새 절·실험·출처는 모두 `complete`·검토 완료 상태여야 한다.

### 4.6 테스트 추가 정책

사용자 공통 규칙에 따라 "지우면 어떤 실제 버그를 놓치는가"에 답할 수 있는 검사만 추가한다.

| 추가할 검사 | 놓치게 될 버그 |
|---|---|
| 생성기: 모르는 컴포넌트 id·중복·미사용 거부(`content_checks.py` 한 테스트에 묶음) | 원고의 오타로 빈 상자가 출판되는 것 |
| 생성기: 워크스루 `focus` 선택자가 0개 또는 2개 이상이면 거부 | 엉뚱한 줄을 강조해 잘못된 설명이 되는 것 |
| 그림 엔진: 링 렌더러가 `used=capacity`와 `used=0`을 다른 호(弧)로 그리는지, wrap 구간이 두 조각으로 나뉘는지 | 링 실험이 가득 참과 빈 상태를 같은 그림으로 보여 주는 것 |
| 시나리오 테스트: `view().visual.kind`가 `BookFigures.kinds`에 있는지 | 모델 쪽 오타로 실험이 범용 상자로 조용히 되돌아가는 것 |
| 브라우저 검사: 워크스루 다음/이전, 그림 재생·정지, 퀴즈 선택 저장, 보관 자료 토글, 용어 팝오버 Esc 복귀 | 실제 독자 조작에서 JS 오류·포커스 유실 |

추가하지 않는 것: 렌더러의 SVG 문자열 스냅숏 비교, 퀴즈 정답 수 세기 같은 데이터 복사 테스트, 각 그림 종류별 분리된 사소한 케이스.

### 4.7 예산

- `index.html` 4MiB 이하(생성기가 초과 시 실패). 목표 2.8MB.
- 첫 렌더 2초 이하, 실험 단계 100ms 이하, 검색 100ms 이하(기존 브라우저 검사 유지). 그림 단계 전환 50ms 이하(브라우저 검사에 추가).
- 보관 자료 숨김 상태에서 인쇄 PDF가 현재(모두 펼침)보다 작아야 한다.

---

## 5. 태스크 상세

### T1. 컴포넌트 선언 규약과 빌드 검증 확장

**목적.** 이후 모든 태스크가 사용할 선언형 컴포넌트 기반(정의 파일, 생성기 검증, JS 레지스트리, 인쇄 대체물 훅)을 만든다. 이 태스크가 끝나면 빈 정의로도 책이 그대로 빌드되어야 한다.

**선행.** 없음.

**변경할 파일.** `build_book.py`, `book.js`, `book.css`, `book_template.html`, `figures.json`·`walkthroughs.json`·`quizzes.json`·`map.json`(신규, 초기에는 빈 목록), `manifest.json`(`modelFiles`에 `models/figure_engine.js` 추가), `models/figure_engine.js`(신규, `kinds`와 빈 `render` 스텁), `tools/book/content_checks.py`, `chapters/a05_book_maintenance.html`(정의 파일 목록 추가).

**구현 방법.**

1. `build_book.py`
   - `Fragment.handle_starttag`에 `data-figure`, `data-walkthrough`, `data-compare`, `data-quiz`, `data-term`, `data-map` 수집을 추가한다(`self.components = {kind: [ids]}`).
   - `collect_components(definitions, fragments, evidence, glossary, chapters)`를 추가한다. 4.2의 검사를 모두 수행하고, 워크스루 `focus`의 줄 번호를 계산해 결과에 포함한다. 실패 메시지는 "F-C12-1: unknown figure"처럼 id를 앞에 둔다.
   - `data`에 `figures`, `walkthroughs`, `compares`, `quizzes`, `map`을 넣는다. 생성 HTML 크기가 4MiB를 넘으면 `ValueError`.
   - 그림 종류 목록은 `figure_engine.js`에서 정규식으로 읽지 말고 생성기 상수 `FIGURE_KINDS`로 두고, Node 테스트가 두 목록의 일치를 확인한다(불일치는 실제 버그다).
2. `book.js`
   - `const components = {figure: initFigure, walkthrough: initWalkthrough, compare: initCompare, quiz: initQuiz, map: initMap};` 레지스트리와 `initComponents(article)`를 만들고 `renderRoute`에서 `initLab` 직후 호출한다. 이 태스크에서는 각 init이 "집필 중" 문장만 넣는 스텁이어도 된다.
   - `stopPlayback`이 그림 재생 세션도 멈추도록 `playbackSessions` 집합을 두고 실험·그림이 함께 등록한다.
   - `preparePrint`에 `printComponents(clone)` 훅을 추가한다. 스텁은 선언 요소를 `p.small`("인쇄판에서는 생략")로 바꾼다. 이후 태스크가 종류별 대체물을 채운다.
   - `persisted` 스키마 3 이관(4.4).
   - 용어 팝오버·그림 확대용 `dialog#figureDialog`, `div#termPopover`를 템플릿에 추가하고 닫기·포커스 복귀를 기존 `sourceDialog` 패턴으로 구현한다.
3. `book.css`: `.figure`, `.figure-controls`, `.walkthrough`, `.compare`, `.quiz`, `.map`, `.term`, `.predict`, `.recap`(핵심 세 줄), `.mistake`(자주 하는 실수) 클래스의 기본 스타일을 정의한다. 색은 기존 변수만 쓴다. 720px 이하 배치도 함께 정의한다.
4. `content_checks.py`: `test_component_references_are_validated` 하나를 추가한다. 임시 복사본에서 (a) 원고에 모르는 `data-figure`, (b) 정의만 있고 미사용, (c) `focus.start`가 두 번 나오는 워크스루 세 경우가 각각 `ValueError`인지 확인한다. 기존 `self.change` 헬퍼를 쓴다.

**검증 방법.** 공통 검증 명령 전체. 추가로 `python -c "import json;json.load(open('docs/interactive_book/figures.json'))"` 류의 수동 확인은 불필요하다(생성기가 읽는다).

**완료 기준.** 빈 정의 파일 4개로 빌드가 성공하고 `index.html` 크기 증가가 50KB 이하다. 원고에 `data-figure="F-없음"`을 넣으면 빌드가 id를 명시하며 실패한다. 브라우저 검사가 기존과 동일하게 통과한다. A05에 정의 파일과 검사 항목이 적혀 있다.

### T2. 보관 자료 분리와 기본 숨김

**목적.** 입문 독자가 본문·검색·인쇄에서 과거 문서 보관 블록을 만나지 않게 한다. 보관 블록 자체와 `migration.json`의 보존 해시는 건드리지 않는다.

**선행.** 없음(T1과 독립).

**변경할 파일.** `manifest.json`(각 장 `sections[]`의 `*-prior-notes`, A03 `decisions-archive`, A04 `history-originals`에 `"archive": true`), `build_book.py`, `book.js`, `book.css`, `book_template.html`(도구 모음에 토글 버튼), `chapters/c01_reading_guide.html`(보관 자료 설명 한 문단), `chapters/a05_book_maintenance.html`.

**구현 방법.**

1. 생성기: `archive: true` 절은 자식으로 `h2`, `p`, `details.archive`만 허용한다(다른 요소가 있으면 실패). 보관 절의 `section` 요소에 `data-archive` 속성이 있어야 한다(원고에 직접 적는다). 보관 블록 개수를 세어 `BOOK.manifest.chapters[].archiveCount`로 포함한다.
2. `book.js`
   - `persisted.showArchive`(기본 false). 도구 모음 버튼 "보관 자료 표시/숨김". 숨김 상태에서는 `section[data-archive]`에 `hidden`을 주고, 목차에서 그 절을 빼며, 장 끝에 "이전 문서 보관 자료 N건 보기" 버튼을 넣어 누르면 그 장만 펼친다(전역 토글과 별개의 장 단위 펼침).
   - 검색 색인 생성 시 `node.closest('[data-archive]')`인 항목에 `archive: true`를 붙이고, 숨김 상태에서는 결과에서 제외한다. 표시 상태에서는 유형을 "보관"으로 표시한다.
   - URL `section=`이 보관 절을 가리키면 그 장만 자동으로 펼친다(기존 링크 보존).
   - 인쇄: 숨김 상태에서는 보관 절을 인쇄 DOM에서 제거한다. 표시 상태에서는 현재처럼 펼쳐 인쇄한다.
3. A03·A04는 부록이므로 장 자체는 목차에 남기고 보관 절만 같은 규칙을 적용한다.

**검증 방법.** 공통 검증 명령. 브라우저 검사에 추가: 기본 상태에서 `#navigation`에 `prior-notes` 링크가 없고, 검색 "원문 SHA-256"이 0건이며, 토글 후 1건 이상이고, 토글 상태가 새로고침 뒤 유지된다. 인쇄 PDF 크기가 토글 전후로 다르다.

**완료 기준.** 기본 상태의 C12 화면에 보관 블록이 보이지 않고 장 끝 버튼으로만 열린다. `migration.json`·`inventory.py`·`verify_documents.py`가 변경 없이 통과한다. `index.html` 크기는 변하지 않는다(숨김은 런타임).

### T3. 용어 툴팁과 입문 용어집 확장

**목적.** 본문에서 용어를 누르면 바로 뜻과 관련 장을 보게 한다. 신입 독자용 용어를 보강한다.

**선행.** T1.

**변경할 파일.** `glossary.json`, `book.js`(`initTerms`, `#termPopover`), `book.css`, `chapters/p01_server_basics.html`·`p02`·`p03`·`c02`·`c04`·`c13`(첫 등장 용어에 `dfn` 표시), `chapters/a02_glossary.html`(표 설명 갱신).

**구현 방법.**

1. `glossary.json`에 최소 30개를 추가한다. 후보: 소켓, 포트, listen/accept, 바이트 스트림, 길이 접두 헤더, 빅 엔디언, 콜백, 스레드, 워커, 락(잠금), 원자 변수, 세대(generation), 슬롯, 풀, 링 버퍼, 이중 매핑, 등록 버퍼, RQ, CQ, 완료 배치, 배압, half-close, FIN, keepalive, idle timeout, graceful/immediate, 유계 큐, 샤드, 틱, 브로드캐스트, span, expected. 각 항목은 "처음 읽는 사람용 한 문장 + 책에서의 정확한 의미 한 문장"으로 `definition`을 쓰고 `aliases`에 영문·한글·코드 이름을 넣는다. 관련 `chapters`는 실제 설명이 있는 장만 적는다.
2. 원고에서 장마다 **첫 등장 한 번만** `<dfn data-term="id">표시어</dfn>`로 감싼다. 같은 용어를 장 안에서 반복 표시하지 않는다.
3. `initTerms(article)`: `dfn[data-term]`에 `tabindex="0"`, `role="button"`, `aria-haspopup="dialog"`를 주고, 클릭·Enter·Space로 `#termPopover`를 연다. 내용: 용어, 정의, "자세히: C05 · C14" 링크(기존 `chapterUrl`). 위치는 요소 아래, 화면 밖이면 위로. Esc·바깥 클릭·다른 용어 열기로 닫고 포커스를 되돌린다. 좁은 화면(≤720px)에서는 화면 하단 고정 시트로 표시한다.
4. 인쇄: `dfn`은 밑줄 없이 본문 글자로만 남긴다(용어 표는 A02에 있다).

**검증 방법.** 공통 검증 명령. 브라우저 검사: P01의 첫 `dfn`을 Enter로 열고 Esc로 닫았을 때 `document.activeElement`가 그 `dfn`이다. 생성기는 `data-term` id가 용어집에 없으면 실패한다(T1 검사에 포함).

**완료 기준.** 용어 55개 이상, P01~P03·C02·C04·C13에 `dfn`이 각각 5개 이상, 검색에서 새 용어의 별칭이 동작한다.

### T4. 그림 엔진 1 시퀀스와 상태도

**목적.** 장면을 보여 주는 두 종류의 그림(스윔레인 시퀀스, 상태 전이도)을 JSON 정의에서 그리고 단계별로 재생한다.

**선행.** T1.

**변경할 파일.** `models/figure_engine.js`(`sequence`, `state` 렌더러), `book.js`(`initFigure`: 컨트롤·단계 설명·동기화·확대·인쇄 대체물), `book.css`, `figures.json`(첫 그림 3개), `tools/book/figure_tests.cjs`(신규, 최소), `chapters/p03_echo_overview.html`·`c03_public_api.html`·`c16_connection_close.html`(첫 그림 배치).

**구현 방법.**

1. `figures.json` 스키마(시퀀스):

```json
{"id": "F-P03-1", "kind": "sequence", "title": "Echo 패킷 하나의 왕복",
 "lanes": [{"id": "client", "label": "클라이언트", "role": "remote"},
           {"id": "kernel", "label": "Windows RIO", "role": "kernel"},
           {"id": "worker", "label": "I/O 워커", "role": "thread"},
           {"id": "app", "label": "EchoHandler", "role": "app"}],
 "steps": [
  {"title": "수신 요청을 미리 게시한다", "text": "워커가 수신 링의 빈 칸으로 RIOReceive를 게시한다. 아직 데이터는 없다.",
   "messages": [{"from": "worker", "to": "kernel", "label": "RIOReceive(빈 칸)", "kind": "call"}],
   "evidence": "receive.post", "walkthrough": {"id": "W01", "step": 4}},
  {"title": "바이트가 도착하고 완료가 기록된다", "text": "...",
   "messages": [{"from": "client", "to": "kernel", "label": "7바이트", "kind": "data"},
                {"from": "kernel", "to": "worker", "label": "완료(7)", "kind": "completion"}]}
 ],
 "caption": "단계 간격은 실제 시간이 아니다. 다른 세션의 실행은 생략했다."}
```

   - 렌더러는 각 레인을 세로 생명선으로, 단계의 메시지를 위에서 아래로 쌓는다. 현재 단계까지의 메시지만 진하게 그리고 이후는 흐리게 그린다(전체 모양을 미리 보여 주되 진행을 표시). 각 메시지에는 단계 번호 원을 붙인다.
   - `kind`별 선: call 실선, callback 점선(`--gold`), completion 이중선, data 굵은 선, blocked 빨간 X.
   - 선택 속성 `notes: [{lane, step, text}]`로 레인 옆 말풍선을 그린다(예: "콜백 동안 span 유효").
2. 상태도 스키마:

```json
{"id": "F-C16-1", "kind": "state", "title": "세션 수명 상태",
 "states": [{"id": "free", "label": "Free"}, {"id": "active", "label": "Active"},
            {"id": "draining", "label": "Draining"}, {"id": "closing", "label": "Closing"},
            {"id": "quarantined", "label": "Quarantined"}],
 "transitions": [{"from": "free", "to": "active", "label": "accept 성공", "step": 1}, ...],
 "layout": "row",
 "steps": [{"title": "...", "text": "...", "active": "active", "evidence": "session.lifecycle"}],
 "note": "실제 enum 이름이다. 그림의 단순화는 캡션에 적었다."}
```

   - 상태 id는 가능하면 실제 enum 값과 같은 이름으로 쓰고 캡션에 출처 버튼을 둔다.
3. `initFigure(host)`: 제목, SVG 영역, 컨트롤(처음·이전·재생/정지·다음·단계 칩 n개), 설명 영역(`role="status"`), "실제 코드" 버튼(단계의 `evidence`), "코드 따라가기에서 보기" 버튼(단계의 `walkthrough`), "확대" 버튼, 캡션. 재생 간격은 실험과 같다(900ms, reduced-motion 1400ms). 단계 칩은 `aria-pressed`.
   - 동기화 API: `figureSessions.get(id).setStep(n)`를 두어 워크스루(T7)와 지도(T9)가 호출한다.
   - 인쇄 대체물: 마지막 단계 SVG(모든 메시지 진하게) + `ol`로 단계 제목·설명.
4. `figure_tests.cjs`: 시퀀스 렌더러가 단계 n에서 `activeIds`에 n단계까지의 메시지 id만 담는지 한 테스트. 상태도는 전이 `step`이 단계 수를 넘으면 `render`가 throw하는지 한 테스트(정의 오류를 빌드가 아니라 브라우저에서 조용히 넘기는 버그를 잡는다). 이 두 가지 외에는 추가하지 않는다. `package.json`의 `test:models`에 파일을 넣는다.
5. 첫 그림 3개를 배치한다: F-P03-1(Echo 왕복 시퀀스, P03 `echo-ov-flow`), F-C03-1(`ServerState` 상태도, C03 `api-lifecycle`), F-C16-1(세션 수명 상태도, C16 `close-reasons`). 각 원고에 "그림으로 보기" 문단을 쓴다.

**검증 방법.** 공통 검증 명령. 브라우저 검사: P03에서 `[data-figure="F-P03-1"]`의 "다음" 3회 후 설명 영역 텍스트가 3단계 제목을 포함하고, "재생"→"정지" 버튼 전환이 되며, 장 이동 시 타이머가 멈춘다(`playbackSessions` 비어 있음). 단계 전환 50ms 이하.

**완료 기준.** 두 렌더러가 정의에서 그려지고, 그림 3개가 밝은/어두운 화면과 390px 폭에서 깨지지 않으며(가로 스크롤 허용, 페이지 넘침 금지), 인쇄 DOM에 단계 목록이 들어간다.

### T5. 그림 엔진 2 링 슬롯 큐 바이트 의존 그래프

**목적.** 메모리와 자원 구조를 보여 주는 렌더러를 만든다. 실험(T6)과 그림이 공유한다.

**선행.** T1. (T4와 병행 가능)

**변경할 파일.** `models/figure_engine.js`, `figure_tests.cjs`, `figures.json`(정적 사용 예 각 1개), `book.css`, `chapters/c09_ring_buffer.html`·`c10_accept_and_session.html`·`c17_server_shutdown.html`(그림 배치).

**구현 방법.** 렌더러별 spec과 표현:

| kind | spec 필드 | 그리는 것 |
|---|---|---|
| `ring` | `capacity, read, write, used, posted, labels?` | 왼쪽: 원형 링(칸 = capacity, 미소비 구간 채움, 게시 구간 빗금, read/write 바늘). 오른쪽: 길이 2×capacity의 직선 띠로 이중 매핑 두 view와 wrap을 넘는 연속 구간. `used=0`과 `used=capacity`를 다른 모양으로 그린다. |
| `slots` | `slots: [{index, owner, generation, state}], free: [index], workers` | 워커별 열로 나눈 슬롯 표. 상태별(비어 있음·사용·격리) 채움. free list 순서를 번호로 표시. |
| `queue` | `shards: [{slots: [...], head, count}], gates?: [{session, parked}]` | 샤드별 가로 칸 배열, head 화살표, 가득 참 표시, parked close 아이콘. |
| `bytes` | `bytes: [...], headerBytes, arrived, consumed, marks?` | 기존 L09 바이트 띠를 일반화. 16진·십진 토글은 하지 않는다(단순 유지). |
| `graph` | `nodes: [{id,label,state}], edges: [{from,to,label}]` | 종료 의존 관계 DAG. 상태: 대기·진행·완료·차단. 위상 정렬로 세로 배치. |
| `layers` | `layers: [{id,label,nodes:[{id,label,sub}]}], links` | 시스템 지도 바탕(T9에서 사용). P02의 현재 SVG 구조를 재현한다. |

- `figure_tests.cjs`에 링 테스트 하나(4.6의 "링 렌더러" 항목)만 추가한다.
- 정적 그림 배치: F-C09-1(링 8칸, read=6, write=2, used=4: wrap 상태), F-C10-2(슬롯 6개·워커 2개, interleaved free list), F-C17-1(종료 의존 그래프: accept 중지 → 신규 진입 차단 → 송신 완료 → 소켓 닫기 → 수신 취소 완료 → 워커 join → 등록 버퍼 해제 → CQ 해제). 각 그림에 단계를 둘 수 있다(`steps[].spec` 덮어쓰기로 상태 변화 표현).

**검증 방법.** 공통 검증 명령. 브라우저 검사의 전 장 순회가 렌더 오류를 잡는다.

**완료 기준.** 다섯 렌더러가 spec에서 그려지고, 링 테스트가 통과하며, 세 그림이 본문에 보인다.

### T6. 실험 20개의 전용 시각화 전환

**목적.** 실험의 범용 상자 그림을 상태에 맞는 전용 그림으로 바꾼다. 모델의 전이 로직과 기존 테스트는 바꾸지 않는다.

**선행.** T4, T5.

**변경할 파일.** `models/network_models.js`, `models/packet_framing.js`, `models/partial_send.js`, `models/configuration.js`(각 `view()`에 `visual` 추가), `book.js`(`visualState`가 `view.visual`이 있으면 `BookFigures.render`로 그림), `tools/book/scenario_tests.cjs`(visual.kind 검사 한 줄), `tools/book/verify_reader.cjs`(기존 `svg[role="group"]` 검사가 새 SVG에서도 참이 되도록 `role` 유지).

**구현 방법.**

1. `view()` 반환에 `visual: {kind, ...spec}`를 추가한다. 매핑:

| 실험 | kind | 표현 |
|---|---|---|
| L01 Echo 왕복 | `sequence` | F-P03-1과 같은 레인. 현재 step까지 진하게. |
| L02 게시·완료 | `sequence` | 워커·커널 두 레인. 실패 시 blocked X. |
| L03 콜백 교차 | `sequence` | 워커 A·B, 세션 A·B 네 레인. `order` 배열을 메시지로. |
| L04 등록 메모리 | `graph` | 등록·송신 참조·수신 참조·신규 게시 노드와 해제 조건 간선. 차단 시 빨간 간선. |
| L05 링 버퍼 | `ring` | 그대로 매핑. |
| L06 슬롯 배정 | `slots` | 그대로 매핑. |
| L07 세션 식별 | `slots` + 텍스트 | 슬롯 7 하나를 크게, 세대 변화와 지연 작업 ID 비교. |
| L08 수신 경로 | `ring` | 수신 링: 커널 쓰기 구간(게시)과 콜백 읽기 구간(used) 색 구분. |
| L09 패킷 경계 | `bytes` | 기존 띠 유지(렌더러로 이관). |
| L10 송신 접수 | `ring` | 송신 링: in-flight 구간과 대기 구간. |
| L11 부분 송신 | `bytes` | 기존 메모리 띠를 bytes로 이관, offset 표시. |
| L12 연결 종료 | `state` | 세션 상태도. 현재 상태 강조, 종료 원인 라벨. |
| L13 서버 종료 | `graph` | F-C17-1 그래프. 선택한 순서대로 완료·차단 표시. |
| L14 로직 큐 | `queue` | 샤드 하나, parked close 표시. |
| L15 ChatServer | `queue` + `sequence` | 샤드 n개 큐와 룸 방송 시퀀스 중 현재 단계에 맞는 쪽. 두 개를 세로로 모두 그린다. |
| L16 애플리케이션 큐 | `queue` | 틱별 유입·처리 막대는 HTML 표로, 큐는 렌더러로. |
| L17 설정 계산 | 없음 | 현재 값 표 유지 + 메모리 예산 가로 막대(HTML/CSS). |
| L18 진단 | 없음 | 선택지 표 유지. |
| L19 롤백 | `graph` | 확보 자원 노드 N개, 실패 지점 이후 "미확보", 정리 순서 역순 번호. |
| L20 측정 | 없음 | 표 유지. |

2. `visualState`: `view.visual`이 있으면 `BookFigures.render(visual.kind, visual, visual.step ?? 0)` 결과를 `.diagram-scroll`에 넣는다. 없으면 기존 범용 상자. L09·L11의 전용 코드(`if(session.definition.id==='L09')`)는 제거하고 모델의 `visual`로 대체한다. SVG 루트에는 기존 검사와 같은 `role="group"`과 `aria-label`을 유지한다.
3. 노드 클릭으로 단계 이동하던 기능(`stepSequence`가 있는 실험)은 시퀀스 렌더러의 단계 번호 원에 같은 동작을 연결한다.
4. 모델의 `visual`은 순수 데이터다. 렌더러 호출은 `book.js`에서만 한다(모델 테스트가 DOM 없이 돌아야 한다).

**검증 방법.** 공통 검증 명령. `scenario_tests.cjs`는 각 실험의 모든 접두 상태에서 `visual.kind ∈ BookFigures.kinds`를 확인한다(그림 엔진을 같은 vm 컨텍스트에 로드). 브라우저 검사는 20개 실험을 이미 순회한다.

**완료 기준.** 20개 실험 중 17개가 전용 그림을 보이고(L17·L18·L20 제외), `visualState`에 실험 id 분기가 없으며, 기존 모델 테스트가 수정 없이 통과한다. 대표 화면(L05, L06, L13)의 스크린샷을 `.artifacts/book/reader`에 남긴다.

### T7. 코드 따라가기 컴포넌트와 Echo 워크스루

**목적.** 함수 하나를 단계별로 읽는 "코드 따라가기(워크스루)"를 만들고, 첫 두 개(Echo `main`, `EchoHandler::OnData`)를 집필한다.

**선행.** T1. (T4가 있으면 그림 동기화까지 확인한다.)

**변경할 파일.** `walkthroughs.json`, `build_book.py`(워크스루 검증은 T1에서 구현. 여기서는 `calls` 참조 검증 추가), `book.js`(`initWalkthrough`, `initCompare`), `book.css`, `evidence.json`(필요 시 `echo.main`·`echo.on_data` 구간 재확인), `chapters/p03_echo_overview.html`·`c04_echo_walkthrough.html`.

**구현 방법.**

1. 스키마:

```json
{"schemaVersion": 1,
 "walkthroughs": [
  {"id": "W01", "title": "EchoServer의 main을 처음부터 끝까지 읽는다",
   "evidence": "echo.main", "level": "입문",
   "intro": "서버 프로그램의 뼈대는 설정 → 핸들러 → Create → Start → 기다림 → Stop이다.",
   "steps": [
    {"title": "설정을 채운다", "focus": {"start": "  rionetlite::ServerConfig config;", "end": "  std::string stop_file;"},
     "text": "포트만 바꾸고 나머지는 기본값을 쓴다. 기본값의 의미는 C21에서 본다.",
     "figure": {"id": "F-P03-1", "step": 0}, "term": "config"},
    {"title": "서버보다 오래 살 객체를 먼저 만든다", "focus": {"start": "  ConsoleLogger logger;", "end": "  if (!server) {"},
     "text": "handler와 logger는 Server가 빌려 쓴다. 이 둘이 먼저 사라지면 콜백이 사라진 객체를 부른다.",
     "mistake": "X-C04-1"},
    {"title": "...", "focus": {...}, "calls": "W02"}
   ]}
 ],
 "compares": [
  {"id": "X-C04-1", "title": "핸들러를 서버보다 먼저 없애는 실수",
   "wrong": {"label": "학습용 예시 · 이렇게 하면 안 된다", "code": "auto server = ...;\n{\n  EchoHandler handler(...);\n  ...\n}  // handler 소멸, server는 아직 살아 있다"},
   "right": {"label": "실제 코드", "evidence": "echo.main", "focus": {"start": "  ConsoleLogger logger;", "end": "  handler.Attach(*server);"}},
   "why": "Server::Create 주석의 'handler and logger must outlive this Server instance'가 그 이유다.",
   "evidence": "api.server_contract"}
 ]}
```

2. `initWalkthrough(host)` 화면 구성(넓은 화면: 좌우 2열, 좁은 화면: 세로):
   - 왼쪽 코드 창: 출처 구간 전체를 실제 줄 번호와 함께 표시(`SOURCES`에서 추출, `escapeHtml`). 현재 단계의 `focus` 줄들을 `.focusline`으로 강조하고 스크롤을 맞춘다. 단계 번호를 여백에 표시한다.
   - 오른쪽: 단계 목록(`ol`, 현재 단계 `aria-current="step"`), 현재 단계의 설명, 버튼: "이전"·"다음"·"그림에서 보기"(`figure` 있을 때, 같은 장의 그림 세션 `setStep`)·"실제 코드 전체 열기"(기존 `openEvidence`)·"들어가 보기"(`calls` 있을 때, 다른 워크스루로 이동하고 뒤로 가기 스택 유지)·"자주 하는 실수 보기"(`mistake` 있을 때, 해당 비교 블록으로 스크롤).
   - 키보드: 호스트에 포커스가 있을 때 ←/→. 단계 변경은 `role="status"` 영역에 제목만 알린다.
   - 상단 메타: 파일 경로·검토 기준 커밋·SHA-256(코드 탐험기와 같은 문구). "실제 코드" 라벨.
3. `initCompare(host)`: 두 열(좌 `wrong`, 우 `right`). 왼쪽은 `pre`에 학습용 라벨을 붙이고 테두리를 `--danger`로, 오른쪽은 출처에서 추출한 실제 코드. 아래에 `why`와 출처 버튼.
4. 인쇄 대체물: 코드 전체 + 여백 단계 번호 + 단계 목록. 비교 블록은 그대로 인쇄.
5. 집필: W01(Echo main, 7단계: 설정 → 수명 객체 → Create 실패 처리 → Attach → Start → 종료 신호 대기 루프 → Stop), W02(`OnData`, 5단계: Decode 호출 → 패킷 콜백에서 Send → 결과별 정책 → 디코드 오류 처리 → 소비량 반환). X-C04-1(수명 순서), X-C04-2(`OnData`의 span을 저장하는 실수 vs 복사). P03 `echo-ov-code`에 W01, C04 `echo-on-data`에 W02와 비교 블록을 배치하고 본문을 "그림 → 워크스루 → 실수" 순서로 고친다.

**검증 방법.** 공통 검증 명령. 브라우저 검사: C04의 W02에서 "다음"을 두 번 누르면 `.focusline` 줄 번호가 단계 정의의 계산 줄과 같고, "들어가 보기" 후 "돌아가기"가 이전 워크스루 단계를 복원한다. 생성기 검사: `focus.start`가 출처 텍스트에 두 번 나오면 실패(T1 테스트).

**완료 기준.** W01·W02가 P03·C04에서 동작하고, 두 비교 블록이 보이며, 인쇄 DOM에 코드와 단계 목록이 있다.

### T8. 핵심 경로 워크스루 10개 집필

**목적.** 연결·수신·송신·종료·정지·로직 큐의 실제 코드를 단계별로 설명한다. 이 태스크가 "주요 코드 이해"의 본체다.

**선행.** T7. 출처 추가가 필요하므로 `evidence.json` 검토 규칙(해시는 자동 갱신하지 않고 검토 후 기록)을 따른다.

**변경할 파일.** `walkthroughs.json`, `evidence.json`, 해당 장 원고 10개.

**구현 방법.** 각 워크스루는 5~9단계, 단계마다 `text` 2~4문장, 가능하면 `figure`·`mistake`·`term`을 연결한다. 아래 표의 출처는 기존 64건에 있으면 그대로 쓰고, 없으면 `kind: implementation`으로 추가한다(`testSource`·`testName`은 실제 선언이 있는 테스트로 고른다. 후보: `tests/integration/smoke_test.cpp`, `tests/unit/worker_control_test.cpp`, `rio_request_test.cpp`, `public_api_test.cpp`, `config_test.cpp`, `chat_protocol_test.cpp`).

| ID | 장·절 | 함수(파일) | 단계 요지 |
|---|---|---|---|
| W03 | C10 `accept-first-receive` | `Accepter::Run` (`server_net_accepter.h`) | accept 대기 → 실패 시 백오프 → 빈 슬롯 빌리기(없으면 거절) → 담당 워커의 CQ 선택 → `NewSession` → `OnConnect` 콜백 → 첫 수신 게시 → `CompleteConnectCallback`로 슬롯 보호 해제 |
| W04 | C12 `receive-validation` | `IoWorkerThread` + `ProcessRIOCQ` 분기부 (`server_net_io_worker.h`) | 루프 조건 → Dequeue → corrupt CQ → 컨텍스트 null → 소유 워커 불일치 → 세대 불일치(stale) → status 오류 → 0바이트(FIN) → 수신/송신 분기 → 지연 종료 처리·idle 스윕 → 빈 폴링 대기 |
| W05 | C12 `receive-callback` | `ProcessRIOCQ` 수신 분기 + `TcpSession::RecvCompletion`·`PostRecv`·`ConsumeRecvBuffer` (`tcp_session.h`) | 닫힌 세션의 지연 완료 → 완료 길이 검사와 링 반영 → 다음 수신 선게시(가득 차면 보류) → `InvokeReceiveCallback` → 소비량 검사 → 보류했던 게시 재시도 |
| W06 | C13 `framing-decoder` | `LengthPrefixedDecoder::Decode` (`packet_decoder.h`) | 헤더 4바이트 확보 조건 → 빅 엔디언 길이 읽기 → 길이 하한 오류 → 상한 오류 → 미완성이면 반환 → 핸들러 호출 → 소비량·개수 증가 → 반복 |
| W07 | C14 `send-admission`, `send-coalescing` | `Server::Send` → `TcpSession::PostSend` → `SendImmediately::PostSendScatter`·`PreparePostLocked`·`IssuePost` (`send_immediately.h`) | 크기 검사 → 잠금 안: 접수 허용·세션 uid·여유 공간 → 링에 복사 → in-flight면 반환 → 요청 준비 → 잠금 밖 syscall → 실패 정리 |
| W08 | C15 `repost-remainder` | `SendImmediately::Completed` | 완료 길이 검사 → 링 읽기 위치 전진 → 남은 바이트가 있으면 재게시 → 없으면 in-flight 해제 → 실패 분기 |
| W09 | C16 `close-peer-drain`, `close-mailbox` | `ProcessRIOCQ`의 0바이트 분기 → `TcpSession::BeginPeerClose` → 지연 종료 관리자 → `TcpSession::Disconnect` | FIN 관찰 → 새 송신 차단 → 지연 목록 등록 → 미완료 송신 대기와 기한 → 종료 승자 결정 → 소켓 닫기 → 닫힘 통지 한 번 → 격리와 반환 |
| W10 | C17 `shutdown-order` | `Server::Stop` → `ServerNetManagerImpl::End` (`server_net.h`) | 종료 소유자 결정 → accept 중지 → 신규 진입 차단 → graceful 대기 → 소켓 닫기 → 완료 배출 → 워커 join → 기한 초과 처리 → 자원 해제 순서 |
| W11 | C18 `queue-consumption`, `parked-close` | `QueuedEventAdapter::Impl::Push`, `TryPop` (`queued_event_adapter.cpp`) | 샤드 선택 → 슬롯 확보 실패 정책 → payload 복사 → 세션 게이트와 parked close → `TryPop`의 세 결과 → 깊이 지표 |
| W12 | C19 `chat-shard`, `chat-broadcast` | `ChatApplication::RunShard` (`samples/chat_server/main.cpp`) | 루프 조건 → `TryPop` → 이벤트 종류별 처리(연결·종료·데이터) → `ConsumeInput` → 방송 큐 소비와 룸 필터 → idle 스캔 → 진행 없으면 sleep |

각 장 원고에서 워크스루 선언을 해당 절에 넣고, 절의 본문을 "먼저 결론 한 문단 → 워크스루 → 보충 설명" 순서로 재배치한다. 워크스루가 설명하는 내용을 본문이 중복 서술하지 않게 줄인다.

**검증 방법.** 공통 검증 명령. 새 출처마다 `reviewNote`에 검토 일자와 대조한 테스트를 적는다. 브라우저 검사의 전 장 순회로 렌더 오류를 확인한다.

**완료 기준.** W03~W12가 모두 동작하고, 각 단계의 `focus`가 설명 문장과 일치하는지 저자가 코드 창에서 직접 확인했다는 기록을 이 계획 문서의 검증 기록에 남긴다. 출처 수는 80건 이상이 된다.

### T9. 인터랙티브 시스템 지도와 장별 위치 표시

**목적.** "큰 그림 먼저"를 화면으로 만든다. P02의 지도를 누를 수 있게 하고, 모든 장의 머리에 현재 위치를 표시한다.

**선행.** T5(`layers` 렌더러).

**변경할 파일.** `map.json`, `manifest.json`(`chapters[].mapFocus`), `book.js`(`initMap`), `book.css`, `chapters/p02_library_overview.html`, `c06_architecture.html`(기존 `data-component` 패널을 지도로 대체), 모든 C장 머리(미니 지도 선언 한 줄).

**구현 방법.**

1. `map.json`:

```json
{"schemaVersion": 1, "id": "system",
 "layers": [
  {"id": "app", "label": "애플리케이션", "nodes": [
    {"id": "handler", "label": "EventHandler 구현", "thread": "콜백을 부른 스레드", "owns": ["Player·룸 등 게임 상태(로직 워커 소유 권장)"],
     "does": ["OnConnected·OnData·OnClosed 처리", "Send·RequestClose 호출"], "chapters": ["P02", "C03", "C04", "C07"], "evidence": ["api.callback_contract"], "labs": ["L03"]},
    {"id": "logic-queue", "label": "QueuedEventAdapter", ...},
    {"id": "logic-worker", "label": "게임 로직 워커", ...}]},
  {"id": "public", "label": "공개 Server", "nodes": [{"id": "server", ...}]},
  {"id": "transport", "label": "RIONetLite 내부", "nodes": [
    {"id": "accepter", ...}, {"id": "session-pool", ...}, {"id": "session", ...},
    {"id": "recv-ring", ...}, {"id": "send-ring", ...}, {"id": "io-worker", ...}, {"id": "lazy-close", ...}]},
  {"id": "os", "label": "Windows 소켓·RIO", "nodes": [{"id": "rio", ...}, {"id": "kernel-socket", ...}]}],
 "links": [{"from": "handler", "to": "server", "label": "Send", "kind": "call"}, {"from": "io-worker", "to": "handler", "label": "OnData", "kind": "callback"}, ...],
 "scenes": [{"id": "accept", "label": "② 연결", "nodes": ["accepter", "session-pool", "session"], "chapters": ["C10", "C11"]}, ...]}
```

2. 전체 지도(`initMap` 기본): `layers` 렌더러로 그린 SVG의 부품을 누르면 오른쪽(좁은 화면: 아래) 패널에 역할·하는 일·소유·스레드·관련 장 링크·실험 링크·"실제 코드" 버튼을 표시한다. 상단에 장면 칩(①~⑥)을 두고 누르면 그 장면의 부품만 강조한다. Echo/Chat 전환 토글은 `links`에 `mode` 속성으로 구현한다(기존 C06 토글 대체).
3. 미니 지도(`data-mini`): 같은 렌더러를 작게(높이 110px 내외) 그리고 `data-focus`의 부품만 채운다. 클릭하면 P02 전체 지도의 해당 부품으로 이동한다(`#chapter=library-overview&section=overview-inside` + 포커스 부품 파라미터 `map=`). 제목 없이 한 문장 캡션만 둔다.
4. `manifest.json`의 모든 C장에 `mapFocus`를 적는다(예: C12 → `["io-worker","recv-ring","handler"]`). 생성기는 `mapFocus`의 id가 `map.json`에 있는지 검사한다.
5. 인쇄 대체물: 전체 지도는 정지 SVG + 부품 표(라벨·역할·장). 미니 지도는 캡션 문장만.

**검증 방법.** 공통 검증 명령. 브라우저 검사: P02 지도에서 `io-worker`를 Enter로 선택하면 패널에 "C12" 링크가 있고, C12 머리의 미니 지도를 누르면 P02로 이동하며 같은 부품이 선택된다.

**완료 기준.** 지도 부품 12개 이상, 장면 6개, 모든 C장에 미니 지도가 있고, C06의 기존 `data-component` 패널이 지도로 대체되어 `renderArchitecture` 특수 코드가 `book.js`에서 사라진다.

### T10. 학습 경로 선택과 진도 표시

**목적.** 독자가 자기 경로를 고르고, 목차와 장 끝에서 "다음 읽을 장"과 진도를 보게 한다.

**선행.** 없음(T1과 독립). T11이 있으면 퀴즈 결과를 진도에 포함한다.

**변경할 파일.** `manifest.json`(`readingRoutes[]`에 `id`, `description`, `audience` 추가, 경로 구성 재검토), `book.js`, `book.css`, `book_template.html`(목차 상단 경로 선택), `chapters/c01_reading_guide.html`(경로 카드 재작성).

**구현 방법.**

1. 경로를 다음 네 개로 정리한다(기존 네 개를 대상별로 다시 쓴다):
   - `first`: 처음 읽는 사람 — C01 → P01 → P02 → P03 → C02 → C04 → C13 → C03 → C06 → C07 → C10 → C12 → C14 → C16 → C18 → C19 → C20
   - `run-first`: 서버를 먼저 실행해 보기 — C01 → C02 → P03 → C04 → C13 → C19 → C20 → C21
   - `internals`: 내부 동작 이해하기 — P02 → C06 → C05 → C08 → C09 → C10 → C11 → C12 → C14 → C15 → C16 → C17
   - `operate`: 문제를 찾고 고치기 — C21 → C22 → C23 → C24 → C25 → C26 → C27 → C28
2. C01을 "시작 페이지"로 고친다: 책 소개 세 문단, 경로 카드 4개(대상·소요 장 수·끝나면 할 수 있는 일·"이 경로로 읽기" 버튼), 그 아래 기존 절들. 버튼은 `data-route="first"` 선언으로 두고 JS가 `persisted.route`를 설정한다.
3. 목차: 경로가 선택되면 상단에 "경로: 처음 읽는 사람 · 5/17 · 다음: C13 패킷 경계" 요약과 "경로 순서로 보기/부 순서로 보기" 토글. 경로 순서 보기에서는 경로의 장만 순서대로 나열하고 완료 표시(장의 `*-understood` 체크)를 붙인다. 경로 밖 장은 접힌 "그 밖의 장"에 둔다.
4. 장 끝: "경로의 다음 장" 버튼(경로 미선택 시 "다음 장"). 기존 `#next`는 유지한다.
5. 진도는 `data-check` 체크 수로 계산한다. T11 이후에는 퀴즈 정답 수를 괄호로 덧붙인다("5/17 · 퀴즈 12/20").

**검증 방법.** 공통 검증 명령. 브라우저 검사: C01에서 경로 버튼을 누른 뒤 새로고침해도 목차 상단 요약이 남고, C13의 체크를 켜면 요약 숫자가 1 증가한다.

**완료 기준.** 경로 4개가 동작하고, 경로의 모든 장 id가 manifest 검증을 통과하며, C01이 시작 페이지 구성을 갖는다.

### T11. 확인 퀴즈와 예측 질문 컴포넌트

**목적.** 읽은 내용을 스스로 확인하게 한다. 틀려도 진행을 막지 않고, 해설이 핵심 설명으로 이어진다.

**선행.** T1.

**변경할 파일.** `quizzes.json`, `book.js`(`initQuiz`), `book.css`, 입문·중급 장 원고(퀴즈 선언과 `details.predict`).

**구현 방법.**

1. 스키마:

```json
{"id": "Q-C13-1", "chapter": "C13",
 "question": "클라이언트가 7바이트 패킷 한 개를 보냈는데 서버의 OnData가 세 번 불렸다. 무엇이 정상인가?",
 "options": [
  {"text": "네트워크 오류다. 연결을 끊어야 한다.", "explain": "아니다. TCP는 경계 없는 바이트 흐름이라 한 패킷이 여러 번에 나뉘어 올 수 있다."},
  {"text": "정상이다. 완성될 때까지 0을 반환하면 된다.", "correct": true, "explain": "맞다. 2+3+2로 와도 세 번째 호출에서 7을 소비한다. L09에서 확인한다."},
  {"text": "정상이지만 매번 받은 만큼 소비량으로 돌려줘야 한다.", "explain": "아니다. 불완전한 앞부분을 소비하면 다음 호출에서 헤더를 잃는다."}],
 "evidence": "framing.decode", "lab": "L09"}
```

2. `initQuiz(host)`: 문항, 라디오 보기(`fieldset/legend`), "확인" 버튼. 확인 후 선택한 보기의 해설과(틀렸다면) 정답 보기의 해설을 함께 표시하고, "실제 코드"·"실험에서 확인" 버튼을 보인다. 선택은 `persisted.quiz[id]`에 저장하고 다시 열면 결과 상태로 보인다. "다시 풀기"로 초기화. 정답 여부는 `aria-live`로 한 번 알린다.
3. 예측 질문은 JS 없이 `<details class="predict"><summary>먼저 예측해 보기 · 수신 완료가 3바이트일 때 OnData는 몇 바이트를 받는가?</summary><p>…</p></details>`로 쓴다. 스타일만 추가한다. 그림·실험 바로 앞에 둔다.
4. 집필 분량: 아래 표의 장에 2~4문항씩, 총 60문항 이상. 문항은 "상황 → 무엇이 정상인가/무엇을 해야 하는가" 형식으로 쓰고, 보기 세 개 중 두 개는 실제로 신입이 할 법한 오답으로 만든다. 해설은 장의 설명 문장과 중복되지 않게 한 줄 근거와 링크로 끝낸다.

| 장 | 문항 수 | 주제 예 |
|---|---|---|
| P01 | 3 | 스트림 경계, 동기/비동기, 등록 버퍼의 뜻 |
| P02 | 3 | 콜백 스레드, 세 가지 API, 라이브러리/앱 책임 |
| P03·C04 | 4 | 수명 순서, Send 결과 정책, OnData 반환값 |
| C02 | 2 | 빌드 실패 계층, Debug/Release |
| C03 | 3 | Create/Start 실패 구분, move-only, 콜백 안 Stop |
| C06·C07 | 4 | 세션별 순서·세션 간 동시성, 콜백에서 긴 작업 |
| C10·C11 | 4 | 슬롯 고갈, SessionId 전체 비교, 재사용 조건 |
| C12·C13 | 5 | 선게시, 소비량 0의 의미, 길이 오류 |
| C14·C15 | 4 | kWouldBlock의 뜻, 병합, 부분 완료 |
| C16·C17 | 4 | FIN 뒤 송신, graceful/immediate, 종료 순서 |
| C18·C19·C20 | 6 | span 복사, parked close, 샤드 소유, 틱 순서 |
| C21·C22·C23 | 5 | 설정 관계, 지표 단위, 진단 순서 |
| 심화 장(C05·C08·C09·C24) | 각 2 | 핵심 안전 조건 |

**검증 방법.** 공통 검증 명령. 생성기: 정답 1개·해설 전부·`chapter` 일치·`evidence`/`lab` 존재를 검사한다. 브라우저 검사: C13 퀴즈 하나를 풀고 새로고침 후 결과 상태가 유지된다.

**완료 기준.** 퀴즈 60문항 이상이 동작하고, 예측 질문이 그림·실험 앞에 20개 이상 배치되어 있다.

### T12. 0부 1부 입문 장 원고 보강

**목적.** 신입 독자가 처음 만나는 P01~P03, C02, C04, C13을 2.2의 표준 구성과 2.3의 집필 규칙에 맞게 완성한다.

**선행.** T3, T4, T5, T7, T11(T9·T10은 있으면 반영, 없으면 미니 지도 자리만 비워 둔다).

**변경할 파일.** 해당 원고 6개, `figures.json`, `quizzes.json`, `walkthroughs.json`(비교 블록), `glossary.json`, `manifest.json`(절 추가 시 `sections`).

**구현 방법.** 장별 요구:

| 장 | 그림 | 워크스루·비교 | 퀴즈 | 추가 원고 |
|---|---|---|---|---|
| P01 | F-P01-1 "TCP는 물줄기"(bytes: 보낸 묶음 3개가 도착 조각 2개로), F-P01-2 "동기 대 비동기 주문"(sequence 2레인 × 2판 비교), F-P01-3 "RIO의 등록된 그릇"(ring 정지) | 없음 | 3 | 비유 블록 3개, 자주 하는 오해 표(스레드=세션? 1회 send=1회 recv?) |
| P02 | 시스템 지도(T9) 또는 기존 SVG, F-P02-2 여섯 장면(sequence 1단계=1장면, 각 단계에 장 링크) | 없음 | 3 | "내가 쓰는 코드 vs 라이브러리 코드" 비교 표에 `dfn` |
| P03 | F-P03-1(T4) | W01(T7) | 4(C04와 공유) | 실행 결과 화면을 글로 재현한 `pre`(실제 실행 로그 형식), "다음에 읽을 것" 경로 연결 |
| C02 | F-C02-1 빌드 파이프라인(graph: vcpkg → 라이브러리 → 예제 → 테스트), F-C02-2 실패 계층(state: 환경·빌드·링크·실행 중 어디서 멈췄는가) | 없음 | 2 | 오류 메시지 → 원인 → 조치 표를 "독자가 보는 메시지" 열부터 시작하게 재작성 |
| C04 | F-C04-1 Send 결과 정책(state: kQueued·kWouldBlock·kMessageTooLarge·kStaleSession·kClosing·kUnavailable → 계속/끊기/로그) | W02, X-C04-1, X-C04-2 | 4 | `echo-send-results` 절을 결정 표로, `echo-extension` 절을 T14의 입구로 |
| C13 | F-C13-1 헤더와 본문(bytes 7바이트에 길이 필드 강조), F-C13-2 2+3+2 도착(sequence 3단계, 각 단계의 소비량) | W06, X-C13-1(불완전 앞부분을 소비하는 실수) | 3 | `framing-errors` 절에 "길이 오류 뒤 연결을 끊는 이유" 한 문단 |

공통: 각 장 머리에 미니 지도 선언, 끝에 `.recap`(핵심 세 줄)과 경로 다음 장. 내부 식별자 첫 등장 규칙 적용. 보관 절은 손대지 않는다.

**검증 방법.** 공통 검증 명령. 저자 자체 검수: 2.3의 마지막 질문을 절마다 적용하고 결과를 이 계획 문서의 검증 기록에 장별 한 줄로 남긴다. 스크린샷 `.artifacts/book/reader/p01.png`~`c13.png`.

**완료 기준.** 여섯 장이 표준 구성 8단계를 모두 갖추고(C02는 실험 없음 허용), 그림 10개 이상·퀴즈 19개 이상·비교 블록 3개 이상이 동작한다.

### T13. 2부에서 4부 핵심 장 원고 보강

**목적.** 중급 핵심 장 C03, C06, C07, C10, C12, C14, C16, C18을 표준 구성으로 완성한다.

**선행.** T4, T5, T6, T8, T11.

**변경할 파일.** 해당 원고 8개, `figures.json`, `quizzes.json`, `walkthroughs.json`, `manifest.json`.

**구현 방법.** 장별 요구:

| 장 | 그림 | 워크스루·비교 | 퀴즈 | 원고 수정 요지 |
|---|---|---|---|---|
| C03 | F-C03-1(T4), F-C03-2 소유 관계(layers 축소: 앱이 handler·logger 소유, Server가 Impl·transport 소유) | X-C03-1 콜백 안에서 Stop 호출 | 3 | 각 절 첫 문단을 "상황"으로 시작. `expected` 오류 처리 예를 실제 코드(echo.main)로 |
| C06 | 지도(T9) + F-C06-1 Echo/Chat 경로 비교(sequence 2판) | 없음 | 2 | 기존 `data-component` 패널 제거 |
| C07 | F-C07-1 두 워커·두 세션 타임라인(sequence 4레인) | X-C07-1 콜백 안 DB 호출 | 2 | `callback-cost` 절에 "워커가 멈추면 같은 워커의 다른 세션도 멈춘다"를 그림 단계로 |
| C10 | F-C10-1 accept 시퀀스, F-C10-2(T5) | W03 | 2 | 첫 절에 "세션 풀은 왜 미리 만드는가" 한 문단 |
| C12 | F-C12-1 수신 완료 시퀀스 + 링 두 구간(sequence, 레인 옆 노트로 링 상태) | W04, W05, X-C12-1 소비량 과대 반환 | 3 | 첫 절의 내부 이름(RecvFailed, BeginPeerClose 등) 소개 문장 추가. 절 순서를 "정상 수신 → 다음 게시 → 콜백 → 오류·종료" 순으로 |
| C14 | F-C14-1 송신 링과 in-flight(ring 3단계: 접수 → 게시 → 완료 뒤 병합) | W07, X-C14-1 kWouldBlock을 무시하는 실수 | 2 | `send-policy` 절을 결정 표로 |
| C16 | F-C16-1(T4), F-C16-2 FIN 뒤 drain 타임라인(sequence) | W09 | 2 | 종료 원인 표에 "독자가 보는 CloseReason" 열 |
| C18 | F-C18-1 샤드 큐와 게이트(queue 3단계: 가득 참 → close 보류 → 새 세대) | W11, X-C18-1 span 저장 | 3 | `queue-ownership` 절을 Echo→Chat 변화의 첫 단계로 연결(T14) |

**검증 방법.** T12와 같다.

**완료 기준.** 여덟 장이 표준 구성을 갖추고, 내부 식별자 소개 규칙 위반이 저자 검수에서 0건이다.

### T14. 활용 트랙 Echo에서 Chat으로 그리고 확장 과제

**목적.** "활용을 잘 하기"를 위해 Echo에서 Chat으로 가는 변화를 실제 코드로 추적하고, 독자가 스스로 확장해 볼 과제를 둔다. 새 C++ 샘플은 이번 범위에 넣지 않는다(7절 결정 사항).

**선행.** T7, T8(W12), T11.

**변경할 파일.** `chapters/c04_echo_walkthrough.html`(`echo-extension` 절), `c19_chat_server.html`, `c20_application_design.html`, `walkthroughs.json`(비교 블록), `figures.json`, `quizzes.json`, `evidence.json`(Chat `main`·`ConsumeInput`·`Send` 래퍼 출처 추가).

**구현 방법.**

1. C04 `echo-extension`: "Echo가 게임 서버가 되려면 무엇이 달라지는가" 표(변화 6개: 패킷 종류 → 헤더 확장 / 상태(로그인·로비·룸) → Player / 느린 처리 → 로직 큐 / 다수 수신자 → 방송 / 세션 재사용 → SessionId 보관 / 종료 → 큐 배출). 각 행에 Chat의 해당 코드 출처 버튼과 C19의 절 링크.
2. C19에 비교 블록 5개: X-C19-1 핸들러(EchoHandler vs `QueuedEventAdapter` 생성), X-C19-2 OnData(즉시 Send vs 큐에 복사), X-C19-3 상태 보관(없음 vs `Player`와 `PlayerMap`), X-C19-4 응답(단일 Send vs 룸 방송 루프), X-C19-5 종료(Stop vs `StopAfterDrain` 뒤 Stop). 왼쪽은 Echo의 실제 코드(출처), 오른쪽도 Chat의 실제 코드(출처)로, 이 블록들은 `wrong` 대신 `left/right` 라벨을 쓴다(스키마에 `mode: "evolution"` 추가).
3. C19 `chat-wire`: F-C19-1 8바이트 헤더(bytes: 길이 4·버전 2·타입 2 색 구분) + 로그인·입장·채팅 패킷 예 3개를 단계로. `docs/protocol/chat-fixtures.tsv`의 실제 행을 쓴다.
4. C20 확장 과제 3개(`aside.exercise` 복원, 본문용 스타일):
   - 과제 1 귓속말: 패킷 타입 추가, 대상 Player 찾기, 상대가 다른 샤드일 때의 처리. 해설은 설계 결정(방송 inbox 재사용 vs 샤드 간 메시지 큐)과 바뀌어야 할 함수 이름(`ConsumeInput`, `RunShard`, `PopBroadcast`)을 적고, 코드는 "학습용 예시" 라벨의 10줄 이내 조각만 둔다.
   - 과제 2 하트비트: `idle_timeout_ms`와 앱 수준 ping의 차이, 어느 쪽을 쓸지 결정 기준.
   - 과제 3 룸 목록 요청: 읽기 전용 응답을 로직 워커에서 만들 때 Send 결과 처리.
   - 각 과제에 "확인 방법"으로 기존 실험(L14·L15·L16)의 조건 변경과 C# 클라이언트 실행 절차를 적는다.
5. C20에 "설계 체크리스트" 표: 네트워크 콜백에서 하지 않는 일, SessionId 보관 규칙, 큐 포화 정책, 종료 순서. 각 항목에 근거 장·출처.
6. 퀴즈 6문항(T11 표의 C18~C20).

**검증 방법.** 공통 검증 명령. 새 출처의 테스트 연결은 `tests/unit/chat_protocol_test.cpp`와 `tools/test-chat.ps1`가 가리키는 통합 검사로 한다.

**완료 기준.** 변화 표·비교 블록 5개·헤더 그림·과제 3개·체크리스트가 동작하고, 학습용 조각은 모두 라벨이 붙어 있다.

### T15. 심화 장과 부록 최소 보강

**목적.** 심화 장(C05, C08, C09, C11, C15, C17, C21~C28)과 부록을 표준 구성의 필수 항목(지도 위치, 쉽게 말하면, 자주 하는 실수 또는 그림 1개, 핵심 세 줄)으로 맞춘다.

**선행.** T4, T5, T9(미니 지도).

**변경할 파일.** 해당 원고 15개, `figures.json`, `quizzes.json`.

**구현 방법.** 장별 최소 추가:

| 장 | 추가 |
|---|---|
| C05 | F-C05-1 RQ·CQ·등록 버퍼·컨텍스트 관계(layers 축소), 내부 이름 소개 문장 |
| C08 | F-C08-1 등록 버퍼 수명(graph: 등록 → 요청 참조 → 완료 → 해제 조건), W 없음 |
| C09 | F-C09-1(T5) 단계 3개(빈 → wrap → 가득) |
| C11 | F-C11-1 슬롯 재사용 타임라인(sequence: 옛 연결·새 연결·지연 작업 3레인) |
| C15 | W08(T8), 기존 L11 유지 |
| C17 | F-C17-1(T5), W10(T8) |
| C21 | F-C21-1 메모리 예산 막대(HTML/CSS, L17 계산값과 같은 데이터), 설정 관계 그림(graph: 설정 간 제약) |
| C22 | F-C22-1 지표가 가리키는 파이프라인 위치(layers 축소에 카운터 라벨) |
| C23 | F-C23-1 증상 → 확인 → 조치 흐름(state 또는 graph) |
| C24 | F-C24-1 테스트 계층(graph: 순수·fake·Win32·통합) |
| C25~C28 | 그림 대신 "자주 하는 실수" 비교 블록 1개씩(측정 조건 혼동, 과거 수치 인용, 패키지 구성 불일치, 회귀 선택 누락) |
| A01 | 심볼 표에 "처음 쓸 때 읽을 장" 열 추가 |
| A02 | 용어 표에 입문 표시(`level`) 열 |
| A05 | 새 정의 파일·컴포넌트·검사 절차 반영(T16에서 최종 갱신) |

**검증 방법.** 공통 검증 명령.

**완료 기준.** 모든 장에 미니 지도·쉽게 말하면·핵심 세 줄이 있고, 심화 장마다 그림 또는 비교 블록이 1개 이상이다.

### T16. 접근성 인쇄 성능 회귀 검증과 문서 갱신

**목적.** 새 컴포넌트 전체를 브라우저 검사에 넣고, 인쇄·접근성·크기 예산을 확인하며, 유지관리 문서와 README를 갱신한다.

**선행.** T1~T15 중 구현한 것 전부(이 태스크는 마지막에 한 번 수행한다. 1차 완료 세트만 끝냈다면 그 범위로 수행하고 체크 메모에 범위를 적는다).

**변경할 파일.** `tools/book/verify_reader.cjs`, `chapters/a05_book_maintenance.html`, `README.md`, `README_kr.md`, `CHANGELOG.md`, `manifest.json`(`verification.json`의 검사 항목 갱신 포함).

**구현 방법.**

1. `verify_reader.cjs`에 다음을 추가한다(기존 흐름 뒤에 한 블록으로):
   - 모든 장을 순회하며 `[data-figure],[data-walkthrough],[data-quiz],[data-map]` 각각이 "집필 중" 문장 없이 렌더됐는지(`.no-script`가 남아 있지 않은지) 확인.
   - 그림 하나: 다음·이전·재생·정지, 장 이동 후 타이머 0개, 단계 전환 50ms 이하.
   - 워크스루 하나: 다음 두 번 뒤 `.focusline` 줄 번호 일치, Esc/뒤로 가기 복원.
   - 퀴즈 하나: 선택 저장과 새로고침 유지.
   - 보관 자료 토글, 용어 팝오버 포커스 복귀, 경로 선택 유지.
   - 인쇄: `#printBook` 안에 `.figure-controls`·`.quiz input`이 보이지 않고, 그림 단계 목록 `ol`이 있다. 보관 절이 기본 상태에서 인쇄 DOM에 없다.
   - 축소 동작: `reducedMotion: 'reduce'`에서 그림 재생 간격이 1400ms로 늘어난다(세션 객체 값 확인).
   - receipt에 새 검사 이름을 추가한다.
2. 크기·성능: `index.html` 크기와 첫 렌더 시간을 receipt에 기록하고 4.7 예산과 비교한다. 초과 시 정의 파일의 중복 텍스트(동일 설명 반복)를 줄인다.
3. 키보드 점검(수동): Tab만으로 C12 전체를 조작하고 결과를 이 계획 문서의 검증 기록에 적는다. 스크린 리더 자동 검증은 하지 않는다(범위 밖).
4. 문서:
   - A05: 정의 파일 4개, 그림 엔진, 컴포넌트 검증 규칙, 새 브라우저 검사 항목, 보관 자료 토글, 저장 스키마 3을 적는다. `maintenance-acceptance` 절의 실행 범위 표를 갱신한다.
   - `README_kr.md` 17행의 "28개 장·5개 부록·20개 설명용 실험"을 실제 수치(36개 장, 실험 20개, 그림 n개, 코드 따라가기 n개, 퀴즈 n문항)로 바꾸고 "처음 읽는 사람의 경로" 링크를 추가한다. `README.md`도 같은 내용으로 갱신한다(두 README 모두 `docs/interactive_book/index.html` 링크 유지 — `verify_documents.py`가 검사).
   - `CHANGELOG.md`에 문서 항목으로 기록한다(라이브러리 버전은 올리지 않는다).
   - `verification.json`에 이번 판에서 실행한 검사와 미실행 범위를 적는다.

**검증 방법.** 공통 검증 명령 전체와 `npm.cmd --prefix tools/book run test:browser`의 receipt 확인. 대표 스크린샷(P02 지도, C12 워크스루, L05 링, 퀴즈 결과, 인쇄 미리보기)을 `.artifacts/book/reader`에 남긴다.

**완료 기준.** 브라우저 검사가 새 항목을 포함해 통과하고, receipt에 크기·첫 렌더·단계 전환 수치가 예산 안에 있으며, README 두 개·A05·CHANGELOG가 갱신되어 `tools\check-docs.ps1`가 통과한다.

---

## 6. 권장 진행 순서

의존 관계와 "독자에게 빨리 보이는 가치"를 함께 고려한 순서다. 한 세션에 끝나지 않으므로 체크박스로 이어 간다.

1. **기반**: T1 → T2 (T2는 작고 효과가 커서 먼저 한다)
2. **보는 장치**: T4 → T7 (시퀀스 그림과 워크스루가 나오면 P03·C04에서 방향을 확인할 수 있다)
3. **1차 완료 세트**: T3 → T11 → T12 (입문 장이 표준 구성으로 완성된다. 여기까지가 최소 공개 가능 상태다)
4. **구조**: T5 → T6 → T9 → T10
5. **핵심 코드**: T8 → T13 → T14
6. **마무리**: T15 → T16

각 단계가 끝날 때마다 `index.html`을 재생성해 커밋하고, 브라우저 검사 receipt를 남긴다. T16은 마지막에 한 번 수행하지만, 1차 완료 세트가 끝났을 때 README의 수치만은 먼저 맞춘다.

---

## 7. 결정 사항과 남은 위험

### 7.1 이 계획에서 내린 결정

| 결정 | 선택 | 이유와 대안 |
|---|---|---|
| 보관 자료 처리 | 원고와 `migration.json`은 그대로 두고 런타임 숨김(T2) | 블록을 다른 장으로 옮기면 493건의 `destination`과 보존 해시를 모두 손봐야 한다. 숨김은 하루 작업이고 되돌리기 쉽다. 나중에 별도 부록 장(A06)으로 이관하는 것은 `EXPECTED_CHAPTERS` 변경을 포함한 별도 작업으로 남긴다. |
| 새 장 추가 | 추가하지 않음 | 36개 장 안에서 절을 보강한다. 활용 트랙은 C04·C19·C20에 넣는다. 장 번호와 slug가 바뀌지 않아 외부 링크와 저장된 읽기 위치가 유지된다. |
| Chat 종료 비교 | 실제 `Server::Stop` → `StopAfterDrain` 순서로 설명 | 원안의 `StopAfterDrain` 후 Stop 표현은 현재 main과 달라 원문에 맞췄다. |
| 새 C++ 샘플 | 이번 범위에서 제외 | 컴파일·테스트·포맷·패키지 검증까지 범위가 커진다. 활용 과제는 기존 Chat 코드와 실험으로 확인하게 한다. 사용자가 원하면 `samples/room_server`를 후속 계획으로 만든다. |
| 그림 작성 방식 | JSON 정의 + 순수 렌더러(필요하면 정지 SVG를 원고에 직접 작성 허용) | 같은 레인·색·선 규칙으로 36개 장의 그림이 일관된다. 실험과 그림이 렌더러를 공유해 "실험에서 본 모양"이 본문 그림과 같아진다. 손으로 그린 SVG는 P01·P02처럼 개념 비유 그림에만 쓴다. |
| 코드 참조 | 항상 `evidence.json` 경유 | 해시 검토 규칙을 우회하지 않는다. 워크스루 단계의 `focus`는 출처 안의 부분 범위다. |
| 퀴즈 정책 | 진행 제한 없음, 결과 저장 | 책은 시험이 아니다. 진도 표시는 체크와 퀴즈를 구분해 보여 준다. |
| 저장 스키마 | 3으로 올리고 제자리 이관 | 기존 독자의 체크·위치를 잃지 않는다. |

### 7.2 남은 위험

- **원고 분량**: T8·T12·T13·T14의 집필량이 크다. 한 장을 끝낼 때마다 빌드·검사·커밋해 중간 상태를 남긴다.
- **출처 해시**: 라이브러리 소스가 바뀌면 출처 검토가 다시 필요하다. 이 계획 기간에는 소스를 바꾸지 않는다.
- **단일 HTML 크기**: 정의 파일과 JS가 늘어난다. 4.7 예산을 생성기가 강제한다. 초과 시 보관 자료를 별도 JSON으로 빼서 지연 생성하는 방안을 검토한다(현재는 원고에 인라인).
- **브라우저 검사 시간**: 새 항목으로 수 분이 더 걸린다. 장 순회 루프는 한 번만 돌도록 기존 루프에 검사를 합친다.
- **모바일 폭**: 시퀀스 그림과 워크스루 2열은 390px에서 가로 스크롤·세로 배치로 처리한다. 페이지 자체의 가로 넘침은 기존 검사가 막는다.
- **용어 일관성**: 용어가 55개로 늘면 본문과 어긋날 수 있다. 자동 검사는 `dfn` id 존재만 확인한다. 어긋남은 저자 검수 항목으로 남긴다.

---

## 8. 부록

### 8.1 그림 카탈로그 초안

| ID | 장 | 종류 | 보여줄 것 | 단계 | 만드는 태스크 |
|---|---|---|---|---|---|
| F-P01-1 | P01 | bytes | 보낸 묶음과 도착 조각의 불일치 | 3 | T12 |
| F-P01-2 | P01 | sequence | 동기 호출 vs 비동기 요청·완료 | 4 | T12 |
| F-P01-3 | P01 | ring | 미리 등록한 그릇 | 1 | T12 |
| F-P02-2 | P02 | sequence | 연결의 여섯 장면 | 6 | T12 |
| F-P03-1 | P03 | sequence | Echo 패킷 왕복 | 6 | T4 |
| F-C02-1 | C02 | graph | 빌드 산출물 의존 관계 | 1 | T12 |
| F-C02-2 | C02 | state | 실패 계층 | 4 | T12 |
| F-C03-1 | C03 | state | ServerState 전이 | 5 | T4 |
| F-C03-2 | C03 | layers | 소유·대여 관계 | 1 | T13 |
| F-C04-1 | C04 | state | Send 결과별 정책 | 6 | T12 |
| F-C05-1 | C05 | layers | RQ·CQ·등록 버퍼·컨텍스트 | 1 | T15 |
| F-C06-1 | C06 | sequence | Echo 경로 vs Chat 경로 | 5 | T13 |
| F-C07-1 | C07 | sequence | 두 워커·두 세션 교차 | 6 | T13 |
| F-C08-1 | C08 | graph | 등록 버퍼 해제 조건 | 4 | T15 |
| F-C09-1 | C09 | ring | 빈 → wrap → 가득 | 3 | T5·T15 |
| F-C10-1 | C10 | sequence | accept에서 첫 수신까지 | 7 | T13 |
| F-C10-2 | C10 | slots | 슬롯과 interleaved free list | 1 | T5 |
| F-C11-1 | C11 | sequence | 슬롯 재사용과 지연 작업 | 5 | T15 |
| F-C12-1 | C12 | sequence | 수신 완료·선게시·콜백 | 6 | T13 |
| F-C13-1 | C13 | bytes | 길이 접두 헤더 | 1 | T12 |
| F-C13-2 | C13 | sequence | 2+3+2 도착과 소비량 | 3 | T12 |
| F-C14-1 | C14 | ring | 송신 링과 in-flight 병합 | 3 | T13 |
| F-C16-1 | C16 | state | 세션 수명 상태 | 5 | T4 |
| F-C16-2 | C16 | sequence | FIN 뒤 drain과 기한 | 4 | T13 |
| F-C17-1 | C17 | graph | 종료 의존 관계 | 8 | T5 |
| F-C18-1 | C18 | queue | 샤드 큐와 parked close | 3 | T13 |
| F-C19-1 | C19 | bytes | Chat 8바이트 헤더 | 3 | T14 |
| F-C21-1 | C21 | graph | 설정 간 제약 | 1 | T15 |
| F-C22-1 | C22 | layers | 지표가 가리키는 위치 | 1 | T15 |
| F-C23-1 | C23 | graph | 증상 → 확인 → 조치 | 1 | T15 |
| F-C24-1 | C24 | graph | 테스트 계층 | 1 | T15 |

### 8.2 워크스루 카탈로그

| ID | 장 | 대상 | 태스크 |
|---|---|---|---|
| W01 | P03 | `main` (echo) | T7 |
| W02 | C04 | `EchoHandler::OnData` | T7 |
| W03 | C10 | `Accepter::Run` | T8 |
| W04 | C12 | `IoWorkerThread`·`ProcessRIOCQ` 분기 | T8 |
| W05 | C12 | 수신 분기·`RecvCompletion`·`PostRecv`·`ConsumeRecvBuffer` | T8 |
| W06 | C13 | `LengthPrefixedDecoder::Decode` | T8 |
| W07 | C14 | `Send` → `PostSendScatter` → `IssuePost` | T8 |
| W08 | C15 | `SendImmediately::Completed` | T8 |
| W09 | C16 | FIN 분기 → `BeginPeerClose` → `Disconnect` | T8 |
| W10 | C17 | `Server::Stop` → `End` | T8 |
| W11 | C18 | `Push`·`TryPop` | T8 |
| W12 | C19 | `ChatApplication::RunShard` | T8 |

### 8.3 비교 블록 카탈로그

| ID | 장 | 잘못된 사용(학습용) | 실제 코드 |
|---|---|---|---|
| X-C04-1 | C04 | 핸들러를 서버보다 먼저 소멸 | `echo.main` 수명 객체 순서 |
| X-C04-2 | C04 | `OnData`의 span 주소 저장 | Chat adapter의 복사 |
| X-C03-1 | C03 | 콜백 안에서 `Stop` | `Server::Stop` 주석과 `kInvalidState` |
| X-C07-1 | C07 | 콜백 안 DB 대기 | Chat의 큐 전달 |
| X-C12-1 | C12 | readable보다 큰 소비량 반환 | `RejectReceiveCallback`·`ConsumeRecvBuffer` |
| X-C13-1 | C13 | 불완전한 앞부분 소비 | `Decode`의 미완성 반환 |
| X-C14-1 | C14 | `kWouldBlock` 무시 | Echo의 결과별 정책 |
| X-C18-1 | C18 | 큐에 span 저장 | adapter `Push`의 복사 |
| X-C19-1~5 | C19 | (evolution) Echo | Chat |
| X-C25~28 | C25~C28 | 측정·기록·패키지·회귀의 흔한 실수 | 해당 도구 스크립트 |

### 8.4 다음 세션 시작 절차

1. 이 문서와 `CLAUDE.md`를 읽는다. `git status`가 깨끗한지, `master`가 `59fb614` 이후인지 확인한다.
2. `npm.cmd --prefix tools/book ci`로 검사 도구를 준비하고 공통 검증 명령이 현재 상태에서 통과하는지 먼저 확인한다.
3. 6절 순서의 첫 미완료 태스크부터 시작한다. 태스크 하나가 끝나면 체크박스를 즉시 갱신하고 커밋한다.
4. 계획과 다른 결정이 필요하면 이 문서의 7.1 표에 행을 추가하고 이유를 적는다. 계획서를 새로 만들지 않는다.
