# 탄약 예측 GIF 촬영 계획

> 목적: 포트폴리오 `01_EmploymentProj.md` §2-4의 이미지 슬롯
> "지연 200ms 설정에서 발사와 같은 프레임에 잔탄 숫자가 줄어드는 GIF. 지연 설정값이 화면에 보이게"를 채운다.
> 상태: 계획 (2026-09-26). 코드는 아직 없다.

---

## 0. 결론 (2026-09-26 수정 — 엔진 내장 패널로 대체)

**예측값/서버 확정값 비교는 새로 안 만든다.** `showdebug AbilitySystem`이 이미 속성마다
`Ammo 29.00 (Base: 30.00)` 형태로 CurrentValue(예측)와 BaseValue(서버 확정)를 같이 찍는다
(§1의 근거와 정확히 일치 — 직접 촬영해 확인함, `스크린샷 2026-09-26 220749.png`).
새 UMG 위젯으로 같은 정보를 또 만드는 건 CLAUDE.md §2 기준 "이미 있는 걸 다시 만드는" 것이라 안 한다.

이 패널에 **없는 것 딱 하나**, 지연 설정값(RTT/에뮬레이션 값)만 최소 위젯으로 얹는다.

```
[showdebug AbilitySystem 패널 — 화면 좌측, 엔진 기본 위치]     [지연 위젯 — 화면 우측 상단, 새로 추가]
Ammo 29.00 (Base: 30.00)                                        RTT 250ms | Emu Out 100 / In 100
  None AddFinal -1.00 - Default__GE_ConsumeAmmo_C
Health 100.00
...
```

- **지연 설정값**은 손으로 쓴 라벨이 아니라 NetDriver에 **실제로 적용된 값**을 읽어 찍는다 (§3)
- 두 패널이 화면의 다른 자리에 따로 뜬다 — `showdebug AbilitySystem`은 위치를 지정할 수 없는 엔진 내장 레이아웃이라 감수한다. GIF 캡처 1회용이므로 미관보다 "새 코드를 최소화"를 우선한다

`stat net`이나 `AddOnScreenDebugMessage`는 쓰지 않는다. `stat net`은 필요 없는 줄이 수십 개이고, OnScreen 메시지는 왼쪽 위에 쌓여서 `showdebug` 패널과 겹친다.

---

## 1. `showdebug AbilitySystem` 여는 법 + 왜 BaseValue가 "서버 확정 값"인가

콘솔(`~`)에 순서대로:
```
showdebug AbilitySystem
showdebug Attributes
```
(속성 목록이 이미 보이면 두 번째 줄은 생략해도 된다. `DisplayDebug`는 `Attributes`/`Ability`/`GameplayEffects` 세 하위 카테고리를 토글하는데(`AbilitySystemComponent.cpp:2496-2516`), Attributes를 켜야 확실히 나온다.)

패널 자체는 `Debug_Internal`이 그리고, 화면 좌측 고정 위치라 옮길 수 없다 (`AbilitySystemComponent.cpp:2468` `DisplayInfo.IsDisplayOn(TEXT("AbilitySystem"))`).

클라이언트에서 한 속성은 값 두 개를 가진다.

| 값 | 읽는 API | 클라이언트에서의 의미 |
|---|---|---|
| CurrentValue | `ASC->GetNumericAttribute(Attr)` | BaseValue + 예측 모디파이어. 지금 HUD가 쓰는 값 |
| BaseValue | `ASC->GetNumericAttributeBase(Attr)` | **서버가 복제해 준 값 그대로** |

근거 (UE 5.7):
- `OnRep_Ammo` → `GAMEPLAYATTRIBUTE_REPNOTIFY` → `SetBaseAttributeValueFromReplication` (`AttributeSet.h:403-407`)
- 이 함수가 aggregator의 BaseValue를 서버 값으로 덮는다 (`GameplayEffect.cpp:3690`, `:3704`). 예측 모디파이어를 포함한 평가는 CurrentValue에만 들어간다 (`:3699`)
- 발사 비용 GE는 Instant인데, 클라이언트에서 예측으로 적용되면 무한 지속 모디파이어가 된다 → BaseValue는 건드리지 않고 CurrentValue만 −1 (`DOCS/Mine/Concepts/PredictionKey.md` 참조)

