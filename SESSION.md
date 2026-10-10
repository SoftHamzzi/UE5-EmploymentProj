# SESSION.md

세션 관리 전략: **STATUS 파일 앵커 방식**

세션을 역할별로 나누지 않는다. STATUS 파일이 세션 간 상태를 전달하므로, 하나의 흐름으로
계속 진행한다. 새 세션을 열어도 STATUS 파일만 읽으면 이전 상태를 복원한다.

> 2026-10-02에 역할별 세션(`/session-start`·`/handoff`)을 도입했다가 2026-10-04에 철회했다.
> 경위는 `DOCS/SETUP_SessionRoles.md` §6. 그때 함께 들어온 이름 규칙·DECISIONS 분리·가이드
> 규칙은 유지한다.

---

## 구조

영역마다 2단 구조(작아서 단계가 안 나뉘는 영역은 위 단계 하나로 끝나기도 한다 — Polish가 그 예):

- `Status/NN_<영역>_STATUS.md` — 그 영역 전체 진행 상황(어느 단계까지 완료됐는지)
  - `DOCS/Notes/05/Status/05_Loot_STATUS.md`
  - `DOCS/Notes/04/Status/04_Polish_STATUS.md`
  - (완료) `DOCS/Notes/04/Status/04_GAS_STATUS.md`
- `Status/NN_<영역>_SS_<이름>_STATUS.md` — 단계별 상세 상태(Step별 완료 여부, 버그, 미완료 항목)

영역 설계는 `NN_<영역>_Design.md`, 결정 경위는 `NN_<영역>_DECISIONS.md`, 가이드는 `Guide/`.
폴더·이름 규칙 전체는 `DOCS/ROADMAP.md` §6.

코드는 사용자가 직접 작성하고, Claude는 구현 방법을 단계 문서(구현서·가이드)에 기술한다.
코드 수정 후 "STATUS 파일 정리해줘"라고 요청하면 Claude가 코드를 읽고 STATUS 파일을 갱신한다.

---

## 세션 시작 템플릿

```
@NN_<영역>_STATUS.md @NN_<영역>_SS_<이름>_STATUS.md
Step X 진행.
```

---

## 규칙

1. **STATUS 파일이 진실의 원천** — 단계 문서는 예정 코드를 보여줄 뿐, 실제 구현 여부는
   STATUS 파일로 판단한다.
2. **Step 완료 여부는 코드를 읽고 대조해서만 갱신한다.** 코드 수정 후 매번 갱신한다 — 미루면
   다음 세션에서 복원 비용이 커진다.
3. **단계가 완료되면** 단계 STATUS 상단의 "전체 상태"를 완료로 바꾸고, 영역 STATUS의 체크박스를
   갱신한다.
4. **새 작업 영역을 시작할 때** 이 파일을 고치지 않고 그 영역의 Design·DECISIONS·STATUS를 새로
   만든다 — 이 파일은 패턴 설명이지 영역 목록이 아니다.
