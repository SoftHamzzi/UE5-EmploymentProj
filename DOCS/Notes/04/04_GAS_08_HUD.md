# GAS 08 — Overwatch형 HUD

> GAS 마이그레이션 마지막 단계. `DOCS.md` 4-1단계 스펙 기준.
> 선행 조건: 04_GAS_07 Skills 구현 완료 (ShieldOn 버그 수정 포함).
> 진행 상태는 `Status/04_GAS_08_HUD_STATUS.md`로 확인할 것 (이 문서는 예정 코드일 뿐).

---

## 1. 목표

오버워치 스타일 HUD (캐릭터 초상화 제외). **GAS Tag/Attribute 변화에 실시간 반응**하는 것이 완료 기준.

| 요소 | 위치 | 데이터 소스 |
|------|------|------------|
| 크로스헤어 | 중앙 | 기존 `UEPCrosshairWidget` **유지 — 이번 단계에서 건드리지 않음** |
| 체력바 (숫자 + 바) | 하단 좌 | `Health`/`MaxHealth` Attribute 변경 델리게이트 |
| 탄약 카운터 (현재/최대) | 하단 우 | `Ammo`/`MaxAmmo` Attribute 변경 델리게이트 |
| 장전 중 표시 | 탄약 옆 | `State.Reloading` 태그 이벤트 |
| 스킬 아이콘 3종 (쿨타임/진행중/잠금 3-상태 오버레이) | 하단 중앙-우 | `Cooldown.Skill.*` + 자기 진행 중 태그들(`ActiveTags` — 예: Heal의 `State.Healing`, Shield의 `State.Shielded`) + 공용 `State.Casting` 태그 이벤트 + GE 남은 시간 쿼리 |
| 화면 중앙 시전 게이지 (모양 교체 가능: 링/호/막대) | 화면 정중앙 | 공용 `State.Casting` 태그(스킬 무관, 채널링형 전체 공용) + GE 남은 시간 쿼리. 실제 렌더링은 `IEPGaugeVisual` 구현체를 갈아 끼워 결정 |
| 라운드 타이머 | 상단 중앙 | `AEPGameState::RemainingTime` (기존 복제 변수) |

킬 피드백은 HUD 요소가 아니라 사운드다 — 킬 성공 시 킬러 본인에게만 찰진 사운드 하나 재생 (§2 참조).

**범위 밖:** 킬 피드 UI(전체 클라이언트 브로드캐스트 안 함 — 사운드로 대체), 타 플레이어 체력바, 인벤토리 UI. 최소 구현 후 필요 시 확장.

---

## 2. 설계 결정

### C++ 베이스 + WBP 서브클래스 (BindWidget)

로직은 C++ `UUserWidget` 서브클래스, 레이아웃/스타일은 WBP 디자이너. `meta = (BindWidget)` UPROPERTY와 **같은 이름·같은 타입**의 위젯을 WBP에 배치하면 자동 연결된다. 이름이 다르면 WBP 컴파일 에러 — 오타를 에디터가 잡아준다.

### 데이터 소스는 3계층, 전부 이벤트 구동 (폴링 최소화)

| 계층 | API | 용도 |
|------|-----|------|
| Attribute | `ASC->GetGameplayAttributeValueChangeDelegate(Attr).AddUObject(...)` | Health/Ammo. 클라에서는 OnRep의 `GAMEPLAYATTRIBUTE_REPNOTIFY`가 브로드캐스트 (이미 `REPNOTIFY_Always` 설정됨) |
| Tag | `ASC->RegisterGameplayTagEvent(Tag, NewOrRemoved).AddUObject(...)` | State.Reloading 표시 토글, 쿨타임 오버레이 on/off |
| 복제 변수 | `AEPGameState::GetRemainingTime()` | 타이머. 1초 단위 갱신이라 NativeTick에서 읽어 포맷만 함 |

**남은 시간 숫자**(쿨타임 잔여/지속시간 잔여)만은 태그 이벤트로 알 수 없으므로, Cooldown/Active 상태일 때만 NativeTick에서 `GetActiveEffectsTimeRemainingAndDuration` 쿼리한다 (슬롯 3개 × 해당 상태 중에만 — 비용 무시 가능). Cooldown은 `CooldownTag`, Active는 `ActiveTags`로 쿼리 대상만 바뀐다.

> **Mixed Replication 전제**: 쿨타임 GE의 남은 시간은 GE 전체가 복제되는 **소유 클라이언트에서만** 조회 가능하다. HUD는 소유 클라 전용이므로 문제 없음. 반대로 타 플레이어 상태 UI를 만들 땐 태그/Cue만 쓸 수 있다는 것이 Mixed 모드의 제약.

### HUD 초기화 타이밍 — `InitASC` 수렴 지점에서

클라이언트에서 `OnRep_PlayerState`/`OnRep_Controller` 도착 순서는 비결정적이라, 기존 코드는 양쪽 모두 `InitASC()`를 호출해 나중에 도착한 쪽이 성공하는 구조다 (`EPCharacter.cpp:110~148, 491`). HUD 바인딩도 **같은 수렴 지점**에 얹는다 — PC의 `BeginPlay`에서 하면 ASC가 아직 null일 수 있다.

`InitASC()` 말미의 주석 처리된 `OnHealthChanged` 예시 코드(498~503행)는 이 단계로 대체되므로 삭제해도 된다.

### 킬 피드백 — 기존 Client_OnKill에 사운드만 추가

킬 피드(전체 클라이언트에 보이는 로그 UI)는 만들지 않는다. 킬 성공 시 **킬러 본인에게만** 찰진 사운드 하나를 재생하는 것으로 충분하다. `AEPPlayerController::Client_OnKill`(서버→킬러 개인, Reliable)이 이미 이 경로로 존재하므로, GameState Multicast나 HUD 위젯 추가 없이 `Client_OnKill_Implementation`에서 `HitConfirmSound`와 같은 패턴으로 `KillConfirmSound`를 재생하면 끝난다 (`EPPlayerController.cpp:39~42`).

### 스킬 슬롯 4-상태 + 중앙 시전 게이지 (오버워치 참고, 신규)

오버워치 참고 결과, 스킬 슬롯은 아래→위 순서로 레이어를 쌓는다: **중앙 면 → 주황색 면(쿨타임/진행중) → 검은 픽토그램 → 테두리 프레임 → 잠금 대각선**. 여기에 "다른 스킬이 시전 중이라 이 슬롯을 못 쓴다"는 잠금 상태까지 더해 슬롯 하나가 4가지 상태를 표현해야 한다:

| 상태 | 트리거 | 시각 |
|------|--------|------|
| Ready | 아무 태그도 없음 | 흰 테두리+흰 중앙 면, 오버레이 없음, 검은 픽토그램 |
| Cooldown | 자신의 `Cooldown.Skill.*` 태그 | 흰 베이스, 주황 오버레이가 **아래→위로 차오름**(0→1, 회복 진행도), 남은 초 표시 |
| Active (자기 진행 중) | 자신의 `ActiveTags` 중 하나라도 켜짐 — **채널링형**(예: Heal의 `State.Healing`, 효과 발동 전 시전 중) 또는 **지속형**(예: Shield의 `State.Shielded`, 효과 발동 후 유지 중) 둘 다 해당. CastTime=0이고 발동 후 지속 상태도 없는 순수 즉발 스킬(Dash)만 해당 없음 | 흰 베이스, 주황 오버레이가 **전체를 고정으로 덮음**(애니메이션 없음) |
| Locked (타 스킬 시전으로 잠김) | 공용 `State.Casting` 태그 (다른 누군가가 채널링 중이면 항상 켜져 있음) | 테두리 **불투명 빨강** + 중앙 면 **반투명 빨강** + 픽토그램 **불투명 빨강** + 우상향 대각선 `/` **불투명 빨강** (오버워치 잠금 룩) |

우선순위는 `Active > Locked > Cooldown > Ready` — 이 슬롯 자신이 진행 중이면 그게 최우선으로 보여야 하고(자기 자신은 잠기지 않음), 그 다음이 잠금이다.

**"채널링(사전)"과 "지속(사후)"은 서로 다른 GAS 매커니즘이지만 UI에선 같은 시각으로 통합**: `State.Casting`(공용 상호잠금)은 오직 `CastTime>0`인 채널링 중에만 걸리고 다른 모든 스킬을 잠근다(Heal). 반면 `State.Shielded` 같은 발동 후 지속 태그는 그냥 평범한 Duration GE의 GrantedTags일 뿐이고(`Cooldown.Skill.*`와 동일한 매커니즘), 다른 스킬을 잠그지 않는다(방벽 켜놨다고 Dash/Heal이 막히면 안 됨). 위젯 입장에선 둘 다 "내 스킬이 지금 뭔가 하고 있다"는 같은 의미라서 `ActiveTags`라는 하나의 태그 컨테이너로 묶어서 구독하고, 둘 중 뭐든 하나라도 켜지면 똑같이 오렌지로 덮는다 — 어떤 GAS 매커니즘에서 온 태그인지는 위젯이 몰라도 된다.

**GAS 레이어와 UI 레이어 분리**: "다른 스킬이 진짜로 활성화되지 못하게 막는 것"은 `04_GAS_07_Skills.md`의 `UEPGA_Skill_Base` 생성자가 모든 스킬 공통으로 추가하는 `ActivationBlockedTags`의 `State.Casting`이 담당 — 서버가 실제로 거부한다(개정: 예전엔 Dash/ShieldOn 생성자에 `State.Healing`을 하드코딩했으나, 스킬이 늘어날 때마다 서로의 태그를 알아야 하는 문제가 있어 공용 태그 하나로 대체됨). 이 문서가 다루는 건 그 상태를 **보여주는 것**뿐이다. 위젯은 GAS가 이미 관리하는 태그(`State.Casting`, 그리고 자기 자신의 `ActiveTags`)를 구독만 할 뿐, 잠금 여부를 스스로 판단하지 않는다. 모든 슬롯이 같은 태그 하나(`State.Casting`)만 `LockTags`로 구독하면 되므로, 새 스킬이 추가돼도 기존 슬롯의 `LockTags` 설정은 손댈 필요가 없다.

