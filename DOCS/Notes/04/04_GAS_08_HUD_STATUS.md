# 04_GAS_08_HUD — 구현 상태

**전체 상태: 구현 완료. 4-상태 스킬 슬롯 + 중앙 게이지 2종(시전/방벽) + 게이지 시각화 인터페이스 분리까지 전부 반영. PIE 2인 멀티 검수 완료 (2026-07-26 코드·에셋 검증)**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷 (2026-07-26 기준). 에셋 내부 구조는 uasset 문자열 추출로 확인.

---

## 클래스 구성

```
UEPHUDWidget                      화면 루트. ASC 어트리뷰트/태그 구독
├─ HealthBar / HealthText / AmmoText / ReloadingText / TimerText
├─ DashSlot / HealSlot / ShieldSlot   : UEPSkillSlotWidget   (BindWidget)
├─ CastGauge                          : UEPCastGaugeWidget   (BindWidget)         ← State.Casting
└─ ShieldGauge                        : UEPCastGaugeWidget   (BindWidgetOptional) ← State.Shielded

UEPCastGaugeWidget                태그 하나를 감시해 남은 시간 비율을 계산
└─ GaugeVisual : UWidget (BindWidgetOptional) — IEPGaugeVisual 구현체면 무엇이든 가능

IEPGaugeVisual                    게이지 "모양"과 "계산"을 분리하는 인터페이스
├─ SetGaugeProgress(float)        BlueprintNativeEvent. 계약: 항상 "남은 비율" 1→0
├─ SetGaugeVisible(bool)
├─ UEPMaterialGaugeWidget         MID 스칼라 파라미터로 전달 (링/방사형 등 머티리얼 기반)
└─ UEPBarGaugeWidget              UMG ProgressBar::SetPercent로 전달
```

**핵심 설계 결정 — 중앙 게이지가 2개인 이유:**

`State.Casting`(시전, 취소 가능, 다른 스킬 잠금)과 `State.Shielded`(발동 이후 버프 지속시간, 잠금 없음)는 의미가 다르고 **동시에 켜질 수 있다.** 힐 시전 중에 방벽이 돌아갈 수 있으므로 게이지 하나를 태그 컨테이너로 공유하면 안 된다 — `NativeTick`의 max 선택 루프가 남은 시간이 긴 쪽(방벽 5초)을 골라 힐 시전 게이지를 덮어버린다. 따라서 **위젯 인스턴스를 분리**하고, 화면상 겹치지 않는 위치에 서로 다른 모양으로 배치한다.

`ChannelTag`는 각 컨테이너 WBP의 **클래스 디폴트**로 지정한다. WBP_HUD 인스턴스에서 오버라이드하지 말 것 — 과거 힐 게이지가 안 뜨던 버그의 원인이었다.

---

## 구현 완료 — 코드

### `EPHUDWidget.h/.cpp`
- Health/MaxHealth/Ammo/MaxAmmo 어트리뷰트 + `State.Reloading` 태그 구독
- `RefreshHealth` / `RefreshAmmo`, `InitWithASC` / `UnbindAll` (리스폰 재바인딩 안전)
- `InitWithASC`에서 3개 슬롯 + `CastGauge` + `ShieldGauge`(null 가드) 전파

### `EPSkillSlotWidget.h/.cpp` — 4-상태
```
EEPSkillSlotState { Ready, Cooldown, Active, Locked }
우선순위: Active > Locked > Cooldown > Ready   (RecomputeState)
```
- 감시 태그: `CooldownTag`(단일) / `ActiveTags`(컨테이너) / `LockTags`(컨테이너)
- 세 슬롯 모두 `LockTags = { State.Casting }` — 스킬별로 다르지 않다
- 바인딩 위젯: `SlotBorder` / `SlotCenter` / `SkillIcon` / `LockSlash` / `CooldownBar` / `CooldownText` (전부 `BindWidget`)
- 스타일 UPROPERTY 7종 (Ready/Cooldown/Locked 색상)
- `NativeTick`: 상태에 따라 `ActiveTags` 또는 `CooldownTag`로 `GetActiveEffectsTimeRemainingAndDuration` 질의 → 남은 시간 최대값 선택 → 쿨타임 바는 `1 - Remaining/Duration`(차오름), 텍스트는 `CeilToInt(Remaining)`

### `EPCastGaugeWidget.h/.cpp`
- `ChannelTag` 하나만 감시. `OnChannelTagChanged`에서 자기 Visibility + `SetGaugeVisible` 전파
- `NativeTick`: `MakeQuery_MatchAnyOwningTags(ChannelTag)` → `Remaining/Duration`(줄어듦) → `Execute_SetGaugeProgress`
- `NativeConstruct`에서 `GaugeVisual`이 `IEPGaugeVisual` 미구현이면 경고 로그 + 시작 시 Collapsed

