# Post 2-5 작성 가이드 — 복제(Replication) 개념 정리

> **예상 제목**: `[UE5] 추출 슈터 2-5. 멀티플레이어 복제 설계 — 이 프로젝트의 모든 동기화 결정 근거`
> **참고 문서**: DOCS/Mine/Rep.md, DOCS/Notes/02_Replication.md
> **성격**: 독립 개념글. 2단계 전체의 복제 설계 결정을 한 곳에서 설명.

---

## 개요

**이 포스팅에서 다루는 것:**
- UE5 복제 시스템 핵심 개념 (클래스 존재 범위, 프로퍼티 복제, RPC)
- **이 프로젝트에서 내린 모든 동기화 결정과 그 이유**

**이 글이 필요한 이유:**
- 앞선 포스팅(CMC, CombatComponent)에서 "왜 이 방식으로 동기화했냐"는 질문에 대한 답을 한 곳에 정리
- 이후 포스팅(HP, 사망, 피격)의 구현 이유를 미리 이해하게 함
- "UE5 Replication 정리"로 검색 유입 가능한 독립 레퍼런스

---

## 구현 전 상태 (Before)

복제를 모르고 짜면 반복되는 실수:

```cpp
// 서버에서만 HP가 줄고 클라는 항상 100
void AEPCharacter::TakeDamage(...)
{
    HP -= DamageAmount;  // Replicated 없으면 클라에 전달 안 됨
}

// Server RPC로 Sprint 동기화했더니 이동 시 스냅 발생
UFUNCTION(Server, Reliable)
void Server_SetSprinting(bool bSprinting);  // 이동 패킷과 별개 → 불일치
```

---

## 구현 내용

### 1. 전제: 서버가 유일한 진실

```
서버 = 게임 상태의 원본 (HasAuthority() == true)
클라 = 서버 상태를 복제받아 표현 + Server RPC로 요청
```

클라에서 직접 상태를 바꾸면 서버에 반영이 안 됨. 모든 상태 변경은 서버에서.

### 2. 클래스별 존재 범위

표 + 시각화로 보여줄 것:

```
[서버]
├── GameMode      ─────── 서버 전용 (매치 판정, 스폰 규칙)
├── GameState     ───┬─── 복제 → 모든 클라 (라운드 타이머)
├── PlayerController─┼─── 복제 → 소유 클라만 (입력, HUD)
├── PlayerState   ───┼─── 복제 → 모든 클라 (킬 수, 추출 여부)
└── Character     ───┴─── 복제 → 모든 클라 (위치, 애니메이션)

[클라이언트 A 입장]
  GameMode        ✗  (없음)
  GameState       ✓
  PlayerController A ✓  (내 것만)
  PlayerController B ✗  (남의 것 없음)
  PlayerState A,B ✓  (모두 있지만 COND_OwnerOnly 변수는 제한)
  Character A,B   ✓
```

### 3. 프로퍼티 복제 3패턴

```cpp
// 패턴 1: 단순 복제
UPROPERTY(Replicated)
float HP;
DOREPLIFETIME(AEPCharacter, HP);

// 패턴 2: 복제 + 콜백 (클라이언트에서 반응 필요할 때)
UPROPERTY(ReplicatedUsing = OnRep_HP)
float HP;
UFUNCTION() void OnRep_HP(float OldHP);  // 이전 값도 받을 수 있음

// 패턴 3: 조건부 복제
DOREPLIFETIME_CONDITION(AEPPlayerState, KillCount, COND_OwnerOnly);
```

**COND 선택 기준표:**

| 조건 | 의미 | 이 프로젝트 사용 예 |
|------|------|---------------------|
| `COND_None` | 모두에게 | EquippedWeapon (장착 무기 시각화) |
| `COND_OwnerOnly` | 소유자만 | HP, KillCount, CurrentAmmo |
| `COND_InitialOnly` | 최초 1회 | 캐릭터 고정 속성 |

### 4. RPC 3종류와 선택 기준

```cpp
// Server RPC — 클라의 요청을 서버에 전달
UFUNCTION(Server, Reliable)
void Server_Fire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir);

// Client RPC — 서버가 특정 클라에게만 알림
UFUNCTION(Client, Unreliable)
void Client_PlayHitConfirmSound();  // 쏜 사람만 "챡" 소리

// NetMulticast — 서버가 모든 클라에 브로드캐스트
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayFireEffect(FVector MuzzleLocation);
```

**Reliable vs Unreliable:**

| | Reliable | Unreliable |
|--|----------|------------|
| 보장 | 반드시 도달 | 손실 허용 |
| 비용 | 재전송 오버헤드 | 없음 |
| 사용 | 사격 요청, 사망 | 이펙트, 사운드 |

### 5. 이 프로젝트의 모든 동기화 결정 — 이유 포함

포스팅의 핵심 섹션. 각 결정을 이유와 함께:

---

**결정 1: Sprint/ADS → CMC CompressedFlags**