**이동속도 감소(채널링 중 20%)는 HUD와 무관** — `EPCharacterMovement`가 어트리뷰트를 직접 읽어 처리한다 (`04_GAS_07_Skills.md` Step 8). HUD는 이 값을 표시하지 않는다.

**슬롯은 머티리얼 없이 레이어드 이미지 + 틴트로 구현**: "아래→위로 차오름"은 UMG `ProgressBar`의 내장 `Fill Type = Bottom to Top` 옵션 + `SetPercent`만으로 충분하다. 잠금 룩(테두리 불투명 빨강 / 중앙 반투명 빨강 / 대각선)도 머티리얼이 아니라 **이미지를 역할별로 분리(테두리/중앙 면/대각선)해서 각각 다른 색으로 틴트**하면 끝이다 — 반투명은 틴트 색의 알파로, 대각선은 얇은 Image를 RenderTransform으로 45° 돌리면 아트 에셋조차 필요 없다. 머티리얼은 "UMG 기본 기능으로 못 그리는 것"에만 쓰는 것이 원칙이고, 이 프로젝트에서 그에 해당하는 건 **중앙 시전 게이지(방사형 마스크)** 하나뿐 — UMG에 radial ProgressBar가 없어서다. 상태별 색상은 전부 `EditAnywhere` 프로퍼티로 노출해 디자이너가 리컴파일 없이 튜닝한다.
**중앙 시전 게이지는 "옵션 1+2 동시 적용", 그리고 스킬 무관하게 자동 확장됨**: 슬롯의 주황 오버레이(옵션 2, 이미 구현)에 더해 화면 정중앙에 원형 게이지(옵션 1)를 추가한다. 슬롯 오버레이는 "회복까지 얼마나 진행됐나"(0→1 차오름)를 보여주고, 중앙 게이지는 기본값 기준 반대로 "남은 시전 시간"(1→0 줄어듦)을 보여준다 — 같은 정보를 다른 은유로 중복 표시하는 것은 의도적이다 (오버워치도 이렇게 함). 중앙 게이지의 방향은 비주얼 위젯의 `bInvertProgress`로 0→1 차오름으로 뒤집을 수도 있다 (취향 옵션, §4 참고). 중앙 게이지는 스킬별 고유 태그가 아니라 **공용 `State.Casting`**을 구독한다 — 상호잠금 매커니즘상 한 번에 한 스킬만 이 태그를 가질 수 있고, 채널링형 스킬은 어차피 자기 `GE_CastingClass`에 이 태그를 넣어야만 상호잠금이 동작하므로(이미 필수 조건), 나중에 메르시 부활형 같은 채널링 스킬이 추가돼도 이 위젯은 코드/설정 변경 없이 자동으로 반응한다. 반대로 Shield의 `State.Shielded`처럼 지속형(비채널링) 태그는 중앙 게이지에 절대 뜨지 않는다 — 그건 슬롯의 `ActiveTags`만의 몫이다.

**게이지가 실제로 어떤 모양(링/호/막대)으로 그려지는지는 이 태그 추적 로직과 완전히 분리돼 있다** — `IEPGaugeVisual` 인터페이스를 구현하는 위젯을 WBP에서 갈아 끼우는 것으로 결정된다. 자세한 구조는 아래 코드 참고.

---

## 3. Step 0 — Build.cs 모듈 의존성

`EmploymentProj.Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Niagara", "AnimGraphRuntime", "AIModule", "GameplayAbilities", "GameplayTags", "GameplayTasks", "UMG" });

PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
```

기존 `EPCrosshairWidget`은 전이 의존성 덕에 빌드가 통과했지만, 이번 단계부터 `UProgressBar`/`UTextBlock`/`UVerticalBox`를 직접 쓰므로 **의존성을 명시**한다 (전이 의존성에 기대는 것은 엔진 버전업 시 깨질 수 있음).

**verify:** 프로젝트 파일 재생성 후 빌드 통과.

---

## 4. Step 1 — UEPSkillSlotWidget (스킬 슬롯 1칸) + UEPCastGaugeWidget (중앙 게이지)

`UEPSkillSlotWidget`은 Ready/Cooldown/Active/Locked 4-상태를 감시하는 재사용 위젯. WBP_HUD에 3개 배치하고 인스턴스별로 `CooldownTag`/`ActiveTags`/`LockTags`를 다르게 지정한다 (표는 §9 Step 6 참고). `UEPCastGaugeWidget`은 화면 중앙에 하나만 배치하는 별도 위젯이다.

### `Public/HUD/EPSkillSlotWidget.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "EPSkillSlotWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
class UImage;

enum class EEPSkillSlotState : uint8
{
	Ready,
	Cooldown,
	Active,    // 이 슬롯 자신의 스킬이 채널링 중이거나(예: Heal) 발동 후 지속 효과가 유지 중(예: Shield)
	Locked,    // 다른 스킬의 시전 때문에 이 슬롯이 잠김
};

UCLASS()
class EMPLOYMENTPROJ_API UEPSkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// HUD 위젯이 ASC 준비 시점에 호출. 리스폰 재호출에 안전 (기존 바인딩 해제 후 재바인딩)
	void InitWithASC(UAbilitySystemComponent* InASC);
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	
	// 이 슬롯이 감시할 쿨타임 태그 (Cooldown.Skill.Dash / Heal / Shield) — 슬롯 인스턴스별 지정
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTag CooldownTag;
	
	// 이 슬롯 자신의 스킬이 "지금 뭔가 하고 있다"고 표시할 태그들 — 채널링 태그(예: Heal의 State.Healing)든
	// 발동 후 지속 효과 태그(예: Shield의 State.Shielded)든 종류 상관없이 하나라도 켜지면 오렌지로 덮인다.
	// 지속 상태 자체가 없는 순수 즉발 스킬(Dash)은 비워둠
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTagContainer ActiveTags;
	
	// 이 중 하나라도 켜져 있으면 잠김 — 모든 슬롯에 동일하게 {State.Casting} 하나만 넣으면 됨.
	// 자기 자신이 진행 중일 때도 State.Casting은 켜져 있지만 ActiveTags가 우선순위에서 이기므로 안전(RecomputeState 참고)
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTagContainer LockTags;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SlotBorder;    // 테두리 프레임 (가운데 뚫린 텍스처) — 평소 흰색, 잠금 시 불투명 빨강
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SlotCenter;    // 중앙 면 (꽉 찬 텍스처) — 평소 흰색, 잠금 시 반투명 빨강
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SkillIcon;     // 검은 픽토그램 — 평소 검정, 잠금 시 불투명 빨강
	
	// 잠금 대각선 "/" — Locked에서만 표시. Optional이라 슬래시 없는 미니멀 스타일을 원하면 WBP에서 아예 안 놓아도 됨
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockSlash;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CooldownBar;  // 주황 오버레이 — WBP에서 Fill Type = Bottom to Top으로 설정
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CooldownText;
	
	// --- 상태별 스타일 (디자이너가 WBP에서 리컴파일 없이 튜닝) ---
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor ReadyBorderColor = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor ReadyCenterColor = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor ReadyIconColor = FLinearColor::Black;
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor CooldownFillColor = FLinearColor(1.f, 0.5f, 0.f, 1.f);   // 주황 (Cooldown/Active 공용)
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor LockedBorderColor = FLinearColor(0.8f, 0.05f, 0.05f, 1.f);   // 불투명 빨강
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor LockedCenterColor = FLinearColor(0.8f, 0.05f, 0.05f, 0.45f); // 반투명 빨강 (알파로 반투명 표현)
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor LockedIconColor = FLinearColor(0.8f, 0.05f, 0.05f, 1.f);     // 불투명 빨강 (슬래시도 같은 색)
	
private:
	void OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnActiveTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnLockTagChanged(const FGameplayTag Tag, int32 NewCount);
	void RecomputeState();
	void ApplyState(EEPSkillSlotState NewState);
	
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle CooldownHandle;
	TArray<FDelegateHandle> ActiveHandles;
	TArray<FDelegateHandle> LockHandles;
	
	bool bCoolingDown = false;
	bool bActive = false;
	bool bLocked = false;
	EEPSkillSlotState CurrentState = EEPSkillSlotState::Ready;
	float LastShownRemaining = -1.f;   // 표시 지터 방지 래치 (-1 = 미설정)
};
```

### `Private/HUD/EPSkillSlotWidget.cpp`

```cpp
#include "HUD/EPSkillSlotWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UEPSkillSlotWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	// 리스폰 재호출 대비 — 기존 바인딩 전부 해제 (Cooldown/Active/Lock 3종)
	if (ASC.IsValid())
	{
		if (CooldownTag.IsValid() && CooldownHandle.IsValid())
			ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(CooldownHandle);
		
		int32 ActiveIdx = 0;
		for (const FGameplayTag& Tag : ActiveTags)
		{
			if (ActiveHandles.IsValidIndex(ActiveIdx) && ActiveHandles[ActiveIdx].IsValid())
				ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).Remove(ActiveHandles[ActiveIdx]);
			++ActiveIdx;
		}
		
		int32 LockIdx = 0;
		for (const FGameplayTag& LockTag : LockTags)
		{
			if (LockHandles.IsValidIndex(LockIdx) && LockHandles[LockIdx].IsValid())
				ASC->RegisterGameplayTagEvent(LockTag, EGameplayTagEventType::NewOrRemoved).Remove(LockHandles[LockIdx]);
			++LockIdx;
		}
	}
	ActiveHandles.Reset();
	LockHandles.Reset();
	
	ASC = InASC;
	if (!ASC.IsValid()) return;
	
	if (CooldownTag.IsValid())
		CooldownHandle = ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEPSkillSlotWidget::OnCooldownTagChanged);
	
	for (const FGameplayTag& Tag : ActiveTags)
		ActiveHandles.Add(ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEPSkillSlotWidget::OnActiveTagChanged));
	
	for (const FGameplayTag& LockTag : LockTags)
		LockHandles.Add(ASC->RegisterGameplayTagEvent(LockTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEPSkillSlotWidget::OnLockTagChanged));
	
	// 바인딩 시점 초기 상태 반영 (리스폰 직후 등)
	bCoolingDown = CooldownTag.IsValid() && ASC->HasMatchingGameplayTag(CooldownTag);
	bActive = ASC->HasAnyMatchingGameplayTags(ActiveTags);
	bLocked = ASC->HasAnyMatchingGameplayTags(LockTags);
	RecomputeState();
}

