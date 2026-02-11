# [UE5 C++] 2단계-2: 무기 장착 + 서버 권한 사격

> 02_Implementation Step 3 (무기 시스템), Step 4 (사격 Server RPC + 이펙트)

---

## 1. 이번 글에서 다루는 것

- AEPWeapon: 복제되는 무기 Actor 구현
- 무기 장착: 서버 스폰 → 소켓 Attach → 클라이언트 복제
- UEPWeaponData: DataAsset 기반 무기 스펙
- Server_Fire: 서버 권한 히트스캔 (레이캐스트)
- Multicast_PlayFireEffect: 모든 클라이언트에 이펙트 재생
- FVector_NetQuantize / FVector_NetQuantizeNormal: 대역폭 절감

---

## 2. 전체 구조 개요

### 사격 흐름

```
[클라이언트]
Input_Fire() → Server_Fire(Origin, Direction) RPC 전송

[서버]
Server_Fire_Implementation()
  ├─ 연사 속도 / 탄약 검증
  ├─ LineTraceSingleByChannel (히트스캔)
  ├─ 히트 시 ApplyDamage
  └─ Multicast_PlayFireEffect() → 모든 클라이언트에 이펙트

[모든 클라이언트 + 서버]
Multicast_PlayFireEffect_Implementation()
  └─ 총구 이펙트 / 사운드 / 탄착 파티클
```

### 관련 클래스

| 클래스 | 역할 |
|--------|------|
| `AEPWeapon` | 무기 Actor. 메시, 탄약, 발사 로직 |
| `UEPWeaponData` | DataAsset. 데미지, 연사속도, 최대탄약 등 스펙 |
| `AEPCharacter` | 무기 소유, 입력 → Server_Fire 호출 |
| `AEPGameMode` | HandleStartingNewPlayer에서 무기 스폰/지급 |

---

## 3. AEPWeapon — 무기 Actor

### 3-1. 헤더 (EPWeapon.h)

```cpp
UCLASS()
class EMPLOYMENTPROJ_API AEPWeapon : public AActor
{
    GENERATED_BODY()

public:
    AEPWeapon();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<UEPWeaponData> WeaponData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;

    void Fire(FVector Origin, FVector Direction);
    float GetDamage() const;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo)
    int32 CurrentAmmo;

    UFUNCTION()
    void OnRep_CurrentAmmo() const;

protected:
    float LastFireTime = 0.f;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

<!--
  설계 포인트:
  - bReplicates = true: 모든 클라이언트에 무기 액터 복제
  - CurrentAmmo: COND_OwnerOnly → 소유자만 탄약 확인
  - WeaponData: DataAsset으로 스펙 분리 (Damage, FireRate, MaxAmmo 등)
  - WeaponMesh: 콜리전 비활성화 (캐릭터 메시 소켓에 Attach)
-->

### 3-2. 생성자

```cpp
AEPWeapon::AEPWeapon()
{
    bReplicates = true;
    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
```

<!--
  - bReplicates = true: 서버에서 스폰 시 클라이언트에도 자동 생성
  - 콜리전 비활성화: 캐릭터 메시에 Attach되므로 별도 충돌 불필요
-->

### 3-3. GetDamage()

```cpp
float AEPWeapon::GetDamage() const
{
    return WeaponData ? WeaponData->Damage : 0.f;
}
```

<!--
  WeaponData null 체크: DataAsset 미할당 시 안전하게 0 반환
-->

### 3-4. Fire()

```cpp
void AEPWeapon::Fire(FVector Origin, FVector Direction)
{
    // TODO: 연사 속도 검증, 탄약 소모, 레이캐스트
}
```

<!--
  구현 예정 내용:
  - GetWorld()->GetTimeSeconds() - LastFireTime < FireRate 검증
  - CurrentAmmo-- 후 탄약 0 체크
  - LineTraceSingleByChannel 또는 Character에서 직접 처리
-->

### 3-5. 복제 설정

```cpp
void AEPWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(AEPWeapon, CurrentAmmo, COND_OwnerOnly);
}

void AEPWeapon::OnRep_CurrentAmmo() const
{
    // 클라이언트 UI 갱신
}
```

<!--
  COND_OwnerOnly:
  - 탄약은 소유 클라이언트에만 복제 (다른 플레이어에게 보여줄 필요 없음)
  - Owner 체인: Weapon → Character(Owner) → PlayerController → Connection
  - GameMode에서 SpawnActor 시 Params.Owner = Character로 설정
-->

---

## 4. 무기 장착 시스템

### 4-1. 서버에서 무기 스폰 (GameMode)

```cpp
void AEPGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    AEPCharacter* Char = Cast<AEPCharacter>(NewPlayer->GetPawn());
    if (!Char || !DefaultWeaponClass) return;

    FActorSpawnParameters Params;
    Params.Owner = Char;
    Params.Instigator = Char;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AEPWeapon* Weapon = GetWorld()->SpawnActor<AEPWeapon>(
        DefaultWeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Weapon) return;

    Char->SetEquippedWeapon(Weapon);
}
```

<!--
  왜 GameMode에서 스폰하는가:
  - GameMode는 서버 전용 → 무기 스폰은 서버 권한
  - HandleStartingNewPlayer: 플레이어 Pawn 생성 직후 호출
  - Params.Owner = Char: COND_OwnerOnly 복제를 위한 소유권 체인 설정
  - AlwaysSpawn: 충돌 무시하고 반드시 스폰
-->

### 4-2. Character에 장착

```cpp
// EPCharacter.h
UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon, BlueprintReadOnly, Category = "Combat")
TObjectPtr<AEPWeapon> EquippedWeapon;

void SetEquippedWeapon(AEPWeapon* Weapon);

