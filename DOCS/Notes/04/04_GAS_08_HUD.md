# GAS 08 — Overwatch형 HUD

> GAS 마이그레이션 마지막 단계. `DOCS.md` 4-1단계 스펙 기준.
> 선행 조건: 04_GAS_07 Skills 구현 완료 (ShieldOn 버그 수정 포함).
> 진행 상태는 `04_GAS_08_HUD_STATUS.md`로 확인할 것 (이 문서는 예정 코드일 뿐).

---

## 1. 목표

오버워치 스타일 HUD (캐릭터 초상화 제외). **GAS Tag/Attribute 변화에 실시간 반응**하는 것이 완료 기준.

| 요소 | 위치 | 데이터 소스 |
|------|------|------------|
| 크로스헤어 | 중앙 | 기존 `UEPCrosshairWidget` **유지 — 이번 단계에서 건드리지 않음** |
| 체력바 (숫자 + 바) | 하단 좌 | `Health`/`MaxHealth` Attribute 변경 델리게이트 |
| 탄약 카운터 (현재/최대) | 하단 우 | `Ammo`/`MaxAmmo` Attribute 변경 델리게이트 |
| 장전 중 표시 | 탄약 옆 | `State.Reloading` 태그 이벤트 |
| 스킬 아이콘 3종 (쿨타임/시전/잠금 3-상태 오버레이) | 하단 중앙-우 | `Cooldown.Skill.*` + 자기 시전 태그(예 `State.Healing`) + 공용 `State.Casting` 태그 이벤트 + GE 남은 시간 쿼리 |
| 화면 중앙 시전 게이지 (링) | 화면 정중앙 | 채널링 태그(`State.Healing`) + GE 남은 시간 쿼리 |
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

쿨타임 **남은 시간 숫자**만은 태그 이벤트로 알 수 없으므로, 쿨타임 중일 때만 NativeTick에서 `GetActiveEffectsTimeRemainingAndDuration` 쿼리한다 (슬롯 3개 × 활성 중에만 — 비용 무시 가능).

> **Mixed Replication 전제**: 쿨타임 GE의 남은 시간은 GE 전체가 복제되는 **소유 클라이언트에서만** 조회 가능하다. HUD는 소유 클라 전용이므로 문제 없음. 반대로 타 플레이어 상태 UI를 만들 땐 태그/Cue만 쓸 수 있다는 것이 Mixed 모드의 제약.

### HUD 초기화 타이밍 — `InitASC` 수렴 지점에서

클라이언트에서 `OnRep_PlayerState`/`OnRep_Controller` 도착 순서는 비결정적이라, 기존 코드는 양쪽 모두 `InitASC()`를 호출해 나중에 도착한 쪽이 성공하는 구조다 (`EPCharacter.cpp:110~148, 491`). HUD 바인딩도 **같은 수렴 지점**에 얹는다 — PC의 `BeginPlay`에서 하면 ASC가 아직 null일 수 있다.

`InitASC()` 말미의 주석 처리된 `OnHealthChanged` 예시 코드(498~503행)는 이 단계로 대체되므로 삭제해도 된다.

### 킬 피드백 — 기존 Client_OnKill에 사운드만 추가

킬 피드(전체 클라이언트에 보이는 로그 UI)는 만들지 않는다. 킬 성공 시 **킬러 본인에게만** 찰진 사운드 하나를 재생하는 것으로 충분하다. `AEPPlayerController::Client_OnKill`(서버→킬러 개인, Reliable)이 이미 이 경로로 존재하므로, GameState Multicast나 HUD 위젯 추가 없이 `Client_OnKill_Implementation`에서 `HitConfirmSound`와 같은 패턴으로 `KillConfirmSound`를 재생하면 끝난다 (`EPPlayerController.cpp:39~42`).

### 스킬 슬롯 4-상태 + 중앙 시전 게이지 (오버워치 참고, 신규)

오버워치 참고 결과, 스킬 슬롯은 아래→위 순서로 3겹이다: **흰색 베이스면 → 주황색 면(쿨타임/시전) → 검은 픽토그램**. 여기에 "다른 스킬이 시전 중이라 이 슬롯을 못 쓴다"는 잠금 상태(빨강)까지 더해 슬롯 하나가 4가지 상태를 표현해야 한다:

