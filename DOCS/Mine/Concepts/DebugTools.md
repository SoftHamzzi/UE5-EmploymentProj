# UE5 디버그 도구 총정리

> 목적: "코드에 뭘 찍을까"가 아니라 "**어떤 도구가 어떤 질문에 답하는가**"를 정리한다.
> 도구 하나를 잘 쓰는 것이 여러 개를 대충 아는 것보다 낫다.
> 근거: UE 5.7 엔진 소스 직독. 이 프로젝트(`EPHUDWidget`, `UEPCombatDeveloperSettings`,
> GAS 사용)와 연결되는 지점은 각 절 끝에 적었다.

---

## 0. 먼저 질문을 분류한다

| 질문의 모양 | 맞는 도구 |
|---|---|
| "이 값이 지금 얼마야?" (한두 개) | 화면 텍스트 (`AddOnScreenDebugMessage` / HUD Canvas / UMG) |
| "이 액터의 상태를 몇 개 동시에, 매 틱 보고 싶다" | `showdebug` (§1) 또는 Gameplay Debugger (§2) |
| "지금은 안 보이지만, 지난 N초 동안 무슨 일이 있었는지" | Visual Logger (§3) |
| "이 UI가 왜 이 위치/크기로 그려졌지" | Slate Widget Reflector (§4) |
| "이 프레임이 왜 느렸지" (CPU/GPU/메모리) | `stat` 명령 + Unreal Insights (§5) |
| "네트워크로 뭐가 오가는지" | Network Profiler / `stat net` (§6) |
| "값 하나를 코드 재컴파일 없이 바꿔가며 보고 싶다" | Console Variable (CVar) (§7) |
| "물리적 위치/궤적/범위를 3D로 보고 싶다" | `DrawDebugHelpers` (§8) |

---

## 1. `showdebug` — 액터 하나에 여러 카테고리를 겹쳐 보기

**무엇인가:** `AHUD`에 내장된 시스템. 콘솔에 `showdebug <카테고리>`를 치면 해당 카테고리가 매 프레임 화면 좌측에 텍스트로 쌓인다. 카테고리는 토글식이라 여러 개를 동시에 켤 수 있다.

**켜는 법**
```
showdebug ai        # 카테고리 하나 토글
showdebug net        # 네트워크 관련 (핑, 대역폭 등)
showdebug collision
showdebug reset       # 전부 끄기
```
`showdebug` (인자 없이)는 전체 디버그 표시를 토글한다.

**동작 원리** (`HUD.cpp:357` `AHUD::ShowDebug`): `DebugType`을 `DebugDisplay` 배열에 추가/제거하고 `bShowDebugInfo`를 세운다. 매 프레임 `AHUD::DrawDebug` → 대상 액터의 `AActor::DisplayDebug(Canvas, DebugDisplay, YL, YPos)`가 호출된다.

**직접 카테고리를 추가하려면** 액터에서 `DisplayDebug`를 오버라이드한다:
```cpp
void AEPCharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
    Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);
    if (DebugDisplay.IsDisplayOn(TEXT("Ammo")))
    {
        Canvas->DrawText(GEngine->GetSmallFont(),
            FString::Printf(TEXT("Ammo %.0f/%.0f"), Cur, Base), 4, YPos);
        YPos += YL;
    }
}
```
**언제 쓰나:** 한 액터(주로 내 캐릭터/폰)의 여러 상태를 계속 겹쳐서 보고 싶을 때. WBP를 안 건드리고, Shipping에서도 (콘솔 명령이 열려 있으면) 그대로 쓸 수 있다.

---

## 2. Gameplay Debugger — 월드의 여러 액터를 카테고리별로 보기 (GAS 카테고리 내장)

**무엇인가:** `'`(Apostrophe) 키로 켜는 오버레이. `showdebug`와 달리 **월드에 있는 여러 액터**를 대상으로, 카테고리(AI, Ability, Perception, Navigation …)를 숫자 키로 골라 보여준다. 조이스틱/키보드로 대상 액터를 바꿔가며 볼 수 있다.

