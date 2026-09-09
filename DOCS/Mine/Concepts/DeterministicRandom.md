# 결정론적 난수 — 클라와 서버가 값을 주고받지 않고 같은 난수를 얻는 법

> **검증:** 2026-09-09, UE 5.7 소스 직독 (`C:\Program Files\Epic Games\UE_5.7\Engine`).
> 인용은 전부 파일:줄로 대응된다.

## 왜 필요한가 — 그리고 대개는 필요하지 않다

탄 분산처럼 난수가 섞이는 것을 **클라가 미리 그리려면** 클라도 같은 난수를 알아야 한다.
그런데 여기 피할 수 없는 긴장이 있다:

> **클라가 미리 그리려면 미리 알아야 하고, 미리 알면 핵도 미리 안다.**

시드를 클라에 주는 순간 "다음 탄이 어디로 튀는지"를 계산할 수 있고, 그건 반동 제어 핵의
재료가 된다. 그래서 실무 판단은 "어떻게 공유할까"가 아니라 **"공유 안 해도 되게 만들 수
있나"**부터 시작한다.

### 판단 두 단계

**1. 틀려도 티가 나는가?**
- **안 난다** → 클라 로컬 난수로 대충 그린다. 시드 공유도 검증도 필요 없다.
  예광탄이 대표적이다. 50ms면 사라져서 서버 값과 대조할 방법이 없다
- **난다** → 2번으로

**2. 시드가 발사 *전에* 클라에 도달할 수 있는가?**
- **안 된다** → 예측을 포기하고 서버 결과를 기다린다.
  어설프게 예측했다가 서버 값과 달라 튀는 게 더 나쁘다
- **된다** → 아래 시드 공유 방식

> **핵심:** "틀려도 되는 것"과 "맞아야 하는 것"을 나누고, **틀려도 되는 것만 예측한다.**
> 벽에 남는 탄흔 데칼은 맞아야 하고, 순식간에 사라지는 예광탄은 틀려도 된다.

---

## 재료 1 — `FRandomStream`

시드를 주면 항상 같은 난수열이 나오는 결정론적 RNG. `Math/RandomStream.h`.

```cpp
struct FRandomStream
{
    FRandomStream(int32 InSeed);
    float FRand() const;                                    // [0, 1)
    int32 RandHelper(int32 A) const;                        // [0, A)
    int32 RandRange(int32 Min, int32 Max) const;            // [Min, Max]
    FVector VRandCone(FVector const& Dir, float HalfAngleRad) const;
    void Reset() const;                                     // 초기 시드로 되감기
};
```

`FMath::FRand()`와 결정적으로 다른 점은 **상태를 자기가 들고 있다**는 것이다. 전역 난수는
누가 언제 몇 번 뽑았는지에 따라 결과가 달라지지만, 스트림은 자기 `Seed` 멤버만 굴린다.

```cpp
FRandomStream A(1234), B(1234);
A.FRand() == B.FRand();   // 항상 참. 서로 다른 머신에서도
```

**주의 — 헤더가 직접 경고한다**(`RandomStream.h:17`):
> *Very bad quality in the lower bits. Don't use the modulus (%) operator.*

`% N`으로 범위를 만들지 말 것. `RandHelper`/`RandRange`는 이 함정을 피해 만들어져 있다 —
나머지 연산 대신 **0~1 실수를 뽑아 곱하고 자른다**(`:187`, 상위 비트를 쓴다):

```cpp
UE_FORCEINLINE_HINT int32 RandHelper( int32 A ) const
{
    // GetFraction guarantees a result in the [0,1) range.
    return ((A > 0) ? FMath::TruncToInt(GetFraction() * float(A)) : 0);
}

inline int32 RandRange( int32 Min, int32 Max ) const
{
    const int32 Range = (Max - Min) + 1;
    return Min + RandHelper(Range);
}
```

## 재료 2 — `HashCombine`

두 정수를 섞어 새 정수 하나를 만든다. `Templates/TypeHash.h:36`.

```cpp
[[nodiscard]] inline constexpr uint32 HashCombine(uint32 A, uint32 C)
{
    uint32 B = 0x9e3779b9;
    A += B;
    A -= B; A -= C; A ^= (C>>13);
    ...   // 비트를 뒤섞는 연산 9줄
    return C;
}
```

**왜 그냥 더하면 안 되나** — `MasterSeed + ShotIndex`는 연속된 시드가 되고, SRand 계열은
인접 시드에서 비슷한 초반값을 낸다. `HashCombine`은 1만 차이 나도 완전히 흩어진 값을 준다.

`constexpr`이고 **부동소수점이 안 섞인 순수 정수 연산**이라 플랫폼이 달라도 같은 입력이면
같은 출력이 나온다. 결정론이 필요한 이 용도에 중요한 성질이다.

---

## 구조 — 시드는 유도하고, 결과는 각자 계산한다

```
Seed = HashCombine(ServerMasterSeed, ShotIndex)
```

| 값 | 누가 정하나 | 어떻게 전달되나 |
|---|---|---|
| `ServerMasterSeed` | **서버** | 장착·재장전 등 **발사 경로 밖에서 미리** 한 번 |
| `ShotIndex` | 각자 자기 카운터 | **전달 안 함.** 양쪽이 각자 센다 |
| `Seed` | 유도값 | **전달 안 함.** 양쪽이 각자 계산 |

