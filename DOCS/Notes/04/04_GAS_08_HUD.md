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
| 스킬 아이콘 3종 + 쿨타임 오버레이 | 하단 중앙-우 | `Cooldown.Skill.*` 태그 이벤트 + GE 남은 시간 쿼리 |
| 킬 피드 | 우측 상단 | `AEPGameState` Multicast RPC (신규) |
| 라운드 타이머 | 상단 중앙 | `AEPGameState::RemainingTime` (기존 복제 변수) |

**범위 밖:** 킬 피드 스타일링(아이콘/색상), 타 플레이어 체력바, 인벤토리 UI. 최소 구현 후 필요 시 확장.

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

### 킬 피드 — GameState Multicast (신규)

기존 `Client_OnKill`은 **킬러 개인 피드백**(사운드 등)이므로 유지. 킬 피드는 **모든 클라이언트**에 보여야 하므로 `AEPGameState`에 `Multicast_KillFeed`를 추가하고 `AEPGameMode::OnPlayerKilled`에서 호출한다. 킬은 저빈도 이벤트라 Reliable.

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

## 4. Step 1 — UEPSkillSlotWidget (스킬 슬롯 1칸)

쿨타임 태그 하나를 감시하는 재사용 위젯. WBP_HUD에 3개 배치하고 인스턴스별로 `CooldownTag`만 다르게 지정한다.

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
	
	// 이 슬롯이 감시할 쿨타임 태그 — WBP_HUD 디자이너에서 슬롯 인스턴스별로 지정
	// (Cooldown.Skill.Dash / Cooldown.Skill.Heal / Cooldown.Skill.Shield)
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTag CooldownTag;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CooldownBar;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CooldownText;
	
private:
	void OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);
	void SetCooldownVisible(bool bVisible);
	
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle TagChangedHandle;
	bool bCoolingDown = false;
};
```

### `Private/HUD/EPSkillSlotWidget.cpp`

```cpp
#include "HUD/EPSkillSlotWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UEPSkillSlotWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	// 리스폰 재호출 대비 — 기존 바인딩 해제
	if (ASC.IsValid() && TagChangedHandle.IsValid())
		ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(TagChangedHandle);
	
	ASC = InASC;
	if (!ASC.IsValid() || !CooldownTag.IsValid()) return;
	
	TagChangedHandle = ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UEPSkillSlotWidget::OnCooldownTagChanged);
	
	// 바인딩 시점에 이미 쿨타임 중일 수 있음 (리스폰 직후 등) — 초기 상태 반영
	OnCooldownTagChanged(CooldownTag, ASC->HasMatchingGameplayTag(CooldownTag) ? 1 : 0);
}

void UEPSkillSlotWidget::OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bCoolingDown = NewCount > 0;
	SetCooldownVisible(bCoolingDown);
}

void UEPSkillSlotWidget::SetCooldownVisible(bool bVisible)
{
	const ESlateVisibility NewVis = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (CooldownBar)  CooldownBar->SetVisibility(NewVis);
	if (CooldownText) CooldownText->SetVisibility(NewVis);
}

void UEPSkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (!bCoolingDown || !ASC.IsValid()) return;
	
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
	
	if (CooldownBar)
		CooldownBar->SetPercent(Duration > 0.f ? Remaining / Duration : 0.f);
	if (CooldownText)
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
}

