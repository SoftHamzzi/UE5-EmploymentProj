# 작업 지시서: 역할별 세션 + Codex 교차 검토 + 단계별 가이드 도입

> 작성: 2026-10-02, `krafton_ai` 프로젝트의 질문 세션에서 사용자와 논의한 결과를 옮긴 것.
> 이 문서를 받은 세션은 아래 **§4 작업 목록**을 순서대로 수행한다. **§3 "사용자가 정할 것"은 작업 전에 먼저 물어본다.**
> 파일 이름: §1과 §4-2의 "지금" 열은 **현재 이름**(예: `LOOT_STATUS.md`, `05_Loot_DOCS.md`), §2.7 이후 트리·규칙은 **바뀐 뒤 이름**이다. "영역 설계(Design)"는 `NN_<영역>_DOCS.md` → `NN_<영역>_Design.md`를 가리킨다. §4-2에서 이름을 바꾸면 이 문서의 경로도 함께 고친다.
> 참고 구현: `C:\Github\krafton_ai\.claude\skills\session-start\SKILL.md`, `C:\Github\krafton_ai\.claude\skills\handoff\SKILL.md`
> (그대로 복사하지 않는다. 그쪽은 Claude가 코드를 쓰는 대회 프로젝트라 역할과 관문이 다르다.)

---

## 1. 왜 바꾸나

### 지금 방식
- developer 세션 **하나**가 계획·설계·구현서·가이드 작성과 리뷰를 모두 맡고, 길어지면 수동으로 compact한다 (`autoCompactEnabled: false`).
- 코드는 사용자가 직접 쓴다. Claude는 구현서(상세 코드 포함)와 가이드(`*_Guide.md`, 코드 없이 계약만)를 쓴다.
- `SESSION.md`: "세션을 구현/검증으로 나누지 않는다. STATUS 파일이 상태를 전달한다."

### 문제
1. **compact는 손실 압축이다.** 무엇이 빠졌는지 알 수 없다. STATUS가 원천인데 세션은 압축된 기억으로 판단한다.
2. **옛 결정이 섞인다.** 예: `04_Polish_WeaponFireRate_PrimaryUse_Guide.md`에 09-22 구조와 09-23 구조가 공존한다. 한 세션이 둘 다 기억하면 옛 구조가 새어 나온다.
3. **쓴 세션이 검토하면 치우친다.** 구현서를 쓴 세션이 사용자 코드를 리뷰하면 그 구현서 기준으로 맞다고 본다. 설계 문서도 마찬가지다.
4. **가이드가 "글로 쓴 코드"에 가깝다.** 함수별 단계 순서·정확한 API 호출·인자까지 적혀 있어, 가장 배울 게 많은 "순서 결정"을 문서가 대신 한다.
5. **자기 점검이 채점되지 않는다.** PrimaryUse 가이드 §3의 답에 빈틈이 있는데(예: 1번에서 두 번째 클릭의 `InputPressed → bPendingShot → OnFireTimerTick` 경로 누락, 6번 호스트 분기 지점 미확인) 피드백 없이 남아 있다. §3-B는 미응답이다. 또 §3-B 4번처럼 답이 본문에 그대로 있는 문항은 독해만 확인한다.

`SESSION.md`도 이미 "STATUS만 읽으면 새 세션이 상태를 복원한다"고 한다. 그렇다면 compact 대신 **인수인계 후 새 세션**이 원래 설계에도 맞다.

---

## 2. 바뀐 뒤의 모습

### 2.1 역할 다섯 가지 (세션 하나 = 역할 하나)

| 역할 | 시작 | 하는 일 | 읽는 것 | 남기는 것 | 끝 |
|---|---|---|---|---|---|
| **설계** | `/session-start 설계 <영역>` | 영역 설계(Design) 작성·수정, 결정 기록(DECISIONS), Step 나누기, 필요하면 로드맵 조정 | 로드맵, `GAME.md`, 영역 Design·DECISIONS·STATUS, `BACKLOG.md`, 관련 `Mine/` | Design, DECISIONS, Step 목록, BACKLOG | 설계 초안 완료 |
| **검토** | `/session-start 검토 <영역> DOCS` | 설계 문서 반박 검토: Request 작성 → `/codex:adversarial-review` → `review-verifier` 검증 → 반영할 것/안 할 것 보고 | **요구사항(`GAME.md`, `DOCS.md`)과 대상 문서만.** 설계 세션 대화는 읽지 않는다 | `Review/Request`, `Review/Answer` + 검증 결과 | 검토 한 건 |
| **개발** | `/session-start 개발 <영역> <Step>` | 승인된 설계를 따라 그 Step의 **구현서(정답지)와 가이드** 작성, 사용자 코딩 중 질문 응답, STATUS 갱신 | 영역 STATUS, 단계 STATUS, 영역 Design(승인본), 해당 Step 문서 | 구현서, 가이드, STATUS | Step 하나 (또는 컨텍스트가 차면 handoff 후 새 세션) |
| **리뷰** | `/session-start 리뷰 <영역> <Step>` | 사용자 코드 리뷰: 구현서와의 차이, `/codex:review`(git diff), 가이드 자기 점검 **채점**, 오답을 `StudyPath.md`로 | 가이드, 구현서, 사용자 코드(diff), 단계 STATUS. **개발 세션 대화는 읽지 않는다** | 리뷰 결과, 채점, STATUS, StudyPath | 리뷰 한 건 |
| **질문** | `/session-start 질문 <주제>` | 엔진 개념·도구 질문, 조사 | 필요한 것 | 필요하면 `Mine/Concepts/`, BACKLOG | 자유 |

