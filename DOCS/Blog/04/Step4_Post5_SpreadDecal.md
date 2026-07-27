# Post 4-5 작성 가이드 — 탄 분포와 탄흔: CDF 역변환 샘플링 + 데칼

> **예상 제목**: `[UE5] 추출 슈터 4-5. 산탄 분포 설계: CDF 역변환 샘플링과 층화 샘플링, 그리고 탄흔 데칼`
> **참고 문서**: `DOCS/Notes/04/04_GAS_05_Spread.md`, `04_GAS_05_WeaponDecals.md`, `04_GAS_05_WeaponDecals_STATUS.md`

---

## 개요

**이 포스팅에서 다루는 것:**
- 균일 난수 스프레드가 왜 "가운데가 빈 도넛"이 되는가
- 커브 에셋(PDF)을 CDF 룩업 테이블로 바꿔 디자이너가 탄착군을 직접 그리게 하기
- 층화 샘플링으로 각도 편중 잡기
- 탄흔 데칼을 무기 BP가 책임지는 구조
- **데칼을 붙이자 드러난 버그 3개**

**왜 이렇게 구현했는가 (설계 의도):**
- 스프레드는 **손맛의 핵심**인데 코드에 숫자로 박아두면 튜닝이 불가능하다
- 이 편은 GAS와 직접 관련이 없다. 하지만 **"코드에 있던 값을 에셋으로 뺀다"는 시리즈 관통 주제의 가장 순수한 사례**다
- 데칼은 스프레드 검증 도구로 시작했다. **탄착군을 눈으로 봐야 커브를 튜닝할 수 있다**

---

## 구현 전 상태 (Before)

```cpp
// AEPWeapon::Fire — 균일 난수 스프레드
for (int32 i = 0; i < Count; i++)
{
    const float R     = FMath::FRand();          // 반경비율 0~1 균등
    const float Theta = R * HalfAngle;
    const float Phi   = FMath::FRand() * TWO_PI; // 각도 0~2π 균등
    OutPellets.Add(/* 구면 좌표 → 방향 벡터 */);
}
```

**문제점:**
- 탄착군 모양을 바꾸려면 **C++을 고쳐야 한다**
- `R`을 균등하게 뽑으면 실제 원 위에서는 **중심이 비고 바깥에 몰린다** (아래 설명)
- 산탄총에서 펠릿들이 **특정 방향으로 뭉치는** 경우가 자주 나온다

---

## 구현 내용

### 1. ★ 왜 균일 난수가 도넛이 되는가

포스팅에서 그림으로 설명할 핵심 개념.

```
반경 r인 원에서 "고리 하나"의 넓이는 r에 비례해서 커진다.
  r=0.1 근처 고리 넓이  : 작다
  r=0.9 근처 고리 넓이  : 9배 크다

그런데 FRand()는 0.1도 0.9도 같은 확률로 뽑는다.
→ 넓은 바깥 고리에 같은 수의 탄이 떨어지면 밀도는 낮고,
   좁은 중심 고리에 같은 수가 떨어지면 밀도가 높아야 하는데
   눈으로는 "바깥에 많다"로 보인다.
```

정확히는 **면적 균등이 아니라 반경 균등**이라서 시각적 분포가 의도와 다르다. 게임에서 원하는 건 보통 **중심 집중**인데 기본 구현은 그 반대에 가깝다.

**그런데 이걸 "√r 보정"으로 끝내지 않은 이유**: 면적 균등도 정답이 아니다. 무기마다 원하는 탄착군이 다르다 — 소총은 중심 집중, 산탄총은 약간 퍼짐, 특수 무기는 도넛형일 수도 있다. **분포 자체를 데이터로 만들어야 한다.**

### 2. ★ 커브 = PDF, 룩업 테이블 = CDF

디자이너에게 "X축 = 반경비율(0~1), Y축 = 상대 확률"인 커브를 그리게 한다.

| 원하는 분포 | 커브 형태 |
|-------------|-----------|
| **중심 집중 (기본 권장)** | X=0,Y=1.0 → X=1,Y=0 (우하향) |
| 균등 | Y=1 상수 |
| 도넛형 | X=0.3~0.7 구간에서 Y가 높음 |

> Y값의 절댓값은 무관하다 — 정규화하므로 Y=2와 Y=200이 같은 결과를 낸다. **디자이너가 스케일을 신경 쓸 필요가 없다.**

