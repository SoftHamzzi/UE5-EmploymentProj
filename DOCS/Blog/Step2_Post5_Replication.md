# Post 2-5 작성 가이드 — 복제(Replication) 개념 정리

> **예상 제목**: `[UE5] 추출 슈터 2-5. 멀티플레이어 복제(Replication) 핵심 정리 — 이 프로젝트에 적용한 방식`
> **참고 문서**: DOCS/Mine/Rep.md, DOCS/Notes/02_Replication.md

---

## 개요

**이 포스팅에서 다루는 것:**
- UE5 복제 시스템의 핵심 개념 (프로퍼티 복제 / RPC / 역할 구분)
- 이 프로젝트에서 어떤 데이터를 어떤 방식으로 복제했는지와 그 이유

**왜 이 포스팅이 필요한가:**
- 복제 설계를 모르고 코드를 짜면 "서버에서만 바뀌고 클라는 그대로"인 버그가 반복 발생
- CMC로 Sprint/ADS를 동기화한 이유, HP를 UPROPERTY로 복제한 이유 등 앞선 구현의 "왜"를 정리

---

## 개요 (포스팅 첫 문단)

핵심 메시지: **UE5 멀티플레이어는 "서버가 진실, 클라는 예측+표현"**

```
서버: 게임 상태의 유일한 원본 (HasAuthority() == true)
클라: 서버 상태를 복제받아 표현, Server RPC로 요청
```

---

## 구현 전 상태 (Before)

복제를 이해하기 전 흔히 하는 실수:

```cpp
// 잘못된 예: 클라에서 직접 변수 수정
void AEPCharacter::TakeDamage(...)
{
    HP -= DamageAmount;  // 클라에서 실행 시 서버에 반영 안 됨!
}
```

---

## 구현 내용

### 1. 클래스별 존재 범위

표로 정리:

| 클래스 | 서버 | 클라이언트 | 비고 |
|--------|:----:|:----------:|------|
| GameMode | ✅ | ❌ | 서버 전용 (판정, 규칙) |
| GameState | ✅ | ✅ (모두) | `bAlwaysRelevant=true` |
| PlayerController | ✅ | ✅ (소유자만) | `bOnlyRelevantToOwner=true` |
| PlayerState | ✅ | ✅ (모두) | `bAlwaysRelevant=true` |
| Character | ✅ | ✅ (모두) | `bReplicates=true` |

**시각화 (Rep.md 섹션 7)**:

```
[서버]
├── GameMode ──────────────── 서버 전용
├── GameState ─────────────── 복제 → 모든 클라
├── PlayerController A ─────── 복제 → 클라 A만
├── PlayerState A ─────────── 복제 → 모든 클라
└── Character A ──────────── 복제 → 모든 클라

[클라이언트 A]
├── GameMode ✗
├── GameState ✓
├── PlayerController A ✓ (본인 것만)
└── Character A, B ✓
```

### 2. 프로퍼티 복제 3가지 패턴

```cpp
// 패턴 1: 단순 복제 (콜백 없음)
UPROPERTY(Replicated)
int32 KillCount;

// 패턴 2: 복제 + OnRep 콜백 (클라이언트에서 실행)
UPROPERTY(ReplicatedUsing = OnRep_HP)
float HP = 100.f;

UFUNCTION()
void OnRep_HP(float OldHP);  // 이전 값도 받을 수 있음

// 패턴 3: 조건부 복제
DOREPLIFETIME_CONDITION(AEPPlayerState, KillCount, COND_OwnerOnly);
```

**자주 쓰는 COND:**

| 조건 | 의미 | 이 프로젝트 사용 예 |
|------|------|---------------------|
| `COND_None` | 모두에게 (기본) | HP (체력바 표시) |
| `COND_OwnerOnly` | 소유자만 | KillCount, CurrentAmmo |
| `COND_SkipOwner` | 소유자 제외 | (없음) |
| `COND_InitialOnly` | 최초 1회 | 캐릭터 이름 등 |

### 3. RPC 3종류

```cpp
// Server RPC: 클라 → 서버 요청
// - 반드시 서버에서 실행됨
// - 클라에서 호출해도 서버에서 _Implementation이 실행됨
UFUNCTION(Server, Reliable)
void Server_Fire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);

// Client RPC: 서버 → 특정 클라 알림
// - 소유 PlayerController가 있는 클라에서만 실행
UFUNCTION(Client, Unreliable)
void Client_PlayHitConfirmSound();

// NetMulticast: 서버 → 모든 클라 브로드캐스트
// - 서버 포함 모든 곳에서 실행
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayFireEffect(FVector MuzzleLocation);
```

**Reliable vs Unreliable 선택 기준:**

| Reliable | Unreliable |
|----------|------------|
| 게임 상태 변경 (사격 요청, 사망) | 이펙트, 사운드 |
| 반드시 도달해야 함 | 빠짐이 허용됨 |
| 재전송 보장 (대역폭 소모) | 재전송 없음 |

### 4. HasAuthority() 패턴

```cpp
// 서버에서만 실행해야 하는 로직
float AEPCharacter::TakeDamage(float DamageAmount, ...)
{
    if (!HasAuthority()) return 0.f;  // 서버 권한 체크

    HP = FMath::Clamp(HP - DamageAmount, 0.f, MaxHP);
    // HP는 UPROPERTY(Replicated) → 자동으로 클라에 복제됨
    if (HP <= 0.f) Die();
    return DamageAmount;
}
```

**ENetRole로 세부 분기 (필요 시):**
```cpp
// ROLE_Authority: 서버 (또는 Listen Server의 호스트)
// ROLE_AutonomousProxy: 소유 클라이언트 (본인 캐릭터)
// ROLE_SimulatedProxy: 비소유 클라이언트 (다른 플레이어 캐릭터)
```

### 5. 이 프로젝트 복제 설계표

| 데이터 | 방식 | 조건 | 이유 |
|--------|------|------|------|
| Sprint/ADS 상태 | CMC CompressedFlags | 이동 패킷 포함 | 예측 일치 (스냅 방지) |
| Crouch 상태 | CMC 내장 | 자동 | `bIsCrouched` 자동 복제 |
| HP | `UPROPERTY(Replicated)` | COND_None | 체력바를 모두가 봄 |
| EquippedWeapon | `ReplicatedUsing` | COND_None | OnRep에서 attach 처리 |
| CurrentAmmo | `ReplicatedUsing` | COND_OwnerOnly | 본인만 탄약 확인 |
| KillCount | `DOREPLIFETIME_CONDITION` | COND_OwnerOnly | 타르코프 특성 |

**Sprint/ADS가 UPROPERTY 복제가 아닌 이유 강조:**
> CMC의 CompressedFlags를 쓰면 이동 패킷에 Sprint 상태가 같이 담겨 서버 재시뮬레이션 시 클라 예측과 일치함.
> UPROPERTY로 복제하면 별도 패킷 → 이동 패킷과 순서 불일치 → 스냅 발생.

---

## 결과

**이 포스팅이 설명하는 것들의 동작 확인:**
- PIE 2인에서 한 클라의 HP가 줄었을 때 다른 클라에도 변화 보임
- `CurrentAmmo`는 소유자에게만 보임 (COND_OwnerOnly)
- 서버에서만 TakeDamage 처리 (HasAuthority 체크)

---

## 참고

- `DOCS/Mine/Rep.md` — 복제 개념 전체 정리
- `DOCS/Notes/02_Replication.md` — 학습 노트
- UE5 공식: Network Overview, Property Replication