- 세션 이름: `설계-Loot`, `검토-Loot-DOCS`, `개발-Loot-03B`, `리뷰-Loot-03B`, `질문-<주제>`
- 끝낼 때는 `/handoff` (§2.3). **compact 대신 handoff 후 새 세션**이 기본이다.
- 역할 밖의 일이 생기면 하지 않고 STATUS의 "다음 행동"이나 BACKLOG에 적는다. 특히 개발·리뷰 세션에서 설계를 바꿔야 하면 영역 DECISIONS에 **"제안"** 항목으로 적고 멈춘다. 설계 세션이 받아서 처리한다.
- 기존 규칙은 그대로다: **Claude는 코드 파일을 수정하지 않는다**, Review Answer는 `review-verifier`로 검증 후에만 반영, Notes 문서를 쓰면 `notes-review`.

### 2.2 흐름과 관문

```
설계 ─▶ 검토 ─▶ [사용자 승인: 영역 Design "상태: 승인"]
                    ▼
            개발 (구현서 + 가이드 Lx)
                    ▼
            사용자가 직접 코딩 → 빌드·PIE
                    ▼
            리뷰 (차이 + /codex:review + 채점) ─▶ 다음 Step
                    │
                    └─ 설계 수정이 필요하면 "제안" → 설계 세션
```

**관문은 하나다.** 영역 Design 맨 위 `상태:` 줄이 `승인`이 아니면 개발 세션은 구현서·가이드를 쓰지 않고 멈춰서 알린다. 승인은 사용자만 한다.
`상태:` 값: `초안` → `검토 완료` → `승인` (설계가 바뀌면 다시 `초안`).

### 2.3 handoff: 인수인계 칸
각 영역 STATUS(예: `Notes/05/Status/05_Loot_STATUS.md`) 위쪽에 아래 칸을 두고, `/handoff`가 이 칸을 덮어쓴다. 지금 "STATUS 정리해줘"로 하던 일을 형식화한 것이다.

| 항목 | 쓰는 법 |
|---|---|
| 갱신 | 날짜, 세션 이름 |
| 지금 단계 | 설계/검토/Step NN 중 어디, 어떤 상태 |
| **다음 행동** (하나) | `/session-start <역할> <영역> <대상>` 형식 |
| 하지 말 것 | 실패한 시도와 이유, 아직 금지인 것 (미승인 설계 등) |
| 근거 | 파일 경로, 커밋, PIE 확인 내용 |
| 막힌 점 / 사용자 확인 필요 | |

단계 STATUS(예: `05_Loot_03_Inventory_STATUS.md`)의 Step별 완료 여부 갱신은 지금처럼 **코드를 읽고 대조해서만** 한다.

### 2.4 가이드 문서 개편 (개발 세션이 쓰는 것)

**구현서 = 정답지.** 지금처럼 상세하게 쓰되, 사용자는 **리뷰 전까지 열지 않는다.** 가이드 상단에 이 사실을 적는다.

**가이드 레벨** — 영역·Step마다 레벨을 정해 가이드 상단에 적는다.

| 레벨 | 가이드에 담을 것 | 담지 않을 것 |
|---|---|---|
| **L3** | 함수별 계약 + 쓸 API + 단계 순서 + 이유 | 코드 |
| **L2** | 함수 목록 + **불변식** + **도구 목록**(쓸 수 있는 API) + 검증 기준 | 단계 순서, 어디서 어떤 API를 부를지 |
| **L1** | 목표, 제약, PIE 검증 기준 | 함수 나누기, API |

- 새 영역(예: Persistence)은 L3에서 시작, 익숙한 영역은 L2/L1.
- **다음 Step의 레벨 결정**: 리뷰에서 구현서와 차이가 작고 채점을 통과하면 한 단계 내린다. 막힌 곳이 많으면 유지하거나 올린다. 리뷰 세션이 STATUS "다음 행동"에 레벨을 적는다.

**불변식으로 쓰는 법 (L2 예시, PrimaryUse `OnTargetDataReady` 기준)**
- 지금(L3): "4. 버킷 `TryTake` → 5. `CommitAbilityCost` … 4번이 5번보다 위인 이유: …"
- L2: "지켜야 할 것: ① 탄약은 발당 정확히 한 번 깎인다(호스트 포함) ② 버킷이 거절한 발은 서버 탄약을 바꾸지 않는다 ③ 클라가 이미 실패를 아는 발은 서버로 보내지 않는다. 도구: `TryTake`, `CommitAbilityCost`, `CallServerSetReplicatedTargetData`, `FScopedPredictionWindow`. → 순서는 직접 정한다."