| 상태 | 트리거 | 시각 |
|------|--------|------|
| Ready | 아무 태그도 없음 | 흰 베이스, 오버레이 없음, 검은 픽토그램 |
| Cooldown | 자신의 `Cooldown.Skill.*` 태그 | 흰 베이스, 주황 오버레이가 **아래→위로 차오름**(0→1, 회복 진행도), 남은 초 표시 |
| Casting (자기 시전) | 자신의 시전 태그(예: Heal의 `State.Healing`. CastTime=0인 Dash/Shield는 해당 없음) | 흰 베이스, 주황 오버레이가 **전체를 고정으로 덮음**(애니메이션 없음) |
| Locked (타 스킬 시전으로 잠김) | 공용 `State.Casting` 태그 (다른 누군가가 시전 중이면 항상 켜져 있음) | 베이스+픽토그램 모두 **불투명한 빨강** |

우선순위는 `Casting > Locked > Cooldown > Ready` — 이 슬롯 자신이 시전 중이면 그게 최우선으로 보여야 하고(자기 자신은 잠기지 않음), 그 다음이 잠금이다.

**GAS 레이어와 UI 레이어 분리**: "다른 스킬이 진짜로 활성화되지 못하게 막는 것"은 `04_GAS_07_Skills.md`의 `UEPGA_Skill_Base` 생성자가 모든 스킬 공통으로 추가하는 `ActivationBlockedTags`의 `State.Casting`이 담당 — 서버가 실제로 거부한다(개정: 예전엔 Dash/ShieldOn 생성자에 `State.Healing`을 하드코딩했으나, 스킬이 늘어날 때마다 서로의 태그를 알아야 하는 문제가 있어 공용 태그 하나로 대체됨). 이 문서가 다루는 건 그 상태를 **보여주는 것**뿐이다. 위젯은 GAS가 이미 관리하는 태그(`State.Casting`, 그리고 자기 자신의 시전 태그)를 구독만 할 뿐, 잠금 여부를 스스로 판단하지 않는다. 모든 슬롯이 같은 태그 하나(`State.Casting`)만 구독하면 되므로, 새 스킬이 추가돼도 기존 슬롯의 `LockTags` 설정은 손댈 필요가 없다.

**이동속도 감소(채널링 중 20%)는 HUD와 무관** — `EPCharacterMovement`가 어트리뷰트를 직접 읽어 처리한다 (`04_GAS_07_Skills.md` Step 8). HUD는 이 값을 표시하지 않는다.

**슬롯 오버레이(ProgressBar)만으로 구현 — 신규 머티리얼 불필요**: "아래→위로 차오름"은 UMG `ProgressBar`의 내장 `Fill Type = Bottom to Top` 옵션 + `SetPercent`만으로 충분하다. 기존 `CooldownBar`를 그대로 재사용하고, 색상만 `SetFillColorAndOpacity`로 주황/빨강 전환한다. 커스텀 마스크 머티리얼이 필요한 건 **중앙 시전 게이지(원형 링)** 하나뿐 — UMG에 방사형(radial) ProgressBar가 없어서다.

**중앙 시전 게이지는 "옵션 1+2 동시 적용"**: 슬롯의 주황 오버레이(옵션 2, 이미 구현)에 더해 화면 정중앙에 원형 게이지(옵션 1)를 추가한다. 슬롯 오버레이는 "회복까지 얼마나 진행됐나"(0→1 차오름)를 보여주고, 중앙 게이지는 반대로 "남은 시전 시간"(1→0 줄어듦)을 보여준다 — 같은 정보를 다른 은유로 중복 표시하는 것은 의도적이다 (오버워치도 이렇게 함).

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

`UEPSkillSlotWidget`은 Ready/Cooldown/Casting/Locked 4-상태를 감시하는 재사용 위젯. WBP_HUD에 3개 배치하고 인스턴스별로 `CooldownTag`/`CastingTag`/`LockTags`를 다르게 지정한다 (표는 §9 Step 6 참고). `UEPCastGaugeWidget`은 화면 중앙에 하나만 배치하는 별도 위젯이다.

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
	Casting,   // 이 슬롯 자신의 스킬이 시전/채널링 중
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
	
	// 이 슬롯 자신의 스킬이 시전/채널링 중임을 나타내는 태그. 즉발 스킬(Dash/Shield)은 비워둠 — Heal 슬롯만 State.Healing 지정
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTag CastingTag;
	
	// 이 중 하나라도 켜져 있으면 잠김 — 모든 슬롯에 동일하게 {State.Casting} 하나만 넣으면 됨.
	// 자기 자신이 시전 중일 때도 State.Casting은 켜져 있지만 CastingTag가 우선순위에서 이기므로 안전(RecomputeState 참고)
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTagContainer LockTags;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SlotFace;      // 흰색 베이스면 — 평소 흰색, 잠금 시 빨강
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SkillIcon;     // 검은 픽토그램 — 평소 검정, 잠금 시 빨강
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CooldownBar;  // 주황 오버레이 — WBP에서 Fill Type = Bottom to Top으로 설정
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CooldownText;
	
