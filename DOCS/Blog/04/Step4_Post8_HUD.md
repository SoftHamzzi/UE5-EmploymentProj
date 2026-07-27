# Post 4-8 작성 가이드 — HUD: GAS 상태에 반응하는 UI

> **예상 제목**: `[UE5] 추출 슈터 4-8. 오버워치형 HUD: 4-상태 스킬 슬롯과 인터페이스로 분리한 게이지`
> **참고 문서**: `DOCS/Notes/04/04_GAS_08_HUD.md`, `04_GAS_08_HUD_STATUS.md`

> **시리즈 마지막 편.** GAS 마이그레이션의 마무리이자, 앞의 7편에서 만든 Attribute·Tag·GE가 화면에 나타나는 편이다.
> 말미에 시리즈 회고(제거된 것 / 유지된 것 / 남은 이슈)를 붙인다.

---

## 개요

**이 포스팅에서 다루는 것:**
- Attribute / Tag / GE 세 계층을 각각 어떻게 구독하는가
- **폴링하지 않고 이벤트로 UI를 굴리는 방법**과, 그럴 수 없는 하나의 예외
- ASC보다 짧게 사는 위젯의 델리게이트 수명 관리
- 스킬 슬롯 4-상태와 우선순위 설계
- **`IEPGaugeVisual`로 "무엇을 그리는가"와 "어떻게 그리는가"를 분리하기**

**왜 이렇게 구현했는가 (설계 의도):**
- 앞의 7편에서 만든 상태가 **전부 GAS 안에 있다.** UI가 게임 로직을 다시 계산하면 안 된다 — **구독만 한다**
- UI는 이 시리즈에서 유일하게 "읽기 전용" 계층이다. **UI가 상태를 판단하기 시작하면 GAS와 진실이 두 벌이 된다**

---

## 구현 전 상태 (Before)

크로스헤어(`UEPCrosshairWidget`)만 있었다. HP·탄약·쿨타임을 보여줄 방법이 없었다.

그리고 4-2에서 `HP` UPROPERTY를 지우면서 `OnRep_HP` → UI 갱신 경로도 함께 사라진 상태였다.

```cpp
// 4-2에서 임시로 만들어둔 자리 (주석 처리된 예시 코드)
// void AEPCharacter::OnHealthChanged(const FOnAttributeChangeData& Data) { /* UI 갱신 */ }
```

**이 편이 그 자리를 실제로 채운다.**

---

## 구현 내용

### 1. C++ 베이스 + WBP 서브클래스

```cpp
UPROPERTY(meta = (BindWidget))
TObjectPtr<UProgressBar> CooldownBar;
```

WBP에 **같은 이름·같은 타입**의 위젯을 배치하면 자동 연결된다. 이름이 다르면 **WBP 컴파일 에러** — 오타를 에디터가 잡아준다.

| | 담당 |
|---|---|
| C++ (`UEPSkillSlotWidget`) | 태그 구독, 상태 판정, 남은 시간 계산 |
| WBP (`WBP_SkillSlot`) | 레이아웃, 텍스처, 색상 튜닝 |

**색상까지 C++에 두지 않는다** — 전부 `EditAnywhere`로 노출해 디자이너가 리컴파일 없이 만진다.

```cpp
UPROPERTY(EditAnywhere, Category = "Style")
FLinearColor CooldownFillColor = FLinearColor(1.f, 0.5f, 0.f, 1.f);   // 주황
UPROPERTY(EditAnywhere, Category = "Style")
FLinearColor LockedCenterColor = FLinearColor(0.8f, 0.05f, 0.05f, 0.45f); // 반투명 빨강
```

> **`BindWidgetOptional`의 함정**: 이름이 틀려도 **컴파일이 통과하고 조용히 null**이 된다. 필수 위젯에는 쓰지 않는다. 이 프로젝트에서는 "잠금 대각선"처럼 **의도적으로 없앨 수 있는 요소**에만 썼다.

### 2. ★ 데이터 소스 3계층과 각각의 구독 방법

이 편에서 가장 실용적인 표.