그래서 두 값의 차이가 곧 "아직 서버 확인을 받지 못한 발 수"다.

---

## 2. 지연 설정: PIE 네트워크 에뮬레이션

에디터 환경설정 → 레벨 에디터 → 플레이 → Multiplayer Options:

| 항목 | 값 | 이유 |
|---|---|---|
| Net Mode | Play As Client | 데디 서버 + 클라이언트 1. 호스트는 예측을 하지 않으므로 쓰면 안 된다 |
| Number of Players | 1 | 창 하나만 녹화 |
| Enable Network Emulation | ✅ | |
| Emulation Target | **Clients Only** | 설정값을 읽을 NetDriver가 녹화하는 클라이언트 쪽에 있어야 한다 |
| Outgoing traffic Min/Max Latency | 100 / 100 | 고정값. 범위를 주면 RTT 숫자가 흔들린다 |
| Incoming traffic Min/Max Latency | 100 / 100 | 왕복 합 ≈ 200ms |
| Packet Loss | 0 | 손실이 있으면 확정 숫자가 끊겨 보인다 |

PIE 설정은 Outgoing → `PktLagMin/Max`, Incoming → `PktIncomingLagMin/Max`로 들어간다. 전달 경로는 인스턴스가 어디서 도는지에 따라 다르다.

| 인스턴스 위치 | 전달 방법 | 근거 |
|---|---|---|
| 새 프로세스 | 명령줄 `-PktLagMin=…` | `PlayLevelNewProcess.cpp:244-249`, `LevelEditorPlayNetworkEmulationSettings.cpp:289-295` |
| 에디터 프로세스 안 (클라이언트) | 접속 URL `?PktLagMin=…` → `InitBase`가 URL 옵션으로 설정을 **통째로 교체** | `GameInstance.cpp:426-431`, `NetDriver.cpp:1735-1747` |

어느 쪽이든 클라이언트 NetDriver의 `PacketSimulationSettings`에서 같은 이름의 필드를 읽으면 된다.

> "지연 200ms"는 **왕복 200ms**로 적는다. 한쪽 200ms로 잡으면 RTT가 약 400ms가 되어 포트폴리오 문장과 맞지 않는다.

### 2-1. 주의: 콘솔 `NetEmulation.*`는 에디터를 끌 때까지 남는다 (2026-09-27 실측)

**증상:** 같은 100/100 설정에서 "단일 프로세스 하 실행" ON → RTT 약 450ms, OFF → 약 250ms.

**원인:** 예전에 콘솔로 친 `NetEmulation.PktLag 100`, `NetEmulation.PktIncomingLagMin 100`이 에디터 프로세스 전역에 남아 **서버에도** 지연을 걸었다.

1. `NetEmulation.*` 명령은 값을 static 전역 `PersistentPacketSimulationSettings`에 쓴다 (`NetEmulationHelper.cpp:16`, `:340-349`). 이 값은 PIE를 끝내도 지워지지 않고 에디터 프로세스가 끝날 때까지 남는다
2. 이후 그 프로세스에서 만들어지는 **모든** NetDriver는 생성 시 이 값을 그대로 받는다 (`NetDriver.cpp:731-735`)
3. 단일 프로세스 ON이면 데디 서버도 에디터 프로세스 안에서 돈다 (`PlayLevel.cpp:2855-2863`, `:2879-2907`). Emulation Target이 Clients Only라서 서버 URL에는 Pkt 옵션이 붙지 않고 (`GameInstance.cpp:457-461`), 서버는 전역 값(Out 100 / In 100)을 **그대로 쓴다**
4. 클라이언트도 전역 값을 받지만 접속 URL의 100/100이 덮어쓰므로 설정은 정상이다. 그래서 §3 위젯은 `Emu Out 100 / In 100`으로 **정상처럼** 보인다. 이 위젯은 클라이언트 쪽 설정만 읽기 때문에 서버에 걸린 지연은 보이지 않는다
5. 단일 프로세스 OFF이면 데디 서버가 새 프로세스로 뜨고, 새 프로세스에는 전역 값이 없어 서버 쪽 지연은 0이다