UFUNCTION()
void OnRep_EquippedWeapon();
```

```cpp
// EPCharacter.cpp
void AEPCharacter::SetEquippedWeapon(AEPWeapon* Weapon)
{
    EquippedWeapon = Weapon;
}

void AEPCharacter::OnRep_EquippedWeapon()
{
    if (!EquippedWeapon) return;
    EquippedWeapon->AttachToComponent(
        GetMesh(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        TEXT("WeaponSocket")
    );
}
```

<!--
  복제 흐름:
  1. 서버: SetEquippedWeapon() → EquippedWeapon 포인터 설정
  2. 복제: EquippedWeapon이 ReplicatedUsing이므로 클라이언트에 전파
  3. 클라이언트: OnRep_EquippedWeapon() → 메시 소켓에 Attach

  주의: OnRep은 클라이언트에서만 호출됨
  → 서버에서도 Attach가 필요하면 SetEquippedWeapon()에 추가해야 함
-->

---

## 5. 사격 — Server RPC

### 5-1. 입력 → RPC 호출

```cpp
void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
    if (!EquippedWeapon) return;
    FVector Origin = FirstPersonCamera->GetComponentLocation();
    FVector Direction = FirstPersonCamera->GetForwardVector();
    Server_Fire(Origin, Direction);
}
```

<!--
  왜 사격은 CMC가 아닌 Server RPC인가:
  - 사격은 이동속도에 영향 없음 → CMC 확장 대상 아님
  - 이벤트성 행위 (1회 실행) → RPC가 적합
  - Server, Reliable: 게임 로직에 영향 → 누락 불가
-->

### 5-2. Server_Fire_Implementation

```cpp
void AEPCharacter::Server_Fire_Implementation(
    FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)
{
    if (!EquippedWeapon) return;

    // 연사 속도 검증
    float CurrentTime = GetWorld()->GetTimeSeconds();
    // if (CurrentTime - EquippedWeapon->LastFireTime < FireRate) return;

    // 탄약 검증
    // if (EquippedWeapon->CurrentAmmo <= 0) return;
    // EquippedWeapon->CurrentAmmo--;

    // 서버 레이캐스트
    FHitResult Hit;
    FVector End = Origin + Direction * 10000.f;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params))
    {
        // UGameplayStatics::ApplyDamage(Hit.GetActor(), EquippedWeapon->GetDamage(),
        //     GetController(), this, nullptr);
    }

    Multicast_PlayFireEffect(Origin);
}
```

<!--
  FVector_NetQuantize: 위치 양자화 (소수점 1자리) → 대역폭 절감
  FVector_NetQuantizeNormal: 방향 양자화 (16비트 각도)

  서버 권한:
  - 클라이언트가 보낸 Origin/Direction으로 서버에서 레이캐스트
  - 히트 판정은 서버만 수행 → 치트 방지
  - ApplyDamage → TakeDamage (Step2-3에서 구현)
-->

### 5-3. Multicast_PlayFireEffect

```cpp
void AEPCharacter::Multicast_PlayFireEffect_Implementation(FVector MuzzleLocation)
{
    // 총구 이펙트
    // UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, MuzzleLocation);

    // 사운드
    // UGameplayStatics::PlaySoundAtLocation(GetWorld(), FireSound, MuzzleLocation);
}
```

<!--
  NetMulticast, Unreliable:
  - 서버 + 모든 클라이언트에서 실행
  - Unreliable: 이펙트는 누락되어도 게임 로직에 영향 없음
  - 대역폭 절감 (Reliable은 재전송 오버헤드)
-->

### 5-4. WithValidation (선택)

```cpp
bool AEPCharacter::Server_Fire_Validate(
    FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)
{
    // 비정상 값 검증. false 반환 시 클라이언트 강제 접속 해제
    return !Origin.IsZero();
}
```

<!--
  WithValidation 사용 시:
  - _Validate 함수가 false 반환 → 해당 클라이언트 킥
  - 명백한 치트 시도(제로 벡터 발사 등) 차단용
-->

---

## 6. UEPWeaponData — DataAsset

```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPWeaponData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    float Damage = 25.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    float FireRate = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    int32 MaxAmmo = 30;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    EEPFireMode FireMode = EEPFireMode::Auto;
    // ...
};
```

<!--
  DataAsset 분리 이유:
  - 무기 스펙을 코드가 아닌 에디터에서 조정
  - 여러 무기(AR, SMG, Shotgun 등)를 같은 AEPWeapon 클래스로 다른 스펙으로 생성
  - Blueprint에서 수치 튜닝 가능
-->

---

## 7. 복제 정리

### 프로퍼티

| 데이터 | 위치 | 방식 | 조건 |
|--------|------|------|------|
| EquippedWeapon | AEPCharacter | ReplicatedUsing | COND_None |
| CurrentAmmo | AEPWeapon | ReplicatedUsing | COND_OwnerOnly |

### RPC

| RPC | 타입 | 용도 |
|-----|------|------|
| Server_Fire | Server, Reliable | 클라 → 서버 발사 요청 |
| Multicast_PlayFireEffect | NetMulticast, Unreliable | 이펙트/사운드 전파 |

---

## 8. 배운 점 / 삽질 기록

<!--
  - Owner 체인과 COND_OwnerOnly 관계
  - HandleStartingNewPlayer vs PostLogin 차이
  - OnRep은 클라이언트에서만 호출 → 서버 Attach 누락 주의
  - FVector_NetQuantize 사용 시 정밀도 트레이드오프
  - 기타 ...
-->

---

## 9. 다음 단계

<!-- Step2-3: HP/데미지 + 사망/Corpse에서 다룰 내용 미리보기 -->
