# Step 02 — Interaction (상호작용 인터페이스 + 서버 검증)

> 마스터 기획: `05_Loot_DOCS.md` (§4-4 동시 획득 경쟁, §4-5)
> 선행: `05_Loot_01_Spawner.md` — 바닥에 픽업이 있어야 한다

---

## 목표

E키로 픽업에 상호작용하고 **서버가 판정**한다. 이 단계에는 인벤토리가 없으므로 픽업은 파괴되고 로그만 남는다.

**이 컴포넌트는 이번 단계에서 가장 오래 쓰이는 자산이다.** 픽업·컨테이너(§7-1)·자판기(§7-2)·탈출 지점(로드맵 12)이 전부 같은 진입점을 쓴다.

**완료 조건**

- [ ] 조준선에 픽업을 담으면 HUD에 "줍기 — 붕대 x2" 프롬프트가 뜬다
- [ ] E → 픽업이 파괴되고 서버 로그에 획득이 찍힌다
- [ ] **사거리 밖에서 보낸 요청을 서버가 거부**한다 (치트 시뮬레이션: RPC 직접 호출)
- [ ] PIE 2인이 동시에 E → **한 명만 성공**하고 나머지는 실패 사유를 받는다
- [ ] 로컬 컨트롤러가 아닌 캐릭터에서는 트레이스 틱이 아예 안 돈다

---

## 02-1. `IEPInteractable` — 처음부터 4함수

```cpp
UINTERFACE(MinimalAPI, BlueprintType)
class UEPInteractable : public UInterface { GENERATED_BODY() };

class EMPLOYMENTPROJ_API IEPInteractable
{
    GENERATED_BODY()
public:
    virtual FText GetInteractText() const = 0;

    // 상호작용 가능 여부 + 불가 사유. 프롬프트 표시와 서버 판정 양쪽에서 쓴다
    virtual bool CanInteract(AEPCharacter* Instigator, FText& OutReason) const = 0;

    // 채널링 시간(초). 0이면 즉시
    virtual float GetInteractDuration() const { return 0.f; }

    // 서버에서만 호출된다
    virtual void OnInteract(AEPCharacter* Instigator) = 0;
};
```

**나중에 인터페이스를 넓히면 이미 구현한 모든 클래스를 건드려야 한다.** 그래서 지금 확장 지점을 다 확보한다.

| 함수 | 픽업(이번) | 컨테이너(§7-1) | 자판기(§7-2) | 탈출(로드맵 12) |
|---|---|---|---|---|
| `GetInteractText` | "줍기 — 붕대 x2" | "검색" | "1000원 투입" | "탈출" |
| `CanInteract` | `DropCooldown` / 인벤 여유 | 이미 검색됨 | 돈 부족 | 조건 미달 |
| `GetInteractDuration` | `0` | 검색 N초 | `5` | 대기 시간 |
| `OnInteract` | 인벤 삽입 후 파괴 | 내용물 UI 개방 | 돈 차감 + 배출 | 탈출 처리 |

> `GetInteractDuration() > 0`인 채널링은 **GAS 어빌리티로 구현한다.** 상호작용 컴포넌트는 채널링을 직접 만들지 않고 어빌리티를 활성화만 한다. `UEPGA_Skill_Base`의 `CastTime` + `State.Casting` 구조가 이미 있으므로 진행도 게이지(`WBP_CastGauge`)·피격 중단·스킬 잠금이 전부 재사용된다. 이번 단계는 전부 0이라 그 경로를 타지 않는다.

### `UINTERFACE`를 `BlueprintNativeEvent`로 만들지 않는 이유

픽업·컨테이너·자판기는 전부 C++ 액터이고, 상호작용 판정은 **서버 권한 로직**이다. BP에서 오버라이드할 수 있게 열면 판정을 BP로 옮기고 싶은 유혹이 생기고, 그러면 서버 검증이 BP 그래프로 흩어진다. 순수 C++ 인터페이스로 둔다.

---

## 02-2. `UEPInteractionComponent`

```cpp
UCLASS(meta = (BlueprintSpawnableComponent))
class EMPLOYMENTPROJ_API UEPInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEPInteractionComponent();

    void Input_Interact();      // 입력 바인딩에서 호출

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*) override;

    UPROPERTY(EditDefaultsOnly, Category = "Interaction")
    float InteractRange = 250.f;

    // 서버 재검증 시 허용 오차 (지연 보정)
    UPROPERTY(EditDefaultsOnly, Category = "Interaction")
    float ServerRangeTolerance = 100.f;

private:
    void UpdateFocus();

    UPROPERTY()
    TObjectPtr<AActor> FocusedActor;
};
```

### 틱은 0.1초로 낮추고, 로컬이 아니면 끈다

```cpp
UEPInteractionComponent::UEPInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;     // 매 프레임 트레이스 금지
    SetIsReplicatedByDefault(false);              // 순수 로컬 탐지 + RPC 발신
}

void UEPInteractionComponent::BeginPlay()
{
    Super::BeginPlay();

    const AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner());
    if (!Owner || !Owner->IsLocallyControlled())
        SetComponentTickEnabled(false);           // 서버·시뮬프록시는 아예 안 돈다
}
```

