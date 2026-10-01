---
name: session-start
description: 역할(설계·검토·개발·리뷰·질문)과 영역을 받아, 그 역할에 필요한 문서만 읽고 세션을 시작한다. 새 세션의 첫 명령으로 쓴다.
argument-hint: <설계|검토|개발|리뷰|질문> <영역: Loot|Polish|…> [대상: DOCS, Step 번호(예: 03B) 또는 Polish 작업명(예: WeaponFireRate), 질문 주제]
disable-model-invocation: true
---

# 세션 시작: $ARGUMENTS

인자를 `역할 영역 [대상]`으로 해석한다(질문은 `질문 <주제>`). 역할이 다섯 가지 중 하나가 아니거나, 개발·리뷰인데 Step(Polish는 작업명)이 없으면 무엇이 필요한지 묻고 멈춘다.

## 1. 공통으로 먼저 할 일
1. 세션 이름을 정해 알린다: `설계-Loot`, `검토-Loot-DOCS`, `개발-Loot-03B`, `리뷰-Loot-03B`, `질문-<주제>`.
2. 영역 파일을 찾는다. `DOCS/Notes/NN/` 중 `Status/NN_<영역>_STATUS.md`가 있는 폴더가 그 영역이다(예: Loot → `Notes/05/`). 없으면 새 영역이다 — 설계 세션만 시작할 수 있다.
3. 영역 STATUS의 **인수인계** 칸을 읽는다: 다음 행동, 하지 말 것, 막힌 점.
4. 영역 Design(`NN_<영역>_Design.md`) 맨 위 `상태:` 줄을 확인한다(`초안` → `검토 완료` → `승인`). Design이 없는 영역(Polish처럼 작업 문서만 있는 곳)은 대상 작업 문서의 `상태:` 줄을 보고, 그것도 없으면 승인 여부를 사용자에게 묻는다.
5. 문서 이름·위치·가이드 형식은 `DOCS/ROADMAP.md` §6을 따른다.

## 2. 역할별로 읽을 것과 지킬 것

### 설계
- 읽기: `DOCS/ROADMAP.md`, `DOCS/GAME.md`, 영역 Design·DECISIONS·STATUS, `DOCS/BACKLOG.md`, 관련 `DOCS/Mine/`.
- 할 일: Design 작성·수정(바꾸면 `상태: 초안`으로), DECISIONS에 항목 추가(추가만), Step 나누기, `제안` 항목 채택·기각, 필요하면 로드맵 조정.
- 하지 않을 일: **구현서·가이드 작성.** `상태: 승인`으로 바꾸기(사용자만 한다).

### 검토
- 대상: 검토할 문서(`DOCS` = 영역 Design).
- 읽기: **요구사항(`GAME.md`, `ROADMAP.md`)과 대상 문서만.** DECISIONS는 근거 확인용으로만.
- 읽지 않기: 설계 세션의 대화 기록. 결론을 정당화한 맥락을 보면 검토가 치우친다.
- 할 일:
  1. `Review/Request/NN_<영역>_REVIEWnn_<주제>_Request.md` 작성(번호는 기존 다음, 두 자리). 담을 것: 대상과 범위, 지켜야 할 요구사항, 반박해 달라는 가정 목록, 답변 형식(주장마다 근거 파일:줄).
  2. `/codex:adversarial-review`에 Request와 대상 문서 경로를 넘긴다. 중요한 설계는 `--model gpt-6-astra`.
  3. 답변을 `Review/Answer/..._Answer.md`로 저장하고 `review-verifier`로 검증한다.
  4. 반영할 것 / 안 할 것을 정리해 보고한다. **문서 반영은 사용자 확인 후.**

### 개발
- 대상: Step(예: `03B`).
- 읽기: 영역 STATUS, Step STATUS, 영역 Design(승인본), 해당 Step 구현서(있으면).
- 전제: **Design이 `승인`이 아니면 구현서·가이드를 쓰지 않고 멈춰서 알린다.**
- 할 일: 그 Step의 구현서(정답지, 리프 이름 규칙)와 가이드(`Guide/<구현서 이름>_Guide.md`, 레벨은 STATUS "다음 행동"에 적힌 것). 사용자 코딩 중 질문에 답한다. STATUS는 코드를 읽고 대조해서만 갱신한다.
- 하지 않을 일: **코드 파일 수정.** 설계 변경(필요하면 DECISIONS에 `제안` 항목을 쓰고 멈춘다). 다른 Step.

### 리뷰
- 대상: Step.
- 읽기: 가이드, 구현서, Step STATUS, 사용자 코드(`git diff`).
- 읽지 않기: 개발 세션의 대화 기록.
- 할 일:
  1. 구현서와 코드의 차이를 정리한다(의도된 차이인지, 결함인지).
  2. `/codex:review`로 diff를 교차 검토한다. Codex 지적은 `review-verifier`로 검증한 것만 쓴다.
  3. 가이드 자기 점검의 **채점** 칸을 채운다(○/△/✗ + 근거 file:line + 보충). 틀린 개념은 `DOCS/StudyPath.md`에 적는다.
  4. 결과를 `Review/NN_<영역>_REVIEW_<Step>_Code.md`에 쓰고 Step STATUS를 갱신한다.
  5. 다음 Step의 가이드 레벨을 정해 STATUS "다음 행동"에 적는다(차이가 작고 채점을 통과하면 한 단계 내린다. 막힌 곳이 많으면 유지하거나 올린다).
- 하지 않을 일: **코드 파일 수정** — 지적과 설명만 한다. 설계 변경(DECISIONS에 `제안`).

### 질문
- 대상: 주제.
- 읽기: 필요한 것만.
- 남길 곳: 엔진 개념은 `DOCS/Mine/Concepts/`, 미룬 것은 `DOCS/BACKLOG.md`(이유와 함께), 설계에 닿는 것은 DECISIONS에 `제안`.
- 하지 않을 일: 설계 확정, 구현서·가이드 작성.

## 3. 모든 역할에 공통
- 역할 밖의 일이 생기면 하지 않고 STATUS "다음 행동"이나 BACKLOG에 적는다.
- 끝낼 때는 compact 대신 `/handoff` 후 새 세션.

## 4. 시작 보고
읽은 파일, 인수인계 칸의 다음 행동, Design `상태:`, 이 세션에서 할 일과 하지 않을 일을 짧게 보고하고 시작한다.