**자기 점검 개편**
- **예측형 질문**으로 쓴다. 본문에 답이 없고 머리로 돌려 봐야 나오는 질문. 예: "`CommitAbilityCost`를 역할 분기 안에 넣으면 호스트에서 한 발에 탄약이 몇 개 줄어드나?"
- 두 묶음으로 나눈다: **§A 코딩 전**(설계 이해), **§B 코딩 후**(동작 이해). A의 답이 B에서 바뀐 곳이 배운 곳이다.
- 문항마다 아래 칸을 둔다. **채점은 리뷰 세션이 한다** — 근거 파일:줄을 달고, 틀린 개념은 `StudyPath.md`에 적는다.
  ```
  N. 질문
  - 답(코딩 전):
  - 답(코딩 후):
  - 채점: ○ / △ / ✗ — 근거 file:line — 보충
  ```

### 2.5 Codex 교차 검토
- 플러그인: OpenAI 공식 `codex-plugin-cc`. 마켓플레이스는 사용자 전역 설정에 이미 등록돼 있고, 이 프로젝트에서는 **켜기만** 하면 된다.
- Codex 설정(`~/.codex/config.toml`)은 전역이며 `model = "gpt-6.1-sol"`, `model_reasoning_effort = "high"`로 맞춰져 있다 (2026-10-01). 이 프로젝트는 Codex 신뢰 목록에 이미 있다.
- 쓰는 곳:
  - 검토 세션: `/codex:adversarial-review` — 설계 문서 반박. 중요한 설계는 `--model gpt-6-astra`.
  - 리뷰 세션: `/codex:review` — 사용자 코드 diff. 이 프로젝트는 git 저장소라 바로 동작한다.
  - `notes-review` 스킬이 "외부 리뷰 필요"라고 판단하면 → 검토 세션을 다음 행동으로 적는다.
- Codex 답은 제안일 뿐이다. 기존 규칙대로 `review-verifier`로 검증한 뒤에만 반영한다.
- 주의: Codex CLI를 업데이트한 뒤 플러그인이 "model is not supported" 오류를 내면, 옛 `app-server-broker.mjs` 프로세스가 업데이트 전 Codex를 붙잡고 있는 것이다. 그 프로세스와 자식을 종료하면 다음 호출 때 새 버전으로 다시 뜬다.

### 2.6 모델 (세션 입력창의 모델 선택)

| 역할 | 추천 |
|---|---|
| 설계 | Opus 5.5 high |
| 검토 | Opus 5.5 medium (깊은 반박은 Codex 쪽) |
| 개발 | Opus 5.5 medium (가이드 레벨 판단·불변식 정리가 핵심이라 Sonnet보다 Opus) |
| 리뷰 | Opus 5.5 medium |
| 질문 | Sonnet 5.5 medium |

참고: 사용자 전역 `~/.claude/settings.json`의 effort 설정이 이전 세대 키 `claude-sonnet-5`에 걸려 있어 Sonnet 5.5에는 적용되지 않을 수 있다. 전역 설정이라 이 프로젝트 작업 범위 밖이다 — 사용자에게 알리기만 한다.

### 2.7 문서 트리 — 지금과 무엇이 다른가
트리(루트 → 영역 설계 → 리프 + 짝 STATUS)는 **그대로**다. 역할별 세션이 같은 트리의 노드를 나눠 맡을 뿐이다. 아래는 §2.9 이름 규칙을 적용한 뒤의 모습이다.

```
DOCS/
├── ROADMAP.md                (← DOCS.md)   로드맵·실행 순서·§6 문서 규칙    ← 설계
├── GAME.md                                 기획                              ← 검토가 요구사항으로 읽음
├── BACKLOG.md, StudyPath.md                                                  ← 리뷰가 오답을 StudyPath에
├── POLISH_TRACKER.md         (← POLISH.md) GitHub Issues 연결 추적표
└── Notes/05/
    ├── 05_Loot_Design.md               (← 05_Loot_DOCS.md) 지금 유효한 설계, "상태:" 줄   ← 설계/검토/관문
    ├── 05_Loot_DECISIONS.md            [신규] 결정 이력, 추가만 함                      ← 설계
    ├── 05_Loot_03_Inventory.md         구현서 = 정답지 (리뷰 전까지 안 엶)              ← 개발
    ├── 05_Loot_03_Inventory_B_PickupDrop.md  (← 05_Loot_03B_PickupDrop.md) 하위 Step 구현서
    ├── Guide/                          [신규]
    │   └── 05_Loot_03_Inventory_B_PickupDrop_Guide.md  가이드(Lx) + 자기 점검 + 채점 칸  ← 개발 쓰고 리뷰 채점
    ├── Status/
    │   ├── 05_Loot_STATUS.md           (← LOOT_STATUS.md) 진행 상황 + 인수인계 칸      ← 모든 역할의 handoff
    │   └── 05_Loot_03_Inventory_STATUS.md   Step 완료 여부 (코드와 대조)
    ├── Review/
    │   ├── Request/ Answer/            설계 검토 쌍 (05_Loot_REVIEW02_<주제>_Request.md)  ← 검토
    │   └── 05_Loot_REVIEW_03B_Code.md  [신규 규칙] 코드 리뷰 결과                         ← 리뷰
    ├── Issue/  Polish/                 그대로
```