```cpp
// EPWeapon.h
private:
    static constexpr int32 CDFTableSize = 256;
    TArray<float> SpreadCDFTable;
    void  BuildSpreadCDFTable();   // BeginPlay에서 1회
    float SampleSpread() const;    // Fire()에서 매 펠릿
```

**동작 순서:**

```
[BeginPlay]  커브(PDF) → 사다리꼴 적분 → 누적합(CDF) → 정규화 → 256칸 테이블
[Fire()]     균등 난수 U(0~1) → 테이블에서 이진 탐색 → 반경비율 R 반환
```

**역변환 샘플링(Inverse Transform Sampling)** 원리를 그림으로:

```
PDF (커브가 그리는 것)          CDF (적분한 것)
  ↑                              ↑ 1.0
  │╲                             │      ╱───
  │ ╲                            │   ╱
  │  ╲___                        │╱
  └──────→ r                     └──────→ r
                                  ↑
            균등 난수 U를 Y축에서 찍고 ─┘ 대응하는 r을 읽으면
            그 r은 PDF를 따르는 분포가 된다
```

기울기가 가파른 구간(= PDF가 높은 구간)이 Y축을 더 많이 차지하므로 더 자주 뽑힌다. **이게 역변환 샘플링의 전부다.**

**256칸 테이블을 쓰는 이유** — 매 발사마다 적분하면 낭비다. 미리 구워두고 이진 탐색(O(log 256) = 8회 비교)만 한다. 산탄총 10펠릿이어도 무시할 비용.

**커브가 없으면 `FMath::FRand()`로 폴백**한다. 기존 무기 에셋이 그대로 동작하므로 마이그레이션 비용이 0이다.

> **에디터 작업**: Content Browser → Miscellaneous → Curve → CurveFloat → `FC_AK74_Spread`
> `DA_AK74_HitScan` → Weapon|Spread → `SpreadDistributionCurve` 슬롯에 할당
> **스크린샷 위치**: 커브 에디터에서 우하향 곡선을 그린 화면

### 3. ★ 층화 샘플링 — 각도가 뭉치는 문제

CDF는 **반경(R)만** 제어한다. 각도(Phi)는 여전히 독립 난수였다.

```cpp
const float Phi = FMath::FRand() * TWO_PI;   // 펠릿마다 독립
```

**문제**: 산탄총 5발이 우연히 전부 오른쪽 위 사분면에 뽑힐 수 있다. 확률적으로는 정상이지만 **플레이어는 "총이 고장났다"고 느낀다.** 난수의 정당성과 체감은 다르다.

**해결 — 원을 펠릿 수만큼 나누고, 각 펠릿은 자기 섹터 안에서만 뽑는다:**

```cpp
const float SectorSize = TWO_PI / Count;   // 루프 밖

for (int32 i = 0; i < Count; i++)
{
    const float R     = SampleSpread();
    const float Theta = R * HalfAngle;
    const float Phi   = (i * SectorSize) + FMath::FRand() * SectorSize;  // ★

    OutPellets.Add(
        AimDir  * FMath::Cos(Theta)
        + Up    * FMath::Sin(Theta) * FMath::Cos(Phi)
        + Right * FMath::Sin(Theta) * FMath::Sin(Phi));
}
```

```
[독립 난수]                    [층화 샘플링]
    ·  ·                           ·
   ·                          ·         ·
        ·   ·               ·             
  ← 뭉칠 수 있다                 ·     ·
                            ← 반드시 원주 전체에 분산
                              (섹터 안은 여전히 랜덤이라 기계적이지 않다)
```

**섹터 안에서는 여전히 랜덤**이라는 게 중요하다. 완전 균등 배치(`Phi = i * SectorSize`)로 하면 매번 똑같은 별 모양이 나와서 부자연스럽다.

> `PelletCount = 1`(단발)이면 `SectorSize = 2π`이므로 기존 동작과 완전히 동일하다. **단발 무기에 영향이 없다는 걸 확인하고 넣었다.**

### 4. 탄흔 데칼 — 무기가 자기 데칼을 책임진다