→ ON: 클라이언트 200 + 서버 200 + α ≈ 450, OFF: 클라이언트 200 + α ≈ 250. 두 경우 α가 약 50으로 같게 나와 계산과 맞는다.

**조치:** 에디터를 재시작하면 전역 값이 사라진다. 재시작 없이 풀려면 PIE 중에 콘솔에서 `NetEmulation.Off`를 입력한다. 값이 0으로 초기화되고 (`NetEmulationHelper.cpp:88-94`), 이후 만들어지는 드라이버도 0을 받는다. **촬영할 때는 콘솔 `NetEmulation.*`를 쓰지 말고 PIE 설정만 쓴다.**

**콘솔 명령의 적용 범위.** 명령 하나가 두 가지 일을 하는데, 둘의 범위가 다르다 (`NetEmulationHelper.cpp:26-37`, `:340-349`).

| 효과 | 범위 | 시점 |
|---|---|---|
| 즉시 적용 | 콘솔을 친 **그 창의 월드**가 가진 NetDriver만 | 바로 |
| 전역 변수에 저장 | 그 **프로세스**에서 이후 새로 만들어지는 **모든** NetDriver | 다음 PIE부터, 에디터를 끌 때까지 |

PIE를 다시 켰을 때 드라이버별로 받는 값은 다음과 같다. 서버와 클라이언트 모두 생성 시 먼저 전역 값을 받는다 (`NetDriver.cpp:731-735`).

| 조건 | 서버 | 클라이언트 |
|---|---|---|
| 단일 프로세스 ON + PIE 에뮬레이션 ON (Clients Only) | **전역 값 그대로** (서버 URL에 옵션 없음) | 접속 URL의 PIE 설정으로 **통째로 교체** (`NetDriver.cpp:1735-1747`). 합쳐지지 않으므로 전역 값에 있던 `PktLag` 같은 항목도 사라진다 |
| 단일 프로세스 ON + PIE 에뮬레이션 OFF | 전역 값 그대로 | 전역 값 그대로 (URL에 옵션이 없어 교체가 일어나지 않음) |
| 단일 프로세스 OFF | 새 프로세스라 전역 값 없음 | 에디터 프로세스 안에서 돌면 전역 값을 받았다가 URL 설정으로 교체 |

값을 0으로 맞추는 것도 전역 변수에 저장된다. 그래서 `NetEmulation.* 0`을 입력한 뒤에는 새 드라이버가 모두 0으로 시작하고, 클라이언트만 PIE 설정을 받는다. 2026-09-27에 이렇게 맞춘 뒤 RTT가 약 250으로 돌아온 것을 확인했다.

**α ≈ 50ms (미검증):** RTT는 보낸 패킷의 ack가 돌아온 시각으로 잰다 (`NetConnection.cpp:3014-3017`). 따라서 상대가 다음 패킷을 보낼 때까지 기다린 시간과 지연 패킷이 프레임 단위로 처리되며 생기는 대기가 함께 들어간다고 추정한다. 따로 측정하지는 않았다. 포트폴리오에는 "설정 왕복 200ms"라고 적고, 측정 RTT는 약 250ms임을 캡션에 밝힌다.

---

## 3. 코드 (사용자 작성) — 지연 표시 한 줄만

`UEPHUDWidget`에 선택형 텍스트 하나를 추가한다. 예측/확정 잔탄 비교는 §0에서 정한 대로 `showdebug AbilitySystem`이 대신하므로, 여기서는 **지연 설정값만** 찍는다. 별도 위젯 클래스는 만들지 않는다 — 소비자가 하나이고 확장점으로 적힌 문서도 없다.

### 3-1. 헤더

```cpp
// EPHUDWidget.h — protected
UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> NetDebugText;

// private
void RefreshNetDebug();
```

`BindWidgetOptional`이면 WBP에 위젯을 두지 않았을 때 아무 일도 하지 않는다. 촬영용 WBP에만 추가하거나, 평소에는 Collapsed로 둔다.

### 3-2. NativeTick에서 갱신