| | 지금 | 바뀐 뒤 |
|---|---|---|
| 루트·영역 설계·리프·`Status/`·`Issue/`·`Polish/`·`Review/Request·Answer` | 있음 | **그대로** (이름만 §2.9) |
| 가이드 | Loot에는 없음. 전체에서 `Polish/WeaponFireRate/..._PrimaryUse_Guide.md` 하나 | 리프마다 `Guide/..._Guide.md` |
| 영역 STATUS | 진행 상황 + 설계 결정이 섞임 (`LOOT_STATUS.md`: 진행 9~42줄, 결정 43~195줄) | 진행 상황 + **인수인계 칸**만 |
| 결정 이력 | STATUS 안 (WeaponFireRate STATUS §2 "결정 이력", §5 "다시 열지 않는 것", `LOOT_STATUS.md` "확정된 설계 결정") | `NN_<영역>_DECISIONS.md` (§2.8) |
| 영역 설계 상태 | 표시 없음 | 맨 위 `상태:` 줄 |
| 코드 리뷰 기록 | 정해진 자리 없음 | `Review/NN_<영역>_REVIEW_<Step>_Code.md` |
| Polish 작업 | `Polish/<작업명>/` 안에 Implementation·Guide·STATUS | **그대로** — 이미 잘 맞는다 |

**계획(PLAN) 문서는 따로 두지 않는다.** 영역 설계 문서의 "범위"와 "순서 결정" 절(예: `05_Loot_Design.md` §1, §3)이 이미 그 역할을 하고, 전체 순서는 로드맵 §5에 있다. 혼자 하는 학습 프로젝트라 승인 단계를 하나 더 두면 비용만 늘어난다.

### 2.8 결정 기록 분리
**왜:** ① STATUS 규칙은 "코드와 대조해 갱신"인데 설계 결정이 같이 있으면 STATUS를 고치다 결정 이력을 건드린다. ② 같은 결정이 `05_Loot_STATUS.md` "확정된 설계 결정"과 `05_Loot_Design.md` §4 두 곳에 있어 한쪽만 고치면 어긋난다. ③ "왜 B안이고 C안은 왜 버렸나"는 면접·블로그 소재 그 자체다 — 따로 쌓아 둘 가치가 크다.

**세 문서의 역할**
| 문서 | 담는 것 | 갱신 |
|---|---|---|
| `NN_<영역>_Design.md` | 지금 유효한 설계 (범위, 순서, 구조, 인터페이스) | 덮어씀. 바뀌면 `상태: 초안`으로 |
| `NN_<영역>_DECISIONS.md` | 결정마다: 번호, 날짜, 상태(제안/채택/대체됨), 맥락, 결정, **버린 대안과 이유**, 근거(파일:줄, Review 번호), 다시 열 조건 | **추가만 함.** 대체되면 옛 항목에 "대체됨 → D-0NN" 표시 |
| `Status/NN_<영역>_STATUS.md` | 진행 상황, 인수인계 칸 | 코드와 대조해서만 |

- 개발·리뷰 세션이 설계 변경을 원하면 DECISIONS에 **"제안"** 항목으로 쓰고 멈춘다. 설계 세션이 채택/기각한다.
- 형식은 WeaponFireRate STATUS의 §2 "결정 이력"(정정 포함, 지우지 않음)과 §5 "다시 열지 않는 것"이 좋은 본보기다 — 그 구조를 DECISIONS 템플릿으로 삼는다.
- **적용 범위:** 새 영역은 처음부터. 진행 중인 Loot는 한 번 옮긴다(§4-3). 완료된 GAS·Polish는 건드리지 않는다.

### 2.9 이름 규칙
**원칙:** 같은 종류의 문서는 같은 형식, 이름순 정렬 = 읽는 순서, 이름만 보고 종류를 알 수 있게.

| 종류 | 형식 | 예 |
|---|---|---|
| 영역 설계 | `NN_<영역>_Design.md` | `05_Loot_Design.md` |
| 결정 이력 | `NN_<영역>_DECISIONS.md` | `05_Loot_DECISIONS.md` |
| 영역 상태 | `Status/NN_<영역>_STATUS.md` | `05_Loot_STATUS.md` |
| 구현서(정답지) | `NN_<영역>_SS_<이름>.md` — **접미사 없음** (04·05의 현재 규칙 유지) | `05_Loot_04_InventoryUI.md` |
| 하위 Step | 부모 이름 뒤에 `_A_`, `_B_` | `05_Loot_03_Inventory_A_Core.md` |
| 가이드 | `Guide/<구현서 이름>_Guide.md` | `Guide/05_Loot_04_InventoryUI_Guide.md` |
| Step 상태 | `Status/NN_<영역>_SS_<이름>_STATUS.md` | (지금과 같음) |
| 설계 검토 | `Review/Request/NN_<영역>_REVIEWnn_<주제>_Request.md` (+ `Answer`) — 번호 **두 자리** | `05_Loot_REVIEW02_Inventory_Request.md` |
| 검토 요약 | `Review/NN_<영역>_REVIEW_<주제>_Summary.md` | `05_Loot_REVIEW_Inventory_Summary.md` |
| 코드 리뷰 | `Review/NN_<영역>_REVIEW_<Step>_Code.md` | `05_Loot_REVIEW_03B_Code.md` |