**프롬프트 표시는 100ms 지연이 체감되지 않는다.** 반면 매 프레임 트레이스는 그대로 낭비다. 8인 매치에서 서버가 8개 캐릭터의 트레이스를 매 프레임 도는 건 더 낭비인데, 애초에 **탐지는 로컬 클라이언트만** 한다.

> `IsLocallyControlled()`는 `BeginPlay` 시점에 `Controller`가 아직 세팅되지 않았을 수 있다. `PossessedBy`/`OnRep_Controller` 이후에 다시 평가하거나, 안전하게 `Owner->IsLocallyControlled()`를 `UpdateFocus()` 선두에서 매번 확인하는 편이 낫다.

### 탐지

```cpp
void UEPInteractionComponent::UpdateFocus()
{
    AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner());
    if (!Owner || !Owner->IsLocallyControlled()) return;

    FVector Start, Dir;
    /* 카메라 위치/전방 획득 — FirstPersonCamera */

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);

    AActor* NewFocus = nullptr;
    if (GetWorld()->LineTraceSingleByChannel(
            Hit, Start, Start + Dir * InteractRange, EP_TraceChannel_Interact, Params))
    {
        if (Hit.GetActor() && Hit.GetActor()->Implements<UEPInteractable>())
            NewFocus = Hit.GetActor();
    }

    if (NewFocus == FocusedActor) return;
    FocusedActor = NewFocus;
    /* HUD 프롬프트 갱신 (02-4) */
}
```

**전용 트레이스 채널 `EP_TraceChannel_Interact`를 만든다.** 기존 `EP_TraceChannel_Weapon`(`EPTypes.h`)과 같은 패턴이다. `ECC_Visibility`를 재사용하면 벽·소품이 전부 걸려서 픽업 앞의 잡동사니가 상호작용을 막는다.

---

## 02-3. ★ 서버 검증 — 8단계 절차

```cpp
UFUNCTION(Server, Reliable)
void Server_Interact(AActor* Target);
```

```
Server_Interact(Target)
  1. Target이 유효한가 (IsValid && !IsActorBeingDestroyed)
  2. Target이 IEPInteractable을 구현하는가        ← 아무 액터나 보낼 수 있다
  3. ★ 거리 재검증
       DistSq(Character, Target) <= (InteractRange + ServerRangeTolerance)^2
  4. ★ IEPInteractable::CanInteract(Instigator, OutReason) == true 인가
       DropCooldown, 컨테이너 "이미 검색됨", 자판기 돈 부족이 전부 여기서 걸린다
  5. bClaimed == false 인가        ← 이 프레임에 다른 요청이 선점했는지
  6. bClaimed = true 로 즉시 마킹  ← 인벤토리 삽입보다 먼저
  7. OnInteract(Instigator)        ← Step 02에서는 로그 + Destroy()
  8. (Step 03) 부분 획득이면 bClaimed = false 로 되돌리고 픽업을 남긴다

  1~5 중 하나라도 실패 → Client_OnInteractFailed(사유). 조용히 return 하지 않는다
```

### 3·4단계가 왜 ★인가

**이 둘을 빠뜨리면 서버 검증이 사실상 없어진다.** §4-5가 "서버가 거리와 대상 유효성을 재검증한다", "`CanInteract()`는 서버가 다시 호출해 판정한다"고 선언해도, 이 절차에 호출이 없으면 **클라이언트가 프롬프트를 안 그릴 뿐 RPC는 그대로 통과한다.**

특히 Step 03의 `DropCooldown`(버린 직후 0.5초 재획득 금지)이 `CanInteract()`로 구현되므로, 4단계가 없으면 쿨다운이 서버에서 강제되지 않는다.

**클라이언트의 트레이스 결과는 표시용일 뿐이다.** 클라도 `CanInteract()`를 호출해 프롬프트를 회색 처리하지만, 판정은 서버가 다시 한다. 사격 경로에서 이미 확립한 원칙(클라는 "요청", 서버가 "결정")과 같다.

### 5·6단계 — 동시 획득 경쟁

두 플레이어가 같은 픽업에 동시에 E를 누르는 상황은 **반드시 발생한다.** `bClaimed`를 인벤토리 삽입보다 **먼저** 세워야 두 요청이 같은 프레임에 들어와도 하나만 통과한다.

- `bClaimed`는 **복제하지 않는다.** 서버 내부 상태이고, 결과는 액터 파괴(또는 Step 03의 `Quantity` 갱신)로 클라에 전달된다
- **Step 03에서 부분 획득이 생기면 `bClaimed`를 되돌려야 한다.** "성공→파괴 / 실패→해제" 두 갈래로만 두면 픽업이 살아남는 경로에서 `bClaimed`가 true로 굳어 **아무도 그 아이템을 다시 못 줍는다**

### 실패를 조용히 삼키지 않는다

```cpp
UFUNCTION(Client, Reliable)
void Client_OnInteractFailed(const FText& Reason);
```

