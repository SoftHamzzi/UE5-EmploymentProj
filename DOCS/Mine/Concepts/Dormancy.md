# UE5 Network Dormancy (네트워크 휴면)

> 엔진 소스 기준: UE 5.7
> 관련 사용처: `AEPPickup`(`DOCS/Notes/05/05_Loot_01_Spawner.md` 01-4)

---

## 1. 무엇을 푸는 문제인가

서버는 복제 틱마다 이중 루프를 돈다.

```
for (모든 연결)          // 20명
    for (모든 복제 액터)  // 픽업 200개 + 캐릭터 + …
        이 연결에 보낼 게 있나?
```

바닥에 놓인 픽업 200개 × 20명 = **틱마다 4000번 검사.** 그런데 픽업은 스폰된 뒤 값이 하나도 안 변한다. 4000번 전부 "바뀐 거 없음"이라는 답만 나온다.

휴면은 이 낭비를 없앤다.

> **"이 액터는 저 연결에 다 보냈다. 내가 깨우기 전까지 루프에서 빼라."**

**액터를 끄는 기능이 아니다.** 액터는 서버·클라 양쪽에 그대로 살아 있고, 서버의 *검사 대상 목록*에서만 빠진다.

---

## 2. 휴면은 액터가 아니라 **(액터, 연결) 쌍**의 상태다

"액터가 잔다"가 아니다. 액터마다 **누구에게 잠들었는지 목록**을 들고 있다.

```cpp
// NetworkObjectList.cpp:219-223  FNetworkObjectList::MarkDormant
if (!NetworkObjectInfo->DormantConnections.Contains(Connection))
{
    check(ActiveNetworkObjects.Contains(Actor));
    NetworkObjectInfo->DormantConnections.Add(Connection);
```

- A 플레이어에게는 잠들고 B 플레이어에게는 안 잠들 수 있다
- **모든** 연결에 잠들면 그때 활성 목록에서 통째로 빠진다

```cpp
// NetworkObjectList.cpp:263-265
if (NetworkObjectInfo->DormantConnections.Num() == NumConnections)
{
    ObjectsDormantOnAllConnections.Add(*NetworkObjectInfoPtr);
```

**비용이 진짜 0이 되는 지점이 여기다.** 그 전까지는 연결당 `continue` 한 번씩은 돈다.

---

## 2-1. 전제 — 채널은 액터의 속성이 아니다

휴면이 (액터, 연결) 쌍의 단위인 이유는 **채널이 그 단위이기 때문이다.** `UActorChannel`은 액터가 아니라 `UNetConnection`이 들고 있다.

```cpp
// NetConnection.h:772
FActorChannelMap ActorChannels;      // UNetConnection의 멤버

// NetConnection.h:721-724
UActorChannel* FindActorChannelRef(const TWeakObjectPtr<AActor>& Actor)
{
    return ActorChannels.FindRef(Actor);
}
```

클라 20명이면 픽업 하나에 대해 서버가 **최대 20개의 서로 다른 채널**을 가진다. 그래서 §3·§5의 `!Channel`은 *"서버가 **이 연결에 대해** 아직 채널을 안 열었다"*는 뜻이지 액터 전체의 상태가 아니다.

채널은 양쪽 끝에 하나씩 생긴다. 서버가 먼저 열고, 클라는 **첫 번지가 도착했을 때 반응적으로** 같은 인덱스로 만든다.

```cpp
// NetConnection.cpp:4031  UNetConnection::ReceivedPacket
// Reliable (either open or later), so create new channel.
Channel = CreateChannelByName( Bunch.ChName, EChannelCreateFlags::None, Bunch.ChIndex );
```

**클라가 자기 판단으로 채널을 여는 일은 없다.** 클라는 `ServerReplicateActors`를 돌리지 않으므로 `ShouldActorGoDormant`도 실행하지 않는다.

### ★ 액터 존재와 채널 존재는 별개다

이 문서 전체를 푸는 열쇠다.