**지금 이름의 문제 (실제 파일 기준)**
1. 영역 STATUS가 제각각: `GAS_STATUS.md`·`LOOT_STATUS.md`(번호 없음, 대문자) vs `04_Polish_STATUS.md`.
2. "DOCS"가 세 뜻: 폴더 `DOCS/`, 로드맵 `DOCS/DOCS.md`, 영역 설계 `05_Loot_DOCS.md`.
3. Review 번호에 0이 없다. 탐색기·VS Code는 숫자를 알아서 정렬하지만, 명령줄(`ls`, `git`)과 단순 정렬하는 도구에서는 `REVIEW10, 11, 12, 13, 16, 2, 3 …`이 된다. 파일명에 검토 주제가 없다. 번호 없는 `05_Loot_REVIEW_Inventory.md`가 쌍과 섞인다.
4. `05_Loot_03A_Core`·`03B_PickupDrop`이 `03_Inventory`의 하위라는 관계가 이름에 안 보인다. 명령줄 정렬에서는 부모가 자식 뒤에 온다 (`_`가 대문자보다 뒤).
5. 구현서 표기가 시기별로 다름: 01~03·Polish는 `_Implementation`, 04~05는 접미사 없음. → **04·05 규칙(접미사 없음)을 표준으로 한다.** 정답지와 가이드는 `Guide/` 폴더와 `_Guide` 접미사로 구분된다. 구현서에 `_Impl`을 붙이면 `03_Inventory_Impl`보다 `03_Inventory_A_…`가 먼저 정렬되어 4번 문제가 다시 생긴다.
6. 루트 `POLISH.md`와 `Status/04_Polish_STATUS.md`가 둘 다 "Polish 상태"처럼 보인다.

**바꾸는 범위**
- **지금 바꾼다 (계속 쓰이는 파일):** 위 1~4, 6, 그리고 로드맵 `DOCS.md` → `ROADMAP.md`. 03 하위: `05_Loot_03A_Core.md` → `05_Loot_03_Inventory_A_Core.md`, `05_Loot_03B_PickupDrop.md` → `05_Loot_03_Inventory_B_PickupDrop.md`.
- **바꾸지 않는다:** 완료된 01~04 영역 문서, `Blog/`(발행본 날짜 형식은 관례대로 좋다), `Mine/`(이름이 모호하지만 참조가 많아 비용 대비 이득이 작다). 이 셋은 새 규칙을 **새 문서부터** 적용한다.
- 참고(바꾸지 않음): Blog 초안 `Blog/04/Step4_Post5_SpreadDecal.md`처럼 Notes 번호와 맞지 않는 이름이 있다. 새 영역 블로그 초안은 `Blog/05/05_Post1_<주제>.md`처럼 Notes 번호를 따른다.

---

## 3. 사용자가 정할 것 (작업 전에 묻는다)

1. **`SESSION.md` 원칙 변경 승인.** "세션을 나누지 않는다" → "역할별로 세션을 나누고, compact 대신 handoff". 프로젝트 운영 원칙이 바뀐다.
2. **기존 developer 세션 처리.** 그 세션에서 STATUS 인수인계 칸을 한 번 채우고 닫을지.
3. **현재 진행 중인 Step의 가이드 레벨.** Loot 03B 등 진행 중인 Step을 L3로 둘지 L2로 시험할지.
4. **영역 Design의 현재 `상태:`.** 이미 구현 중인 영역(GAS 완료, Loot 진행 중)은 `승인`으로 소급 표기할지.
5. **이름 변경 승인 (§2.9).** 바꿀 목록(§4-2 표)을 보여 주고 승인받는다. 특히 로드맵 `DOCS.md` → `ROADMAP.md`와 `05_Loot_DOCS.md` → `05_Loot_Design.md`는 참조가 많다.
6. **Loot 결정 이력 분리 (§2.8).** `LOOT_STATUS.md`의 설계 결정 부분을 `05_Loot_DECISIONS.md`로 옮길지, 새 영역부터만 적용할지.

---

## 4. 작업 목록

각 항목은 완료 조건을 모두 충족해야 완료다. 끝나면 이 문서 맨 아래 §6에 결과를 적는다.
**순서가 중요하다:** 이름 변경(4-2)과 결정 분리(4-3)를 먼저 해야 스킬·템플릿이 새 경로를 가리킨다.
작업 전에 `git status`를 확인한다 — 커밋되지 않은 변경(Blog 이동, STATUS 수정 등)이 있으면 사용자에게 먼저 커밋할지 묻는다. 4-2·4-3은 각각 **별도 커밋**으로 남긴다 (이름 변경과 내용 변경을 섞지 않는다).

### 4-1. Codex 플러그인 켜기
- `.claude/settings.json`에 추가: `"enabledPlugins": { "codex@openai-codex": true }` (기존 `permissions`는 유지)
- 완료 조건: 새 세션에서 `/codex:setup`이 준비 완료를 보고한다.

### 4-2. 이름 변경 (§3-5 승인 후에만)
§2.9 "지금 바꾼다" 범위만 바꾼다.