private:
	void OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnCastingTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnLockTagChanged(const FGameplayTag Tag, int32 NewCount);
	void RecomputeState();
	void ApplyState(EEPSkillSlotState NewState);
	
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle CooldownHandle;
	FDelegateHandle CastingHandle;
	TArray<FDelegateHandle> LockHandles;
	
	bool bCoolingDown = false;
	bool bCasting = false;
	bool bLocked = false;
	EEPSkillSlotState CurrentState = EEPSkillSlotState::Ready;
};
```

### `Private/HUD/EPSkillSlotWidget.cpp`

```cpp
#include "HUD/EPSkillSlotWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

namespace
{
	const FLinearColor SlotColorOrange = FLinearColor(1.f, 0.5f, 0.f, 1.f);
	const FLinearColor SlotColorRed    = FLinearColor(0.8f, 0.05f, 0.05f, 1.f);
}

void UEPSkillSlotWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	// 리스폰 재호출 대비 — 기존 바인딩 전부 해제 (Cooldown/Casting/Lock 3종)
	if (ASC.IsValid())
	{
		if (CooldownTag.IsValid() && CooldownHandle.IsValid())
			ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(CooldownHandle);
		if (CastingTag.IsValid() && CastingHandle.IsValid())
			ASC->RegisterGameplayTagEvent(CastingTag, EGameplayTagEventType::NewOrRemoved).Remove(CastingHandle);
		
		int32 Idx = 0;
		for (const FGameplayTag& LockTag : LockTags)
		{
			if (LockHandles.IsValidIndex(Idx) && LockHandles[Idx].IsValid())
				ASC->RegisterGameplayTagEvent(LockTag, EGameplayTagEventType::NewOrRemoved).Remove(LockHandles[Idx]);
			++Idx;
		}
	}
	LockHandles.Reset();
	
	ASC = InASC;
	if (!ASC.IsValid()) return;
	
	if (CooldownTag.IsValid())
		CooldownHandle = ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEPSkillSlotWidget::OnCooldownTagChanged);
	
	if (CastingTag.IsValid())
		CastingHandle = ASC->RegisterGameplayTagEvent(CastingTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEPSkillSlotWidget::OnCastingTagChanged);
	
	for (const FGameplayTag& LockTag : LockTags)
		LockHandles.Add(ASC->RegisterGameplayTagEvent(LockTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEPSkillSlotWidget::OnLockTagChanged));
	
	// 바인딩 시점 초기 상태 반영 (리스폰 직후 등)
	bCoolingDown = CooldownTag.IsValid() && ASC->HasMatchingGameplayTag(CooldownTag);
	bCasting = CastingTag.IsValid() && ASC->HasMatchingGameplayTag(CastingTag);
	bLocked = ASC->HasAnyMatchingGameplayTags(LockTags);
	RecomputeState();
}

void UEPSkillSlotWidget::OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bCoolingDown = NewCount > 0;
	RecomputeState();
}

void UEPSkillSlotWidget::OnCastingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bCasting = NewCount > 0;
	RecomputeState();
}

void UEPSkillSlotWidget::OnLockTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bLocked = ASC.IsValid() && ASC->HasAnyMatchingGameplayTags(LockTags);
	RecomputeState();
}

void UEPSkillSlotWidget::RecomputeState()
{
	// 우선순위: 내가 시전 중 > 남 때문에 잠김 > 쿨타임 > 준비됨
	if (bCasting)          ApplyState(EEPSkillSlotState::Casting);
	else if (bLocked)      ApplyState(EEPSkillSlotState::Locked);
	else if (bCoolingDown) ApplyState(EEPSkillSlotState::Cooldown);
	else                   ApplyState(EEPSkillSlotState::Ready);
}