| 시점 | 서버 채널 | 클라 채널 | 클라에 **액터**가 있나 |
|---|---|---|---|
| 스폰 직후 | 없음 (아직 안 열었다) | 없음 (존재를 모른다) | ✗ |
| 릴러번시 통과, 1회 복제 | 있음 | 있음 | ✓ |
| **휴면으로 닫힘** | **없음** | **없음** | **✓ 유지** |
| `Destroy()` | — | — | ✗ |

세 번째 줄 — **채널이 양쪽 다 없는데 클라에는 액터가 있다.** §4의 `ClientSetActorDormant`가 만든 상태다.

그리고 이 분리가 §6의 `DORM_Initial`이 성립하는 이유 자체다.

| | 채널 | 클라 액터 | 어떻게 |
|---|---|---|---|
| 레벨 배치 + `DORM_Initial` | **한 번도 없음** | ✓ | 레벨 로드로 이미 갖고 있다 |
| 동적 스폰 + 휴면 | 한 번 열렸다 닫힘 | ✓ | 채널로 받은 뒤 유지 판정 |
| 동적 스폰 + 아직 멂 | 한 번도 없음 | ✗ | 받은 적이 없다 |

**동적 스폰 액터는 첫 줄이 불가능하다** — 클라가 액터를 얻을 경로가 채널밖에 없기 때문이다. 그래서 §3의 `!Channel` 가드에 걸려 반드시 한 번은 보내게 되고, 결과적으로 §6의 결론이 나온다.

---

## 3. `NetDormancy`는 "자고 있다"가 아니라 **"잘 의향이 있다"**

가장 흔한 오해. 이 변수는 현재 상태가 아니라 **정책**이다.

```cpp
// EngineTypes.h:3359-3372
DORM_Never            // 절대 안 잔다
DORM_Awake            // 잘 수 있지만 지금은 안 잔다. 게임 코드가 재울 것이다
DORM_DormantAll       // 모든 연결에 대해 자고 싶다
DORM_DormantPartial   // 연결마다 GetNetDormancy()로 물어봐라
DORM_Initial          // 맵에 배치된 액터라면 처음부터 자고 있다
```

실제 판정은 여기서 난다.

```cpp
// NetDriver.cpp:5314-5319
static bool ShouldActorGoDormant(AActor* Actor, ..., UActorChannel* Channel, ...)
{
    if (Actor->NetDormancy <= DORM_Awake || !Channel || Channel->bPendingDormancy || Channel->Dormant)
        return false;
```

**`!Channel`을 주목한다.** 채널이 없으면 — 즉 **클라가 이 액터를 아직 모르면 — 재울 수 없다.** 재우려면 먼저 보내야 한다. 이 한 줄이 §6의 결론을 만든다.

`DORM_DormantPartial`은 연결마다 `AActor::GetNetDormancy()`를 호출하므로 오버라이드가 필요하고, 하나라도 false면 전체가 안 잔다(`:5322-5330`).

---

## 4. ★ 채널이 닫히는 이유가 둘이고, 클라 결과가 **정반대**다

**이 절이 dormancy의 핵심이다.** 채널이 닫혔다는 사실만으로는 아무것도 결정되지 않는다. 클라의 `UActorChannel::CleanUp`이 **닫힌 이유**를 보고 액터를 살릴지 죽일지 가른다.

```cpp
// DataChannel.cpp:2691-2698  UActorChannel::CleanUp
else if (Dormant && (CloseReason == EChannelCloseReason::Dormancy) && !Actor->GetTearOff())
{
    Connection->Driver->ClientSetActorDormant(Actor);   // ★ 액터를 살려둔다
    Connection->Driver->NotifyActorFullyDormantForConnection(Actor, Connection);
    bWasDormant = true;
}
else if (...)
{
    // Destroy the actor                                 ← 그 외의 이유면 클라에서 파괴
```

| 닫힌 이유 | 클라 결과 |
|---|---|
| `EChannelCloseReason::Relevancy` (멀어져서) | **액터 파괴.** 다시 가까이 가면 채널 재생성 + 초기 번들 재전송 |
| `EChannelCloseReason::Dormancy` (다 보내서) | **액터 유지.** 채널만 닫히고 화면엔 그대로 |