| 지금 | 바뀐 뒤 |
|---|---|
| `DOCS/DOCS.md` | `DOCS/ROADMAP.md` |
| `DOCS/POLISH.md` | `DOCS/POLISH_TRACKER.md` |
| `Notes/04/Status/GAS_STATUS.md` | `Notes/04/Status/04_GAS_STATUS.md` |
| `Notes/05/Status/LOOT_STATUS.md` | `Notes/05/Status/05_Loot_STATUS.md` |
| `Notes/05/05_Loot_DOCS.md` | `Notes/05/05_Loot_Design.md` |
| `Notes/05/05_Loot_03A_Core.md` | `Notes/05/05_Loot_03_Inventory_A_Core.md` |
| `Notes/05/05_Loot_03B_PickupDrop.md` | `Notes/05/05_Loot_03_Inventory_B_PickupDrop.md` |
| `Notes/05/Review/Request·Answer/05_Loot_REVIEWn_*.md` | `05_Loot_REVIEW0n_<주제>_*.md` (두 자리 번호 + 주제. 주제는 Request 제목에서) |
| `Notes/05/Review/05_Loot_REVIEW_Inventory.md`, `..._StructMigration.md` | `..._Inventory_Summary.md`, `..._StructMigration_Summary.md` |

- `04_GAS_DOCS.md`는 완료 영역이라 **바꾸지 않는다** (§2.9). 단, 4-2에서 바꾼 파일을 가리키는 참조는 모두 고친다.
- 방법: `git mv`로 바꾼 뒤, 옛 이름을 저장소 전체(`DOCS/`, `CLAUDE.md`, `AGENTS.md`, `SESSION.md`, `.claude/`, `.github/`)에서 grep해 새 경로로 고친다. 이 문서(`SETUP_SessionRoles.md`)의 경로도 고친다.
- 완료 조건: 옛 이름 grep 결과 0건 (이 문서 §2.9·§4-2의 "지금" 열 제외), `git status`에 rename으로 잡힘.

### 4-3. 결정 이력 분리 (§3-6 답에 따라)
- 새 템플릿: `NN_<영역>_DECISIONS.md` — §2.8 항목 형식. WeaponFireRate STATUS §2 "결정 이력"과 §5 "다시 열지 않는 것"을 본보기로 삼는다.
- Loot에 적용한다면: `05_Loot_STATUS.md`의 "확정된 설계 결정" 절(현재 43~195줄 근처)을 D-001부터 항목으로 옮긴다. `05_Loot_Design.md` §4와 겹치는 내용은 **Design에는 현재 결정만, DECISIONS에는 경위·버린 대안**만 남겨 중복을 없앤다. STATUS에는 진행 상황과 "결정은 `05_Loot_DECISIONS.md`" 한 줄만 남긴다.
- 옮기면서 내용을 바꾸지 않는다. 서로 모순되는 곳을 발견하면 고치지 말고 §6에 적고 사용자에게 묻는다.
- 완료 조건: STATUS에 설계 결정 본문이 없다. Design과 DECISIONS 사이에 같은 문단이 중복되지 않는다.

### 4-4. `session-start` 스킬
- `.claude/skills/session-start/SKILL.md` 작성. 참고: krafton 쪽 같은 이름 스킬의 형식(frontmatter, `$ARGUMENTS`, `disable-model-invocation: true`).
- 내용: 인자 `역할 영역 [대상]` 해석 → 세션 이름 정하고 알림 → 영역 STATUS 인수인계 칸 읽기 → 영역 Design `상태:` 확인 → §2.1 역할별 읽을 것/할 일/하지 않을 일 → 시작 보고.
- 역할별로 반드시 넣을 것:
  - 설계: Design·DECISIONS 작성, 구현서·가이드 작성 금지
  - 검토: 설계 세션 대화 읽기 금지. DECISIONS는 근거 확인용으로만. 문서 반영은 사용자 확인 후
  - 개발: Design이 `승인`이 아니면 멈춤. 설계 변경은 DECISIONS에 "제안"으로만. 구현서는 리프, 가이드는 `Guide/`에 §2.9 이름으로. **코드 파일 수정 금지**
  - 리뷰: 개발 세션 대화 읽기 금지. 코드 파일 수정 금지(지적과 설명만). 결과는 `Review/NN_<영역>_REVIEW_<Step>_Code.md`, 채점은 가이드의 채점 칸
- 완료 조건: `/session-start 개발 Loot <진행 중 Step>`으로 시작한 세션이 올바른 STATUS 두 개와 Design을 읽고, 역할 경계를 보고한다.

### 4-5. `handoff` 스킬
- `.claude/skills/handoff/SKILL.md` 작성. §2.3 인수인계 칸을 갱신하고, Step STATUS가 코드와 맞는지 확인하고, 결정·제안이 DECISIONS에 있는지, 미룬 것이 BACKLOG에 이유와 함께 있는지 확인한다.
- 완료 조건: 실행하면 영역 STATUS의 인수인계 칸이 갱신되고 "다음 행동"이 `/session-start ...` 형식이다.

### 4-6. STATUS에 인수인계 칸, Design에 `상태:` 줄
- 진행 중인 영역 STATUS(`05_Loot_STATUS.md`, `04_Polish_STATUS.md`)에 §2.3 칸을 추가하고 현재 상태로 채운다. 완료된 `04_GAS_STATUS.md`는 건드리지 않는다.
- 진행 중·예정 영역 Design 맨 위에 `> **상태: …**` 한 줄. 값은 §3-4 답에 따른다.
- 완료 조건: 두 STATUS에 칸이 있고 "다음 행동"이 채워져 있다. Loot Design에 `상태:` 줄이 있다.