void UEPSkillSlotWidget::OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bCoolingDown = NewCount > 0;
	RecomputeState();
}

void UEPSkillSlotWidget::OnActiveTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// ActiveTags가 여러 개일 수 있으므로(향후 스킬이 채널링+지속 태그를 동시에 쓸 경우 대비)
	// 이 태그 하나의 NewCount만 보지 않고 전체를 다시 조회 — LockTags와 동일한 안전 패턴
	bActive = ASC.IsValid() && ASC->HasAnyMatchingGameplayTags(ActiveTags);
	RecomputeState();
}

void UEPSkillSlotWidget::OnLockTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bLocked = ASC.IsValid() && ASC->HasAnyMatchingGameplayTags(LockTags);
	RecomputeState();
}

void UEPSkillSlotWidget::RecomputeState()
{
	// 우선순위: 내가 진행 중(채널링/지속) > 남 때문에 잠김 > 쿨타임 > 준비됨
	if (bActive)           ApplyState(EEPSkillSlotState::Active);
	else if (bLocked)      ApplyState(EEPSkillSlotState::Locked);
	else if (bCoolingDown) ApplyState(EEPSkillSlotState::Cooldown);
	else                   ApplyState(EEPSkillSlotState::Ready);
}

void UEPSkillSlotWidget::ApplyState(EEPSkillSlotState NewState)
{
	CurrentState = NewState;
	LastShownRemaining = -1.f;   // 상태 전환 시 래치 해제 — 새 상태의 첫 쿼리값은 그대로 수용
	
	const bool bShowBar = (NewState == EEPSkillSlotState::Cooldown || NewState == EEPSkillSlotState::Active);
	if (CooldownBar)
		CooldownBar->SetVisibility(bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (CooldownText)
		CooldownText->SetVisibility(bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	
	if (bShowBar && CooldownBar)
	{
		CooldownBar->SetFillColorAndOpacity(CooldownFillColor);
		if (NewState == EEPSkillSlotState::Active)
			CooldownBar->SetPercent(1.f);   // Active 동안 100% 고정 유지 — Tick은 숫자만 갱신
	}
	
	// 잠금 룩 (오버워치): 테두리 불투명 빨강 / 중앙 면 반투명 빨강 / 픽토그램 불투명 빨강 / 대각선 표시
	const bool bLockedLook = (NewState == EEPSkillSlotState::Locked);
	if (SlotBorder) SlotBorder->SetColorAndOpacity(bLockedLook ? LockedBorderColor : ReadyBorderColor);
	if (SlotCenter) SlotCenter->SetColorAndOpacity(bLockedLook ? LockedCenterColor : ReadyCenterColor);
	if (SkillIcon)  SkillIcon->SetColorAndOpacity(bLockedLook ? LockedIconColor : ReadyIconColor);
	if (LockSlash)  // BindWidgetOptional — WBP에 없으면 그냥 슬래시 없는 스타일
	{
		LockSlash->SetColorAndOpacity(LockedIconColor);
		LockSlash->SetVisibility(bLockedLook ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UEPSkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// Cooldown(쿨타임 남은 시간)과 Active(지속시간 남은 시간) 모두 매 프레임 쿼리
	const bool bTickActive = CurrentState == EEPSkillSlotState::Active;
	const bool bTickCooldown = CurrentState == EEPSkillSlotState::Cooldown;
	if ((!bTickActive && !bTickCooldown) || !ASC.IsValid()) return;
	
	// 쿨타임/지속 GE는 GrantedTags로 해당 태그를 소유 → OwningTags 쿼리로 매칭
	// Active면 ActiveTags(State.Shielded 등), Cooldown이면 CooldownTag를 가진 GE를 찾는다
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
		bTickActive ? ActiveTags : FGameplayTagContainer(CooldownTag));
	
	// Pair.Key = 남은 시간, Pair.Value = 전체 Duration (엔진 GetCooldownTimeRemainingAndDuration과 동일 규약)
	TArray<TPair<float, float>> Results = ASC->GetActiveEffectsTimeRemainingAndDuration(Query);
	
	float Remaining = 0.f;
	float Duration = 0.f;
	for (const TPair<float, float>& Pair : Results)
	{
		if (Pair.Key > Remaining)
		{
			Remaining = Pair.Key;
			Duration = Pair.Value;
		}
	}
	
	// --- 표시 지터 래치 ---
	// LocalPredicted 스킬은 GE가 두 번 적용된다(클라 예측본 → 서버 복제본 교체).
	// 교체 순간 시작 시각이 서버 시계 추정치(±핑/2 오차)로 재계산되어 Remaining이 튀고,
	// CeilToInt가 이를 3→2→3 정수 깜빡임으로 증폭한다. 표시값은 단조 감소만 허용해 흡수.
	if (Duration <= 0.f) return;                    // 교체 틈새 프레임(쿼리 빈 결과): 이전 표시 유지
	if (LastShownRemaining >= 0.f)
		Remaining = FMath::Min(Remaining, LastShownRemaining);
	LastShownRemaining = Remaining;
	
	// Active: 바는 ApplyState가 100%로 고정해둠 — Tick은 남은 지속시간 숫자만 갱신
	// Cooldown: 바("회복 진행도" — 쓴 직후 0에서 차오름) + 숫자 모두 갱신
	if (bTickCooldown && CooldownBar)
	{
		const float Progress = Duration > 0.f ? FMath::Clamp(1.f - Remaining / Duration, 0.f, 1.f) : 0.f;
		CooldownBar->SetPercent(Progress);
	}
	if (CooldownText)
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
}

void UEPSkillSlotWidget::NativeDestruct()
{
	if (ASC.IsValid())
	{
		if (CooldownTag.IsValid() && CooldownHandle.IsValid())
			ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(CooldownHandle);
		
		int32 ActiveIdx = 0;
		for (const FGameplayTag& Tag : ActiveTags)
		{
			if (ActiveHandles.IsValidIndex(ActiveIdx) && ActiveHandles[ActiveIdx].IsValid())
				ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).Remove(ActiveHandles[ActiveIdx]);
			++ActiveIdx;
		}
		
		int32 LockIdx = 0;
		for (const FGameplayTag& LockTag : LockTags)
		{
			if (LockHandles.IsValidIndex(LockIdx) && LockHandles[LockIdx].IsValid())
				ASC->RegisterGameplayTagEvent(LockTag, EGameplayTagEventType::NewOrRemoved).Remove(LockHandles[LockIdx]);
			++LockIdx;
		}
	}
	
	Super::NativeDestruct();
}
```

> 기존 코드 대비 바뀐 것: (1) `Percent` 공식이 `Remaining/Duration`(1→0)에서 `1 - Remaining/Duration`(0→1)로 뒤집힘 — WBP에서 `CooldownBar`의 **Fill Type을 Bottom to Top으로 설정**해야 "아래→위로 차오름"이 실제로 보인다. (2) 아이콘이 `Image`(BindWidget 아님)에서 `SkillIcon`(BindWidget)으로 바뀜 — 잠금 시 빨강으로 틴트해야 하므로 C++이 접근해야 한다. 텍스처 자체는 여전히 슬롯 인스턴스별로 WBP에서 지정. (3) `CastingTag`(단일)가 `ActiveTags`(복수 컨테이너)로 바뀜 — 채널링 태그와 지속형 태그를 동시에 여러 개 감시할 수 있도록 `LockTags`와 동일한 패턴으로 통일. (4) `SlotFace`(단일 이미지)가 `SlotBorder`/`SlotCenter`로 분리되고 `LockSlash`가 추가됨 — 잠금 룩이 "전체 균일 빨강"에서 오버워치식 "테두리 불투명 + 중앙 반투명 + 대각선"으로 세분화됐고, 부위별로 다른 색/알파를 주려면 이미지가 역할별로 나뉘어야 한다. 상태 색상은 전부 `EditAnywhere` Style 카테고리로 노출 — 코드의 하드코딩 색 상수(namespace) 제거.

**verify:** 빌드 통과. (동작 확인은 Step 7 이후 PIE에서)

### 중앙 게이지 — 모양을 골라 끼우는 구조 (인터페이스 기반)

링(원형)만 하드코딩하면 나중에 바 형이나 호(arc) 형으로 바꾸고 싶을 때마다 `UEPCastGaugeWidget`의 C++을 고쳐야 한다. 그래서 "태그 구독 + 남은 시간 계산"을 하는 로직과 "그 값을 어떻게 그릴지"를 하는 비주얼을 분리한다:

- `UEPCastGaugeWidget` — 로직 전담. `State.Casting` 구독, `GetActiveEffectsTimeRemainingAndDuration` 쿼리는 그대로. 계산한 진행도(0~1)를 `IEPGaugeVisual` 인터페이스로 전달만 하고, 그걸 어떻게 그리는지는 전혀 모른다.
- `IEPGaugeVisual` — "진행도 숫자 하나 받아서 그린다"는 계약 하나뿐인 인터페이스. `BlueprintNativeEvent`라서 C++ 위젯도, **WBP 전용(그래프만으로 구현) 위젯도** 이 계약을 구현할 수 있다. 계약값은 항상 "남은 비율"(1→0)로 고정 — **화면에 1→0(줄어듦)으로 그릴지 0→1(차오름)으로 뒤집어 그릴지는 각 비주얼의 `bInvertProgress` 표시 옵션**이 결정하므로, 방향 취향도 WBP 인스턴스 설정만으로 바꾼다.
- 실제 그리는 위젯은 갈아 끼우는 대상이다. 이번 문서엔 참고용으로 두 종류를 만든다 — `UEPMaterialGaugeWidget`(머티리얼 마스크 기반, **링이든 오른쪽 호든 마스크 모양은 머티리얼 에셋이 결정** — 클래스는 하나, WBP에서 다른 머티리얼을 꽂으면 그게 곧 링/호 선택이 된다)과 `UEPBarGaugeWidget`(UMG 내장 `ProgressBar` 기반, 막대형 전용 — 머티리얼 불필요). 나중에 다른 모양이 필요하면 이 인터페이스만 구현하는 위젯을 새로 만들면 되고, `UEPCastGaugeWidget`은 한 글자도 안 고쳐도 된다.
- **"아예 안 쓰기"도 선택 가능**: WBP_HUD에 `CastGauge` 자체를 안 놓으면 중앙 게이지 기능 전체가 꺼진다 (`EPHUDWidget`이 이미 `if (CastGauge)`로 null 체크). 게이지 로직(태그 추적)은 유지하되 화면에 아무것도 안 그리고 싶다면, `WBP_CastGauge` 내부의 `GaugeVisual` 슬롯을 비워두면 된다 — `UEPCastGaugeWidget`은 `GaugeVisual`이 없으면 그냥 아무 것도 호출하지 않고 조용히 넘어간다.

### `Public/HUD/EPGaugeVisual.h` — 게이지 비주얼 인터페이스

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EPGaugeVisual.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UEPGaugeVisual : public UInterface
{
	GENERATED_BODY()
};

// 이 인터페이스를 구현하는 위젯이면 무엇이든 UEPCastGaugeWidget의 GaugeVisual 슬롯에 꽂을 수 있다.
// C++ 클래스(UEPMaterialGaugeWidget, UEPBarGaugeWidget)뿐 아니라 WBP 그래프만으로 구현한
// 위젯도 가능 — BlueprintNativeEvent라서 "이벤트 재정의" 형태로 그래프에서 구현 가능
class EMPLOYMENTPROJ_API IEPGaugeVisual
{
	GENERATED_BODY()
	
public:
	// Progress01 = 1일 때 "방금 시작"(남은 비율 가득), 0일 때 "곧 종료" — UEPCastGaugeWidget이 이 규약으로 호출.
	// 이 계약은 항상 "남은 비율" 고정 — 화면에 1→0으로 그릴지 0→1로 뒤집어 그릴지는
	// 구현체(비주얼 위젯)의 표시 옵션(bInvertProgress)이 결정한다. 로직은 방향을 모른다
	UFUNCTION(BlueprintNativeEvent, Category = "Gauge")
	void SetGaugeProgress(float Progress01);
	
	UFUNCTION(BlueprintNativeEvent, Category = "Gauge")
	void SetGaugeVisible(bool bVisible);
};
```

### `Public/HUD/EPCastGaugeWidget.h` — 화면 중앙 시전 게이지 (로직 전담)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "EPCastGaugeWidget.generated.h"

class UAbilitySystemComponent;
class UWidget;

UCLASS()
class EMPLOYMENTPROJ_API UEPCastGaugeWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitWithASC(UAbilitySystemComponent* InASC);
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	
	// 공용 State.Casting을 지정 — 스킬별 고유 태그(State.Healing 등)가 아니다.
	// 상호잠금 매커니즘상 한 번에 한 스킬만 이 태그를 가질 수 있고, 채널링형 스킬은 어차피
	// 자기 GE_CastingClass에 이 태그를 넣어야 상호잠금이 동작하므로(이미 필수),
	// 이 위젯은 스킬이 몇 개든 코드/설정 변경 없이 그대로 재사용된다
	UPROPERTY(EditAnywhere, Category = "Cast")
	FGameplayTag ChannelTag;
	
	// 실제로 그리는 위젯 — IEPGaugeVisual을 구현하는 것이라면 무엇이든 WBP에서 꽂을 수 있다
	// (WBP_RingGauge, WBP_ArcGauge, WBP_BarGauge, 혹은 WBP 전용 커스텀 위젯).
	// 비워두면 태그 추적/쿼리는 그대로 하되 화면엔 아무것도 그리지 않는다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GaugeVisual;
	
private:
	void OnChannelTagChanged(const FGameplayTag Tag, int32 NewCount);
	
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle ChannelHandle;
	bool bActive = false;
	float LastShownRemaining = -1.f;   // 표시 지터 방지 래치 (-1 = 미설정)
};
```

### `Private/HUD/EPCastGaugeWidget.cpp`

```cpp
#include "HUD/EPCastGaugeWidget.h"

#include "AbilitySystemComponent.h"
#include "HUD/EPGaugeVisual.h"

void UEPCastGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (GaugeVisual && !GaugeVisual->Implements<UEPGaugeVisual>())
		UE_LOG(LogTemp, Warning, TEXT("EPCastGaugeWidget: GaugeVisual(%s)가 IEPGaugeVisual을 구현하지 않음"),
			*GaugeVisual->GetName());
	
	SetVisibility(ESlateVisibility::Collapsed);
}

void UEPCastGaugeWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	if (ASC.IsValid() && ChannelTag.IsValid() && ChannelHandle.IsValid())
		ASC->RegisterGameplayTagEvent(ChannelTag, EGameplayTagEventType::NewOrRemoved).Remove(ChannelHandle);
	
	ASC = InASC;
	if (!ASC.IsValid() || !ChannelTag.IsValid()) return;
	
	ChannelHandle = ASC->RegisterGameplayTagEvent(ChannelTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UEPCastGaugeWidget::OnChannelTagChanged);
	
	OnChannelTagChanged(ChannelTag, ASC->HasMatchingGameplayTag(ChannelTag) ? 1 : 0);
}

void UEPCastGaugeWidget::OnChannelTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bActive = NewCount > 0;
	LastShownRemaining = -1.f;   // 시전 시작/종료 시 래치 해제
	SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	
	if (GaugeVisual && GaugeVisual->Implements<UEPGaugeVisual>())
		IEPGaugeVisual::Execute_SetGaugeVisible(GaugeVisual, bActive);
}

void UEPCastGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (!bActive || !ASC.IsValid() || !GaugeVisual || !GaugeVisual->Implements<UEPGaugeVisual>()) return;
	
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(ChannelTag));
	TArray<TPair<float, float>> Results = ASC->GetActiveEffectsTimeRemainingAndDuration(Query);
	
	float Remaining = 0.f;
	float Duration = 0.f;
	for (const TPair<float, float>& Pair : Results)
		if (Pair.Key > Remaining) { Remaining = Pair.Key; Duration = Pair.Value; }
	
	// 표시 지터 래치 — 슬롯 위젯과 동일 (예측본→복제본 교체 순간 Remaining 튐 흡수)
	if (Duration <= 0.f) return;                    // 교체 틈새 프레임: 이전 표시 유지
	if (LastShownRemaining >= 0.f)
		Remaining = FMath::Min(Remaining, LastShownRemaining);
	LastShownRemaining = Remaining;
	
	// 계약값은 항상 "남은 비율"(1=시작, 0=종료) — 화면에 어느 방향으로 그릴지는
	// 비주얼 구현체의 bInvertProgress가 결정하므로 여기서는 방향을 신경 쓰지 않는다
	const float Progress = FMath::Clamp(Remaining / Duration, 0.f, 1.f);
	IEPGaugeVisual::Execute_SetGaugeProgress(GaugeVisual, Progress);
}

void UEPCastGaugeWidget::NativeDestruct()
{
	if (ASC.IsValid() && ChannelTag.IsValid() && ChannelHandle.IsValid())
		ASC->RegisterGameplayTagEvent(ChannelTag, EGameplayTagEventType::NewOrRemoved).Remove(ChannelHandle);
	
	Super::NativeDestruct();
}
```

> 기존 대비 바뀐 것: `RingImage`/`RingMID`/`ProgressParamName`이 전부 제거되고 `GaugeVisual`(범용 `UWidget*`) 하나로 대체됨. 이 위젯은 이제 "무엇을 그리는지" 전혀 모른다 — 진행도 숫자와 보이기/숨기기만 `IEPGaugeVisual`을 통해 통지한다. `BindWidget`이 아니라 `BindWidgetOptional`을 쓴 것도 의도적 — 비주얼을 아예 안 꽂아도(태그 추적만 하고 화면엔 아무것도 안 그리는 구성) WBP 컴파일 에러가 나지 않는다.

**verify:** 빌드 통과.

### `Public/HUD/EPMaterialGaugeWidget.h` — 머티리얼 마스크 비주얼 (링/호 공용)

기존 링 게이지가 하던 일 그대로 — Image + MID 스칼라 파라미터. **링이냐 호냐는 이 클래스가 아니라 WBP에서 꽂는 머티리얼 에셋이 결정**한다 (마스크 모양이 다른 머티리얼 두 개를 만들고, 그걸 각각 WBP_RingGauge/WBP_ArcGauge에 물리면 끝 — 클래스는 재사용).

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HUD/EPGaugeVisual.h"
#include "EPMaterialGaugeWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

UCLASS()
class EMPLOYMENTPROJ_API UEPMaterialGaugeWidget : public UUserWidget, public IEPGaugeVisual
{
	GENERATED_BODY()
	
public:
	virtual void SetGaugeProgress_Implementation(float Progress01) override;
	virtual void SetGaugeVisible_Implementation(bool bVisible) override;
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> GaugeImage;
	
	// GaugeImage 브러시에 지정된 머티리얼(인스턴스)의 스칼라 파라미터 이름 — 링/호 머티리얼 모두 이 이름으로 통일
	UPROPERTY(EditAnywhere, Category = "Gauge")
	FName ProgressParamName = TEXT("Progress");
	
	// false = 남은 시간 그대로 (가득 참 → 줄어듦, 1→0). true = 뒤집어서 진행도로 (비어있음 → 차오름, 0→1).
	// WBP 인스턴스별로 지정 — 같은 클래스로 두 방향 모두 커버
	UPROPERTY(EditAnywhere, Category = "Gauge")
	bool bInvertProgress = false;
	
private:
	TObjectPtr<UMaterialInstanceDynamic> GaugeMID;
};
```