같은 코드가 destruction info 수신 경로에도 있다(`:2317-2321`).

**"휴면 액터는 멀어져도 클라에서 안 사라진다"는 여기서 나온다.** 서버 루프에서 스킵되기 때문이 아니다 — §5의 함정 참고.

정지해 있고 상태가 안 변하는 액터(바닥 아이템, 문, 상자)는 릴러번시 경로로 두면 **거리마다 사라졌다 나타나며 그때마다 초기 번들을 다시 받는다.** 휴면은 정확히 그걸 없앤다.

---

## 5. 서버 루프에서의 위치

```cpp
// NetDriver.cpp:5386-5438  ServerReplicateActors_PrioritizeActors (요약)
UActorChannel* Channel = Connection->FindActorChannelRef(ActorInfo->WeakActor);

if (!Channel)                                          // ① 채널이 없을 때만
{
    if (!IsLevelInitializedForActor(...)) continue;
    if (!IsActorRelevantToConnection(...)) continue;   //    릴러번시를 본다
}

if (Actor->bOnlyRelevantToOwner) { ... }
else if (GSetNetDormancyEnabled != 0)
{
    if (IsActorDormant(ActorInfo, WeakConnection)) continue;          // ② 자면 스킵
    if (ShouldActorGoDormant(...)) Channel->StartBecomingDormant();   // ③ 재울 준비
}
```

**①이 `if (!Channel)` 안쪽이라는 게 중요하다.** 채널이 이미 열려 있으면 릴러번시를 아예 안 본다. 즉 `NetCullDistanceSquared`는 **아직 채널이 없는 액터에만** 작동한다.

`StartBecomingDormant()`는 즉시 재우지 않고 표시만 한다. 남은 프로퍼티를 다 보낸 뒤에 닫힌다.

```cpp
// DataChannel.cpp:4577-4578
Dormant = true;
Close(EChannelCloseReason::Dormancy);
```

> **★ 함정 — "휴면 액터는 릴러번시 검사에 도달하지 않는다"는 틀렸다.**
> 휴면으로 채널이 닫히면 `FindActorChannelRef`가 null을 돌려주므로 다음 패스에서 **①에 들어간다.** 멀리 있으면 거기서 `continue`되고, 가까우면 ②에서 `continue`된다. 어느 쪽이든 아무것도 안 보내지만 **경로가 다르다.**
> 클라에서 액터가 유지되는 이유는 서버 루프 위치가 아니라 **§4의 `CleanUp` 분기**다.

---

## 6. `DORM_Initial` — 맵 배치 액터 전용

### `DORM_Initial`이 실제로 주는 것은 딱 하나다

"더 빨리 잔다"가 아니라 **"네트워크 오브젝트 리스트에 아예 안 들어간다"**이다.

```cpp
// NetworkObjectList.cpp:55
if (IsValid(Actor) && ULevel::IsNetActor(Actor) && !UNetDriver::IsDormInitialStartupActor(Actor))
{
    FindOrAdd(Actor, NetDriver);      // ← 여기 안 들어온다
}
```
```cpp
// NetDriver.cpp:5177-5186
if (IsDormInitialStartupActor(Actor))
{
    NumInitiallyDormant++;
    ActorsToRemove.Add(Actor);        // ← 고려 목록에서 제거
    continue;
}
```

**"채널 열고 → 보내고 → 재운다"를 통째로 건너뛴다.** 가능한 이유는 하나뿐이다 — **클라가 레벨을 로드하면서 그 액터를 이미 갖고 있어서 보낼 게 없기 때문이다.**

### 그 힘은 게이트 뒤에 있다

```cpp
// NetDriver.cpp:8347
bool UNetDriver::IsDormInitialStartupActor(AActor* Actor)
{
    return Actor && Actor->IsNetStartupActor() && (Actor->NetDormancy == DORM_Initial);
    //               ^^^^^^^^^^^^^^^^^^^^^^^^
}
// EngineTypes.h:3370 — "This actor is initially dormant for all connection if it was placed in map."
```