### `EPGaugeVisual.h` / `EPMaterialGaugeWidget.h/.cpp` / `EPBarGaugeWidget.h/.cpp`
- 두 구현체 모두 `bInvertProgress` 보유 — 게이지가 차오르는 방향을 에디터에서 뒤집을 수 있음
- `UEPMaterialGaugeWidget`: `GaugeImage`(BindWidget) 브러시에서 MID 생성, `ProgressParamName`(기본 `"Progress"`)에 값 세팅

### `EPPlayerController`
- `InitHUD`, `Client_OnKill` → `KillConfirmSound`, `Client_PlayHitConfirmSound` → `HitConfirmSound`
- 킬 피드 UI는 **의도적으로 미구현** — 킬 사운드 단독 방식이 최신 결정

---

## 구현 완료 — 에셋 (`Content/Blueprints/HUD/`)

```
WBP_HUD (UEPHUDWidget)
├─ DashSlot / HealSlot / ShieldSlot  = WBP_SkillSlot
│     Cooldown.Skill.Dash / .Heal / .Shield,  ActiveTags: State.Healing / State.Shielded,
│     LockTags: State.Casting (3슬롯 공통)
├─ CastGauge   = WBP_CastGauge      ← ChannelTag = State.Casting   (시전 = 바)
└─ ShieldGauge = WBP_ShieldGauge    ← ChannelTag = State.Shielded  (방벽 = 링)

WBP_CastGauge   (부모: EPCastGaugeWidget)
 └─ GaugeVisual = WBP_BarGauge   (부모: EPBarGaugeWidget)
     └─ GaugeBar (ProgressBar)

WBP_ShieldGauge (부모: EPCastGaugeWidget)
 └─ GaugeVisual = WBP_RingGauge  (부모: EPMaterialGaugeWidget)   ProgressParamName = "Progress"
     └─ GaugeImage (Image)  ← Brush > Image = M_RingGauge   ★ 머티리얼은 여기에 붙는다

WBP_SkillSlot (부모: EPSkillSlotWidget)
 └─ SlotBorder / SlotCenter / SkillIcon / LockSlash / CooldownBar / CooldownText

머티리얼·텍스처: M_RingGauge, SlotBorder, SlotCenter, SkillIcon, LockSlash
```

> 자식 위젯의 **변수명이 반드시 `GaugeVisual`**이어야 `BindWidgetOptional`이 잡는다.

---

## 함정 — 이번 단계에서 실제로 겪은 것

### 1. 머티리얼이 UMG에서 안 보임 (Substrate)

UI 도메인 머티리얼의 블렌드 모드를 `TranslucentColoredTransmittance`로 두면 게이지가 완전히 사라진다. **`TranslucentGreyTransmittance`를 써야 한다.**

- `EngineTypes.h:249-257` — `BLEND_TranslucentGreyTransmittance`는 기존 `BLEND_Translucent`와 **같은 값**(이름만 변경). 반면 `BLEND_TranslucentColoredTransmittance`는 Substrate 전용 신규 모드로 **dual-source blending**을 요구한다.
- `SlateRHIRenderingPolicy.cpp::GetMaterialBlendState()`에 이 모드의 case가 없어 `default:` = `TStaticBlendState<>`(불투명, 블렌딩 없음)로 떨어지고, Slate는 dual-source PSO를 셋업하지 않는다.
- 이 프로젝트는 `Config/DefaultEngine.ini:152`에 `r.Substrate=True`라 비-Substrate 경로의 자동 폴백이 걸리지 않는다.

### 2. 링 게이지가 3시 방향에서 시작함

`VectorToRadialValue`의 각도 매핑은 3시=0, 6시=0.25, 9시=0.5, 12시=0.75다 (UMG의 TexCoord는 V가 아래로 증가해 시계방향으로 읽힌다). 12시에서 시작하려면 각도에 **`+0.25` 후 `Frac`**. 반시계 방향은 `Frac(0.75 - angle)`.

`Frac(x) = x - Floor(x)`이며 항상 `[0,1)` — `Frac(-0.25) = 0.75`이므로 `fmod`와 달리 음수에서도 원형 좌표의 wraparound로 그대로 쓸 수 있다.

### 3. 게이지가 시작 직후 멈칫했다가 다시 줄어듦 ★