### 4-7. 문서 규칙 갱신 (`ROADMAP.md` §6)
- §6 "문서 구조 규칙"에 추가:
  - 표준 서브폴더 표에 `Guide/` 행
  - 영역 문서 세 가지(Design / DECISIONS / STATUS)의 역할과 갱신 방식 (§2.8 표)
  - 이름 규칙 (§2.9 표). "완료 영역과 Blog·Mine은 소급하지 않는다"는 문장 포함
  - 가이드 템플릿: 구현서 = 정답지 원칙, 레벨 L1~L3 표, 불변식·도구 목록 쓰는 법, 자기 점검 형식(§A 코딩 전 / §B 코딩 후 / 채점 칸), 레벨 조정 규칙 (§2.4 내용을 옮기되 예시는 짧게)
- 완료 조건: 개발 세션이 이 절만 보고 L2 가이드를 올바른 위치·이름으로 쓸 수 있다.

### 4-8. `SESSION.md`, `CLAUDE.md`, `AGENTS.md` 고치기 (§3-1 승인 후에만)
근거: Claude Code 공식 문서(code.claude.com/docs/en/memory, /best-practices) — 200줄 이하, 줄마다 "지우면 Claude가 실수하나?"를 묻고 아니면 뺀다, 코드만 봐도 알 수 있는 것·디렉터리 구조·자주 바뀌는 정보는 넣지 않는다, 강조는 한두 줄에만, 서로 모순되는 지시는 Claude가 아무 쪽이나 고를 수 있다, 일부 파일에만 해당하는 규칙은 `.claude/rules/`의 경로 규칙으로. (2026-10-02 사용자와 검토)

**`SESSION.md`**
- 원칙 문장 교체, 세션 시작 템플릿을 `/session-start`로, 역할 표 요약 + "compact 대신 handoff" 규칙. STATUS 앵커 원칙(STATUS가 진실의 원천)은 그대로 둔다. 영역 STATUS 예시 경로를 새 이름으로.

**`CLAUDE.md`** (현재 167줄 → 목표 80~100줄)
1. **오래된 정보 삭제:** `GAS Migration State` 절. "현재 `feature-loot` 브랜치에서 Loot/Inventory 진행 중"이라고 되어 있지만 2026-10-02 현재 브랜치는 `feature-gas-polish`다. 진행 상황은 STATUS에 있으므로 "진행 상황은 영역 STATUS를 본다" 한 줄로 대체.
2. **Karpathy 원문 §3·§4를 문서 작업 기준으로 다시 쓰기.** 지금 §4 예시("Write tests for invalid inputs, then make them pass")와 §3 Surgical Changes는 Claude가 코드를 고친다는 전제라 "Claude는 코드 파일을 수정하지 않는다"와 부딪힌다.
   - §3 → "요청받은 문서·절만 고친다. 다른 문서의 옛 내용은 고치지 말고 지적만 한다. 내가 만든 고아(깨진 참조, 안 쓰는 절)만 정리한다."
   - §4 → "구현서·가이드마다 검증 방법(PIE 확인 표, 임시 로그, 기대 결과)을 적는다. 성공 조건이 약한 요청은 되묻는다."
   - 영문 원문 줄과 한국어 추가 줄이 섞인 부분은 한국어로 통일한다.
3. **코드로 알 수 있는 것·중복 줄이기:**
   - `Project Structure` 트리, `Project Overview`의 DOCS 파일 목록 → "문서 지도는 `ROADMAP.md` §6" 한 줄.
   - `Workflow`의 STATUS 운영 절차 상세 → "`SESSION.md` 참고" 한 줄. Workflow에는 규칙만: 역할별 세션(`/session-start`·`/handoff`), Claude는 코드를 수정하지 않는다, STATUS가 진실의 원천, 리뷰 답변은 `review-verifier` 검증 후 반영, Codex 사용처(검토 = adversarial-review, 리뷰 = review).
4. **경로 규칙으로 옮기기** (`.claude/rules/`, `paths` frontmatter — 해당 파일을 읽을 때만 로드된다):
   - `.claude/rules/ue-cpp.md` (`paths: ["EmploymentProj/Source/**"]`): `Conventions` 절 전체 (리플렉션 매크로, `ReplicatedUsing`/`OnRep_`, RPC 접두사, `GetOwner()->HasAuthority()`, 전방 선언·include 규칙, `WeaponDef->`).
   - `.claude/rules/notes-docs.md` (`paths: ["DOCS/Notes/**", "DOCS/Mine/**"]`): `notes-review` 스킬 사용 규칙, 구현서 = 정답지, 가이드 레벨·형식은 `ROADMAP.md` §6 참고.
