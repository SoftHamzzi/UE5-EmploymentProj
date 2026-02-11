# Weapon 시스템 설계

> AEPWeapon을 중심으로 한 서버 권한 무기 시스템.
> 확장성(부착물, 새 무기 타입)을 고려한 구조.

---

## 1. 전체 구조

```
UEPWeaponData (DataAsset)         AEPWeapon (Actor)
  ├─ WeaponName                     ├─ WeaponData → UEPWeaponData 참조
  ├─ Damage                         ├─ WeaponMesh (SkeletalMesh)
  ├─ FireRate                       ├─ WeaponState (Idle/Firing/Reloading)
  ├─ MaxAmmo                        ├─ CurrentAmmo (Replicated)
  ├─ Spread (기본 퍼짐)              ├─ MaxAmmo (Replicated)
  ├─ RecoilPattern                  ├─ CurrentSpread (서버 런타임)
  ├─ FireMode (Single/Burst/Auto)   ├─ ConsecutiveShots (연사 카운트)
  ├─ SpreadConfig                   │
  └─ ReloadTime                     ├─ CanFire() → 상태/탄약/연사속도 검증
                                    ├─ Fire() → 탄약 소모, 퍼짐 적용
                                    ├─ ApplySpread() → 서버 탄 퍼짐
                                    ├─ StartReload() / FinishReload()
                                    └─ GetRecoilAmount() → 클라 카메라 반동
```

---

## 2. 무기 상태 머신

```cpp
// EPTypes.h에 추가
UENUM(BlueprintType)
enum class EEPWeaponState : uint8
{
    Idle,       // 대기
    Firing,     // 발사 중
    Reloading,  // 재장전 중
    Equipping   // 장착 전환 중
};
```

```
[Idle] ──(발사 입력)──→ [Firing] ──(입력 해제/탄약 0)──→ [Idle]
  │                                                          │
  ├──(재장전 입력)──→ [Reloading] ──(타이머 완료)──→ [Idle]   │
  │                                                          │
  └──(무기 교체)──→ [Equipping] ──(타이머 완료)──→ [Idle]     │
                                                             │
  [Firing] ──(탄약 0 + AutoReload)──→ [Reloading] ──────────→┘
```

상태 전환 규칙:
- Firing 중 재장전 불가
- Reloading 중 발사 불가
- Equipping 중 모든 행동 불가
- 상태 전환은 **서버에서만** 수행

---

## 3. UEPWeaponData — 스펙 정의

```cpp
UCLASS(BlueprintType)
class UEPWeaponData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- 기본 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Info")
    FName WeaponName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Info")
    EEPFireMode FireMode = EEPFireMode::Auto;

    // --- 전투 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float Damage = 20.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float FireRate = 10.f; // 초당 발수 (600RPM = 10)

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    uint8 MaxAmmo = 30;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    float ReloadTime = 2.0f;

    // --- 퍼짐 (Spread) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
    float BaseSpread = 0.5f; // 기본 퍼짐 (도)

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
    float SpreadPerShot = 0.1f; // 연사 시 1발당 증가량

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
    float MaxSpread = 5.0f; // 최대 퍼짐

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
    float SpreadRecoveryRate = 3.0f; // 초당 퍼짐 회복량

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
    float ADSSpreadMultiplier = 0.5f; // ADS 시 퍼짐 배율

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
    float MovingSpreadMultiplier = 1.5f; // 이동 중 퍼짐 배율

    // --- 반동 (Recoil, 클라이언트 연출용) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
    float RecoilPitch = 0.3f; // 수직 반동 (도)

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
    float RecoilYaw = 0.1f; // 수평 반동 (도)

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
    float RecoilRecoveryRate = 5.0f; // 반동 회복 속도
};
```

---

## 4. AEPWeapon — 런타임 로직

### 4-1. 프로퍼티