### `Private/HUD/EPMaterialGaugeWidget.cpp`

```cpp
#include "HUD/EPMaterialGaugeWidget.h"

#include "Components/Image.h"

void UEPMaterialGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// GaugeImage의 브러시 리소스가 머티리얼(인스턴스)로 설정돼 있어야 MID가 생성됨 — WBP에서 미리 지정
	if (GaugeImage)
		GaugeMID = GaugeImage->GetDynamicMaterial();
}

void UEPMaterialGaugeWidget::SetGaugeProgress_Implementation(float Progress01)
{
	if (GaugeMID)
		GaugeMID->SetScalarParameterValue(ProgressParamName, bInvertProgress ? 1.f - Progress01 : Progress01);
}

void UEPMaterialGaugeWidget::SetGaugeVisible_Implementation(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
```

> **머티리얼은 아트 작업 — 이 문서는 그래프까지 만들지 않는다.** 필요한 건 UV를 중심 기준 극좌표(각도)로 변환해 `Progress`(0~1) 스칼라와 비교, 그 결과를 마스크로 써서 일부만 보이게 하는 것뿐이다 (예: `Atan2(UV - 0.5)` → 0~1 정규화 → `Progress`와 `Step`/`If` 비교 → Opacity에 곱). **링**은 각도 범위를 0~360도 전체로, **오른쪽 호**는 예를 들어 -90~90도 같은 부분 범위로 마스킹하면 된다 — 그래프 구조는 거의 동일하고 각도 범위 상수만 다르다. 머티리얼 에디터에서 `Progress` 스칼라 파라미터를 노출시키고, WBP 위젯의 `GaugeImage` 브러시에 그 머티리얼(또는 M.I.)을 지정해두면 `GetDynamicMaterial()`이 런타임에 인스턴스를 만들어준다.

**verify:** 빌드 통과.

### `Public/HUD/EPBarGaugeWidget.h` — UMG 내장 ProgressBar 비주얼 (막대형 전용)

머티리얼 없이 막대형만 원할 때. UMG `ProgressBar`의 `Fill Type`(Left to Right / Right to Left / Fill from Center 등)으로 방향을 고르면 된다 — 링/호는 이 위젯으로 불가능(§4 앞부분 참고, UMG에 방사형 Fill이 없음), 막대형 전용.

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HUD/EPGaugeVisual.h"
#include "EPBarGaugeWidget.generated.h"

class UProgressBar;

UCLASS()
class EMPLOYMENTPROJ_API UEPBarGaugeWidget : public UUserWidget, public IEPGaugeVisual
{
	GENERATED_BODY()
	
public:
	virtual void SetGaugeProgress_Implementation(float Progress01) override;
	virtual void SetGaugeVisible_Implementation(bool bVisible) override;
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> GaugeBar;
	
	// false = 남은 시간 그대로 (1→0). true = 뒤집어서 진행도로 (0→1). WBP 인스턴스별 지정
	UPROPERTY(EditAnywhere, Category = "Gauge")
	bool bInvertProgress = false;
};
```

### `Private/HUD/EPBarGaugeWidget.cpp`

```cpp
#include "HUD/EPBarGaugeWidget.h"

#include "Components/ProgressBar.h"

void UEPBarGaugeWidget::SetGaugeProgress_Implementation(float Progress01)
{
	if (GaugeBar)
		GaugeBar->SetPercent(bInvertProgress ? 1.f - Progress01 : Progress01);
}

void UEPBarGaugeWidget::SetGaugeVisible_Implementation(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
```

**verify:** 빌드 통과. (동작 확인은 Step 7 이후 PIE에서)

---

## 5. Step 2 — UEPHUDWidget (루트 HUD)

### `Public/HUD/EPHUDWidget.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EPHUDWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
class UEPSkillSlotWidget;
class UEPCastGaugeWidget;
class AEPGameState;
struct FOnAttributeChangeData;

UCLASS()
class EMPLOYMENTPROJ_API UEPHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// PC::InitHUD가 ASC 준비 시점에 호출. 리스폰 재호출에 안전
	void InitWithASC(UAbilitySystemComponent* InASC);
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	
	// --- BindWidget (WBP_HUD의 위젯 이름과 정확히 일치해야 함) ---
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AmmoText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ReloadingText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TimerText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPSkillSlotWidget> DashSlot;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPSkillSlotWidget> HealSlot;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPSkillSlotWidget> ShieldSlot;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPCastGaugeWidget> CastGauge;
	
private:
	void UnbindAll();
	
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnAmmoChanged(const FOnAttributeChangeData& Data);
	void OnReloadingTagChanged(const FGameplayTag Tag, int32 NewCount);
	
	// ASC에서 현재 값을 읽어 표시 (캐싱 없음 — 값의 진실은 항상 ASC)
	void RefreshHealth();
	void RefreshAmmo();
	
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<AEPGameState> GameState;
	
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	FDelegateHandle AmmoHandle;
	FDelegateHandle MaxAmmoHandle;
	FDelegateHandle ReloadingHandle;
};
```

### `Private/HUD/EPHUDWidget.cpp`

```cpp
#include "HUD/EPHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/EPGameState.h"
#include "GAS/EPAttributeSet.h"
#include "GAS/EPNativeGameplayTags.h"
#include "HUD/EPSkillSlotWidget.h"
#include "HUD/EPCastGaugeWidget.h"

void UEPHUDWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	UnbindAll();
	
	ASC = InASC;
	if (!ASC.IsValid()) return;
	
	// --- Attribute 델리게이트 ---
	// Max 값 변화도 같은 Refresh로 수렴 (비율/표기 모두 재계산)
	HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UEPHUDWidget::OnHealthChanged);
	MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UEPHUDWidget::OnHealthChanged);
	AmmoHandle = ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetAmmoAttribute())
		.AddUObject(this, &UEPHUDWidget::OnAmmoChanged);
	MaxAmmoHandle = ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMaxAmmoAttribute())
		.AddUObject(this, &UEPHUDWidget::OnAmmoChanged);
	
	// --- Tag 이벤트 ---
	ReloadingHandle = ASC->RegisterGameplayTagEvent(
		EmpGameplayTags::TAG_State_Reloading, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UEPHUDWidget::OnReloadingTagChanged);
	
	// --- 스킬 슬롯에 ASC 전달 ---
	if (DashSlot)   DashSlot->InitWithASC(InASC);
	if (HealSlot)   HealSlot->InitWithASC(InASC);
	if (ShieldSlot) ShieldSlot->InitWithASC(InASC);
	if (CastGauge)  CastGauge->InitWithASC(InASC);
	
	// --- 초기 상태 반영 ---
	RefreshHealth();
	RefreshAmmo();
	OnReloadingTagChanged(EmpGameplayTags::TAG_State_Reloading,
		ASC->HasMatchingGameplayTag(EmpGameplayTags::TAG_State_Reloading) ? 1 : 0);
}

void UEPHUDWidget::UnbindAll()
{
	if (!ASC.IsValid()) return;
	
	ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
	ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
	ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetAmmoAttribute()).Remove(AmmoHandle);
	ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMaxAmmoAttribute()).Remove(MaxAmmoHandle);
	ASC->RegisterGameplayTagEvent(
		EmpGameplayTags::TAG_State_Reloading, EGameplayTagEventType::NewOrRemoved).Remove(ReloadingHandle);
}

void UEPHUDWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshHealth();
}

void UEPHUDWidget::OnAmmoChanged(const FOnAttributeChangeData& Data)
{
	RefreshAmmo();
}

void UEPHUDWidget::RefreshHealth()
{
	if (!ASC.IsValid()) return;
	
	const float Health = ASC->GetNumericAttribute(UEPAttributeSet::GetHealthAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(UEPAttributeSet::GetMaxHealthAttribute());
	
	if (HealthBar)
		HealthBar->SetPercent(MaxHealth > 0.f ? Health / MaxHealth : 0.f);
	if (HealthText)
		HealthText->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(Health), FMath::RoundToInt(MaxHealth))));
}

void UEPHUDWidget::RefreshAmmo()
{
	if (!ASC.IsValid() || !AmmoText) return;
	
	const float Ammo = ASC->GetNumericAttribute(UEPAttributeSet::GetAmmoAttribute());
	const float MaxAmmo = ASC->GetNumericAttribute(UEPAttributeSet::GetMaxAmmoAttribute());
	
	AmmoText->SetText(FText::FromString(
		FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(Ammo), FMath::RoundToInt(MaxAmmo))));
}

void UEPHUDWidget::OnReloadingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (ReloadingText)
		ReloadingText->SetVisibility(NewCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UEPHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// 라운드 타이머 — 1초 단위 복제 변수를 읽어 mm:ss 포맷만 수행
	if (!GameState.IsValid())
		GameState = GetWorld()->GetGameState<AEPGameState>();
	
	if (GameState.IsValid() && TimerText)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(GameState->GetRemainingTime()));
		TimerText->SetText(FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60)));
	}
}

void UEPHUDWidget::NativeDestruct()
{
	UnbindAll();
	Super::NativeDestruct();
}
```

**verify:** 빌드 통과.

---

## 6. Step 3 — EPPlayerController 확장

`EPPlayerController.h` — `private:` HUD 섹션에 추가 + `public:`에 함수 1개:

```cpp
class UEPHUDWidget;   // 파일 상단 전방 선언에 추가

// --- private: 기존 CrosshairWidget 아래 ---
UPROPERTY(EditDefaultsOnly, Category = "HUD")
TSubclassOf<UEPHUDWidget> HUDWidgetClass;

UPROPERTY()
TObjectPtr<UEPHUDWidget> HUDWidget;

// --- public: ---
// EPCharacter::InitASC가 ASC 준비 시점에 호출 (첫 호출 시 위젯 생성, 이후 재바인딩)
void InitHUD(UAbilitySystemComponent* InASC);
```

`EPPlayerController.cpp`:

```cpp
#include "HUD/EPHUDWidget.h"

