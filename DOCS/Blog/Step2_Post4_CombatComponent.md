# Post 2-4 작성 가이드 — CombatComponent & 무기 시스템

> **예상 제목**: `[UE5] 추출 슈터 2-4. CombatComponent 분리와 무기 액터 설계`
> **참고 문서**: DOCS/Notes/02_CombatComponent_Refactor.md, DOCS/Notes/02_Implementation.md

---

## 개요

**이 포스팅에서 다루는 것:**
- 비대해진 ACharacter에서 전투 로직을 UEPCombatComponent로 분리하는 과정
- AEPWeapon 액터 설계 (월드 표현체 역할)
- 서버 권한 사격 흐름 전체

**왜 이렇게 구현했는가 (설계 의도):**
- Character SRP(단일 책임 원칙) 위반 해소: 이동/시점/전투가 한 클래스에 몰리면 유지보수 불가
- GAS 4단계 이관 전 경로 정리: CombatComponent를 진입점으로 단일화해두면 나중에 GA 교체만 하면 됨
- Weapon Actor는 "표현체"일 뿐, 상태 원본은 ItemInstance

---

## 구현 전 상태 (Before)

```cpp
// 기존: AEPCharacter가 전투까지 모두 담당
class AEPCharacter : public ACharacter
{
    AEPWeapon* EquippedWeapon;

    UFUNCTION(Server, Reliable)
    void Server_Fire(FVector Origin, FVector Direction);

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayFireEffect(FVector MuzzleLocation);

    virtual float TakeDamage(...) override;
    // + 이동, 입력, HP, 사망 코드가 모두 여기에...
};
```

**문제점:**
- EPCharacter.cpp가 수천 줄 → 수정 시 충돌, 디버깅 어려움
- 발사/탄착 이펙트 좌표 버그가 Character 내부에서 반복 발생
- 향후 GAS 이관 시 전체 Character를 뒤져야 함

---

## 구현 내용

### 1. ACharacter 역할 분리 원칙

포스팅에서 표로 정리:

| 역할 | 담당 |
|------|------|
| 이동/시점/점프/Sprint/ADS | AEPCharacter 유지 |
| 입력 → 전투 위임 | `Input_Fire() → CombatComponent->HandleFire()` |
| 발사/재장전/RPC/이펙트 | **UEPCombatComponent**로 이동 |
| 장착 표현체 | AEPWeapon (월드 위치/메시/소켓) |
| 아이템 런타임 상태 | UEPWeaponInstance (탄약, 내구도) |

### 2. UEPCombatComponent 핵심 구조

```cpp
// EPCombatComponent.h
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UEPCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 장착 무기 (서버 스폰, 클라에 복제)
    UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon)
    TObjectPtr<AEPWeapon> EquippedWeapon;

    void EquipWeapon(AEPWeapon* NewWeapon);
    void HandleFire();  // Input_Fire에서 호출

    UFUNCTION(Server, Reliable)
    void Server_Fire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayFireEffect(FVector MuzzleLocation);

    UFUNCTION()
    void OnRep_EquippedWeapon();
};
```

### 3. 서버 권한 사격 흐름

흐름도로 보여줄 것:

```
[클라이언트] 마우스 클릭
    → Input_Fire() (Character)
    → CombatComponent->HandleFire()
    → Server_Fire(카메라 위치, 카메라 방향) ─── RPC ───→ [서버]
                                                            Server_Fire_Implementation()
                                                            ├─ 연사속도 검증
                                                            ├─ 탄약 검증
                                                            ├─ LineTraceSingleByChannel
                                                            ├─ ApplyDamage (히트 시)
                                                            └─ Multicast_PlayFireEffect() ─→ [모든 클라이언트]
                                                                                              총구/탄착 이펙트
```

**핵심 포인트:**
- 발사 판정은 오직 서버에서만 (치팅 방지)
- 이펙트는 Unreliable Multicast (패킷 손실 허용, 중요도 낮음)
- `FVector_NetQuantize` 사용으로 네트워크 트래픽 절감

### 4. AEPWeapon 설계

```cpp
UCLASS()
class AEPWeapon : public AActor
{
    GENERATED_BODY()

public:
    AEPWeapon() { bReplicates = true; }  // 반드시 복제

    // Definition 참조 (WeaponData 역할 통합)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<UEPWeaponDefinition> WeaponDef;

    // 월드 표현체 메시
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;

    // 소유자만 보이는 탄약 (COND_OwnerOnly)
    UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo)
    int32 CurrentAmmo;
};
```

**표현체 원칙:**
- AEPWeapon은 시각/소켓(MuzzleSocket, WeaponSocket) 담당
- 상태 원본은 WeaponInstance → 나중에 GAS로 이관 시 Weapon Actor 수정 최소화

### 5. 무기 장착 — WeaponSocket + LinkAnimClassLayers

```cpp
void UEPCombatComponent::EquipWeapon(AEPWeapon* NewWeapon)
{
    EquippedWeapon = NewWeapon;
    AEPCharacter* Owner = GetOwnerCharacter();

    // 손에 부착
    NewWeapon->AttachToComponent(Owner->GetMesh(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        TEXT("WeaponSocket"));  // hand_r 본에 추가한 소켓

    // 무기별 애니메이션 레이어 교체
    if (NewWeapon->WeaponDef && NewWeapon->WeaponDef->WeaponAnimLayer)
        Owner->GetMesh()->LinkAnimClassLayers(NewWeapon->WeaponDef->WeaponAnimLayer);
}

// 클라이언트 OnRep에서도 동일하게
void UEPCombatComponent::OnRep_EquippedWeapon()
{
    // 서버에서 한 것과 동일하게 attach + LinkAnimClassLayers
}
```

> **스크린샷 위치**: 스켈레톤 에디터에서 hand_r 본에 WeaponSocket 추가하는 화면

### 6. 좌표 분리 원칙

포스팅에서 강조할 것:
```
트레이스 시작: 카메라 위치  (화면 중앙 = 플레이어가 보는 방향)
총구 이펙트:  WeaponMesh->MuzzleSocket  (실제 총구 위치)
탄착 이펙트:  Hit.ImpactPoint           (피탄 지점)
```

이 3가지를 혼동하면 이펙트가 엉뚱한 곳에 생성됨

---

## 결과

**확인 항목:**
- 사격 시 서버에서 레이캐스트, 2인 접속에서 HP 감소 확인
- 총구 이펙트가 WeaponMesh MuzzleSocket 위치에 생성
- 무기가 hand_r WeaponSocket에 부착되어 손에 들려보임
- OnRep_EquippedWeapon으로 클라이언트에도 무기 보임

**한계 및 향후 개선:**
- 현재는 Server_Fire가 Character → CombatComponent 위임 구조
- GAS 4단계: `GA_Item_PrimaryUse`가 CombatComponent를 호출하는 구조로 교체 예정
- HandleFire → Server_Fire 사이에 Lag Compensation 삽입 예정 (3단계)

---

## 참고

- `DOCS/Notes/02_CombatComponent_Refactor.md` — 분리 전략 전체
- `DOCS/Notes/02_Implementation.md` Step 3~5
- `FVector_NetQuantize`, `FVector_NetQuantizeNormal` 문서