```
[서버] HandleHitscanFire
  → ConfirmHitscan (캐릭터 히트 + 환경 히트 모두 수집)
  → 캐릭터만 데미지 적용
  → Multicast_PlayImpactEffect (모든 히트를 배열로 1회)

[클라] PlayLocalImpactEffect
  → ImpactFX (Niagara) + ImpactSFX
  → EquippedWeapon->BP_PlayImpactEffect   ← 무기 BP가 구현
      → Spawn Decal at Location
```

```cpp
// EPWeapon.h
UFUNCTION(BlueprintImplementableEvent)
void BP_PlayImpactEffect(const FVector& ImpactPoint, const FVector& ImpactNormal, uint8 SurfaceType);
```
```cpp
// EPCombatComponent.cpp
void UEPCombatComponent::PlayLocalImpactEffect(const FVector& ImpactPoint, const FVector& ImpactNormal)
{
    const FRotator ImpactRot = ImpactNormal.Rotation();

    if (ImpactFX)  UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactFX, ImpactPoint, ImpactRot);
    if (ImpactSFX) UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSFX, ImpactPoint);

    if (EquippedWeapon)
        EquippedWeapon->BP_PlayImpactEffect(ImpactPoint, ImpactNormal, 0);
}
```

**데칼 머티리얼을 C++ UPROPERTY로 두지 않은 이유**: 무기마다 탄흔 모양·크기·수명이 다르다. `BlueprintImplementableEvent`로 열어두면 **무기 BP에서 노드 몇 개로 끝난다.** C++에 `TObjectPtr<UMaterialInterface> ImpactDecal`을 두면 무기별 차이를 다시 코드로 분기해야 한다.

> **에디터 작업** — `BP_WeaponAK74`에서 `BP_PlayImpactEffect` 오버라이드:
> ```
> Spawn Decal at Location
>   Decal Material : (제작한 탄흔 머티리얼)
>   Decal Size     : (2, 10, 10)
>   Location       : ImpactPoint
>   Rotation       : Conv_VectorToRotator(ImpactNormal)   ← X축이 법선 방향
>   Life Span      : 30.0
> ```
> **스크린샷 위치**: BP_WeaponAK74의 BP_PlayImpactEffect 그래프

**머티리얼 설정** (막히기 쉬운 부분):
- 머티리얼 도메인: **Deferred Decal**
- 블렌드 모드: **DBuffer Translucent Color**
- DBuffer 사용 시 Project Settings → Rendering → **DBuffer Decals** 활성화 필요
- 텍스처는 **알파 채널 있는 탄흔 PNG**. 사각형 텍스처를 그냥 쓰면 벽에 네모가 찍힌다

---

## 겪은 문제

이 편의 실전 파트. **"샷건 10발을 쐈는데 탄흔이 2개만 찍힌다"** 하나의 증상에서 원인이 셋 나왔다.

### 원인 ① — Unreliable Multicast 드롭

`Multicast_PlayImpactEffect`를 히트마다 호출했다. 10발이면 한 프레임에 **10회 Multicast**.

- Multicast RPC는 Unreliable이라 대역폭이 몰리면 **조용히 버려진다**
- **수정**: 시그니처를 배열로 바꿔 **1회 호출**로 전부 전달

```cpp
// Before: void Multicast_PlayImpactEffect(FVector Point, FVector Normal);  ← 10회 호출
// After : void Multicast_PlayImpactEffect(const TArray<FVector>& Points, const TArray<FVector>& Normals);
```

**교훈**: 코스메틱을 Unreliable Multicast로 보내는 건 맞지만, **호출 횟수 자체를 줄여야** 한다. "Unreliable이니까 몇 개 빠져도 된다"와 "10개 중 8개가 빠진다"는 다른 이야기다.

### 원인 ② — `ImpactPoints.Add`가 캐릭터 블록 안에 있었다

```cpp
// HandleHitscanFire
if (HitChar)
{
    /* 데미지 적용 */
    ImpactPoints.Add(Hit.ImpactPoint);   // ← 벽 히트는 수집되지 않는다
}
```

벽을 쏜 히트가 애초에 배열에 안 들어갔다. **`if (HitChar)` 블록 밖으로 이동**.

### 원인 ③ — `ConfirmHitscan`이 환경 히트를 버리고 있었다

가장 근본적인 원인. 3단계에서 SSR을 **캐릭터 판정 전용**으로 설계했기 때문에 벽 히트를 아예 반환하지 않았다.