void AEPPlayerController::InitHUD(UAbilitySystemComponent* InASC)
{
	// 데디케이티드 서버의 PC는 로컬이 아니므로 여기서 걸러짐
	if (!IsLocalPlayerController() || !HUDWidgetClass || !InASC) return;
	
	if (!HUDWidget)
	{
		HUDWidget = CreateWidget<UEPHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget)
			HUDWidget->AddToViewport();
	}
	
	if (HUDWidget)
		HUDWidget->InitWithASC(InASC);
}
```

**verify:** 빌드 통과. 기존 CrosshairWidget 생성 경로(BeginPlay)는 무변경.

---

## 7. Step 4 — EPCharacter::InitASC에서 HUD 초기화

`EPCharacter.cpp` — `InitASC()` 말미:

```cpp
void AEPCharacter::InitASC()
{
	AEPPlayerState* PS = GetPlayerState<AEPPlayerState>();
	if (!PS || !ASC) return;
	
	ASC->InitAbilityActorInfo(PS, this);
	
	// HUD는 ASC 준비가 끝난 이 시점에 바인딩
	// (OnRep_PlayerState / OnRep_Controller 도착 순서가 비결정적이라 양쪽 모두 InitASC를 호출하는
	//  기존 구조를 그대로 활용 — 나중에 도착한 쪽에서 성공한다)
	if (IsLocallyControlled())
		if (AEPPlayerController* PC = GetController<AEPPlayerController>())
			PC->InitHUD(ASC);
}
```

기존 주석 처리된 `OnHealthChanged` 블록(498~503행)은 삭제.

**verify:** 빌드 통과. PIE 시작 시 HUD가 뜨고 체력 `100 / 100` 표시 (Step 7 에셋 세팅 후).

---

## 8. Step 5 — 킬 사운드

GameMode/GameState는 무변경. 기존 `AEPGameMode::OnPlayerKilled`가 이미 `KillerPC->Client_OnKill(VictimName)`을 호출하고 있으므로 (`EPGameMode.cpp:130`), 그 RPC 도착 시 사운드만 재생하면 된다 — `Client_PlayHitConfirmSound`/`HitConfirmSound`와 동일한 패턴.

### `EPPlayerController.h` — 기존 `HitConfirmSound` 아래에 추가

```cpp
UPROPERTY(EditDefaultsOnly, Category = "HUD")
TObjectPtr<USoundBase> KillConfirmSound;
```

### `EPPlayerController.cpp`

```cpp
void AEPPlayerController::Client_OnKill_Implementation(const FString& VictimName)
{
	UE_LOG(LogTemp, Log, TEXT("You Kill %s"), *VictimName);
	
	if (KillConfirmSound)
		UGameplayStatics::PlaySound2D(this, KillConfirmSound);
}
```

**verify:** 빌드 통과. PIE 2인에서 킬 발생 → 킬러 화면에서만 사운드 재생 (피해자/제3자에게는 아무 것도 표시/재생되지 않음).

---

## 9. Step 6 — 에디터 (WBP 에셋)

배치 경로: `Content/Blueprints/HUD/` (신규 폴더).

### WBP_SkillSlot (부모: `EPSkillSlotWidget`)

계층 구조 — **위젯 이름이 BindWidget 변수명과 정확히 일치해야 WBP 컴파일 통과**. 아래→위 레이어 순서 그대로:

```
[SizeBox] (64×64)
 └─ [Overlay]
     ├─ SlotCenter   (Image) ← 중앙 면 (꽉 찬 사각/라운드 텍스처). 평소 White, 잠금 시 C++이 반투명 Red로 틴트
     ├─ CooldownBar  (ProgressBar) — Fill Type = Bottom to Top (Details → Appearance),
     │                 FillColorAndOpacity는 C++이 설정하므로 디자이너 기본값은 아무거나
     ├─ SkillIcon    (Image) ← 검은 픽토그램. 슬롯별로 텍스처만 다르게 지정(Dash/Heal/Shield 아이콘),
     │                 색은 평소 Black, 잠금 시 C++이 Red로 틴트
     ├─ SlotBorder   (Image) ← 테두리 프레임 (가운데가 뚫린/투명한 텍스처). 평소 White, 잠금 시 불투명 Red.
     │                 CooldownBar보다 위에 놓아야 차오르는 주황이 테두리를 침범하지 않음
     ├─ LockSlash    (Image) ← 잠금 대각선 "/". 초기 Collapsed — C++이 Locked에서만 켬 (BindWidgetOptional,
     │                 슬래시 없는 스타일을 원하면 이 위젯 자체를 생략 가능)
     └─ CooldownText (TextBlock) — 중앙 정렬, 남은 초
```

- CooldownBar/CooldownText/SkillIcon/LockSlash 색상·Visibility 초기값은 신경 쓰지 않아도 됨 — `InitWithASC`가 상태를 강제한다.
- **아트 분리가 핵심**: 예전처럼 "면+테두리"가 한 장이면 부위별로 다른 색/알파(테두리 불투명 vs 중앙 반투명)를 줄 수 없다. 중앙 면 텍스처와 테두리 프레임 텍스처(가운데 투명)를 분리해서 준비할 것.
- **`LockSlash`는 아트 없이도 가능**: 얇은 흰색 사각 Image(예: 폭 6 × 슬롯 대각선 길이)를 놓고 Render Transform → Angle **-45°**를 주면 좌하단→우상단 `/` 방향이 된다. 색은 C++이 `LockedIconColor`로 틴트하므로 흰색 그대로 두면 됨. 나중에 전용 슬래시 텍스처로 교체해도 C++ 무변경.
- 상태별 색상(Ready/Locked 테두리·중앙·픽토그램, 쿨다운 주황)은 Details → Style 카테고리에서 인스턴스별 튜닝 가능 — 기본값이 오버워치 스펙이므로 보통 손댈 필요 없음.

### WBP_CastGauge (부모: `EPCastGaugeWidget`) — 로직 컨테이너, 비주얼은 자식으로 꽂음

화면 정중앙에 배치. 계층은 이제 "그리는 것" 없이 자식 위젯 하나를 담는 껍데기다:

```
[SizeBox]
 └─ GaugeVisual (WBP_RingGauge, 또는 WBP_ArcGauge, 또는 WBP_BarGauge 중 택1) — 이름이 반드시 `GaugeVisual`
```

- Details → Cast → `ChannelTag` = `State.Casting`(공용 — 스킬별 고유 태그 아님).
- `GaugeVisual` 자리에 무엇을 넣을지가 곧 "게이지 모양 선택"이다. 아래 세 개 중 하나를 만들어서 넣거나, `IEPGaugeVisual`을 구현하는 WBP 전용 위젯을 새로 만들어 넣어도 된다.
- **아예 게이지를 안 쓰려면**: `GaugeVisual` 자리를 비워두면 된다(`BindWidgetOptional`이라 컴파일 에러 안 남) — 태그 추적 로직만 살아있고 화면엔 아무것도 안 그려짐.

### WBP_RingGauge / WBP_ArcGauge (부모: `EPMaterialGaugeWidget`)

```
[SizeBox]
 └─ GaugeImage (Image) — Brush의 Image에 마스크 머티리얼(또는 M.I.) 지정 (§4 참고, 별도 아트 작업)
```

- 링용 머티리얼을 꽂으면 WBP_RingGauge, 호용 머티리얼을 꽂으면 WBP_ArcGauge — **클래스는 완전히 동일**, 머티리얼 에셋만 다르다.
- Details → Gauge → `ProgressParamName`이 머티리얼의 스칼라 파라미터 이름과 일치해야 함 (기본값 `Progress`).
- Details → Gauge → `bInvertProgress`: false(기본) = 가득 참에서 줄어듦(1→0, 남은 시간 은유), true = 빈 상태에서 차오름(0→1, 진행도 은유). 취향대로.

### WBP_BarGauge (부모: `EPBarGaugeWidget`)

```
[SizeBox]
 └─ GaugeBar (ProgressBar) — Fill Type을 원하는 방향으로 설정 (Left to Right 등)