void UEPSkillSlotWidget::NativeDestruct()
{
	if (ASC.IsValid() && TagChangedHandle.IsValid())
		ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(TagChangedHandle);
	
	Super::NativeDestruct();
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
class UVerticalBox;
class UEPSkillSlotWidget;
class AEPGameState;
struct FOnAttributeChangeData;

UCLASS()
class EMPLOYMENTPROJ_API UEPHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// PC::InitHUD가 ASC 준비 시점에 호출. 리스폰 재호출에 안전
	void InitWithASC(UAbilitySystemComponent* InASC);
	
	// 킬 피드 한 줄 추가 (KillFeedLifetime 후 자동 제거)
	void AddKillFeedLine(const FString& KillerName, const FString& VictimName);
	
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
	TObjectPtr<UVerticalBox> KillFeedBox;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPSkillSlotWidget> DashSlot;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPSkillSlotWidget> HealSlot;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPSkillSlotWidget> ShieldSlot;
	
	UPROPERTY(EditDefaultsOnly, Category = "KillFeed")
	float KillFeedLifetime = 5.f;
	
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
#include "Components/VerticalBox.h"
#include "Core/EPGameState.h"
#include "GAS/EPAttributeSet.h"
#include "GAS/EPNativeGameplayTags.h"
#include "HUD/EPSkillSlotWidget.h"
#include "TimerManager.h"

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

void UEPHUDWidget::AddKillFeedLine(const FString& KillerName, const FString& VictimName)
{
	if (!KillFeedBox) return;
	
	UTextBlock* Line = NewObject<UTextBlock>(this);
	Line->SetText(FText::FromString(FString::Printf(TEXT("%s  >  %s"), *KillerName, *VictimName)));
	KillFeedBox->AddChildToVerticalBox(Line);
	
	FTimerHandle Handle;
	TWeakObjectPtr<UTextBlock> WeakLine = Line;
	GetWorld()->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateWeakLambda(this, [this, WeakLine]()
		{
			if (WeakLine.IsValid() && KillFeedBox)
				KillFeedBox->RemoveChild(WeakLine.Get());
		}),
		KillFeedLifetime, false);
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

> 킬 피드는 동적 `UTextBlock`이라 기본 스타일(작은 흰 글씨)이다. 스타일이 필요해지면 entry 전용 위젯 클래스(WBP)로 교체 — 이번 단계에서는 하지 않는다.

**verify:** 빌드 통과.

---

## 6. Step 3 — EPPlayerController 확장

`EPPlayerController.h` — `private:` HUD 섹션에 추가 + `public:`에 함수 2개:

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

// GameState Multicast_KillFeed → 로컬 HUD로 전달
void OnKillFeed(const FString& KillerName, const FString& VictimName);
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

void AEPPlayerController::OnKillFeed(const FString& KillerName, const FString& VictimName)
{
	if (HUDWidget)
		HUDWidget->AddKillFeedLine(KillerName, VictimName);
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

## 8. Step 5 — 킬 피드 파이프라인

### `EPGameState.h`

```cpp
UFUNCTION(NetMulticast, Reliable)
void Multicast_KillFeed(const FString& KillerName, const FString& VictimName);
```

### `EPGameState.cpp`

```cpp
#include "Core/EPPlayerController.h"

void AEPGameState::Multicast_KillFeed_Implementation(const FString& KillerName, const FString& VictimName)
{
	// 각 클라이언트의 로컬 PC로 전달. 데디케이티드 서버에서는 HUDWidget이 null이라 no-op
	if (AEPPlayerController* PC = Cast<AEPPlayerController>(GetWorld()->GetFirstPlayerController()))
		PC->OnKillFeed(KillerName, VictimName);
}
```

### `EPGameMode.cpp` — `OnPlayerKilled` 내 `Client_OnKill` 호출 다음에 추가

```cpp
// 킬 피드 — 모든 클라이언트에 브로드캐스트
FString KillerName = TEXT("Unknown");
if (AEPPlayerState* KillerPS = KillerPC->GetPlayerState<AEPPlayerState>())
	KillerName = KillerPS->GetPlayerName();

if (AEPGameState* GS = GetGameState<AEPGameState>())
	GS->Multicast_KillFeed(KillerName, VictimName);
```

> 기존 킬 카운트 블록에서 이미 KillerPS를 얻고 있으므로, 변수 스코프를 조정해 재사용해도 된다 (중복 조회 제거는 선택).

**verify:** 빌드 통과. PIE 2인에서 킬 발생 → 양쪽 화면 우측 상단에 `Killer > Victim` 표기, 5초 후 제거.

---

## 9. Step 6 — 에디터 (WBP 에셋)

배치 경로: `Content/Blueprints/HUD/` (신규 폴더).

### WBP_SkillSlot (부모: `EPSkillSlotWidget`)

계층 구조 — **위젯 이름이 BindWidget 변수명과 정확히 일치해야 WBP 컴파일 통과**:

```
[SizeBox] (64×64)
 └─ [Overlay]
     ├─ Image        ← 스킬 아이콘 (BindWidget 아님 — 슬롯별 WBP_HUD에서 텍스처 지정)
     ├─ CooldownBar  (ProgressBar) — FillColor 반투명 검정, Percent 1→0으로 줄어듦
     └─ CooldownText (TextBlock)   — 중앙 정렬, 남은 초
```

- CooldownBar/CooldownText의 초기 Visibility는 신경 쓰지 않아도 됨 — `InitWithASC`가 초기 상태를 강제한다.

### WBP_HUD (부모: `EPHUDWidget`)

CanvasPanel 기준 배치 (오버워치 레이아웃):

| 위젯 이름 | 타입 | 위치 | 비고 |
|-----------|------|------|------|
| `HealthBar` | ProgressBar | 하단 좌 | Anchor 좌하단 |
| `HealthText` | TextBlock | HealthBar 위/안 | `100 / 100` |
| `AmmoText` | TextBlock | 하단 우 | `30 / 30`, 큰 폰트 |
| `ReloadingText` | TextBlock | AmmoText 옆 | 텍스트 "RELOADING", 초기 Collapsed |
| `DashSlot` | WBP_SkillSlot | 하단 중앙-우 | Details → Skill → CooldownTag = `Cooldown.Skill.Dash` |
| `HealSlot` | WBP_SkillSlot | DashSlot 옆 | CooldownTag = `Cooldown.Skill.Heal` |
| `ShieldSlot` | WBP_SkillSlot | HealSlot 옆 | CooldownTag = `Cooldown.Skill.Shield` |
| `TimerText` | TextBlock | 상단 중앙 | `10:00` |
| `KillFeedBox` | VerticalBox | 우측 상단 | Anchor 우상단 |

> 슬롯 인스턴스의 CooldownTag는 디자이너에서 해당 슬롯 선택 → Details 패널 → Skill 카테고리에서 지정.
> 크로스헤어는 기존 WBP 그대로 (별도 위젯, 이번 단계 무변경).

### BP_EPPlayerController

- `HUD > HUDWidgetClass` = `WBP_HUD`

**verify:** WBP 컴파일 에러 없음 (BindWidget 이름 검증). PIE에서 HUD 표시.

---

## 10. 검증 체크리스트 (PIE 2인 멀티)

- [ ] 접속 직후: 체력 `100 / 100` + 바 100%, 탄약 표기, 타이머 감소
- [ ] 피격 → 피격자 본인 화면의 HealthBar/HealthText 즉시 갱신
- [ ] 발사 → 탄약 감소 실시간 반영 (LocalPredicted라 즉시)
- [ ] 재장전 → RELOADING 표시 → 종료 시 사라짐 + 탄약 최대치 복구
- [ ] Dash/Heal/Shield 사용 → 해당 슬롯 오버레이 등장 + 초 카운트다운 → 만료 시 사라짐
- [ ] Heal 성공 → 체력 +30 반영 (100 초과 안 함). 채널링 중 피격 취소 시 쿨타임 오버레이 안 뜸
- [ ] 킬 발생 → **양쪽** 클라이언트 킬 피드 표기 → 5초 후 제거
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

---

## 12. 향후 확장 (이번 단계에서 하지 않음)

- 킬 피드 스타일링 (entry 위젯 클래스 + 무기 아이콘/헤드샷 표시)
- 피격 방향 인디케이터, 데미지 숫자 (GameplayCue 기반)
- 타 플레이어 머리 위 체력바 (Mixed 제약 → 태그 또는 Attribute OnRep 경유)
- 크로스헤어를 WBP_HUD로 통합