void UEPSkillSlotWidget::ApplyState(EEPSkillSlotState NewState)
{
	CurrentState = NewState;
	
	const bool bShowBar = (NewState == EEPSkillSlotState::Cooldown || NewState == EEPSkillSlotState::Casting);
	if (CooldownBar)
		CooldownBar->SetVisibility(bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (CooldownText)
		CooldownText->SetVisibility(
			NewState == EEPSkillSlotState::Cooldown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	
	if (NewState == EEPSkillSlotState::Casting && CooldownBar)
	{
		// 시전 중엔 애니메이션 없이 100%로 고정 — Tick에서 건드리지 않음(CurrentState 체크로 스킵)
		CooldownBar->SetFillColorAndOpacity(SlotColorOrange);
		CooldownBar->SetPercent(1.f);
	}
	else if (NewState == EEPSkillSlotState::Cooldown && CooldownBar)
	{
		CooldownBar->SetFillColorAndOpacity(SlotColorOrange);
	}
	
	const bool bRed = (NewState == EEPSkillSlotState::Locked);
	if (SlotFace)  SlotFace->SetColorAndOpacity(bRed ? SlotColorRed : FLinearColor::White);
	if (SkillIcon) SkillIcon->SetColorAndOpacity(bRed ? SlotColorRed : FLinearColor::Black);
}

void UEPSkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// 남은 시간 쿼리는 Cooldown 상태에서만 필요 — Casting은 ApplyState에서 이미 100% 고정
	if (CurrentState != EEPSkillSlotState::Cooldown || !ASC.IsValid()) return;
	
	// 쿨타임 GE는 GrantedTags로 CooldownTag를 소유 → OwningTags 쿼리로 매칭
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag));
	
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
	
	// "회복 진행도"로 표시 — 방금 쓴 직후엔 0(빈 상태), 쿨타임이 끝나갈수록 아래→위로 차오름
	const float Progress = Duration > 0.f ? FMath::Clamp(1.f - Remaining / Duration, 0.f, 1.f) : 0.f;
	if (CooldownBar)
		CooldownBar->SetPercent(Progress);
	if (CooldownText)
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
}

void UEPSkillSlotWidget::NativeDestruct()
{
	if (ASC.IsValid())
	{
		if (CooldownTag.IsValid() && CooldownHandle.IsValid())
			ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(CooldownHandle);
		if (CastingTag.IsValid() && CastingHandle.IsValid())
			ASC->RegisterGameplayTagEvent(CastingTag, EGameplayTagEventType::NewOrRemoved).Remove(CastingHandle);
		
		int32 Idx = 0;
		for (const FGameplayTag& LockTag : LockTags)
		{
			if (LockHandles.IsValidIndex(Idx) && LockHandles[Idx].IsValid())
				ASC->RegisterGameplayTagEvent(LockTag, EGameplayTagEventType::NewOrRemoved).Remove(LockHandles[Idx]);
			++Idx;
		}
	}
	
	Super::NativeDestruct();
}
```

> 기존 코드 대비 바뀐 것: (1) `Percent` 공식이 `Remaining/Duration`(1→0)에서 `1 - Remaining/Duration`(0→1)로 뒤집힘 — WBP에서 `CooldownBar`의 **Fill Type을 Bottom to Top으로 설정**해야 "아래→위로 차오름"이 실제로 보인다. (2) 아이콘이 `Image`(BindWidget 아님)에서 `SkillIcon`(BindWidget)으로 바뀜 — 잠금 시 빨강으로 틴트해야 하므로 C++이 접근해야 한다. 텍스처 자체는 여전히 슬롯 인스턴스별로 WBP에서 지정.

**verify:** 빌드 통과. (동작 확인은 Step 7 이후 PIE에서)

### `Public/HUD/EPCastGaugeWidget.h` — 화면 중앙 시전 게이지

슬롯 오버레이와 별개로 화면 정중앙에 하나만 배치하는 위젯. 원형(radial) 표시는 UMG `ProgressBar`로 불가능하므로 **머티리얼 기반 Image**를 쓴다.

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "EPCastGaugeWidget.generated.h"

class UAbilitySystemComponent;
class UImage;
class UMaterialInstanceDynamic;

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
	
	// 지금은 Heal의 State.Healing 하나만 지정. 향후 다른 채널링 스킬도 링에 띄우려면
	// 슬롯처럼 여러 태그를 감시하도록 확장 필요 — 지금은 범위 밖 (Heal 하나뿐이라 과설계 방지)
	UPROPERTY(EditAnywhere, Category = "Cast")
	FGameplayTag ChannelTag;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> RingImage;
	
	// RingImage 브러시에 지정된 머티리얼(인스턴스)의 스칼라 파라미터 이름
	UPROPERTY(EditAnywhere, Category = "Cast")
	FName ProgressParamName = TEXT("Progress");
	
private:
	void OnChannelTagChanged(const FGameplayTag Tag, int32 NewCount);
	
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TObjectPtr<UMaterialInstanceDynamic> RingMID;
	FDelegateHandle ChannelHandle;
	bool bActive = false;
};
```

