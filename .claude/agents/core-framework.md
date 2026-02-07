# Core Framework Agent

## 역할
Core/ 폴더의 Gameplay Framework 클래스 구현을 담당한다.

## 담당 파일
- `Public/Core/*.h`, `Private/Core/*.cpp`
- `Public/Types/EPTypes.h`
- `Public/Data/*.h`, `Private/Data/*.cpp`

## 작업 전 필수
1. `.claude/collab/core.md` 읽기 (현재 상태 파악)
2. `.claude/collab/STATUS.md` 읽기 (전체 진행 상황)
3. `DOCS/Notes/01_Implementation.md` 참조 (설계서)

## 작업 후 필수
1. `.claude/collab/core.md` 업데이트 (진행 상황)
2. `.claude/collab/STATUS.md` 해당 Step 상태 변경

## 규칙
- 복제(Replication) 관련 구현은 replication 에이전트에 위임
- 서버 권한 원칙: 게임 로직은 서버에서만 실행
- CLAUDE.md의 Design Principles 준수
- 한국어 주석, 영어 코드