`IsNetStartupActor()`는 `bNetStartup` 플래그이고 **레벨 로드 시에만** 켜진다(`Level.cpp:3660`). `SpawnActor`는 안 켜준다.

### 동적 스폰 액터에서 `DORM_Initial` == `DORM_DormantAll`

**"동작이 비슷하다"가 아니라 "생성자에서 둘 중 뭘 써도 관측되는 차이가 없다"는 뜻이다.**

게이트를 통과하지 못하므로 특별 취급을 하나도 못 받고, 그러면 남는 판정은 §3의 이것뿐이다.

```cpp
if (Actor->NetDormancy <= DORM_Awake || !Channel || ...) return false;
```

`DORM_Initial`(4)도 `DORM_DormantAll`(2)도 똑같이 통과한다. **그 이후 코드 경로에서 두 값을 구분하는 곳이 없다.**

| | 레벨 배치 액터 | **동적 스폰 액터** |
|---|---|---|
| `DORM_DormantAll` | 리스트 등록 → 채널 → 1회 복제 → 휴면 | 리스트 등록 → 채널 → 1회 복제 → 휴면 |
| `DORM_Initial` | **리스트에 안 들어감. 채널 없음. 0바이트** | 리스트 등록 → 채널 → 1회 복제 → 휴면 |

오른쪽 열 두 칸이 같다.

> **그래도 `DORM_Initial`을 쓰는 이유**는 의도 표현("초기 1회 뒤 잔다")과, 나중에 일부를 레벨 배치로 바꿨을 때 코드 수정 없이 최적화를 얻기 위해서다. **성능 차이를 기대해서가 아니다.**

---

## 6-1. ★ 서버와 클라의 `NetDormancy`는 값이 다르다

엔진이 **일부러** 다르게 만든다. 디버그 출력으로 두 창을 비교하다 보면 버그로 오인하기 쉽다.

### `NetDormancy`는 복제되지 않는다

바로 위 `Role`과 대조하면 명확하다.

```cpp
// Actor.h:826-832
UPROPERTY(Replicated, VisibleInstanceOnly, Category=Networking)
TEnumAsByte<enum ENetRole> Role;              // ← Replicated 있음

UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category=Replication)
TEnumAsByte<enum ENetDormancy> NetDormancy;   // ← 없음
```

클라의 값은 복제로 온 게 아니라 **CDO(생성자) 기본값**이다.

### 엔진이 클라 값을 강제로 덮어쓴다

§4의 그 함수가 액터를 살려두는 것과 **동시에** 이 대입을 한다.

```cpp
// NetDriver.cpp:6888-6896
void UNetDriver::ClientSetActorDormant(AActor* Actor)
{
    const bool bIsServer = IsServer();

    if (Actor && !bIsServer)
    {
        ENetDormancy OldDormancy = Actor->NetDormancy;
        Actor->NetDormancy = DORM_DormantAll;   // ★ 서버 값과 무관하게
```

| 시점 | 서버 | 클라 |
|---|---|---|
| 클라 액터 생성 직후 | `DORM_Initial` | `DORM_Initial` (복제가 아니라 생성자 값) |
| 휴면으로 채널 닫힘 | `DORM_Initial` (그대로) | **`DORM_DormantAll`** |

**서버는 끝까지 `DORM_Initial`이고 클라만 바뀐다.**

### 그래도 상관없는 이유

**휴면은 전적으로 서버 결정이다.** 클라는 `ServerReplicateActors`를 돌리지 않으므로 자기 `NetDormancy`를 판정에 쓰지 않는다. 클라의 값은 사실상 *"이 액터엔 업데이트가 안 올 것"*이라는 메모다. `FlushNetDormancy()`도 클라에서는 no-op이다(`Actor.cpp:3077`).