**켜는 법**
- 에디터/PIE에서 `'` 키 (기본값, `GameplayDebuggerConfig.cpp:14` `ActivationKey = EKeys::Apostrophe`)
- 콘솔 명령으로도 가능: `EnableGDT` (레거시, `GameplayDebuggerLocalController.cpp:1219`), `gdt.Enable` (`:1225`)
- 켠 다음 `` ` `` + 숫자 키로 카테고리 토글 (화면 안내에 표시됨)

**이 프로젝트와 바로 연결되는 지점:** GAS(GameplayAbilities 플러그인)가 **Ability 카테고리를 이미 만들어 둔다** (`GameplayDebuggerCategory_Abilities.cpp`). 이 카테고리를 켠 상태에서 Shift+1~4로 GameplayTags / Abilities / Effects / Attributes를 개별 토글할 수 있다. 색상 규칙도 정해져 있다 — 청록(Server), 초록(Local), 노랑(Both), 보라(Non-Replicated). 즉 **탄약 속성의 예측값/서버값을 비교하려고 새 UMG 위젯을 만들기 전에, 이 카테고리를 켜보는 편이 더 빠를 수 있다** — `UEPAttributeSet`의 Ammo/MaxAmmo가 Attribute 카테고리에 이미 뜬다.

**언제 쓰나:** GAS/AI처럼 엔진이 이미 카테고리를 준비해 둔 시스템을 볼 때, 또는 월드의 다른 플레이어/AI 상태를 시점 전환하며 보고 싶을 때.

---

## 3. Visual Logger — 시간을 되돌려 보는 로그 (재현 안 되는 버그에 강함)

**무엇인가:** 텍스트 로그가 아니라 **시간축 + 3D 위치가 같이 기록되는 로그**. 기록해 둔 뒤 에디터의 Visual Logger 창(Window → Developer Tools → Visual Logger)에서 타임라인을 스크럽하면, 그 시점의 3D 도형(경로, 범위, 텍스트)이 레벨 뷰포트에 겹쳐 보인다.

**켜는 법**
```
VisLog record         # 기록 시작 (에디터: 메모리에, 독립 실행: 파일로)
VisLog stop            # 기록 중지
```
(`VisualLogger.cpp:1253` `FLogVisualizerExec::Exec_Dev`에서 `record`/`stop` 처리)

**로그를 남기는 코드:**
```cpp
UE_VLOG(this, LogEPCombat, Log, TEXT("Fire origin: %s"), *Origin.ToString());
UE_VLOG_LOCATION(this, LogEPCombat, Log, Origin, 10.f, FColor::Red, TEXT("Origin"));
```
`UE_VLOG`는 `FVisualLogger::IsRecording()`이 false면 아무 비용도 없다 (`VisualLogger.h:30`) — 그래서 **Shipping이 아니면 코드에 상시 남겨둬도 된다.**

**언제 쓰나:** "그때 왜 그 위치에서 놓쳤지" 같은, **재현이 어렵고 그 순간의 3D 맥락이 중요한** 버그. 이 프로젝트의 SSR(Server-Side Rewind) 스냅샷/리와인드 위치 오차 같은 문제가 정확히 이 케이스다 — 지금은 `bEnableSSRDebugDraw`로 그 순간의 라인만 그리는데, Visual Logger로 옮기면 **되감아서** 여러 프레임을 비교할 수 있다.

---

## 4. Slate Widget Reflector — UI가 왜 이렇게 그려졌는지

**무엇인가:** Slate/UMG 위젯 트리를 실시간으로 보여주는 창. 위젯 계층, 각 위젯의 크기/위치(Desired Size, Geometry), 어떤 위젯이 클릭을 먹었는지(Hit Test), 어떤 위젯이 매 프레임 다시 그려지는지(Invalidation)를 보여준다.

**켜는 법:** 에디터 상단 메뉴 `Window → Developer Tools → Widget Reflector`, 또는 `WidgetReflector` 콘솔 명령. "Pick Painted Widgets" 버튼으로 화면의 위젯을 직접 클릭해 트리에서 찾을 수 있다.

**언제 쓰나:** UMG 레이아웃이 생각한 자리에 안 그려지거나, 특정 위젯이 클릭을 안 받거나("내 뒤에 있는 투명한 위젯이 먹고 있다" 같은 경우), 매 프레임 불필요하게 다시 그려지는 위젯(성능 문제)을 찾을 때. 이번에 `EPHUDWidget`에 `NetDebugText`를 추가할 때, 원하는 위치에 정확히 배치됐는지/다른 위젯에 가려지는지를 이걸로 확인하면 빠르다.

---

## 5. `stat` 명령 + Unreal Insights — "왜 느린가"

**무엇인가:**
- `stat` 명령(`stat fps`, `stat unit`, `stat game`, `stat gpu` …)은 **화면에 실시간 숫자**를 띄운다. 매 프레임 비용의 대략적인 분류(Game/Draw/GPU/Frame)를 즉시 본다
- Unreal Insights(`UnrealInsights.exe`, Trace 시스템)는 **타임라인 프로파일러**다. 특정 프레임을 확대해서 어떤 함수가 몇 ms를 썼는지, Thread별로 무슨 일이 겹쳤는지 본다

**켜는 법**
```
stat unit        # Frame/Game/Draw/GPU 시간 (가장 먼저 보는 것)
stat game        # 게임 스레드 세부 항목
```
Insights는 실행 시 `-trace=cpu,frame,log` 같은 커맨드라인 인자를 주거나, 에디터의 `Trace` 메뉴에서 채널을 켜고 `UnrealInsights.exe`로 결과를 연다.

**언제 쓰나:** `stat unit`으로 "Game이냐 GPU냐"부터 좁히고, 안에서 뭐가 문제인지는 Insights로 내려간다. **화면 오버레이(§0의 첫 줄)와는 완전히 다른 목적** — 값 확인이 아니라 시간 측정이다.

---

## 6. 네트워크 전용 도구

이미 이 프로젝트에서 쓰고 있는 것부터:

| 도구 | 확인 방법 | 용도 |
|---|---|---|
| PIE Network Emulation | 에디터 환경설정 → Multiplayer Options | 지연/손실을 인위로 건다 (`DOCS/Mine/Debug/AmmoPredictionCapture.md` §2에서 이미 사용) |
| `showdebug net` | 콘솔 | 폰/커넥션의 네트워크 상태 요약 |
| `stat net` | 콘솔 | 송수신 대역폭, 채널 수, RPC 카운트 |
| Network Profiler | `-networkprofiler` 실행 인자 + `NetworkProfiler.exe` | 패킷 단위로 "이 프로퍼티가 몇 바이트 나갔는지" 분석. `stat net`보다 훨씬 세밀 |

**언제 쓰나:** "이 캐릭터가 replicate하는 데 대역폭을 너무 많이 쓴다" 같은 질문. 이 프로젝트 규모에서는 `stat net` 정도로 충분한 경우가 많고, Network Profiler는 다수 플레이어에서 대역폭이 실제 문제가 될 때 꺼낸다.

---

## 7. Console Variable (CVar) — 재컴파일 없이 값 조절 + 토글

**무엇인가:** `TAutoConsoleVariable<T>`로 등록한 변수. 콘솔/커맨드라인/ini에서 값을 바꿀 수 있다. **CVar 자체는 화면에 아무것도 그리지 않는다** — 매 프레임 그 값을 읽고 그리는 코드가 따로 있어야 한다 (지난 대화에서 다룬 내용).

```cpp
static TAutoConsoleVariable<bool> CVarShowAmmoDebug(
    TEXT("EP.Debug.Ammo"), false, TEXT("Show predicted vs confirmed ammo"));
