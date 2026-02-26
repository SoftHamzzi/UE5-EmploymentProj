# Crosshair HUD 구현

## 1. 아키텍처

```
AEPPlayerController
    └─ CreateWidget<UEPCrosshairWidget>()
           └─ AddToViewport()
```

UI는 항상 **PlayerController**에서 생성/관리. Character가 아닌 이유는 Character는 죽으면 재생성되지만 PlayerController는 세션 내내 유지되기 때문.

---

## 2. C++ 클래스

### 2-1. UEPCrosshairWidget

```
Public/HUD/EPCrosshairWidget.h
Private/HUD/EPCrosshairWidget.cpp
```

```cpp
// EPCrosshairWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EPCrosshairWidget.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API UEPCrosshairWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 크로스헤어 확산 정도 (0 = 정밀, 1 = 최대 확산)
    // 이동/점프 상태에 따라 PlayerController가 설정
    UPROPERTY(BlueprintReadWrite, Category = "Crosshair")
    float CrosshairSpread = 0.f;
};
```

```cpp
// EPCrosshairWidget.cpp
#include "HUD/EPCrosshairWidget.h"
```

### 2-2. EPPlayerController — 위젯 생성

```cpp
// EPPlayerController.h
protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(EditDefaultsOnly, Category = "HUD")
    TSubclassOf<UEPCrosshairWidget> CrosshairWidgetClass;

    UPROPERTY()
    TObjectPtr<UEPCrosshairWidget> CrosshairWidget;
```

```cpp
// EPPlayerController.cpp
#include "HUD/EPCrosshairWidget.h"

void AEPPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 로컬 클라이언트에서만 HUD 생성
    if (IsLocalController() && CrosshairWidgetClass)
    {
        CrosshairWidget = CreateWidget<UEPCrosshairWidget>(this, CrosshairWidgetClass);
        if (CrosshairWidget)
            CrosshairWidget->AddToViewport();
    }
}
```

> **IsLocalController()**: 서버나 다른 클라이언트의 PlayerController에서는 HUD를 생성하지 않음.

---

## 3. Blueprint 에셋

### 3-1. WBP_Crosshair

에디터에서 생성: Content Browser → User Interface → Widget Blueprint
Parent Class: `UEPCrosshairWidget`

**기본 크로스헤어 구성 (Canvas Panel 기준):**

```
[Canvas Panel]
    ├─ [Image: Center Dot]       앵커: 중앙 (0.5, 0.5), 크기 4x4
    ├─ [Image: Line Top]         앵커: 중앙, Position Y: -12
    ├─ [Image: Line Bottom]      앵커: 중앙, Position Y: +12
    ├─ [Image: Line Left]        앵커: 중앙, Position X: -12
    └─ [Image: Line Right]       앵커: 중앙, Position X: +12
```

각 Image는 흰색 1x1 텍스처 또는 단색 Box 사용.

**동적 확산 적용 (선택):**
Tick 또는 Event에서 `CrosshairSpread` 값에 따라 각 라인의 Position 오프셋을 조정.

```
Line Top Position Y = -12 - (CrosshairSpread * 20)
Line Bottom Position Y = +12 + (CrosshairSpread * 20)
...
```

### 3-2. CrosshairSpread 계산

PlayerController에서 매 틱 캐릭터 상태를 읽어 Spread 값을 업데이트:

```cpp
// EPPlayerController.cpp — Tick() 또는 별도 함수
void AEPPlayerController::UpdateCrosshairSpread()
{
    if (!CrosshairWidget) return;

    AEPCharacter* Char = Cast<AEPCharacter>(GetPawn());
    if (!Char)
    {
        CrosshairWidget->CrosshairSpread = 0.f;
        return;
    }

    float Spread = 0.f;

    // 이동 중
    if (Char->GetVelocity().Size2D() > 10.f)
        Spread += 0.5f;

    // 공중
    if (Char->GetCharacterMovement()->IsFalling())
        Spread += 0.75f;

    // 조준 중 (ADS)
    if (Char->GetIsAiming())
        Spread -= 0.3f;

    CrosshairWidget->CrosshairSpread = FMath::Clamp(Spread, 0.f, 1.f);
}
```

---

## 4. 에셋 배치

```
Content/
└─ Blueprints/
    └─ UI/
        └─ WBP_Crosshair
```

BP_EPPlayerController Details:
```
HUD
└─ Crosshair Widget Class: WBP_Crosshair
```

---

## 5. 구현 순서

1. `UEPCrosshairWidget` C++ 클래스 생성
2. `EPPlayerController`에 위젯 생성 코드 추가
3. `WBP_Crosshair` Blueprint 제작 (Canvas + Image 배치)
4. `WBP_Crosshair`의 Parent Class를 `UEPCrosshairWidget`으로 설정
5. `BP_EPPlayerController`의 `CrosshairWidgetClass`에 `WBP_Crosshair` 할당
6. 테스트: PIE 실행 후 화면 중앙에 크로스헤어 확인
7. (선택) `UpdateCrosshairSpread` 연결해서 동적 확산 확인