### `Private/HUD/EPCastGaugeWidget.cpp`

```cpp
#include "HUD/EPCastGaugeWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/Image.h"

void UEPCastGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// RingImage의 브러시 리소스가 머티리얼(인스턴스)로 설정돼 있어야 MID가 생성됨 — WBP에서 미리 지정
	if (RingImage)
		RingMID = RingImage->GetDynamicMaterial();
	
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
	SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UEPCastGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (!bActive || !ASC.IsValid() || !RingMID) return;
	
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(ChannelTag));
	TArray<TPair<float, float>> Results = ASC->GetActiveEffectsTimeRemainingAndDuration(Query);
	
	float Remaining = 0.f;
	float Duration = 0.f;
	for (const TPair<float, float>& Pair : Results)
		if (Pair.Key > Remaining) { Remaining = Pair.Key; Duration = Pair.Value; }
	
	// 링은 "남은 시간"을 그대로 표시 — 가득 찬 링에서 점점 줄어듦 (슬롯의 "회복 진행도"와는 반대 은유)
	RingMID->SetScalarParameterValue(ProgressParamName, Duration > 0.f ? FMath::Clamp(Remaining / Duration, 0.f, 1.f) : 0.f);
}

void UEPCastGaugeWidget::NativeDestruct()
{
	if (ASC.IsValid() && ChannelTag.IsValid() && ChannelHandle.IsValid())
		ASC->RegisterGameplayTagEvent(ChannelTag, EGameplayTagEventType::NewOrRemoved).Remove(ChannelHandle);
	
	Super::NativeDestruct();
}
```

> **머티리얼은 아트 작업 — 이 문서는 그래프까지 만들지 않는다.** 필요한 건 UV를 중심 기준 극좌표(각도)로 변환해 `Progress`(0~1) 스칼라와 비교, 그 결과를 마스크로 써서 링의 일부만 보이게 하는 것뿐이다 (예: `Atan2(UV - 0.5)` → 0~1 정규화 → `Progress`와 `Step`/`If` 비교 → Opacity에 곱). 머티리얼 에디터에서 `Progress` 스칼라 파라미터를 노출시키고, WBP의 `RingImage` 브러시에 그 머티리얼(또는 M.I.)을 지정해두면 `GetDynamicMaterial()`이 런타임에 인스턴스를 만들어준다.

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
     ├─ SlotFace     (Image) ← 흰색 베이스면. 평소 White, 잠금 시 C++이 Red로 틴트
     ├─ CooldownBar  (ProgressBar) — Fill Type = Bottom to Top (Details → Appearance),
     │                 FillColorAndOpacity는 C++이 주황/빨강으로 설정하므로 디자이너 기본값은 아무거나
     ├─ SkillIcon    (Image) ← 검은 픽토그램. 슬롯별로 텍스처만 다르게 지정(Dash/Heal/Shield 아이콘),
     │                 색은 평소 Black, 잠금 시 C++이 Red로 틴트
     └─ CooldownText (TextBlock) — 중앙 정렬, 남은 초
```

- CooldownBar/CooldownText/SkillIcon 색상·Visibility 초기값은 신경 쓰지 않아도 됨 — `InitWithASC`가 상태를 강제한다.
- `SlotFace`에 쓸 아트가 "흰 면 + 테두리"까지 하나의 이미지로 되어 있다면 틴트 시 테두리·면이 함께 빨강으로 바뀐다 (오버워치 참고 스펙과 일치).

### WBP_CastGauge (부모: `EPCastGaugeWidget`)

화면 정중앙에 배치할 원형 게이지. 계층:

```
[SizeBox]
 └─ RingImage (Image) — Brush의 Image에 방사형 마스크 머티리얼(또는 M.I.) 지정 (§4 참고, 별도 아트 작업)
