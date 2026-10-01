# SESSION.md

세션 관리 전략: **역할별 세션 + STATUS 앵커**

세션 하나는 역할 하나를 맡는다. 길어지면 compact하지 않고 `/handoff`로 영역 STATUS의 인수인계 칸을 채운 뒤 새 세션을 연다. STATUS 파일이 세션 간 상태를 전달하므로, 새 세션은 대화 기록 없이 STATUS만 읽고 이어받는다.

---

## 역할

| 역할 | 시작 | 하는 일 | 끝 |
|---|---|---|---|
| 설계 | `/session-start 설계 <영역>` | 영역 Design·DECISIONS 작성, Step 나누기 | 설계 초안 완료 |
| 검토 | `/session-start 검토 <영역> DOCS` | Request 작성 → `/codex:adversarial-review` → `review-verifier` → 반영 여부 보고. 설계 세션 대화는 읽지 않는다 | 검토 한 건 |
| 개발 | `/session-start 개발 <영역> <Step>` | 승인된 설계로 구현서(정답지)와 가이드 작성, 코딩 중 질문 응답, STATUS 갱신 | Step 하나 |
| 리뷰 | `/session-start 리뷰 <영역> <Step>` | 구현서와 코드의 차이, `/codex:review`, 가이드 자기 점검 채점, 오답을 `StudyPath.md`로. 개발 세션 대화는 읽지 않는다 | 리뷰 한 건 |
| 질문 | `/session-start 질문 <주제>` | 엔진 개념·도구 질문, 조사 | 자유 |

흐름: 설계 → 검토 → **사용자 승인**(영역 Design `상태: 승인`) → 개발 → 사용자 코딩·PIE → 리뷰 → 다음 Step.
역할 밖의 일이 생기면 하지 않고 STATUS "다음 행동"이나 `DOCS/BACKLOG.md`에 적는다. 설계를 바꿔야 하면 DECISIONS에 `제안`으로 적고 멈춘다.

---

## STATUS 구조

영역마다 2단 구조(작아서 단계가 안 나뉘는 영역은 위 단계 하나로 끝나기도 한다 — Polish가 그 예):

- `Status/NN_<영역>_STATUS.md` — 영역 전체 진행 상황 + 위쪽 **인수인계** 칸
  - `DOCS/Notes/05/Status/05_Loot_STATUS.md`
  - `DOCS/Notes/04/Status/04_Polish_STATUS.md`
  - (완료) `DOCS/Notes/04/Status/04_GAS_STATUS.md`
- `Status/NN_<영역>_SS_<이름>_STATUS.md` — Step별 완료 여부, 버그, 미완료 항목

폴더·이름 규칙 전체는 `DOCS/ROADMAP.md` §6.

---

## 규칙

1. **STATUS 파일이 진실의 원천** — 단계 문서는 예정 코드를 보여줄 뿐, 실제 구현 여부는 STATUS 파일로 판단한다.
2. **Step 완료 여부는 코드를 읽고 대조해서만 갱신한다.** 코드 수정 후 사용자가 요청하면(또는 `/handoff`에서) Claude가 코드를 읽고 갱신한다.
3. **인수인계 칸은 `/handoff`가 덮어쓴다.** "다음 행동"은 `/session-start <역할> <영역> <대상>` 형식 하나.
4. **Step이 완료되면** Step STATUS 상단의 "전체 상태"를 완료로 바꾸고, 영역 STATUS의 체크박스를 갱신한다.
5. **새 작업 영역을 시작할 때** 이 파일을 고치지 않고 그 영역의 Design·DECISIONS·STATUS를 새로 만든다 — 이 파일은 패턴 설명이지 영역 목록이 아니다.