```
❌ 처음 시도: Server RPC + UPROPERTY(Replicated)
  void Server_SetSprinting(bool b);
  UPROPERTY(Replicated) bool bIsSprinting;

  문제: 이동 패킷과 Sprint 패킷이 별도 전송
       → 서버 재시뮬레이션 시 순서 불일치 → 스냅 발생

✅ 변경: CMC CompressedFlags
  bWantsToSprint = FLAG_Custom_0 (이동 패킷에 포함)
  → 서버가 동일 입력으로 재시뮬레이션 → 클라 예측과 일치
```

→ **이동속도에 영향을 주는 상태는 반드시 CMC로**

---

**결정 2: Crouch → CMC 내장**

```
ACharacter::Crouch() / UnCrouch()
bCanCrouch = true 설정만으로 자동 처리:
  - 서버 동기화
  - 캡슐 높이 조정
  - 클라 예측

별도 RPC/Replicated 변수 불필요
```

---

**결정 3: HP → UPROPERTY(ReplicatedUsing), COND_OwnerOnly**

```
DOREPLIFETIME_CONDITION(AEPCharacter, HP, COND_OwnerOnly);

타르코프 특성: 상대방 HP 바를 볼 수 없음
→ COND_OwnerOnly (나만 내 HP를 받음)
→ OnRep_HP: 클라이언트에서 자신의 UI 업데이트
→ 사망 판정은 Multicast_Die (Reliable) 로 별도 전달
```

---

**결정 4: EquippedWeapon → ReplicatedUsing, COND_None**

```
모든 클라가 상대방이 어떤 무기를 들고 있는지 봐야 함
→ COND_None
→ OnRep_EquippedWeapon: 클라이언트에서 Attach + LinkAnimClassLayers
  (서버에서 한 것을 OnRep에서 동일하게 재현)
```

---

**결정 5: CurrentAmmo → COND_OwnerOnly**

```
탄약은 나만 알면 됨 (타르코프 특성)
상대방 탄약 수는 정보 비대칭 요소
→ COND_OwnerOnly
```

---

**결정 6: KillCount → COND_OwnerOnly**

```
타르코프에서는 킬 수가 공개 정보가 아님
내 킬 수는 나만 앎
→ COND_OwnerOnly
```

---

**결정 7: 피격 피드백 → Multicast Unreliable × 2개**

```cpp
UFUNCTION(NetMulticast, Unreliable) void Multicast_PlayHitReact();   // 피격 애니메이션
UFUNCTION(NetMulticast, Unreliable) void Multicast_PlayPainSound();  // 통증 사운드

모든 클라이언트가 상대방 피격 반응을 봐야 함 → Multicast
애니메이션과 사운드를 분리한 이유: 각각 독립적으로 재생/중단 가능
손실돼도 큰 문제 없음 (다음 피격 때 또 재생됨) → Unreliable
```

---

**결정 8: 히트 확인음 "챡" → Client Unreliable**

```cpp
// EPPlayerController.h
UFUNCTION(Client, Unreliable) void Client_PlayHitConfirmSound();

쏜 사람만 들어야 함 → Client RPC
PlayerController에 배치: HUD/피드백 관련은 Controller 책임
손실돼도 큰 문제 없음 → Unreliable
Stage 3 Lag Compensation과 연결되는 지점
```

---

**결정 8-2: 킬 알림 → Client Reliable**

```cpp
// EPPlayerController.h
UFUNCTION(Client, Reliable) void Client_OnKill(const FString& VictimName);

킬한 사람에게만 알림 → Client RPC
킬 피드(킬 이름) 손실되면 피드백이 사라짐 → Reliable
히트 확인음과 달리 게임 정보(킬 수와 연결)이므로 보장 필요
```

---

**결정 9: 사망 → Multicast Reliable**

```
모든 클라이언트가 사망 처리를 받아야 함 (래그돌)
손실되면 안 됨 (캡슐 콜리전이 그대로 남음)
→ Reliable
```

---

### 6. 한 줄 원칙 정리

```
이동속도 영향 상태   → CMC CompressedFlags
모두가 봐야 하는 상태 → COND_None
나만 알면 되는 상태  → COND_OwnerOnly
클라 → 서버 요청    → Server RPC
서버 → 특정 클라    → Client RPC
서버 → 전체         → Multicast RPC
중요한 것           → Reliable
이펙트/사운드       → Unreliable
```

---

## 결과

이 포스팅이 설명하는 결정들의 동작 확인 체크리스트:
- Sprint 전환 시 2인에서 스냅 없음 (CMC)
- 피격 시 모든 클라에서 애니메이션 재생
- 쏜 사람에게만 "챡" 소리
- CurrentAmmo가 소유자에게만 복제됨

---

## 참고

- `DOCS/Mine/Rep.md` — 복제 개념 전체
- `DOCS/Notes/02_Replication.md` — 학습 노트
- UE5 공식: Network Overview, Replication