**증상**: 방벽/힐 시전 시 진행도가 95% 부근에서 멈췄다가 다시 감소.

**원인**: `LocalPredicted` 어빌리티는 클라에서 예측 GE를 먼저 적용하고, 잠시 뒤 서버의 복제 GE가 도착한다. 복제본은 예측본보다 **지연시간만큼 늦게 시작**했으므로 남은 시간이 더 길다. 두 GE가 공존하는 짧은 구간 동안 `NativeTick`의 `if (Pair.Key > Remaining)` 최대값 선택 루프가 **더 늦게 시작한 쪽**을 골라 진행도가 역행한다.

**수정**: `EPCastGaugeWidget` / `EPSkillSlotWidget` 양쪽에 `LastShownRemaining` 캐시 + 단조 감소 클램프.

```cpp
if (Duration <= 0.f) return;
if (LastShownRemaining >= 0.f)                       // -1.f = 미설정 센티넬 (첫 프레임은 무조건 수용)
    Remaining = FMath::Min(Remaining, LastShownRemaining);
LastShownRemaining = Remaining;
```

- **비율이 아니라 남은 "초"를 캐시하는 것이 중요.** 예측본과 복제본의 `Duration`이 미세하게 다를 수 있어 비율끼리의 비교는 무의미하고, 슬롯에서는 바(비율)와 `CooldownText`(정수 초)가 같은 클램프된 값을 공유해야 서로 어긋나지 않는다.
- **캐시 리셋은 필수.** `EPSkillSlotWidget::ApplyState`(154행)와 `EPCastGaugeWidget::OnChannelTagChanged`(75행)에서 `-1.f`로 되돌린다. 상태가 바뀌면 질의 대상 GE 자체가 바뀌기 때문 — 방벽 Active(5초, `State.Shielded`) → Cooldown(50초, `Cooldown.Skill.Shield`) 전환 시 리셋이 없으면 `Min(45, 0) = 0`에 영원히 고정되어 쿨타임이 0초로 표시된다.

---

## 남은 이슈 (경미)

| # | 위치 | 내용 |
|---|---|---|
| 1 | `EPSkillSlotWidget.cpp:143-149` | `RecomputeState`가 상태 변화 여부와 무관하게 `ApplyState`를 호출한다. 방벽 진행 중(Active) 힐을 시전하면 `OnLockTagChanged` → 여전히 Active인데 `LastShownRemaining`이 리셋된다. `ApplyState` 선두에 `if (NewState == CurrentState) return;` 가드를 넣을 수 있으나, `CurrentState` 기본값이 `Ready`라 초기 시각 세팅이 통째로 스킵되지 않도록 `Uninitialized` 항목 추가 등이 필요 |
| 2 | `EPSkillSlotWidget.cpp:163-168` | Active 진입 시에만 `SetPercent(1.f)`. Cooldown 진입 시 `SetPercent(0.f)`가 없어 첫 틱 전 1프레임 동안 이전 값이 남을 수 있음 |
| 3 | `EPCastGaugeWidget.cpp:72-80` | `OnChannelTagChanged`가 활성화 시 진행도를 밀어주지 않는다(`SetGaugeProgress(1.f)` 없음). UMG 틱 순서상 실제로 보이지 않을 가능성이 높아 미조치 |
| 4 | 명명 | `UEPCastGaugeWidget`은 이제 시전 전용이 아니다(방벽 지속시간에도 씀). `UEPDurationGaugeWidget` / `ChannelTag` → `WatchTag`로의 개명은 CoreRedirects가 필요해 보류 |

---

## PIE 2인 멀티 검수 (완료 — 2026-07-26)

- [x] Dash/Shield 사용 → 슬롯 오버레이 차오름 → 완료 시 원복
- [x] Heal 시전 → HealSlot Active 표시 + 중앙 바 게이지(1→0) + Dash/ShieldSlot 빨강 잠금(LockSlash 표시)
- [x] Heal 시전 중 Dash/Shield 입력 무반응 (활성화 자체 차단)
- [x] Heal 채널링 중 피격 취소 → 모든 시각 요소 즉시 원상복귀
- [x] Shield 사용 → 링 게이지로 5초 지속시간 표시, 종료 후 슬롯이 쿨타임으로 전환
- [x] 방벽 지속 중 힐 시전 → 링/바 게이지 동시 표시, 서로 간섭 없음
- [x] 킬 발생 → 킬러 화면에서만 사운드 재생
- [x] 게이지 진행도 역행 없음 (클라이언트 측에서 확인 — 리슨서버 호스트는 예측=권위라 증상이 나타나지 않음)