| 계층 | API | 이 프로젝트 용도 |
|------|-----|------------------|
| **Attribute** | `ASC->GetGameplayAttributeValueChangeDelegate(Attr).AddUObject(...)` | Health / MaxHealth / Ammo / MaxAmmo |
| **Tag** | `ASC->RegisterGameplayTagEvent(Tag, NewOrRemoved).AddUObject(...)` | `State.Reloading` 표시, 쿨타임/잠금 on-off |
| **복제 변수** | `AEPGameState::GetRemainingTime()` | 라운드 타이머 (1초 단위라 Tick에서 읽어 포맷만) |

**Attribute 델리게이트가 클라에서 동작하는 근거**는 4-1에서 이미 깔아뒀다 — `OnRep`의 `GAMEPLAYATTRIBUTE_REPNOTIFY`가 브로드캐스트를 담당하고, `REPNOTIFY_Always` 덕분에 예측으로 값이 같아도 스킵되지 않는다. **4-1의 매크로 두 줄이 여기서 값을 한다.**

### 3. ★ 폴링할 수밖에 없는 하나 — 남은 시간

태그 이벤트는 **켜짐/꺼짐만** 알려준다. "쿨타임 3.2초 남음" 같은 숫자는 알 수 없다.

```cpp
// 쿨타임/Active 상태일 때"만" Tick에서 쿼리
if (bCoolingDown || bActive)
{
    // Key = 남은 시간, Value = 전체 Duration  (엔진 GameplayAbility.cpp:1206 사용례 기준)
    TPair<float, float> TimeRemainingAndDuration = /* GetActiveEffectsTimeRemainingAndDuration(Query) */;
    const float Percent = 1.f - (TimeRemainingAndDuration.Key / TimeRemainingAndDuration.Value);
    CooldownBar->SetPercent(Percent);
}
```

**규칙: 태그 이벤트로 on/off를 토글하고, 켜져 있는 동안에만 Tick에서 숫자를 읽는다.** 슬롯 3개 × 해당 상태일 때만이므로 비용은 무시할 수준이다. 상시 폴링은 하지 않는다.

> `GetActiveEffectsTimeRemainingAndDuration`의 Pair 순서가 헷갈린다 — **Key가 남은 시간, Value가 전체 Duration**이다. 반대로 쓰면 게이지가 이상하게 움직인다.

### 4. ★ HUD 초기화 타이밍 — `InitASC` 수렴 지점

4-4에서 겪은 "PIE 시작 직후 어빌리티 활성화 실패"와 **같은 뿌리의 문제**다.

```
클라이언트에서 OnRep_PlayerState / OnRep_Controller 도착 순서는 비결정적이다.
→ 기존 코드는 양쪽 모두 InitASC()를 호출해 "나중에 도착한 쪽이 성공"하는 구조
→ HUD 바인딩도 같은 수렴 지점에 얹는다
```

```cpp
// EPCharacter::InitASC() 말미
if (IsLocallyControlled())
    if (AEPPlayerController* PC = GetController<AEPPlayerController>())
        PC->InitHUD(ASC);
```

**PC의 `BeginPlay`에서 하면 안 된다** — 그 시점에 클라 ASC는 null일 수 있다. 4-1에서 만든 `InitASC()`가 이 타이밍을 보장하는 유일한 지점이다.

### 5. ★ 델리게이트 수명 — ASC가 위젯보다 오래 산다

이 편에서 가장 놓치기 쉬운 부분.

```
ASC       → PlayerState 소속. 매치 내내 산다 (4-1의 배치 결정)
위젯      → Character가 죽거나 HUD가 재생성되면 파괴된다

→ 위젯이 죽었는데 ASC가 콜백을 날리면 크래시
```

```cpp
void UEPSkillSlotWidget::NativeDestruct()
{
    if (ASC.IsValid())
    {
        // 등록한 핸들을 전부 되돌린다
        ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(CooldownHandle);
        for (const FDelegateHandle& H : ActiveHandles) { /* ... */ }
        for (const FDelegateHandle& H : LockHandles)   { /* ... */ }
    }
    Super::NativeDestruct();
}
```

그리고 **`InitWithASC`는 재진입 안전해야 한다:**