```cpp
// EPServerSideRewindComponent.cpp — ConfirmHitscan
AEPCharacter* HitChar = Cast<AEPCharacter>(Hit.GetActor());
if (HitChar && CandidateSet.Contains(HitChar))
{
    OutConfirmedHits.Add(Hit);
}
else if (!HitChar)          // ★ 추가 — 환경(벽/바닥) 히트, 이펙트 재생 목적
{
    OutConfirmedHits.Add(Hit);
}
```

`else if (!HitChar)` 조건이 중요하다. **`else`로 뭉뚱그리면 "후보군이 아닌 캐릭터"까지 통과**해버려, 3단계에서 만든 후보 필터가 무력화된다.

**교훈**: 한 시스템의 출력 계약을 나중에 넓힐 때는 **원래 계약이 왜 좁았는지**를 확인해야 한다. `CandidateSet.Contains()` 필터는 리와인드하지 않은 캐릭터에 맞는 걸 막는 3단계의 안전장치였고, 그걸 유지한 채로 환경만 추가해야 했다.

### 부수 버그 — 종료 시 `Set Fade Screen Size` null

- **원인**: 월드 해체 중 `Spawn Decal at Location`이 null을 반환하는데 그 반환값에 노드를 이어붙였다
- **수정**: Return Value에 IsValid 체크 추가 (BP)

---

## 함정 정리

| 상황 | 원인 | 해결 |
|------|------|------|
| 샷건 탄흔이 일부만 찍힘 | 다중 Unreliable Multicast 드롭 | 배열 RPC 1회 호출 |
| 벽 탄흔이 전혀 안 찍힘 | SSR이 환경 히트 미반환 | `else if (!HitChar)` 추가 |
| 후보 아닌 캐릭터에 맞음 | `else`로 뭉뚱그림 | `!HitChar` 조건 명시 |
| 데칼이 안 보임 | 머티리얼 도메인이 Deferred Decal이 아님 | 도메인/블렌드 모드 확인 |
| 데칼이 네모로 찍힘 | 알파 없는 텍스처 | 알파 채널 있는 탄흔 PNG |
| 종료 시 null 에러 | 월드 해체 중 스폰 실패 | Return Value IsValid 체크 |
| 커브를 넣었는데 분포가 그대로 | `BuildSpreadCDFTable()` 미호출 | BeginPlay 확인 |

---

## 결과

**확인 항목:**
- `PelletCount = 5` 이상으로 벽 사격 → 탄흔 데칼로 탄착군 육안 확인
- 중심 집중 커브 → 탄흔이 중심부에 밀집
- 균등 커브 → 이전 `FRand()` 방식과 동일 분포
- 층화 샘플링 적용 후 → 펠릿이 원주 전체에 분산 (한쪽 뭉침 없음)
- `PelletCount = 1` → 기존 동작과 동일

> **스크린샷 위치 (이 편의 핵심 비주얼)**: 벽에 찍힌 탄착군 3종 비교
> ① 균등 난수 ② CDF 중심 집중 ③ CDF + 층화 샘플링

**한계 및 향후 개선:**
- **반동에 따른 스프레드 증가가 없다.** 연사할수록 `HalfAngle`이 커지는 동적 스프레드는 미구현. 현재는 발사마다 같은 분포
- 조준(ADS) 시 스프레드 감소도 아직 없다
- 데칼 개수 제한이 없다 — 장시간 교전 시 데칼이 누적된다. `LifeSpan 30초`로 완화했을 뿐이라 풀링이 필요할 수 있다
- 표면별 데칼 분기 미구현 — `BP_PlayImpactEffect`가 `SurfaceType`을 파라미터로 받지만 현재 항상 0을 넘긴다. **4-6에서 PhysicalMaterial을 태그로 다루기 시작하면 이 값을 채울 근거가 생긴다**

---

## 참고

- `DOCS/Notes/04/04_GAS_05_Spread.md` — CDF 구현 + 층화 샘플링
- `DOCS/Notes/04/04_GAS_05_WeaponDecals.md` — 데칼 아키텍처
- `DOCS/Notes/04/04_GAS_05_WeaponDecals_STATUS.md` — 버그 3종 기록
- Step 3-2 포스팅 — `ConfirmHitscan` 원래 구조 (이 편에서 계약을 넓힌 대상)