```
```
EP.Debug.Ammo 1      # 콘솔에서 켜기
```

**언제 쓰나:** 로그 Verbosity 조절(`Log LogEPCombat Verbose`)이나 화면 오버레이 토글처럼, **다른 도구(§1~§6)를 코드 재빌드 없이 켜고 끄는 스위치**로. CVar는 그 자체가 도구가 아니라, 다른 도구들의 "리모컨"이라고 보는 게 맞다.

---

## 8. `DrawDebugHelpers` — 3D 공간에 도형 찍기

**무엇인가:** 라인/구/박스/캡슐/화살표/문자열을 월드 공간에 즉시 그리는 함수 모음 (`DrawDebugLine`, `DrawDebugSphere` …, `DrawDebugHelpers.h:22, :45`).

```cpp
DrawDebugSphere(GetWorld(), ImpactPoint, 8.f, 12, FColor::Red, false, 2.f);
```

**언제 쓰나:** 트레이스 경로, 콜리전 범위, 스폰 위치 같은 **위치/모양**을 그 자리에서 확인할 때. 이 프로젝트의 SSR 디버그(`bEnableSSRDebugDraw`, Blue/Red/White/Yellow 라인)가 정확히 이것이다 — Visual Logger(§3)와 달리 **되감기가 안 되고, 지금 이 프레임만** 보여준다.

---

## 9. 이 프로젝트에 있는 것과 매칭

| 이미 있는 것 | 분류 |
|---|---|
| `UEPCombatDeveloperSettings.bEnableSSRDebugDraw` | §8 (DrawDebugHelpers)를 부울 하나로 감싼 것 |
| `UEPCombatDeveloperSettings.bEnableSSRDebugLog` | 로그 카테고리를 부울로 감싼 것 (§0 표의 "로그"에 해당, 이 문서엔 별도 절 없음 — `UE_LOG` + Verbosity가 표준형) |
| `EPHUDWidget` (잔탄/체력 텍스트) | §0의 "값이 얼마야" — 그 자체가 게임 UI, 디버그 도구는 아니다 |
| `DOCS/Mine/Debug/AmmoPredictionCapture.md`의 `NetDebugText` 계획 | §0 "값 확인" 목적의 UMG 오버레이. §2(Gameplay Debugger Ability 카테고리)로 대체 가능한지 먼저 검토할 것 — 카메라 배치/GIF 캡처가 목적이라 위치를 잔탄 옆에 고정해야 하므로 이번엔 UMG가 맞는 선택 |

**아직 없는 것 중 다음에 쓸 만한 것:**
- GAS Ability 카테고리(§2)는 이미 엔진이 준다 — 새로 만들 필요 없음, `'` 키로 확인만 하면 됨
- SSR 리와인드 오차처럼 "그 순간을 되감아 보고 싶은" 문제는 Visual Logger(§3)가 지금의 즉시 드로잉보다 낫다. 단, 이건 상상 속 확장점이 아니라 **문서에 이미 적힌 문제**(`DOCS/Mine/ServerSideRewind.md`)이므로 실제로 필요해지면 만든다