```cpp
void UEPSkillSlotWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
    // 항상 기존 핸들을 먼저 제거하고 재바인딩 — 같은 ASC여도 동일 경로
    // 리스폰 시 InitASC()가 다시 불리므로 이 가드가 없으면 콜백이 중복 등록된다
}
```

**리스폰이 잦은 게임에서는 "재진입 안전"이 기본 요구사항이다.** 4-7의 `MoveSpeedMultiplier` 델리게이트도 정확히 같은 패턴으로 처리했다.

> `AddUObject`로 등록하면 `this`가 GC될 때 자동 해제되긴 한다. 하지만 위젯은 GC 전에 `NativeDestruct`로 먼저 논리적으로 죽으므로, **명시적 해제가 맞다.**

### 6. 스킬 슬롯 4-상태와 우선순위

```cpp
enum class EEPSkillSlotState : uint8
{
    Ready,
    Cooldown,
    Active,    // 이 슬롯 자신의 스킬이 채널링 중이거나 지속 효과 유지 중
    Locked,    // 다른 스킬의 시전 때문에 잠김
};
```

| 상태 | 트리거 | 시각 |
|------|--------|------|
| Ready | 아무 태그 없음 | 흰 테두리 + 흰 중앙, 검은 픽토그램 |
| Cooldown | 자신의 `Cooldown.Skill.*` | 주황 오버레이가 **아래→위로 차오름**, 남은 초 표시 |
| Active | 자신의 `ActiveTags` 중 하나 | 주황 오버레이가 **전체를 고정으로 덮음** |
| Locked | 공용 `State.Casting` | 테두리 불투명 빨강 + 중앙 반투명 빨강 + 대각선 |

**우선순위: `Active > Locked > Cooldown > Ready`**

```cpp
void UEPSkillSlotWidget::RecomputeState()
{
    // ★ bActive를 bLocked보다 먼저 체크한다
    // 힐 시전 중이면 State.Casting도 켜져 있지만, 자기 자신은 "잠김"이 아니라 "진행 중"이다
}
```

**이 순서를 틀리면 힐 슬롯이 자기 자신을 잠금 상태로 표시한다.** 실제로 흔한 실수라 함정 표에도 넣는다.

### 7. ★ 태그 하나로 채널링형과 지속형을 통합한다

GAS 관점에서 `State.Healing`과 `State.Shielded`는 **완전히 다른 메커니즘**이다:

| | `State.Casting` / `State.Healing` | `State.Shielded` |
|---|---|---|
| 시점 | 효과 발동 **전** (채널링) | 효과 발동 **후** (지속) |
| 다른 스킬 잠금 | **잠근다** | 안 잠근다 (방벽 켰다고 대시가 막히면 안 됨) |
| 메커니즘 | `GE_CastingClass` GrantedTags | 평범한 Duration GE GrantedTags |

**그런데 위젯 입장에서는 둘 다 "내 스킬이 지금 뭔가 하고 있다"로 같다.** 그래서 `ActiveTags`라는 하나의 컨테이너로 묶어 구독하고, 뭐든 하나 켜지면 똑같이 주황으로 덮는다.

**위젯은 어떤 GAS 메커니즘에서 온 태그인지 몰라도 된다.** 이게 "UI는 판단하지 않고 구독만 한다" 원칙의 구체적인 모습이다.

**잠금은 반대로 전부 같은 태그 하나만 본다:**

```cpp
// 세 슬롯 전부 동일: LockTags = { State.Casting }
```

4-7에서 공용 태그로 통일해둔 덕분에, **새 스킬이 추가돼도 기존 슬롯의 설정을 손댈 필요가 없다.**

> **`LockTags`(컨테이너)와 `CooldownTag`(단일 태그)는 타입이 다르다.** 헷갈려서 엉뚱한 곳에 넣으면 슬롯이 빨갛게 안 바뀐다.

### 8. ★ `IEPGaugeVisual` — 로직과 모양의 분리

중앙 시전 게이지를 링(원형)으로 하드코딩하면, 나중에 막대나 호로 바꿀 때마다 C++을 고쳐야 한다.

