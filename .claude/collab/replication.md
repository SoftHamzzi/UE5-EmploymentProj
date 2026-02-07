# Replication 작업 문서

> 담당 에이전트: replication
> 담당 영역: 복제 변수, RPC, 네트워크 관련 전반

---

## 담당 범위

- 모든 클래스의 `GetLifetimeReplicatedProps()` 구현
- `DOREPLIFETIME` / `DOREPLIFETIME_CONDITION` 설정
- `OnRep_*` 콜백 구현
- Server/Client/Multicast RPC 구현
- Relevancy 설정 검토

---

## 복제 설계 (확정)

| 클래스 | 변수 | 복제 조건 | 이유 |
|--------|------|----------|------|
| GameState | RemainingTime | 모두 | UI 표시 |
| GameState | MatchPhase | 모두 | UI 표시 |
| PlayerState | KillCount | COND_OwnerOnly | 본인만 |
| PlayerState | bIsExtracted | COND_OwnerOnly | 본인만 |
| Corpse | Inventory | 모두 (Relevancy) | 루팅용 |
| Corpse | PlayerName | 모두 (Relevancy) | 식별용 |

### 제외된 변수
| 변수 | 원래 위치 | 제외 이유 |
|------|----------|----------|
| bIsDead | PlayerState | Corpse 액터로 대체 (정보 은폐) |
| Money | PlayerState | 인벤토리 아이템으로 처리 |
| AlivePlayerCount | GameState | GameMode(서버 전용)에서 관리 |

---

## RPC 설계

| RPC | 타입 | 위치 | 용도 |
|-----|------|------|------|
| Client_OnKill | Client, Reliable | PlayerController | 킬 피드백 |

---

## 완료된 작업

(없음 - Step 4, 5에서 시작)

---

## 대기 중 작업

### Step 4: GameState 복제
- [ ] GetLifetimeReplicatedProps 구현
- [ ] OnRep_RemainingTime 구현
- [ ] OnRep_MatchPhase 구현
- [ ] SetRemainingTime / SetMatchPhase 구현

### Step 5: PlayerState 복제
- [ ] GetLifetimeReplicatedProps 구현 (COND_OwnerOnly)
- [ ] OnRep_KillCount 구현
- [ ] OnRep_IsExtracted 구현
- [ ] AddKill / SetExtracted 구현 (Authority 체크)

### Corpse 복제 (Step 미정)
- [ ] GetLifetimeReplicatedProps 구현
- [ ] bReplicates = true 확인
- [ ] Relevancy 테스트

### RPC 구현 (Step 미정)
- [ ] Client_OnKill_Implementation

---

## 참조 문서

- DOCS/Mine/Rep.md (네트워킹 총 정리)
- DOCS/Notes/01_Implementation.md