```

- 머티리얼 불필요. 막대형만 가능(링/호는 이 위젯으로 표현 불가).
- `bInvertProgress`로 방향 선택 (WBP_RingGauge/ArcGauge와 동일한 옵션).

### WBP_HUD (부모: `EPHUDWidget`)

CanvasPanel 기준 배치 (오버워치 레이아웃):

| 위젯 이름 | 타입 | 위치 | 비고 |
|-----------|------|------|------|
| `HealthBar` | ProgressBar | 하단 좌 | Anchor 좌하단 |
| `HealthText` | TextBlock | HealthBar 위/안 | `100 / 100` |
| `AmmoText` | TextBlock | 하단 우 | `30 / 30`, 큰 폰트 |
| `ReloadingText` | TextBlock | AmmoText 옆 | 텍스트 "RELOADING", 초기 Collapsed |
| `DashSlot` | WBP_SkillSlot | 하단 중앙-우 | CooldownTag=`Cooldown.Skill.Dash`, ActiveTags={} (비움), LockTags={`State.Casting`} |
| `HealSlot` | WBP_SkillSlot | DashSlot 옆 | CooldownTag=`Cooldown.Skill.Heal`, ActiveTags={`State.Healing`}, LockTags={`State.Casting`} |
| `ShieldSlot` | WBP_SkillSlot | HealSlot 옆 | CooldownTag=`Cooldown.Skill.Shield`, ActiveTags={`State.Shielded`}, LockTags={`State.Casting`} |
| `CastGauge` | WBP_CastGauge | 화면 정중앙 | ChannelTag=`State.Casting`, 내부 `GaugeVisual`에 WBP_RingGauge/WBP_ArcGauge/WBP_BarGauge 중 하나 배치 |
| `TimerText` | TextBlock | 상단 중앙 | `10:00` |

> 슬롯 인스턴스의 CooldownTag/ActiveTags/LockTags는 디자이너에서 해당 슬롯 선택 → Details 패널 → Skill 카테고리에서 지정. **`LockTags`는 세 슬롯 전부 동일하게 `{State.Casting}`** — 개정 전엔 Dash/ShieldSlot만 LockTags를 채우고 HealSlot은 비워야 했지만(자기 잠금 방지), 지금은 우선순위(Active > Locked)가 그 문제를 대신 해결해주므로 모든 슬롯이 같은 값을 써도 안전하다. `ActiveTags`는 여전히 스킬마다 다름 — **Dash만 비워둔다**(진짜 순간적이라 지속 상태 자체가 없음). Shield는 즉발(CastTime=0)이지만 발동 후 5초간 `State.Shielded`가 유지되므로 **비워두면 안 된다** — 이 오버레이가 이번 재설계로 채워진 부분이다.
> 크로스헤어는 기존 WBP 그대로 (별도 위젯, 이번 단계 무변경).

### BP_EPPlayerController

- `HUD > HUDWidgetClass` = `WBP_HUD`
- `HUD > KillConfirmSound` = 킬 확인 사운드 큐

**verify:** WBP 컴파일 에러 없음 (BindWidget 이름 검증). PIE에서 HUD 표시.

---

## 10. 검증 체크리스트 (PIE 2인 멀티)

- [ ] 접속 직후: 체력 `100 / 100` + 바 100%, 탄약 표기, 타이머 감소
- [ ] 피격 → 피격자 본인 화면의 HealthBar/HealthText 즉시 갱신
- [ ] 발사 → 탄약 감소 실시간 반영 (LocalPredicted라 즉시)
- [ ] 재장전 → RELOADING 표시 → 종료 시 사라짐 + 탄약 최대치 복구
- [ ] Dash 사용 → 사용 직후 오버레이 아래→위로 차오르기 시작(회복 진행도) → 완료 시 사라짐 (지속 상태 없음, 즉시 쿨타임)
- [ ] Shield 사용 → 방벽 지속 5초간 ShieldSlot 전체가 고정 주황으로 덮임(Active, 애니메이션 없음) + 이 5초 동안 Dash/Heal은 잠기지 않고 자유롭게 사용 가능(State.Casting이 아니므로) → 5초 종료 시 자동으로 쿨타임 차오름 오버레이로 전환
- [ ] Heal 시전 시작 → HealSlot 전체가 고정 주황으로 덮임(Active, 애니메이션 없음) + 화면 중앙 게이지 등장(배치한 비주얼·`bInvertProgress` 설정대로 감소 또는 차오름) + Dash/ShieldSlot이 잠금 룩으로 전환(테두리 불투명 빨강, 중앙 면 반투명 빨강, 픽토그램 불투명 빨강, `/` 대각선 표시)
- [ ] Heal 시전 중 Dash/Shield 입력 → 활성화 자체가 안 됨(서버 CommitAbility 로그도 없음), 잠금 UI와 실제 차단이 일치
- [ ] Heal 성공 → 체력 +30 반영 (100 초과 안 함), HealSlot이 쿨타임(차오르는 주황)으로 전환, Dash/ShieldSlot 잠금 즉시 해제
- [ ] Heal 채널링 중 피격 취소 → State.Healing 즉시 해제 → HealSlot·중앙 게이지·Dash/ShieldSlot 잠금이 전부 즉시 원상복귀, 쿨타임 오버레이 안 뜸
- [ ] Heal 시전 중 이동속도 체감 20%로 감소(스프린트 시도해도 SprintSpeed×0.2), 시전 종료/취소 즉시 원래 속도로 복귀
- [ ] `LogAbilitySystem`에 `RemoveActiveGameplayEffect called without Authority` 경고가 더 이상 안 뜸
- [ ] 킬 발생 → **킬러 화면에서만** 사운드 재생 (피해자/제3자는 무반응)
- [ ] 무기 교체 → MaxAmmo 변경이 탄약 표기에 반영
- [ ] 리스폰 → HUD 정상 동작 (바인딩 중복/누락 없음 — ASC가 PlayerState 소속이라 동일 ASC 재바인딩)
- [ ] `showdebug abilitysystem`과 HUD 표기 일치

---

## 11. 함정 (미리 알아둘 것)

| 함정 | 설명 |
|------|------|
| BindWidget 이름 불일치 | WBP의 위젯 이름이 C++ 변수명과 다르면 **WBP 컴파일 에러**. 타입도 일치해야 함 (`DashSlot`은 WBP_SkillSlot 인스턴스) |
| 델리게이트 해제 누락 | ASC는 PlayerState 소속이라 위젯보다 오래 산다. `NativeDestruct`에서 핸들 Remove 필수 — 안 하면 파괴된 위젯으로 콜백이 날아감 |
| HUD 초기화를 PC BeginPlay에서 | 클라에서 그 시점엔 ASC가 null일 수 있다. 반드시 `InitASC` 수렴 지점에서 `InitHUD` 호출 |
| 리스폰 재바인딩 | `InitWithASC`는 재진입 안전해야 한다 — 항상 기존 핸들 제거 후 재바인딩 (같은 ASC여도 동일 경로) |
| Mixed 모드 착각 | 쿨타임 남은 시간(GE 쿼리)은 소유 클라 전용. 타 플레이어용 UI를 만들 땐 복제되는 태그만 사용 가능 |
| Attribute 델리게이트가 클라에서 안 옴 | OnRep의 `GAMEPLAYATTRIBUTE_REPNOTIFY`가 브로드캐스트를 담당. `REPNOTIFY_Always` 없으면 같은 값 복제 시 스킵됨 — 이 프로젝트는 이미 설정돼 있음 (`EPAttributeSet.cpp` DOREPLIFETIME_CONDITION_NOTIFY) |
| 쿨타임 오버레이 폴링 남용 | 태그 이벤트로 on/off를 토글하고, **쿨타임 중일 때만** Tick에서 남은 시간을 쿼리한다. 상시 폴링 금지 |
| `GetActiveEffectsTimeRemainingAndDuration` Pair 순서 | `Key = 남은 시간`, `Value = 전체 Duration` (엔진 `GameplayAbility.cpp:1206` 사용례 기준) |
| 쿨타임 오버레이가 아래→위로 안 차오름 | WBP에서 `CooldownBar`의 Fill Type을 Bottom to Top으로 안 바꾼 경우 — 기본값(Left to Right)이면 옆으로 채워짐. C++ Percent 공식(1 - Remaining/Duration)과 Fill Type 둘 다 맞아야 함 |
| 슬롯이 빨강으로 안 바뀜 | `LockTags`(FGameplayTagContainer)에 `State.Casting`을 안 넣은 경우 — `CooldownTag`(FGameplayTag 단일)와 헷갈리기 쉬움, 타입이 다름. 세 슬롯 전부 동일하게 `{State.Casting}`이어야 함(더 이상 스킬마다 다른 값 아님) |
| 잠금 시 테두리·중앙이 같은 색으로 균일하게 빨개짐 | 면+테두리가 한 장인 텍스처를 `SlotCenter`/`SlotBorder` 양쪽에 같이 쓴 경우 — 부위별 알파 차이(불투명 테두리 vs 반투명 중앙)를 주려면 중앙 면 텍스처와 가운데가 투명한 테두리 프레임 텍스처를 분리해야 함 |
| 대각선이 안 보임 | `LockSlash`는 `BindWidgetOptional`이라 WBP 위젯 이름이 틀려도 컴파일 에러가 안 남(조용히 null) — 이름이 정확히 `LockSlash`인지 확인. 의도적으로 슬래시 없는 스타일이면 이 상태가 정상 |
| 차오르는 주황이 테두리를 덮음 | `SlotBorder`가 `CooldownBar`보다 아래 레이어에 있는 경우 — 계층 순서상 `SlotBorder`는 `CooldownBar`/`SkillIcon` 위에 놓아야 오버워치처럼 테두리가 항상 온전하게 보임 |
| Heal 슬롯이 자기 자신을 잠금 상태로 표시 | `RecomputeState()`의 우선순위(`Active > Locked`)가 안 지켜진 경우 — `bActive`를 `bLocked`보다 먼저 체크해야 함. 정상 구현이면 `LockTags`에 `State.Casting`이 있어도 자기 진행 중엔 Active가 항상 이김 |
| Shield 사용 중인데 오렌지 오버레이가 안 뜸 | `ShieldSlot`의 `ActiveTags`가 비어있는 경우 — Dash와 똑같이 취급해서 비워두기 쉬운데, Dash는 진짜 순간적이라 지속 상태가 없지만 Shield는 발동 후 `State.Shielded`가 5초간 유지되는 지속형이라 `ActiveTags={State.Shielded}`를 반드시 채워야 함 |
| 새 스킬 추가 후 잠금이 하나도 안 걸림 | 새 GA를 `UEPGA_Skill_Base` 대신 `UGameplayAbility`를 직접 상속해서 만든 경우 — `State.Casting` 부여/차단이 전부 베이스 클래스 책임이라 직접 상속하면 이 메커니즘이 아예 없음 (`04_GAS_07_Skills.md` Step 8-4 참고) |
| 새 채널링 스킬을 추가했는데 중앙 게이지에 안 뜸 | `CastGaugeWidget`의 `ChannelTag`를 그 스킬만의 고유 태그로 착각해서 설정한 경우 — `ChannelTag`는 항상 공용 `State.Casting`이어야 자동으로 모든 채널링형 스킬을 커버함. 새 스킬의 `GE_CastingClass`에 `State.Casting`을 GrantedTags로 넣기만 하면 위젯은 그대로 재사용됨 |
| 중앙 게이지가 항상 안 보임 (머티리얼형 사용 시) | `EPMaterialGaugeWidget`의 `GaugeImage` 브러시 리소스에 머티리얼(인스턴스)을 지정 안 해서 `GetDynamicMaterial()`이 null 반환 — WBP 디자이너에서 Brush → Image에 머티리얼 자산을 먼저 넣어야 함 |
| 게이지 모양을 바꿨는데 반영이 안 됨 | `WBP_CastGauge` 안의 자식 위젯 이름이 `GaugeVisual`이 아닌 경우 — `BindWidgetOptional`이라 컴파일은 통과하지만 `UEPCastGaugeWidget`이 null로 보고 아무 것도 그리지 않는다. 반대로 의도적으로 게이지를 끄고 싶다면 이 상태(비워둠)가 정상 |
| 게이지가 안 보이는데 `GaugeVisual`은 채워져 있음 | `GaugeVisual`에 배치한 위젯이 `IEPGaugeVisual`을 구현하지 않은 경우(엉뚱한 WBP를 잘못 꽂음) — `NativeConstruct`의 `UE_LOG(Warning)` 확인. WBP 전용 구현이라면 `IEPGaugeVisual`의 두 이벤트를 그래프에서 재정의(Implement Event)했는지 확인 |
| 힐 도중 `RemoveActiveGameplayEffect called without Authority` 경고 | `EndAbility`가 authority 체크 없이 호출 — 개정판에선 `UEPGA_Skill_Base::EndAbility`가 이미 가드 처리 (`04_GAS_07_Skills.md` Step 8-4 참고, HUD 문제 아님) |

---

## 12. 향후 확장 (이번 단계에서 하지 않음)

- 킬 피드 UI (전체 클라이언트 브로드캐스트 로그) — 현재는 의도적으로 배제, 필요해지면 GameState Multicast로 재검토
- 피격 방향 인디케이터, 데미지 숫자 (GameplayCue 기반)
- 타 플레이어 머리 위 체력바 (Mixed 제약 → 태그 또는 Attribute OnRep 경유)
- 크로스헤어를 WBP_HUD로 통합

### 지속형 버프 게이지 추가 (예: 방벽 남은 시간 — 선택 확장)

`UEPCastGaugeWidget`은 이름만 Cast일 뿐 내부는 "**태그 하나를 구독해 그 GE의 남은 시간을 그린다**"는 범용 로직이라, `State.Shielded` 같은 지속형 태그를 물려도 **C++ 로직 변경 없이** 그대로 동작한다. 게이지 인스턴스를 하나 더 놓는 것으로 끝난다.

**먼저 확인할 것 — 정말 필요한가:** 방벽 잔여시간은 이미 화면에 있다. `ShieldSlot`이 `ActiveTags={State.Shielded}`로 주황 오버레이를 띄우고 `NativeTick`이 `CooldownText`에 남은 초를 찍고 있다. 게이지를 추가하면 같은 정보가 두 곳에 뜬다. 오버워치가 캐스팅 바만 중앙에 두고 버프 지속시간은 아이콘에만 두는 이유는, 화면 중앙을 "지금 안 보면 손해 보는 것"(끊길 수 있는 캐스팅) 전용으로 비워두기 위해서다 — 수동적인 버프 타이머가 섞이면 중앙을 무시하는 습관이 생긴다. 슬롯의 숫자만으로 부족하다고 판단될 때만 진행할 것.

**① `EPHUDWidget.h`** — 기존 `CastGauge` 아래:

```cpp
// BindWidget이 아니라 Optional — 나중에 빼도 WBP_HUD 컴파일이 깨지지 않는다
UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UEPCastGaugeWidget> ShieldGauge;
```

**② `EPHUDWidget.cpp::InitWithASC`** — 기존 `if (CastGauge)` 줄 옆:

```cpp
if (ShieldGauge) ShieldGauge->InitWithASC(InASC);
```

**③ 에셋** — `WBP_CastGauge`를 복제해 `WBP_ShieldGauge`를 만들고, 그 WBP의 **클래스 기본값** `ChannelTag`를 `State.Shielded`로 지정한다. WBP_HUD에 배치하면서 위젯 이름을 정확히 `ShieldGauge`로.

내부 `GaugeVisual` 자리엔 **`WBP_RingGauge`/`WBP_BarGauge` 중 아무거나** 넣으면 된다(인스턴스가 별개라 `CastGauge` 쪽과 충돌 없음). 다만 **두 게이지가 동시에 뜰 수 있으므로 서로 다른 모양을 권장** — 중앙 `CastGauge`는 링(크로스헤어를 감싸 시선 한가운데에서 읽힘), `ShieldGauge`는 막대로 두면 한눈에 "캐스팅이 아니라 버프"로 구분된다. 둘 다 링이면 방벽 유지 중 힐을 시전할 때 똑같이 생긴 링이 두 개 뜬다.

> **머티리얼은 `WBP_ShieldGauge`가 아니라 두 단계 아래에 있다.** 게이지 위젯 계층과 머티리얼 적용 지점은 다음과 같다:
>
> ```
> WBP_ShieldGauge          (부모: EPCastGaugeWidget)      ← ChannelTag = State.Shielded
>  └─ GaugeVisual = WBP_RingGauge  (부모: EPMaterialGaugeWidget)  ← ProgressParamName = "Progress"
>      └─ GaugeImage (Image)                              ← Brush > Image = M_RingGauge  ★ 머티리얼은 여기
> ```
>
> `NativeConstruct`의 `GaugeImage->GetDynamicMaterial()`이 이 브러시 머티리얼로 MID를 만들고, `SetGaugeProgress_Implementation`이 거기에 `Progress` 스칼라를 쓴다. `UEPCastGaugeWidget`은 인터페이스 함수만 호출할 뿐 링인지 막대인지, 머티리얼을 쓰는지조차 모른다 — §4의 로직/비주얼 분리가 실제로 작동하는 지점이며, 그래서 `ShieldGauge`에 무엇을 꽂든 C++은 무변경이다.

> **WBP_HUD 인스턴스에서 `ChannelTag`를 오버라이드하지 말 것.** 반드시 `WBP_ShieldGauge`의 클래스 기본값으로만 설정한다 — 인스턴스 오버라이드가 클래스 기본값을 가리는 것이 실제로 힐 시전 게이지가 안 뜨던 원인이었다(§11 해당 행 참고). 설정 지점이 두 곳이면 같은 사고가 반복된다.

**반드시 지켜야 할 제약 — 두 게이지가 동시에 뜬다:**
`GE_ShieldOn`은 `State.Shielded`만 부여하고 `State.Casting`은 주지 않으므로 **방벽은 다른 스킬을 잠그지 않는다.** 즉 방벽 5초가 도는 도중에 힐 3초 캐스팅을 시작할 수 있고, 그러면 두 게이지가 같이 켜진다. `ShieldGauge`를 화면 정중앙에 놓으면 `CastGauge`와 겹치므로 **크로스헤어 아래·체력바 근처 등 중앙을 피한 자리**에 배치할 것. `bInvertProgress`는 기본값(false, 1→0 줄어듦)이 잔여시간 은유에 맞는다.

**이 방법은 쓰지 말 것 — `ChannelTag`의 컨테이너화:**
게이지 하나에 `FGameplayTagContainer ChannelTags`로 두 태그를 다 구독시키는 접근은 실패한다. Tick 루프가 "남은 시간이 가장 긴 것" 하나만 고르므로, Shield(5초)와 Heal 캐스팅(3초)이 겹치면 **Shield가 이겨서 정작 급한 캐스팅 게이지가 가려진다.** 게이지 하나로는 동시 상태를 표현할 수 없다는 것이 근본 한계라 인스턴스를 나누는 것이 맞다. 덤으로 컨테이너화하면 아래 "핸들 저장 방식" 잠복 버그까지 그대로 딸려온다.

### 스킬 장착 시스템 대비 (슬롯에 스킬을 동적 지정 — 예정된 확장)

나중에 "Q에 Dash가 들어갈지 Heal이 들어갈지 런타임에 결정"되는 장착 시스템이 붙는다. 그때를 위한 설계 메모 — **지금 구현하지 않는다.**

**그대로 살아남는 것 (설계 검증 완료):**
- 4-상태 머신(`RecomputeState`/`ApplyState`), Tick 쿨타임 쿼리 — 태그를 데이터로 받으므로 무변경
- `LockTags = {State.Casting}` 공용 설계 — 어떤 스킬이 어느 슬롯에 가든 잠금 규칙 동일, 슬롯 교체와 무관
- 중앙 게이지 — `State.Casting`만 보므로 무변경
- `InitWithASC`의 "전부 해제 후 재바인딩" 재진입 경로 — 리스폰용으로 만든 이 경로가 그대로 "스킬 교체" 경로로 재사용됨

**바뀌는 것:**
- 슬롯 이름: `DashSlot`/`HealSlot`/`ShieldSlot` → `SkillSlot0/1/2` (입력 키에 대응하는 무명 슬롯)
- 스킬별 UI 데이터(CooldownTag/ActiveTags/아이콘)의 출처: WBP 디자이너 인스턴스 설정 → **스킬 단위 데이터 주입**. 스킬 1개의 UI 데이터를 DataAsset(예: `UEPSkillUIData`: CooldownTag, ActiveTags, Icon)으로 묶고, 장착 결정 시 `SkillSlot0->SetSkill(HealUIData, ASC)` 형태로 호출. 아이템 3-tier(Definition DataAsset)와 같은 패턴. 이 시점부터는 "스킬별 UI 데이터를 스킬 쪽에 두는" 것이 정답이 된다 — 슬롯이 동적이면 UI 데이터는 스킬에 딸려 다녀야 하기 때문 (§2의 LockTags 레이어 분리 논의 참고)

**전환 시 반드시 함께 고칠 잠복 버그 — 핸들 저장 방식:**
현재 해제 루프는 태그 컨테이너와 핸들 배열을 **인덱스로 짝지어** 순회한다 (`for (Tag : ActiveTags) ... Remove(ActiveHandles[Idx])`). 리스폰(태그 불변)에선 안전하지만, 스킬 교체로 `ActiveTags`를 먼저 바꾼 뒤 재바인딩하면 **새 태그의 델리게이트에서 옛 핸들을 제거하려다 조용히 실패** → 옛 구독이 유령으로 남는다. 전환 시 핸들을 바인딩했던 태그와 쌍으로 저장하도록 변경할 것:

```cpp
TArray<TPair<FGameplayTag, FDelegateHandle>> ActiveHandles;   // 태그+핸들 쌍

// 해제 시 현재 ActiveTags를 보지 않고 저장된 쌍만 순회 — 컨테이너가 언제 바뀌어도 안전
for (const auto& [Tag, Handle] : ActiveHandles)
    ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).Remove(Handle);
```