```
UEPCastGaugeWidget          "State.Casting을 구독하고 남은 비율(0~1)을 계산한다"
       │  SetGaugeProgress(Progress01)
       ▼
IEPGaugeVisual              "진행도 숫자 하나를 받아 그린다" — 계약은 이것뿐
       ├── UEPMaterialGaugeWidget   머티리얼 마스크 (링/호는 머티리얼 에셋이 결정)
       ├── UEPBarGaugeWidget        UMG 내장 ProgressBar (막대형)
       └── (WBP 전용 커스텀 위젯)    BlueprintNativeEvent라 그래프만으로도 구현 가능
```

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class UEPGaugeVisual : public UInterface { GENERATED_BODY() };

class EMPLOYMENTPROJ_API IEPGaugeVisual
{
    GENERATED_BODY()
public:
    // Progress01 = 1이면 "방금 시작", 0이면 "곧 종료" — 계약은 항상 "남은 비율" 고정.
    // 화면에 1→0으로 그릴지 0→1로 뒤집을지는 구현체의 bInvertProgress가 결정한다
    UFUNCTION(BlueprintNativeEvent, Category = "Gauge")
    void SetGaugeProgress(float Progress01);

    UFUNCTION(BlueprintNativeEvent, Category = "Gauge")
    void SetGaugeVisible(bool bVisible);
};
```

**계약값을 "남은 비율"로 고정한 게 설계의 핵심이다.** 로직은 방향을 모른다. 차오를지 줄어들지는 순수한 표시 취향이므로 **비주얼 쪽 옵션(`bInvertProgress`)**으로 밀어냈다.

```cpp
// 비워두면 태그 추적은 그대로 하되 화면엔 아무것도 안 그린다
UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UWidget> GaugeVisual;
```

**"안 쓰기"까지 선택지에 넣었다** — WBP_HUD에 `CastGauge`를 안 놓으면 기능 전체가 꺼지고, `GaugeVisual`만 비우면 로직은 살아있되 안 그린다.

**머티리얼을 쓴 곳은 딱 하나뿐**이라는 것도 짚는다:
- 슬롯의 "아래→위 차오름" → UMG `ProgressBar`의 `Fill Type = Bottom to Top`으로 충분
- 잠금 룩(반투명 빨강, 대각선) → 이미지를 역할별로 분리해 **틴트 색의 알파**로. 대각선은 얇은 Image를 45° 회전
- **중앙 시전 게이지(방사형 마스크)** → UMG에 radial ProgressBar가 없어서 **여기만 머티리얼**

> **원칙: 머티리얼은 UMG 기본 기능으로 못 그리는 것에만 쓴다.**

### 9. 중앙 게이지가 스킬을 몰라도 되는 이유

```cpp
UPROPERTY(EditAnywhere, Category = "Cast")
FGameplayTag ChannelTag;   // 항상 State.Casting — 스킬별 고유 태그가 아니다
```

- 상호잠금 메커니즘상 **한 번에 한 스킬만** 이 태그를 가질 수 있다
- 채널링형 스킬은 **어차피 자기 `GE_CastingClass`에 이 태그를 넣어야만** 상호잠금이 동작한다 (이미 필수 조건)

→ 나중에 어떤 채널링 스킬이 추가돼도 **이 위젯은 코드도 설정도 안 바뀐다.**

반대로 `State.Shielded` 같은 지속형 태그는 중앙 게이지에 절대 뜨지 않는다. 그건 슬롯의 `ActiveTags` 몫이다.

### 10. 킬 피드백 — 만들지 않기로 한 것

킬 피드 UI(전체 클라 브로드캐스트 로그)는 **의도적으로 만들지 않았다.** 킬 성공 시 킬러 본인에게만 사운드 하나를 재생한다.

```cpp
// AEPPlayerController::Client_OnKill_Implementation (서버→킬러 개인, Reliable)
// 이미 존재하던 경로 — HitConfirmSound와 같은 패턴으로 KillConfirmSound만 추가
```

**GameState Multicast도, 새 위젯도 필요 없다.** 있는 경로에 두 줄 얹는 게 전부다.

> **"안 만든 것"을 근거와 함께 적는 게 포트폴리오 글에서는 오히려 신뢰를 준다.** 범위를 아는 사람으로 보인다.

> **에디터 작업 (WBP 6종)**
> `WBP_SkillSlot` / `WBP_CastGauge` / `WBP_RingGauge` / `WBP_ArcGauge` / `WBP_BarGauge` / `WBP_HUD`
> **스크린샷 위치**: ① 완성된 HUD 전체 화면 ② 슬롯 4-상태 비교 (Ready/Cooldown/Active/Locked) ③ 중앙 게이지를 링→막대로 갈아 끼운 비교

---

## 함정 정리

| 함정 | 설명 |
|------|------|
| `BindWidget` 이름 불일치 | WBP 컴파일 에러. 타입도 일치해야 한다 |
| **델리게이트 해제 누락** | ASC가 위젯보다 오래 산다. `NativeDestruct`에서 Remove 필수 |
| HUD 초기화를 PC `BeginPlay`에서 | 클라에서 ASC가 아직 null일 수 있다. `InitASC` 수렴 지점에서 |
| 리스폰 후 콜백 중복 | `InitWithASC`가 재진입 안전하지 않음 → 항상 해제 후 재바인딩 |
| **Mixed 모드 착각** | 쿨타임 GE 쿼리는 **소유 클라 전용**. 타 플레이어 UI는 복제되는 태그만 |
| Attribute 델리게이트가 클라에서 안 옴 | `REPNOTIFY_Always` 누락 (이 프로젝트는 4-1에서 설정 완료) |
| 쿨타임 폴링 남용 | 태그로 on/off, **쿨타임 중일 때만** Tick 쿼리 |
| Pair 순서 혼동 | `Key = 남은 시간`, `Value = 전체 Duration` |
| 오버레이가 옆으로 채워짐 | `Fill Type`이 기본값(Left to Right) → Bottom to Top |
| 슬롯이 빨강으로 안 바뀜 | `LockTags`(컨테이너)와 `CooldownTag`(단일)를 혼동 |
| 잠금 시 테두리·중앙이 같은 색 | 면+테두리가 한 장인 텍스처를 양쪽에 씀 → 텍스처 분리 필요 |
| 대각선이 안 보임 | `LockSlash`가 `BindWidgetOptional` — 이름이 틀려도 조용히 null |
| 차오르는 주황이 테두리를 덮음 | `SlotBorder`가 `CooldownBar`보다 아래 레이어 |
| **Heal 슬롯이 자기를 잠금으로 표시** | `RecomputeState()`에서 `bActive`를 `bLocked`보다 먼저 체크해야 함 |
| Shield 슬롯에 오렌지가 안 뜸 | `ActiveTags`가 비어 있음 — Dash와 달리 Shield는 지속형이라 `{State.Shielded}` 필요 |
| 새 채널링 스킬이 중앙 게이지에 안 뜸 | `ChannelTag`를 스킬 고유 태그로 설정 → 항상 공용 `State.Casting` |
| 머티리얼 게이지가 안 보임 | 브러시에 머티리얼 미지정 → `GetDynamicMaterial()`이 null |
| 게이지 모양 변경이 반영 안 됨 | `WBP_CastGauge` 안 자식 위젯 이름이 `GaugeVisual`이 아님 |

---

## 결과

**확인 항목 (PIE 2인):**
- 피격 → 체력바가 즉시 반응 (Attribute 델리게이트)
- 발사/재장전 → 탄약 숫자 반응, `State.Reloading` 표시 on/off
- 스킬 사용 → 슬롯이 Cooldown으로, 주황이 아래→위로 차오름, 남은 초 표시
- 힐 시전 → **힐 슬롯은 Active(주황 고정), 나머지 두 슬롯은 Locked(빨강)** ← 이 편의 핵심 검증
- 힐 시전 중 중앙 게이지가 줄어듦, 취소 시 즉시 사라짐
- Shield 발동 → Shield 슬롯 Active 5초 → 이후 Cooldown 30초
- 리스폰 → HUD 재바인딩 정상, 콜백 중복 없음, 크래시 없음
- 중앙 게이지 비주얼을 링 → 막대로 교체 → **C++ 무변경으로 동작**

**한계 및 향후 개선:**
- 타 플레이어 머리 위 체력바 없음 — **Mixed 모드 제약** 때문에 태그 또는 Attribute OnRep을 경유해야 한다
- 피격 방향 인디케이터, 데미지 숫자 미구현 — GameplayCue 기반으로 붙일 자리는 열려 있다
- 크로스헤어가 아직 `WBP_HUD`로 통합되지 않고 별도 위젯이다

**알면서 남긴 이슈:**
- **`EPSkillSlotWidget.cpp:143-149`** — 상태 변화가 없어도 `ApplyState`를 호출해 불필요한 캐시 리셋이 일어난다. 동작에는 문제없지만 낭비
- **`UEPCastGaugeWidget` 이름이 부정확하다** — 방벽(지속형) 지속시간 표시에도 쓰이면서 "Cast"라는 이름이 맞지 않게 됐다. 개명하려면 `CoreRedirects`가 필요해서 미뤘다

---

## 시리즈 마무리 — GAS 마이그레이션 결산

포스팅 말미에 붙일 회고. **표 두 개면 충분하다.**

**제거된 것 (전부 grep 검증 완료):**

| 제거 대상 | 대체 | 편 |
|---|---|---|
| `AEPCharacter::HP` / `MaxHP` / `TakeDamage()` / `OnRep_HP()` | `UEPAttributeSet::Health` + GE 파이프라인 | 4-2 |
| `UEPCombatComponent::Server_Fire` RPC | `GA_Item_PrimaryUse` | 4-3 |
| `UEPCombatComponent::LastServerFireTime` | `GE_FireCooldown` | 4-3 |
| `UEPCombatComponent::Server_Reload` RPC | `GA_Item_Reload` | 4-4 |
| `AEPWeapon::CurrentAmmo` / `StartReload` / `FinishReload` / `ReloadTimerHandle` | `Ammo` Attribute + `WaitDelay` | 4-4 |
| `AEPWeapon::WeaponState` (enum) | `State.Reloading` GameplayTag | 4-4 |
| `UEPWeaponDefinition::BoneDamageMultiplierMap` | `TagDamageMultiplierMap` | 4-6 |
| `UEPPhysicalMaterial::bIsWeakSpot` | `MaterialTags` | 4-6 |
| Stamina 전체 (Attribute / GE / `State.Dashing`) | 설계 변경으로 폐기 | 4-7 |

**끝까지 유지된 것 — 이게 더 중요한 표다:**

| 유지 대상 | 이유 |
|---|---|
| `UEPServerSideRewindComponent` **전체** | 호출 위치만 CombatComponent → GA로. **구조 무변경** |
| Physics Asset 본별 히트박스 | 태그 시스템이 그 위에 얹혔을 뿐 |
| `EP_TraceChannel_Weapon` | 무변경 |
| `UEPCombatComponent` 코스메틱 헬퍼 | GA가 호출하는 서버/로컬 헬퍼로 유지 |
| `EEPBallisticType` switch | GA 내부로 이동만 |
| `AEPWeapon::Fire()` 스프레드 계산 | 4-5에서 CDF로 개선하며 유지 |

**결론 문장 (이런 톤으로):**

> GAS 이관은 전면 재작성이 아니었다. **어려운 부분(랙 보상, 본 단위 판정)은 그대로 두고, 상태를 관리하던 코드만 걷어냈다.**
> 가장 크게 달라진 건 기능이 아니라 **"기능을 추가할 때 무엇을 열어야 하는가"**다. 이전에는 Character와 CombatComponent를 열었고, 지금은 GA 클래스 하나와 DataAsset을 만든다.

**다음 단계 예고**: 5단계 Loot/인벤토리 — 4-7 말미의 스킬 슬롯 로드아웃이 여기로 이어진다.

---

## 참고

- `DOCS/Notes/04/04_GAS_08_HUD.md` — 구현 전체
- `DOCS/Notes/04/04_GAS_08_HUD_STATUS.md` — 실제 구현 및 남은 이슈
- `DOCS/Notes/04/GAS_STATUS.md` — 전체 진행 상황 및 레거시 제거 검증
- 엔진 `GameplayAbility.cpp:1206` — `GetActiveEffectsTimeRemainingAndDuration` 사용례