늦은 요청·사거리 초과·조건 미달을 전부 회신한다. **아무 반응이 없으면 플레이어는 입력이 씹혔다고 느낀다.** 서버 로그만 남기고 끝내면 QA에서 "가끔 E가 안 먹혀요"로 올라온다.

---

## 02-4. HUD 프롬프트

`UEPHUDWidget`에 최소한만 추가한다.

```cpp
UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> InteractPrompt;

void SetInteractPrompt(const FText& Text, bool bEnabled);   // bEnabled false면 회색
```

- `BindWidgetOptional`로 둔다 — WBP에 아직 없어도 컴파일·실행이 깨지지 않는다
- 컴포넌트가 HUD를 직접 참조하지 않고 `AEPPlayerController`를 거친다. HUD 수명(리스폰 시 재생성)에 컴포넌트가 묶이지 않게 하기 위함
- `CanInteract()`가 false면 사유와 함께 회색으로 표시한다. **눌러보고 나서 실패하는 것보다 낫다**

---

## 02-5. `AEPPickup`의 인터페이스 구현 (이번 단계 버전)

```cpp
FText AEPPickup::GetInteractText() const
{
    // DefinitionSubsystem->FindData(ItemId)->DisplayName 사용
    return FText::Format(NSLOCTEXT("EP", "PickupFmt", "줍기 — {0} x{1}"),
                         DisplayName, FText::AsNumber(Quantity));
}

bool AEPPickup::CanInteract(AEPCharacter* Instigator, FText& OutReason) const
{
    if (bClaimed) { OutReason = /* "이미 획득됨" */; return false; }
    return true;                    // 인벤토리 여유 확인은 Step 03
}

void AEPPickup::OnInteract(AEPCharacter* Instigator)
{
    // ★ Step 03에서 AddItem() 호출로 대체되는 유일한 지점
    UE_LOG(LogTemp, Log, TEXT("[Pickup] %s x%d 획득 (인벤토리 미구현)"),
           *ItemId.ToString(), Quantity);
    Destroy();
}
```

> **★ Step 02와 03의 경계.** Step 02 시점에는 인벤토리가 없다. `CanInteract()`의 "인벤 여유 확인"과 `OnInteract()`의 "인벤 삽입"은 Step 03에서 채운다. §3이 내세운 "임시 코드가 안 생긴다"를 지키려면, **위 `UE_LOG` + `Destroy()` 한 덩어리가 Step 03에서 대체되는 유일한 지점**이어야 한다. 다른 곳에 임시 처리를 흩뿌리면 안 된다.

> 여유 확인 없이 파괴하므로 Step 02 단독으로는 부분 획득을 검증할 수 없다. 그건 Step 03의 완료 조건이다.

---

## 02-6. 입력 배선

기존 스킬 입력과 같은 패턴이다.

- `IA_Interact` 생성 → `Content/Characters/InputActions/`
- `AEPPlayerController`에 `InteractAction` UPROPERTY + FORCEINLINE 게터 (Dash/Heal/Shield와 동일)
- `AEPCharacter::SetupPlayerInputComponent`에서 null 가드 후 `Triggered` 바인딩 → `InteractionComponent->Input_Interact()`
- `AEPCharacter` 생성자에 `InteractionComponent = CreateDefaultSubobject<UEPInteractionComponent>(...)` 추가 (`CombatComponent`/`RewindComponent` 옆)

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | 서버에서 거리 재검증 누락 | 맵 반대편 아이템을 RPC로 획득 가능 | 3단계 |
| 2 | 서버에서 `CanInteract()` 미호출 | Step 03의 `DropCooldown`이 무력화 | 4단계 |
| 3 | `bClaimed`를 삽입 후에 세움 | 같은 프레임 2요청이 둘 다 성공 → 아이템 복사 | 6단계를 7단계보다 먼저 |
| 4 | 실패 시 조용히 return | "가끔 E가 안 먹혀요" QA 리포트 | `Client_OnInteractFailed` |
| 5 | `ECC_Visibility` 재사용 | 픽업 앞 잡동사니가 상호작용을 막음 | 전용 채널 `EP_TraceChannel_Interact` |
| 6 | 매 프레임 트레이스 | 불필요한 비용 | `TickInterval = 0.1f` |
| 7 | 시뮬프록시에서도 틱 | 8인이면 8배 낭비 | 로컬 아니면 `SetComponentTickEnabled(false)` |
| 8 | `Implements<>` 확인 누락 | 조작된 클라가 임의 액터를 보내면 크래시 | 2단계 |
| 9 | 임시 획득 처리를 여러 곳에 분산 | Step 03에서 지울 곳을 놓침 | `OnInteract()` 한 곳에만 |

---

## 이 단계에서 하지 않는 것

- 인벤토리 삽입 / 부분 획득 / `bClaimed` 되돌리기 → **Step 03**
- 채널링(`GetInteractDuration() > 0`) → 컨테이너·자판기 시점
- 상호작용 대상 하이라이트(아웃라인) → 필요해지면 그때