```cpp
UCLASS()
class AEPWeapon : public AActor
{
    GENERATED_BODY()

public:
    AEPWeapon();

    // --- 스펙 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<UEPWeaponData> WeaponData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;

    // --- 복제 ---
    UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo, BlueprintReadOnly, Category = "Weapon")
    uint8 CurrentAmmo = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
    uint8 MaxAmmo = 0;

    // --- 인터페이스 ---
    bool CanFire() const;
    void Fire(FVector& OutDirection);        // 서버 전용: 탄약 소모 + 퍼짐 적용
    FVector ApplySpread(const FVector& Direction) const;

    void StartReload();
    void FinishReload();

    float GetDamage() const;
    float GetRecoilPitch() const;   // 클라이언트 카메라 반동용
    float GetRecoilYaw() const;

protected:
    // --- 서버 런타임 상태 (복제 안 함) ---
    EEPWeaponState WeaponState = EEPWeaponState::Idle;
    float LastFireTime = 0.f;
    float CurrentSpread = 0.f;      // 현재 퍼짐 (연사 시 누적)
    uint8 ConsecutiveShots = 0;     // 연속 발사 수

    FTimerHandle ReloadTimerHandle;

    void UpdateSpread(float DeltaTime);     // 퍼짐 회복
    float CalculateSpread() const;          // 최종 퍼짐 계산

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_CurrentAmmo();
};
```

### 4-2. CanFire — 발사 가능 여부

```cpp
bool AEPWeapon::CanFire() const
{
    if (WeaponState != EEPWeaponState::Idle &&
        WeaponState != EEPWeaponState::Firing) return false;
    if (CurrentAmmo <= 0) return false;
    if (!WeaponData) return false;

    // 연사 속도 체크
    float FireInterval = 1.f / WeaponData->FireRate;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastFireTime < FireInterval) return false;

    return true;
}
```

### 4-3. Fire — 서버 발사 처리

```cpp
// 서버에서만 호출 (Character의 Server_Fire에서)
void AEPWeapon::Fire(FVector& OutDirection)
{
    if (!HasAuthority()) return;

    // 탄약 소모
    CurrentAmmo--;
    LastFireTime = GetWorld()->GetTimeSeconds();

    // 퍼짐 누적
    CurrentSpread = FMath::Min(
        CurrentSpread + WeaponData->SpreadPerShot,
        WeaponData->MaxSpread
    );
    ConsecutiveShots++;

    // 서버 퍼짐 적용
    OutDirection = ApplySpread(OutDirection);

    // 상태 전환
    WeaponState = EEPWeaponState::Firing;

    // 탄약 0이면 자동 재장전 (선택)
    // if (CurrentAmmo <= 0) StartReload();
}
```

### 4-4. ApplySpread — 서버 탄 퍼짐

```cpp
FVector AEPWeapon::ApplySpread(const FVector& Direction) const
{
    float FinalSpread = CalculateSpread();
    float HalfAngle = FMath::DegreesToRadians(FinalSpread * 0.5f);
    return FMath::VRandCone(Direction, HalfAngle);
}

float AEPWeapon::CalculateSpread() const
{
    float Spread = WeaponData->BaseSpread + CurrentSpread;

    // 캐릭터 상태에 따른 배율 (Owner에서 읽기)
    if (AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner()))
    {
        if (Owner->GetIsAiming())
            Spread *= WeaponData->ADSSpreadMultiplier;  // ADS: 퍼짐 감소
        if (Owner->GetVelocity().Size2D() > 10.f)
            Spread *= WeaponData->MovingSpreadMultiplier; // 이동: 퍼짐 증가
    }

    return FMath::Clamp(Spread, 0.f, WeaponData->MaxSpread);
}
```

### 4-5. UpdateSpread — 퍼짐 회복 (Tick)