> **리슨 서버는 예외적으로 안 바뀐다.** `!bIsServer` 가드 때문에 호스트 창에서는 `ClientSetActorDormant`가 안 불린다. 같은 픽업이 **호스트 창에서 `DORM_Initial`, 클라 창에서 `DORM_DormantAll`**로 보인다.

**결론: 클라에서 `NetDormancy`를 읽고 게임플레이 판단을 하지 마라.** 서버와 값이 다르고, 언제 바뀌는지가 채널 생명주기에 달려 있다.

---

## 7. 깨우기 — 유일한 위험

자는 동안 서버가 값을 바꿔도 **클라에 안 간다.** 루프에서 빠져 있기 때문이다.

```cpp
Quantity = 3;
FlushNetDormancy();   // 없으면 서버만 3, 클라는 계속 옛날 값
```

증상이 "서버는 정상인데 클라만 옛날 값"이라 **복제 문제가 아니라 로직 버그처럼 보인다.** 휴면을 쓰기로 한 액터에서 가장 비싼 실수다.

```cpp
// Actor.cpp:3075-3093
void AActor::FlushNetDormancy()
{
    if (IsNetMode(NM_Client) || NetDormancy <= DORM_Awake || IsPendingKillPending()) return;

    bool bWasDormInitial = false;
    if (NetDormancy == DORM_Initial)
    {
        NetDormancy = DORM_DormantAll;   // ★ 되돌아가지 않는다
        bWasDormInitial = true;
    }
```

- **클라에서 부르면 no-op다** (`:3077`)
- **`DORM_Initial`은 한 번 flush되면 영구히 `DORM_DormantAll`이 된다.** 되돌리려면 직접 대입해야 한다
- 완전히 깨워두려면 `SetNetDormancy(DORM_Awake)`

### 설계로 위험을 없애는 법

**복제되는 값이 스폰 시점에 확정되고 이후 안 바뀌면 `FlushNetDormancy()`를 부를 자리가 아예 없다.** `AEPPickup`이 `ItemId` 하나만 복제하도록 좁힌 것이 이 효과를 낸다 — 스택 수량(`Quantity`)이 있었다면 부분 획득마다 flush가 필요했다.

**휴면을 안전하게 쓰는 가장 좋은 방법은 flush 지점을 만들지 않는 설계다.**

---

## 8. `Destroy()`는 별도 경로로 나간다

채널이 닫혀 있는데 파괴를 어떻게 알리나.

```cpp
// NetDriver.cpp:4336-4356 주석
// "Make a new destruction info if necessary. It is necessary if the actor is dormant or
//  recently dormant because even though the client knew about the actor at some point,
//  it doesn't have a channel to handle destruction."
```

| 연결 상태 | 처리 |
|---|---|
| 채널 있음 | `Channel->Close()` |
| 채널 없음 + 휴면/최근 휴면 | **destruction info**를 따로 생성해 전송 |
| 채널이 한 번도 없었음 (계속 멀리 있었음) | 아무것도 안 보냄 — 클라가 모르므로 무해 |

**휴면이라 파괴가 누락되는 일은 없다.** 엔진이 그 경우를 따로 처리한다.

---

## 9. 선택 기준

| 액터 | 권장 | 이유 |
|---|---|---|
| 캐릭터, 발사체 | `DORM_Never` | 매 틱 변한다. 재우면 flush 비용만 생긴다 |
| 바닥 픽업, 시체 | `DORM_Initial` / `DORM_DormantAll` | 스폰 시 확정 후 불변. 개수가 많다 |
| 문, 스위치, 컨테이너 (맵 배치) | `DORM_Initial` | 클라가 레벨에서 이미 로드했다. 상태 변경 시 flush |
| 연결마다 다르게 굴려야 하는 것 | `DORM_DormantPartial` | `GetNetDormancy()` 오버라이드 필요 |

### 판단 기준 세 줄

1. **값이 자주 변하나?** → 변하면 재우지 마라. flush가 이득을 먹는다
2. **개수가 많나?** → 적으면 재워도 티가 안 난다
3. **flush 지점을 셀 수 있나?** → 못 세면 언젠가 하나를 빠뜨린다