```
서버: Seed = HashCombine(MasterSeed, MyShotCounter) → FRandomStream(Seed) → 분산
클라: Seed = HashCombine(MasterSeed, MyShotCounter) → FRandomStream(Seed) → 분산
```

**둘이 시드도 분산도 주고받지 않는다.** 각자 계산하는데 재료가 같아서 결과가 같다.
네트워크로 오가는 건 `MasterSeed` 하나뿐이다.

### 시드 배송 타이밍이 관건이다

"발사 직후 서버가 시드를 내려주는" 식이면 도착에 RTT가 걸려서 **예측이라는 목적 자체가
사라진다.** 시드는 발사 순간에 이미 클라 손에 있어야 한다 — 그래서 장착·재장전 같은
**발사 경로 밖 시점**에 미리 배송한다.

---

## 치트 관점 — 판단 기준은 "누가 씨앗을 고르는가"

**막아지는 것:** 클라가 자기 `ShotIndex`를 조작해도 **서버는 서버 카운터로 자기 시드를
만들므로** 실제 판정이 안 바뀐다. 클라 화면의 예광탄만 엉뚱해지고 손해는 자기가 본다.

**절대 하면 안 되는 것 — 클라가 시드/시각을 보내고 서버가 그걸로 계산:**
브루트포스로 "정중앙에 몰리는 값"을 찾아 보내면 **무반동 핵**이 된다. 방향이 반대여야 한다.

> **한 줄 기준:** 난수의 씨앗을 **서버가 고르면 안전**하고, **클라가 고를 여지가 있으면 뚫린다.**

### 보안 유비 — HMAC / TOTP와 같은 형태, 다만 보장은 없다

```
HMAC:  MAC  = HMAC(SecretKey,  Message)
TOTP:  OTP  = HMAC(SharedSecret, 현재시간/30초)
여기:  Seed = HashCombine(MasterSeed, ShotIndex)
```

**비밀값 하나 + 공개 카운터 하나 → 매번 다른 결과**, 그리고 **비밀은 미리 한 번만 공유하고
그 뒤로는 아무것도 주고받지 않는다**는 점까지 같다. TOTP가 인증할 때마다 서버에 묻지 않는
것과, 시드를 발사 경로 밖에서 미리 배송하는 것이 같은 이유다.

**다만 결정적으로 다르다.** 암호학적 HMAC은 결과를 잔뜩 봐도 비밀키를 역산할 수 없게
설계돼 있지만, `HashCombine`은 그런 보장이 **전혀 없다.** 그냥 비트를 섞는 함수다.

그래서 이 구조가 막는 것은 **"클라가 원하는 결과를 고르는 것"**뿐이고,
**"클라가 미리 아는 것"은 못 막는다.** 애초에 클라가 예측하려면 `MasterSeed`를 받아야
하므로 이미 손에 있다.

정보 유출을 줄이려면 마스터 시드를 자주 갱신(재장전마다 등)하거나, 애초에 **첫 탄 분산을
0으로 두고 지속 사격에서만 퍼지게** 설계하는 쪽이 흔하다. 그 대가가 아깝다면 예측을
포기하고 위 판단 1단계로 돌아가는 것이 맞다.

---

## 이 프로젝트의 현재 상태 (2026-09-09)

**지금은 시드 공유가 필요 없다.** 분산은 서버 전용이고 치트 표면이 이미 없다:

- `AEPWeapon::Fire()`가 `if (!HasAuthority()) return;`로 막혀 있다(`EPWeapon.cpp:65`).
  `SampleSpread()`와 `FMath::FRand()`(`:86`, `:89`)는 그 가드 아래라 클라에서 도달 못 한다
- 클라가 보내는 것은 `Server_ConfirmFire(Origin, Direction, AbilityHandle)`가 전부다.
  **난수도, 분산이 적용된 방향도 없다**(`EPGA_Item_PrimaryUse.cpp:129`)
- 클라가 로컬로 하는 건 `PlayLocalMuzzleEffect()` — 총구 화염과 소리뿐이다
- 탄착은 서버가 `Multicast_PlayImpactEffect(ImpactPoints, ImpactNormals)`로 배열째 내려준다
  (`EPCombatComponent.cpp:346`) → **벽에 남는 건 항상 정확하다**

즉 늦게 보이는 것은 궤적이 아니라 **탄착 이펙트**(RTT만큼)이고, 그것도 히트스캔이라 크게
티나지 않는다. **체감 문제가 확인되기 전엔 예광탄 로컬 난수(판단 1단계)로 충분하다.**

발사체 무기로 확장하거나 탄도를 오래 보여줘야 해질 때 위 시드 구조를 꺼내면 된다.

---

## 인용 색인

| 주장 | 위치 |
|---|---|
| `FRandomStream` 정의 | `Core/Public/Math/RandomStream.h:19` |
| 하위 비트 품질 경고 | 같은 파일 `:17` |
| `RandHelper` / `RandRange` | 같은 파일 `:185-202` |
| `HashCombine` | `Core/Public/Templates/TypeHash.h:36` |
| `Fire()`의 서버 가드 | `EPWeapon.cpp:65` |
| 클라가 보내는 값 | `EPGA_Item_PrimaryUse.cpp:129` |
| 탄착 배열 방송 | `EPCombatComponent.cpp:342-346` |