```cpp
void AEPWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!HasAuthority()) return;

    // 연사 퍼짐 회복
    if (CurrentSpread > 0.f)
    {
        CurrentSpread = FMath::Max(
            0.f,
            CurrentSpread - WeaponData->SpreadRecoveryRate * DeltaTime
        );
    }

    // 발사 중이 아니면 Idle로
    float FireInterval = 1.f / WeaponData->FireRate;
    if (WeaponState == EEPWeaponState::Firing &&
        GetWorld()->GetTimeSeconds() - LastFireTime > FireInterval * 2.f)
    {
        WeaponState = EEPWeaponState::Idle;
        ConsecutiveShots = 0;
    }
}
```

### 4-6. 재장전

```cpp
void AEPWeapon::StartReload()
{
    if (!HasAuthority()) return;
    if (WeaponState == EEPWeaponState::Reloading) return;
    if (CurrentAmmo >= MaxAmmo) return;

    WeaponState = EEPWeaponState::Reloading;

    GetWorldTimerManager().SetTimer(
        ReloadTimerHandle,
        this, &AEPWeapon::FinishReload,
        WeaponData->ReloadTime,
        false
    );
}

void AEPWeapon::FinishReload()
{
    if (!HasAuthority()) return;

    CurrentAmmo = MaxAmmo;
    WeaponState = EEPWeaponState::Idle;
    ConsecutiveShots = 0;
    CurrentSpread = 0.f;
}
```

---

## 5. Character ↔ Weapon 연동

### 5-1. Server_Fire 흐름

```cpp
// AEPCharacter
void AEPCharacter::Server_Fire_Implementation(const FVector& Origin, const FVector& Direction)
{
    if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;

    // Weapon이 탄약 소모 + 퍼짐 적용
    FVector SpreadDir = Direction;
    EquippedWeapon->Fire(SpreadDir);

    // 서버 레이캐스트
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(EquippedWeapon);

    if (GetWorld()->LineTraceSingleByChannel(
        Hit, Origin, Origin + SpreadDir * 10000.f, ECC_Visibility, Params))
    {
        UGameplayStatics::ApplyDamage(
            Hit.GetActor(), EquippedWeapon->GetDamage(),
            GetController(), this, nullptr);
    }

    // 이펙트 (모든 클라)
    Multicast_PlayFireEffect(Origin);
}
```

### 5-2. 클라이언트 입력 + 반동 (카메라 킥)

클라이언트에서 발사 가능 여부를 사전 체크한 뒤, 통과한 경우에만 RPC + 반동을 적용한다.
이 체크는 **최적화용**(불필요한 RPC 방지)이며, 최종 판정은 서버의 CanFire()가 한다.

```cpp
// AEPCharacter.h — 클라이언트 전용 연사속도 추적
protected:
    float LocalLastFireTime = 0.f;
```

```cpp
// AEPCharacter.cpp
void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponData) return;

    // --- 클라이언트 사전 검증 (서버가 최종 판단) ---
    // 탄약 체크 (CurrentAmmo는 COND_OwnerOnly로 복제됨)
    if (EquippedWeapon->CurrentAmmo <= 0) return;

    // 연사속도 체크 (FireRate는 DataAsset이라 클라에도 있음)
    float FireInterval = 1.f / EquippedWeapon->WeaponData->FireRate;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LocalLastFireTime < FireInterval) return;
    LocalLastFireTime = CurrentTime;

    // --- 발사 요청 ---
    FVector Origin = FirstPersonCamera->GetComponentLocation();
    FVector Direction = FirstPersonCamera->GetForwardVector();
    Server_Fire(Origin, Direction);

    // --- 로컬 반동 (사전 검증 통과 시에만) ---
    if (IsLocallyControlled())
    {
        float Pitch = EquippedWeapon->GetRecoilPitch();
        float Yaw = FMath::RandRange(
            -EquippedWeapon->GetRecoilYaw(),
             EquippedWeapon->GetRecoilYaw());
        AddControllerPitchInput(-Pitch);
        AddControllerYawInput(Yaw);
    }
}
```