```

- Details → Cast → `ChannelTag` = `State.Healing`, `ProgressParamName`은 머티리얼의 스칼라 파라미터 이름과 일치시킴 (기본값 `Progress`).

### WBP_HUD (부모: `EPHUDWidget`)

CanvasPanel 기준 배치 (오버워치 레이아웃):

| 위젯 이름 | 타입 | 위치 | 비고 |
|-----------|------|------|------|
| `HealthBar` | ProgressBar | 하단 좌 | Anchor 좌하단 |
| `HealthText` | TextBlock | HealthBar 위/안 | `100 / 100` |
| `AmmoText` | TextBlock | 하단 우 | `30 / 30`, 큰 폰트 |
| `ReloadingText` | TextBlock | AmmoText 옆 | 텍스트 "RELOADING", 초기 Collapsed |
| `DashSlot` | WBP_SkillSlot | 하단 중앙-우 | CooldownTag=`Cooldown.Skill.Dash`, CastingTag=(비움), LockTags={`State.Casting`} |
| `HealSlot` | WBP_SkillSlot | DashSlot 옆 | CooldownTag=`Cooldown.Skill.Heal`, CastingTag=`State.Healing`, LockTags={`State.Casting`} |
| `ShieldSlot` | WBP_SkillSlot | HealSlot 옆 | CooldownTag=`Cooldown.Skill.Shield`, CastingTag=(비움), LockTags={`State.Casting`} |
| `CastGauge` | WBP_CastGauge | 화면 정중앙 | ChannelTag=`State.Healing` |
| `TimerText` | TextBlock | 상단 중앙 | `10:00` |

> 슬롯 인스턴스의 CooldownTag/CastingTag/LockTags는 디자이너에서 해당 슬롯 선택 → Details 패널 → Skill 카테고리에서 지정. **`LockTags`는 세 슬롯 전부 동일하게 `{State.Casting}`** — 개정 전엔 Dash/ShieldSlot만 LockTags를 채우고 HealSlot은 비워야 했지만(자기 잠금 방지), 지금은 우선순위(Casting > Locked)가 그 문제를 대신 해결해주므로 모든 슬롯이 같은 값을 써도 안전하다. `CastingTag`만 여전히 스킬마다 다름(즉발 스킬은 비움).
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
- [ ] Dash/Shield 사용 → 사용 직후 오버레이 아래→위로 차오르기 시작(회복 진행도) → 완료 시 사라짐
- [ ] Heal 시전 시작 → HealSlot 전체가 고정 주황으로 덮임(애니메이션 없음) + 화면 중앙 링 게이지 등장(1→0 감소) + Dash/ShieldSlot이 빨강(테두리+면)으로 잠김 + 픽토그램도 빨강
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
| Heal 슬롯이 자기 자신을 잠금 상태로 표시 | `RecomputeState()`의 우선순위(`Casting > Locked`)가 안 지켜진 경우 — `bCasting`을 `bLocked`보다 먼저 체크해야 함. 정상 구현이면 `LockTags`에 `State.Casting`이 있어도 자기 시전 중엔 Casting이 항상 이김 |
| 새 스킬 추가 후 잠금이 하나도 안 걸림 | 새 GA를 `UEPGA_Skill_Base` 대신 `UGameplayAbility`를 직접 상속해서 만든 경우 — `State.Casting` 부여/차단이 전부 베이스 클래스 책임이라 직접 상속하면 이 메커니즘이 아예 없음 (`04_GAS_07_Skills.md` Step 8-4 참고) |
| 중앙 게이지가 항상 안 보임 | `RingImage`의 브러시 리소스에 머티리얼(인스턴스)을 지정 안 해서 `GetDynamicMaterial()`이 null 반환 — WBP 디자이너에서 Brush → Image에 머티리얼 자산을 먼저 넣어야 함 |
| 힐 도중 `RemoveActiveGameplayEffect called without Authority` 경고 | `EndAbility`가 authority 체크 없이 호출 — 개정판에선 `UEPGA_Skill_Base::EndAbility`가 이미 가드 처리 (`04_GAS_07_Skills.md` Step 8-4 참고, HUD 문제 아님) |

---

## 12. 향후 확장 (이번 단계에서 하지 않음)

- 킬 피드 UI (전체 클라이언트 브로드캐스트 로그) — 현재는 의도적으로 배제, 필요해지면 GameState Multicast로 재검토
- 피격 방향 인디케이터, 데미지 숫자 (GameplayCue 기반)
- 타 플레이어 머리 위 체력바 (Mixed 제약 → 태그 또는 Attribute OnRep 경유)
- 크로스헤어를 WBP_HUD로 통합