5. **굵게는 두 줄에만:** "Claude는 코드 파일을 직접 수정하지 않는다", "STATUS가 진행 상태의 진실의 원천". 나머지 굵게는 푼다.
6. **남길 것:** §1(특히 "대상·성공 조건·제약 중 하나라도 빠지면 되묻는다"), §2 Extensibility First 전체, Architecture, Build Commands, Session Start의 `PROJECT_CONTEXT.md`, Agent Rules(하위 에이전트 예외 두 개). 상단 Tradeoff 한 줄과 하단 "이 지침이 작동하고 있다면" 문단은 지워도 되는지 사용자에게 묻는다.
7. **하지 않는 것:** 코드 폴더(`EmploymentProj/Source/**`)의 Edit·Write를 settings.json 권한으로 막는 것 — 사용자가 원하지 않음 (2026-10-02). CLAUDE.md 규칙으로만 둔다.

**`AGENTS.md`** (Codex가 읽는 파일)
- **"코드는 사용자가 직접 작성한다. 리뷰·검토에서는 지적과 설명만 하고 소스 파일을 수정하지 않는다"를 추가한다.** Codex에 리뷰·작업을 맡길 때 코드를 고치지 않게 하기 위해서다.
- 폴더 목록을 CLAUDE.md와 맞춘다 (지금 AGENTS.md는 `Core/, Data/, Types/`, CLAUDE.md는 `Core/, Combat/, Data/, Movement/, Animation/, GAS/` — 실제 `Source/EmploymentProj/Public/`를 보고 맞춘다).
- `DOCS/DOCS.md` 언급을 새 이름으로. 문서 위치(Design/DECISIONS/STATUS/Guide/Review)를 짧게 추가한다 — Codex 검토·리뷰가 근거를 찾을 수 있게.

**확인**
- 고친 뒤 이 세션에서 `/claude-api prompt-audit`를 실행해 낡은 내용·모순·없는 파일 참조를 점검한다 (보고서와 수정안만 나오고, 반영은 사용자 확인 후).
- **새 세션**을 열어 `/context`로 CLAUDE.md와 rules가 로드되는지 확인한다 (CLAUDE.md는 세션 시작 때 읽히므로 고친 세션에서는 반영되지 않는다). `Source/` 파일을 하나 읽은 뒤 `ue-cpp.md`가 로드되는지도 확인한다.
- 완료 조건: CLAUDE.md 120줄 이하, prompt-audit에서 모순·없는 참조 0건, 세 문서(`SESSION.md`·`CLAUDE.md`·`AGENTS.md`)에 "나누지 않는다" 같은 옛 문장과 옛 파일명이 없다.

### 4-9. 시험 운용 (작업 마지막)
- 가장 가까운 Step 하나로 `개발 → (사용자 코딩) → 리뷰` 한 바퀴를 돈다. 또는 미응답인 PrimaryUse 가이드 §3-B를 리뷰 세션에서 채점 방식으로 처리해 본다.
- 결과(무엇이 불편했는지, 가이드 레벨이 맞았는지, 이름 규칙이 실제로 편한지)를 §6에 적고, 필요하면 이 문서와 `ROADMAP.md` §6의 규칙을 고친다.

---

## 5. 옮기지 않는 것 (krafton 쪽에만 있는 것)
- **구현 역할**: 이 프로젝트는 사용자가 코드를 쓴다. 개발 + 사용자 코딩 + 리뷰가 그 자리를 대신한다.
- **PLAN 문서와 PLAN 승인 관문**: 영역 Design의 "범위"·"순서 결정" 절과 로드맵 §5가 그 역할을 한다 (§2.7). 관문은 영역 Design 하나.
- **Experiments/results.tsv, DEAD_ENDS, 실험 역할**: 점수 최적화 대회용 장치다. "다시 열지 않는 것"은 DECISIONS의 "기각" 항목과 "다시 열 조건"이 맡는다.
- (참고) **DECISIONS는 옮긴다** — 처음 논의에서는 빼려 했으나, 실제로는 결정 이력이 STATUS 안에 섞여 있어 분리하는 편이 낫다고 결론 냈다 (§2.8).

---

## 6. 수행 기록
(작업한 세션이 채운다: 날짜, 항목, 결과, 사용자 결정)

### 2026-10-02 — §3 사용자 결정
| §3 | 결정 |
|---|---|
| 선행 커밋 | 기존 미커밋 변경(Blog 이동, STATUS 수정, 코드, 새 문서)은 **사용자가 먼저 커밋**한 뒤 4-1부터 시작한다 |
| 1. `SESSION.md` 원칙 변경 | **승인** — 4-8 진행 |
| 2. 기존 developer 세션 | 이 설정 작업을 한 세션이 4-6에서 인수인계 칸을 채우고 닫는다 |
| 3. 진행 중 Step 가이드 레벨 | **L2로 시험** |
| 4. 영역 Design `상태:` | Loot Design은 **`승인`으로 소급** (GAS는 완료 영역이라 건드리지 않음) |
| 5. 이름 변경 | §4-2 **표 전체 승인** |
| 6. Loot 결정 이력 분리 | **지금 옮긴다** (4-3) |
| (4-8 항목 6) CLAUDE.md Tradeoff 줄, "이 지침이 작동하고 있다면" 문단 | **둘 다 지운다** |

사실 확인 (작업 전): §4-2 표의 "지금" 파일은 모두 있다. Review 쌍은 13개 번호(2~13, 16)이다.