**흐름:**
```
Input_Fire()
  ├─ CurrentAmmo <= 0?       → return (RPC 안 보냄, 반동 없음)
  ├─ 연사속도 안 됐으면?      → return (RPC 안 보냄, 반동 없음)
  ├─ Server_Fire() RPC 전송
  └─ 로컬 반동 적용           ← 사전 검증 통과했으므로 서버와 거의 일치

Server_Fire_Implementation()
  ├─ CanFire() 실패?          → return (서버 거부, 탄약/이펙트 변화 없음)
  └─ Fire() + 레이캐스트       ← 서버 최종 처리
```

**서버가 거부하는 경우 (드묾):**
- 클라이언트 반동만 1회 헛발생 — 다음 복제에서 CurrentAmmo가 보정됨
- 게임플레이에 체감 영향 거의 없음
```

### 5-3. 재장전 RPC

```cpp
// AEPCharacter
UFUNCTION(Server, Reliable)
void Server_Reload();

void AEPCharacter::Server_Reload_Implementation()
{
    if (!EquippedWeapon) return;
    EquippedWeapon->StartReload();
}
```

---

## 6. 퍼짐(Spread) 시스템 상세

### 서버에서 계산하는 이유

클라이언트가 퍼짐을 계산하면 핵으로 제거 가능.
서버가 최종 탄착 방향을 결정하므로, 노리코일/노스프레드 핵이 무효화됨.

### 퍼짐에 영향을 주는 요소

| 요소 | 효과 | DataAsset 필드 |
|------|------|----------------|
| 기본 퍼짐 | 무기 고유 정확도 | `BaseSpread` |
| 연사 누적 | 연속 발사 시 증가 | `SpreadPerShot` |
| 최대 퍼짐 | 누적 상한 | `MaxSpread` |
| 퍼짐 회복 | 발사 중단 시 감소 | `SpreadRecoveryRate` |
| ADS | 퍼짐 감소 | `ADSSpreadMultiplier` (0.5 = 50%) |
| 이동 중 | 퍼짐 증가 | `MovingSpreadMultiplier` (1.5 = 150%) |
| 점프/낙하 | 퍼짐 크게 증가 | (추가 가능) |

### 퍼짐 흐름 예시

```
정지 ADS:     0.5 * 0.5 = 0.25도
정지 비조준:   0.5도
이동 비조준:   0.5 * 1.5 = 0.75도
연사 10발째:   0.5 + (0.1 * 10) = 1.5도
연사 + 이동:   (0.5 + 1.0) * 1.5 = 2.25도
```

---

## 7. 반동(Recoil) 시스템 상세

### 역할 분리

| | 위치 | 역할 | 치트 영향 |
|--|------|------|----------|
| Spread (퍼짐) | **서버** | 실제 탄착 결정 | 치트 불가 |
| Recoil (반동) | **클라이언트** | 카메라 킥 연출 | 제거 가능하지만 무의미 |

반동을 제거해도 서버 퍼짐이 있으므로 정확도 이득 없음.

### 반동 패턴 (선택적 확장)

단순 반동:
```cpp
// 매 발사 시
AddControllerPitchInput(-RecoilPitch);
AddControllerYawInput(RandRange(-RecoilYaw, RecoilYaw));
```

패턴 반동 (확장):
```cpp
// DataAsset에 패턴 배열 추가
UPROPERTY(EditDefaultsOnly, Category = "Recoil")
TArray<FVector2D> RecoilPattern; // (Pitch, Yaw) 시퀀스

// 연사 인덱스에 따라 다른 반동
int32 Index = FMath::Min(ConsecutiveShots, RecoilPattern.Num() - 1);
FVector2D Kick = RecoilPattern[Index];
```

---

## 8. 확장 포인트

### 8-1. 부착물 (Attachment)

확장탄창, 소음기 등은 WeaponData를 런타임에 수정하는 방식:

```cpp
// DataAsset 값을 직접 수정하지 않음 — 런타임 오프셋
UPROPERTY(Replicated)
int8 BonusMaxAmmo = 0; // 확장탄창: +10

