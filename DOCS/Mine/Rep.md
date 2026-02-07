# UE5 네트워킹 총 정리

---

## 1. 클래스별 존재 범위

| 클래스 | 서버 | 클라이언트 | 결정 요소 |
|--------|:----:|:----------:|-----------|
| GameMode | O | X | `bReplicates=false`, `bNetLoadOnClient=false` |
| GameState | O | O (모두) | `bReplicates=true`, `bAlwaysRelevant=true` |
| PlayerController | O | O (소유자만) | `bReplicates=true`, `bOnlyRelevantToOwner=true` |
| PlayerState | O | O (모두) | `bReplicates=true`, `bAlwaysRelevant=true` |
| Character | O | O (모두) | `bReplicates=true` |

---

## 2. 변수 복제 범위

```cpp
// 복제 안 함 (각자 독립)
UPROPERTY()
int32 LocalValue;

// 서버 → 모든 클라
DOREPLIFETIME(Class, Variable);

// 서버 → 소유 클라만
DOREPLIFETIME_CONDITION(Class, Variable, COND_OwnerOnly);

// 서버 → 소유자 제외
DOREPLIFETIME_CONDITION(Class, Variable, COND_SkipOwner);
```

---

## 3. 데이터 흐름 방향

```
서버 → 클라이언트:  Replication (UPROPERTY(Replicated))
클라이언트 → 서버:  Server RPC (UFUNCTION(Server, Reliable))
서버 → 특정 클라:   Client RPC (UFUNCTION(Client, Reliable))
서버 → 모든 클라:   NetMulticast RPC (UFUNCTION(NetMulticast))
```

---

## 4. 누가 알게 할 것인가

| 목표 | 방법 |
|------|------|
| 서버만 | GameMode에 저장 |
| 서버 + 본인 | PlayerState + `COND_OwnerOnly` |
| 서버 + 본인 | PlayerController + Replicated |
| 서버 + 모두 | PlayerState/GameState + `DOREPLIFETIME` |
| 클라이언트만 | 복제 안 함 (로컬 변수) |
| 클라 → 서버 전송 | Server RPC |

---

## 5. 복제 콜백

```cpp
// 값만 복제 (콜백 없음)
UPROPERTY(Replicated)
int32 Value;

// 값 복제 + 콜백 (클라이언트에서 실행)
UPROPERTY(ReplicatedUsing = OnRep_Value)
int32 Value;

UFUNCTION()
void OnRep_Value()
{
    // 클라이언트에서만 실행됨
    // UI 업데이트 등
}
```

---

## 6. 액터 복제 플래그

| 플래그 | 의미 |
|--------|------|
| `bReplicates` | 동적 스폰 시 복제 |
| `bNetLoadOnClient` | 레벨 로드 시 클라 생성 |
| `bAlwaysRelevant` | 모든 클라에게 항상 복제 |
| `bOnlyRelevantToOwner` | 소유자에게만 복제 |

---

## 7. 시각화

```
[서버]
├── GameMode ────────────────── 서버 전용 (판정, 규칙)
├── GameState ─────────────┬── 복제 → 모든 클라 (공개 상태)
├── PlayerController A ────┼── 복제 → 클라 A만 (입력, RPC)
├── PlayerState A ─────────┼── 복제 → 모든 클라 (공개 데이터)
│   └── KillCount ─────────┼── COND_OwnerOnly → 클라 A만
└── Character A ───────────┴── 복제 → 모든 클라 (위치, 애니메이션)

[클라이언트 A]
├── GameMode ✗
├── GameState ✓
├── PlayerController A ✓ (본인 것)
├── PlayerController B ✗ (남의 것)
├── PlayerState A ✓
├── PlayerState B ✓ (단, COND_OwnerOnly 변수는 0)
└── Character A, B ✓
```

---

## 8. 타르코프 스타일 적용

| 데이터 | 위치 | 복제 | 이유 |
|--------|------|------|------|
| 킬 수 | PlayerState | COND_OwnerOnly | 본인만 앎 |
| 탈출 여부 | PlayerState | COND_OwnerOnly | 본인만 앎 |
| 생존 플레이어 수 | GameMode | 없음 | 서버만 앎 |
| 돈 | 인벤토리 아이템 | 본인만 | 가방에 있는 아이템 |
| 사망 여부 | **Corpse 액터** | Relevancy | 시체로 확인 (가까워야 보임) |
| 매치 시간 | GameState | 모두 | UI 표시 |

---

## 9. 자주 쓰는 COND 조건들

| 조건 | 의미 |
|------|------|
| `COND_None` | 조건 없음 (항상 복제, 기본값) |
| `COND_OwnerOnly` | 소유 클라이언트에게만 |
| `COND_SkipOwner` | 소유자 제외 모두에게 |
| `COND_SimulatedOnly` | Simulated Proxy에게만 |
| `COND_AutonomousOnly` | Autonomous Proxy에게만 |
| `COND_InitialOnly` | 최초 복제 시 1회만 |
| `COND_InitialOrOwner` | 최초 1회 또는 소유자에게 |

---

## 10. RPC 종류

| 종류 | 실행 위치 | 호출자 | 용도 |
|------|----------|--------|------|
| Server RPC | 서버 | 클라이언트 | 클라→서버 요청 |
| Client RPC | 소유 클라 | 서버 | 서버→특정 클라 알림 |
| NetMulticast | 서버+모든 클라 | 서버 | 서버→전체 브로드캐스트 |

```cpp
// Server RPC (클라 → 서버)
UFUNCTION(Server, Reliable)
void Server_Fire();

// Client RPC (서버 → 소유 클라)
UFUNCTION(Client, Reliable)
void Client_ShowHitMarker();

// NetMulticast (서버 → 모두)
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlaySound();
```