---

## 10. 함정 정리

| # | 함정 | 증상 |
|---|---|---|
| 1 | 값을 바꾸고 `FlushNetDormancy()`를 안 부름 | 서버만 정상, 클라는 옛날 값. **로직 버그로 오인** |
| 2 | `DORM_Initial`이면 동적 스폰 액터도 처음부터 잔다고 생각 | `IsNetStartupActor()` 조건 때문에 안 걸린다 (§6) |
| 3 | 휴면 액터가 멀어지면 클라에서 사라질 거라 기대 | 안 사라진다. 그게 정상이고 대부분 원하는 동작 (§4) |
| 4 | 클라 코드에서 `FlushNetDormancy()` 호출 | no-op. 조용히 아무 일도 안 일어난다 (`Actor.cpp:3077`) |
| 5 | `DORM_Initial` 액터를 flush한 뒤 다시 `DORM_Initial`일 거라 가정 | `DORM_DormantAll`로 바뀌어 있다 (`Actor.cpp:3092`) |
| 6 | 클라에서 액터가 유지되는 이유를 서버 루프 위치로 설명 | 틀린 근거. 실제 이유는 `CleanUp`의 close reason 분기 (§5 함정) |
| 7 | 클라에서 `NetDormancy`를 읽고 게임플레이 판단 | **복제되지 않는 값이고 엔진이 클라 쪽만 `DORM_DormantAll`로 덮어쓴다** (§6-1) |
| 8 | 두 창의 `NetDormancy` 출력이 다른 것을 버그로 오인 | 정상이다. 리슨서버 호스트 창은 서버 값 그대로, 클라 창만 바뀐다 (§6-1) |

---

## 참고

### 엔진 소스

- `NetDriver.cpp` — `ServerReplicateActors_PrioritizeActors`(5386~), `ShouldActorGoDormant`(5314), `IsDormInitialStartupActor`(8347), `NotifyActorDestroyed`(4336)
- `DataChannel.cpp` — `UActorChannel::CleanUp`(2691), `StartBecomingDormant`(4577)
- `NetworkObjectList.cpp` — `MarkDormant`(205), `MarkActiveInternal`(299), 리스트 등록 게이트(55)
- `NetDriver.cpp` — `ClientSetActorDormant`(6888), 고려 목록 제외(5177)
- `NetConnection.h` / `.cpp` — `ActorChannels`(772), `FindActorChannelRef`(721), 클라 채널 생성(4031)
- `Actor.cpp` — `FlushNetDormancy`(3075), `IsNetStartupActor`(742)
- `Actor.h` — `NetDormancy` 선언(832) — `Replicated` 지정자가 없다
- `EngineTypes.h` — `ENetDormancy`(3359)

### 외부 자료

- [언리얼 멀티플레이 이해하기 — MoOrY (velog, 2023-11)](https://velog.io/@seok9403/%EC%96%B8%EB%A6%AC%EC%96%BC-%EB%A9%80%ED%8B%B0%ED%94%8C%EB%A0%88%EC%9D%B4-%EC%9D%B4%ED%95%B4%ED%95%98%EA%B8%B0)
  — `UNetDriver` / `UNetConnection` / `UChannel` 구조, **연관성(Relevancy)의 기본 규칙 6가지**, `NetUpdateFrequency`·`NetPriority`, RPC, `GetLifetimeReplicatedProps`, `ENetRole`을 한 편에 정리한 글.
  **이 문서가 전제하는 배경이 거기 있다.** 특히 §4·§5를 읽으려면 "릴러번시가 무엇이고 `NetCullDistance`가 어떻게 쓰이는가"를 먼저 알아야 하는데 그 부분이 잘 설명돼 있다.
  단 **휴면(dormancy)은 다루지 않는다** — 그래서 겹치지 않고 이어진다. 액터 채널도 언급만 하고 깊이 들어가지 않으므로 §4의 close reason 분기는 여기서만 볼 수 있다.