// 실제 사용 시
uint8 GetEffectiveMaxAmmo() const { return MaxAmmo + BonusMaxAmmo; }
```

또는 부착물을 별도 DataAsset으로:
```
UEPAttachmentData (DataAsset)
  ├─ BonusAmmo: +10
  ├─ SpreadMultiplier: 0.8 (레이저 사이트)
  ├─ RecoilMultiplier: 0.7 (수직 손잡이)
  └─ DamageMultiplier: 1.0 (소음기, 대미지 변경 없음)
```

### 8-2. 새 발사 모드

Burst 모드 추가:
```cpp
UENUM(BlueprintType)
enum class EEPFireMode : uint8
{
    Single,     // 1발
    Burst,      // 3점사
    Auto        // 연사
};

// Burst 처리: 1회 입력 → 3발 타이머
// Single: 입력당 1회만 Server_Fire
// Auto: 입력 유지 중 FireRate 간격으로 Server_Fire
```

### 8-3. 투사체 무기 (미래)

히트스캔이 아닌 투사체(로켓, 유탄):
```cpp
// AEPWeapon을 상속한 AEPProjectileWeapon
// Fire()에서 레이캐스트 대신 Projectile Actor 스폰
// 서버에서 Projectile 스폰 → 복제 → 클라이언트에서 시각 표현
```

---

## 9. 무기별 DataAsset 예시

| | AR (돌격소총) | SMG (기관단총) | Sniper (저격) |
|--|:---:|:---:|:---:|
| Damage | 25 | 18 | 90 |
| FireRate | 10 | 15 | 1.2 |
| MaxAmmo | 30 | 35 | 5 |
| BaseSpread | 0.5 | 0.8 | 0.1 |
| SpreadPerShot | 0.1 | 0.15 | 0.5 |
| MaxSpread | 4.0 | 5.0 | 2.0 |
| ADSSpreadMul | 0.5 | 0.6 | 0.1 |
| RecoilPitch | 0.3 | 0.2 | 1.5 |
| RecoilYaw | 0.1 | 0.15 | 0.3 |
| FireMode | Auto | Auto | Single |
| ReloadTime | 2.0 | 1.5 | 3.0 |

DataAsset을 바꾸는 것만으로 새 무기 생성 가능. C++ 코드 수정 불필요.

---

## 10. 복제 정리

| 데이터 | 위치 | 방식 | 이유 |
|--------|------|------|------|
| CurrentAmmo | AEPWeapon | ReplicatedUsing, COND_OwnerOnly | UI 갱신 |
| MaxAmmo | AEPWeapon | Replicated, COND_OwnerOnly | 확장탄창 반영 |
| WeaponState | AEPWeapon | 복제 안 함 | 서버에서만 판단 |
| CurrentSpread | AEPWeapon | 복제 안 함 | 서버에서만 계산 |
| ConsecutiveShots | AEPWeapon | 복제 안 함 | 서버에서만 추적 |
| EquippedWeapon | AEPCharacter | ReplicatedUsing | 무기 시각 동기화 |
| Server_Fire | RPC | Server, Reliable | 발사 요청 |
| Server_Reload | RPC | Server, Reliable | 재장전 요청 |
| Multicast_PlayFireEffect | RPC | NetMulticast, Unreliable | 이펙트/사운드 |

---

## 11. 구현 순서 권장

1. WeaponState enum 추가 (EPTypes.h)
2. CanFire() 검증 로직 구현
3. Fire()에 탄약 소모 + 연사속도 체크
4. ApplySpread() + CalculateSpread() 서버 퍼짐
5. Tick에서 퍼짐 회복
6. StartReload / FinishReload
7. 클라이언트 반동 (카메라 킥)
8. DataAsset에 Spread/Recoil 필드 추가
9. (선택) Burst 모드
10. (선택) 부착물 시스템