```cpp
// EPHUDWidget.cpp
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerState.h"

void UEPHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// ... 기존 TimerText ...
	RefreshNetDebug();
}

void UEPHUDWidget::RefreshNetDebug()
{
#if !UE_BUILD_SHIPPING
	if (!NetDebugText) return;

	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* PS = PC ? PC->PlayerState : nullptr;
	const float RttMs = PS ? PS->GetPingInMilliseconds() : 0.f;

	FString Emu = TEXT("Emu off");
#if DO_ENABLE_NET_TEST
	if (const UNetDriver* Driver = GetWorld()->GetNetDriver())
	{
		const FPacketSimulationSettings& S = Driver->PacketSimulationSettings;
		Emu = FString::Printf(TEXT("Emu Out %d / In %d"),
			S.PktLag > 0 ? S.PktLag : S.PktLagMax, S.PktIncomingLagMax);
	}
#endif

	NetDebugText->SetText(FText::FromString(FString::Printf(TEXT("RTT %.0fms | %s"), RttMs, *Emu)));
#endif
}
```

**라벨이 영어인 이유:** `.cpp` 안의 `TEXT("한글")`은 소스 파일 인코딩에 따라 깨지거나 C4819 경고가 난다. 한국어 설명은 캡션에 쓴다.

**`PktLag`를 먼저 보는 이유:** 콘솔 `NetEmulation.PktLag 100`으로 고정 지연을 걸면 `PktLag`에 들어가고, 그러면 Min/Max는 무시된다 (`NetDriver.h:528` 주석). 어느 방식으로 걸어도 실제 적용값이 찍힌다.

### 3-3. WBP 배치

- `showdebug AbilitySystem`이 화면 좌측을 채우므로, 이 위젯은 **화면 우측 상단**에 둔다 — 겹치지 않는 자리면 된다. 잔탄 텍스트와 정렬을 맞출 필요는 없어졌다
- 폰트는 1600×900으로 녹화하고 800×450으로 줄였을 때 최소 11px이 되도록 **24pt 이상**
- 반투명 검정 배경(Border, α 0.35)을 깔면 맵 밝기와 관계없이 읽힌다

---

## 4. 촬영 순서

1. PIE 시작 후 콘솔에 `showdebug AbilitySystem` (필요하면 `showdebug Attributes`도)
2. **3초 이상 기다린다.** `ExactPing`은 여러 샘플의 평균이라 (`PlayerState.cpp:83`) 처음에는 낮게 나온다. **약 250**에서 멈출 때까지 기다린다. 400 이상이면 §2-1 상황이다
3. 탄창을 가득 채운 상태에서 **단발로 3번**, 0.5초 간격 → `Ammo` 줄은 즉시, `(Base: ..)`는 약 200ms 뒤에 따라가는 것이 한 발씩 보인다
4. 이어서 **연사 1초** → `Ammo`와 `Base`가 2~3발 차이로 벌어졌다가 연사를 멈추면 같아진다
5. 전체 4~5초. 25fps로 녹화하면 약 7~9MB 안에 들어간다 (지난 GIF 3개는 50fps 기준 11~14MB)

---

## 5. "같은 프레임" 확인

GIF만으로는 같은 프레임인지 증명할 수 없다. 원본 녹화(mp4)를 프레임 단위로 넘기며 확인한다.

- 총구 섬광이 처음 보이는 프레임과 `showdebug AbilitySystem`의 `Ammo` 값이 바뀌는 프레임이 **같아야 한다**
- `Base` 값이 바뀌는 프레임은 60fps 녹화 기준 약 15프레임 뒤여야 한다 (측정 RTT 약 250ms)

다르면 포트폴리오 문장("발사와 같은 프레임")을 고친다. 문장에 맞춰 영상을 고르지 않는다.

### 볼 것 하나 더

서버 확인이 도착하는 순간 `Ammo` 값이 **한 프레임 동안 1 더 줄었다가 돌아오는지** 본다. 속성 복제(BaseValue −1)와 예측 키 ack(모디파이어 제거)가 다른 시점에 도착하면 그 사이에 두 번 뺀 값이 보인다. 보이면 GIF에 넣을지 판단하기 전에 먼저 기록한다. 이번 계획에서 원인을 추정하지 않는다.

---

## 6. 포트폴리오 캡션 예시

> 왕복 200ms 에뮬레이션(측정 RTT 약 250ms). `Ammo`(예측)는 발사 프레임에 줄고, `Base`(서버 확정 값)는 RTT만큼 뒤에 따라온다. 롤백 코드는 없다.
