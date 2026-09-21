# 04 Polish STATUS: 무기 발사 (발사 간격 · 발사 RPC · 재장전 태그)

> 이 파일이 **결정의 진실의 원천**이다. 설계 문서 `../04_Polish_WeaponFireRate.md`는 이 파일과
> 어긋나면 이 파일이 맞다. 결정이 뒤집힐 때마다 §2에 줄을 추가하고 §1을 고친다 — 지우지 않는다.

**최종 갱신:** 2026-09-21
**구현 상태:** 전부 **미구현.** 설계만 확정. 스킬 쪽(`../SkillDisplay/`) 구현·PIE 검증 뒤 착수.

---

## 1. 현재 확정 설계 (2026-09-21 기준 — **안 B + UT식 원점 동기**, 안 C 폐기)

| 항목 | 결정 | 위치 |
|---|---|---|
| **발사 간격 가변** | **지원한다** (사용자 명시, 2026-09-19 — 키리코 여우길형 발사 속도 버프). 배율 출처: 자기 버프는 `FEPLocalModifiers` **`Modifier.FireRate.*`**(카테고리 신설), 남이 건 버프는 GE/속성(§3-0 규칙, RTT 지연 수용) | `EPNativeGameplayTags`, `FEPLocalModifiers` |
| 페이싱 (클라 오너·호스트) | `FEPLocalTimer FireTimer` — 남은 시간 모델. 매 발 `Start(Now, Interval)` 후 `SetTimer(GetRemaining(Now, Rate), 반복 X)`로 **다음 발을 매번 다시 예약**. 배율 변경은 **다음 발부터** (진행 중 간격은 옛 배율로 끝남, 최대 한 간격 한 번 — `Bank`+재예약 안 함, 09-20). 고정 반복 `SetTimer`는 **폐기** | `AEPWeapon` + `UEPGA_Item_PrimaryUse` |
| **배율 출처** | `유효 발사 속도 = WeaponDef->FireRate × Character.LocalModifiers.Product(Modifier.FireRate)`. **무기 스탯을 바꾸지 않는다** — `WeaponDef`는 공유 DataAsset, 버프는 플레이어에게 걸려 무기 교체를 따라간다. 클라 `Rate`와 서버 버킷 `Refill`이 같은 식 | `AEPWeapon::GetBaseFireRate()` + `EPCharacter.h:164` |
| 클라 연타 게이트 | 같은 `FireTimer`를 `Single`의 `CanActivateAbility`에서 `IsElapsed(Now, Rate, ε)`로 검사 | `AEPWeapon` |
| **단발 연타 예약** | 간격 안의 클릭은 **거절하지 않고 한 칸 예약**(UT `PendingFireSequence` 방식). `Single` 어빌리티는 발사 후 즉시 끝나지 않고 `Interval`까지 살아 있으며, 그 사이 클릭은 `Input_Fire` → `AbilitySpecInputPressed` → `InputPressed`로 `bPendingShot = true` → 타이머 틱에 `FireOnce` 후 종료. **`Auto`/`Single` 한 수명 경로.** 서버 인스턴스는 클라의 `ServerEndAbility`로 끝난다(`bReplicateEndAbility=true`, 지금은 `false`) — 서버가 먼저 끝내면 늦은 예약 발이 죽은 어빌리티 앞으로 온다 (09-20). **`Input_StopFire`는 `CancelAbilities` → `AbilitySpecInputReleased`** — 뗌이 취소면 예약이 안 찬다; 어빌리티 `InputReleased`가 `Auto`만 종료 | `UEPGA_Item_PrimaryUse`, `EPCharacter::Input_Fire` |
| 서버 속도 검증 | **토큰 버킷** `FEPRateLimiter`, `Refill = FireRate × Product(Modifier.FireRate)`(TryTake 시점 배율), `MaxTokens = FireRateBurstAllowance`(설정, 기본 2). 타이머+허용오차 아님. Source의 `sv_maxusrcmdprocessticks`(클라 명령 개수 제한)에 해당 — "클라가 발사 시각을 정하는 설계"의 표준 짝 | `AEPWeapon` (서버) |
| 발사 순번 | **제외.** Reliable TargetData는 정확히 한 번 배달되므로 중복 제거가 필요 없다. Unreliable로 되돌릴 일이 생기면 그때 `FireIndex`를 페이로드에 추가 (계약 변경은 그때 한 번) | — |
| **발사 전송** | **GAS TargetData** — `CallServerSetReplicatedTargetData` → `AbilityTargetDataSetDelegate` → `ConsumeClientReplicatedTargetData`. 커스텀 `FEPTargetData_Fire` | `UEPGA_Item_PrimaryUse` |
| 페이로드 | `{ FVector_NetQuantizeNormal Direction, float ClientMoveTimeStamp }` — **원점 없음, 순번 없음.** 타임스탬프 = 클라가 쏜 순간 마지막으로 완료된 세이브드 무브의 `CurrentTimeStamp`(+4B). 검증 불필요 — 거짓 값으로 얻는 건 히스토리 안의 자기 과거 위치뿐 | `FEPTargetData_Fire` |
| **원점** | **서버**가 **그 타임스탬프의 무브 결과 카메라 위치**에서 (UT `SavedPositions`/`GetDelayedShotPosition` 방식, 2026-09-21 채택). **히스토리의 주인은 `UEPServerSideRewindComponent`** (UT `SavedPositions`처럼 "이 캐릭터의 과거 위치"는 한 곳에 — 되감기 스냅샷과 나란히, 배열은 따로): CMC `OnMovementUpdated`가 **모든 무브 타입**(Old/Pending/New)에서 `OnServerMoveProcessed(서버시간, 위치, 클라타임스탬프, bNewMove)`를 브로드캐스트하고, SSR 컴포넌트가 매 무브 `{ClientTimeStamp, 카메라 위치}`를 링버퍼(`ShotOriginHistoryCount`, 기본 64)에 기록(동기 — PostPhysics로 미루는 건 본 스냅샷뿐), `HandleServerFire`가 페이로드 타임스탬프로 찾는다 → **오차 0** (채널 순서 무관). 히스토리에 없으면(무브가 아직 안 옴 / 4분 타임스탬프 리셋 직후) **현재 카메라 위치로 폴백** = 최대 이동 1프레임분. UT처럼 무브를 기다리지 않는다 — 기다리면 발사 시각이 밀려 SSR 되감기와 어긋난다 | `UEPServerSideRewindComponent::GetShotOriginAt()`, `HandleServerFire()` |
| 방향 | **클라**가 페이로드로 (`GetControlRotation().Vector()`) | 페이로드 |
| 200cm 드리프트 검사 | **삭제** (원점을 안 받으니 검증 대상 없음) | `EPCombatComponent.cpp:67-71` |
| 쿨다운 GE | `GE_FireCooldown`·`CooldownGameplayEffectClass`·`ApplyCooldown` 오버라이드·`CommitAbilityCooldown` **전부 삭제** | |
| 재장전 태그 | `GE_Reloading` → `ActivationOwnedTags.AddTag(State.Reloading)`. `IsNetAuthority()` 가드·핸들 제거 코드 삭제 | `UEPGA_Item_Reload` |
| **어빌리티 배칭** | **사용, 독립 단계.** `Single`/`Burst`: 클릭당 3개 → **2개** (배치[활성화+TargetData] + End. End는 예약 슬롯 때문에 타이머 틱에서 일어나 배치 밖 — "3→1"은 예약 슬롯 전 계산이었음, 09-20 정정). `Auto` N발: N+2 → N+1 | ASC 서브클래스 + `Input_Fire` |
| 배칭 전제 1 | `UEPAbilitySystemComponent : UAbilitySystemComponent` — `ShouldDoServerAbilityRPCBatch() override { return true; }` (기본 false, `ASC.h:1305`). `EPPlayerState`가 이 클래스로 생성 | 신규 |
| 배칭 전제 2 | `EPCharacter::Input_Fire`(`:433-442`, 한 곳)의 `TryActivateAbilitiesByTag`를 **핸들 기반**으로 바꾸고(스펙은 에셋 태그로 찾는다 — `FindAbilitySpecFromClass`는 BP 서브클래스에서 `nullptr`) `FScopedServerAbilityRPCBatcher Batcher(ASC, Handle)` 스코프 안에서 `TryActivateAbility(Handle)` — GASShooter `BatchRPCTryActivateAbility` 패턴. 첫 발의 TargetData 호출은 `ActivateAbility` 안에서 동기적이라 같은 스코프에 들어간다; End는 아니다 | `EPCharacter.cpp` |
| 롤백 | **안 함** — 거절돼도 잃는 건 총구 이펙트뿐. 스킬의 `NewRejectedDelegate` 훅 안 씀 | |
| 탄약 예측 | 안 함 (범위 밖, 후속 후보) | |
| RPC 플러딩 방어 | 범위 밖. `HasTokenAvailable(Now)` 피크 / `bRPCDoSDetection=true` 후속 | 설계 문서 §4-8 |

### 왜 이 조합인가 — 한 줄씩

- **버킷:** 간격 0.1s 무기에 허용오차 타이머는 검사가 무의미하거나 지속 초과를 허용. 버킷은 순간 몰림 허용 + 지속 속도 정확히 상한.
- **TargetData 전송:** 예측 키로 서버가 활성화와 자동 매칭(먼저 온 TargetData도 `AbilityTargetDataMap`에 캐시). 무기와 향후 스킬 타게팅이 **한 파이프라인**. Lyra와 같은 모양.
- **원점 서버 + 타임스탬프 동기:** 클라가 원점을 보내면 "얼마나 검증하나"가 영원히 남는다(200cm 구멍). 서버가 계산하되 **어느 무브의 위치인지**를 페이로드가 알려주면 채널 순서와 무관하게 클라가 쏜 순간의 위치가 나온다. 페이로드 4B + 서버 링버퍼 하나로 안 B의 유일한 대가(원점 ±1프레임)를 지운다. 폴백이 있어 최악이 안 B와 같다.
- **방향 클라:** 클라가 `FireOnce` 순간 읽은 벡터를 그대로 보내므로 **오차 0**. 서버 컨트롤러에서 꺼내는 방식(안 C)은 서버 타이머와 `ServerMove` 도착이 독립 사건이라 **항상 ±1프레임**(60fps 16ms — 트래킹 100°/s면 1.6°, 30m에서 84cm). 주류 FPS(Source/Valorant)가 명령마다 시점 각도를 동봉하는 것과 같은 모양. 조작해도 얻는 게 없는 값이라 검증 불필요.
- **안 B가 정석인 이유 (2026-09-20):** 발사 명령 + 그때의 각도가 한 패킷 = usercmd 모델. 안 C(UT식 상태 동기화)는 각도를 다른 패킷에서 가져와 경쟁 조건이 생긴다. 안 B의 대가는 발당 RPC 1개와 버킷이고, 그 둘은 이 모델의 표준 구성이다.
- **배칭:** 전송을 GAS TargetData로 바꾸는 순간 `Single`이 클릭당 Reliable RPC 3개가 된다 — 배칭이 그걸 2개로 줄인다(활성화+TargetData 묶음, End는 예약 슬롯 탓에 따로 — §2 09-20). 커스텀 Unreliable RPC일 때 "무의미"했던 판단은 전송 방식이 바뀌면서 뒤집혔다(§2).

---

## 2. 결정 이력 (정정 포함 — 지우지 않는다)

| 날짜 | 결정 / 정정 | 근거 |
|---|---|---|
| 09-05 | 문제 규명: `FireMode::Single`이 쿨다운 GE에 막힘. `ForceCooldown`은 적용만 강제 | `GameplayAbility.cpp:611-629` |
| 09-05 | 안 A: `float LastFireTime` + `GetServerWorldTimeSeconds()` + `IntervalEpsilon`, 서버도 같은 검사 + `PredictionFudgeSeconds` | (당시) |
| 09-18 | 안 A 갱신: `FEPLocalTimer`(스킬과 공용), **시계는 `World->GetTimeSeconds()`** — 값이 기계를 안 건너감. "SSR과 같은 시계"는 **틀렸음** | Lyra `LyraWeaponInstance.cpp:58` |
| 09-18 | 서버 검증 = **토큰 버킷**. 타이머+허용오차는 비율 문제·패킷 몰림 문제 | 설계 문서 §4-3 |
| 09-18 | 재장전 태그 → `ActivationOwnedTags` (스킬 문서 §3-0에서 인계) | `GameplayAbility.cpp:983, :870` |
| 09-19 | 사용자 추가: §4-7(지금 §4-8) RPC 플러딩 방어(범위 밖). `HasTokenAvailable`은 **리필 계산 필수** 정정, `bRPCDoSDetection` 기본 false 확인 | `RPCDoSDetection.h:184, :382`, `BaseEngine.ini:1865` |
| 09-19 | 비교: 배칭은 발마다 활성화하는 구조의 최적화라 우리 구조엔 무의미. Lyra·GASShooter는 클라 히트를 검증 없이 믿음(`bIsTargetDataValid = true` 하드코딩) | `LyraGameplayAbility_RangedWeapon.cpp` |
| 09-19 | **UT식 채택**: 원점·방향 모두 서버가 계산, 페이로드는 핸들+`FireIndex` | `UTWeapon.cpp:443-465`, `Pawn.cpp:341`, CMC `:9861/:9907/:1739-1754` |
| 09-19 | **정정:** "서버 원점은 U만큼 과거라 피커가 벽에 막힌다(60cm)"는 **틀렸음**. 서버는 이동 패킷 사이에 자기 캐릭터를 안 움직이고, 발사는 프레임의 회전·이동 적용 전에 일어나므로 순서만 맞으면 정확. 남는 건 번들링 1프레임 | `PlayerController.cpp PlayerTick`, CMC `:1739-1754` |
| 09-19 | **정정:** "한 프레임 오차가 생긴다"도 처음 설명이 틀렸음 — 정상 프레임엔 오차 0, `PendingMove` 번들링 때만 1프레임 | 위와 동일 |
| 09-19 | **전송을 GAS TargetData로** (사용자 제안). 그에 따라 **방향은 다시 클라 페이로드로** — ASC(PlayerState) 채널과 캐릭터 채널 간 순서 보장 없음. 원점은 서버 유지 | `AbilitySystemComponent.h:1570`, `EPPlayerState.cpp:14` |
| 09-19 | **정정:** "Reliable RPC가 유실되면 이동이 막힌다"는 **캐릭터 채널 RPC**(지금 `Server_ConfirmFire`)에만 해당. TargetData RPC는 PlayerState 채널이라 해당 없음 | 채널 = 액터 단위 |
| 09-19 | **UT 발사 페이싱 확인 → 안 C 후보 (미결, §4).** UT는 발마다 RPC가 없다: 클라 `StartFire`/`StopFire`만 보내고 **서버가 자기 `RefireCheckTimer`로 매 발을 직접 쏜다**(`UTWeaponStateFiring.cpp BeginState/RefireCheckTimer`). 속도 제한 = 서버 타이머 자체 — 버킷·순번·TargetData 전부 불필요. 반자동 연타는 거절이 아니라 **큐**(`PendingFireSequence`, 다음 틱에 발사). 방향은 서버 컨트롤러(±1프레임), 종료 지터로 발수 N±1 가능 | `UTWeaponStateFiring.cpp`, `UTWeapon.cpp:2206 HandleContinuedFiring` |
| 09-19 | **발사 간격 가변 지원 확정** (사용자 명시). `FEPLocalTimer`가 무기 페이싱으로 **복귀** — 직전에 "무기에선 뺀다"고 한 건 간격 상수 전제였음. 역할 분리: 타이머 = 다음 발 언제(페이싱), 버킷 = 이 발 허락하나(검증, 안 B만). UT엔 버킷이 없다 — 서버 타이머가 페이싱하니 검증할 게 없고, `PendingFireSequence`(1슬롯 큐)는 연타 흡수용이지 속도 제한이 아님 | `UTWeaponStateFiring.cpp` |
| 09-20 | `FireIndex` **제외** — Reliable 전송에서 중복이 생길 수 없어 죽은 필드. Unreliable 복귀 시 재도입 | `AbilitySystemComponent.h:1570` |
| 09-20 | **안 B 확정, 안 C 폐기.** 정정: 안 C를 밀며 "방향 오차는 번들링 때만"이라 한 건 **틀렸음** — 단발 첫 발에만 맞고, 연사에선 서버 타이머 vs `ServerMove` 도착의 경쟁으로 항상 ±1프레임. 안 B는 방향 오차 0. 조준 정확도가 슈터의 핵심이라 B. 안 C의 장점(연사 RPC 2개, 연타 큐)은 각각 "대역폭상 무시 가능", "클라 예약 한 칸으로 B에서도 가능"으로 흡수 | 위 §1 "왜 이 조합인가" |
| 09-20 | **정정 (UT 사실):** "UT 방향은 서버 컨트롤러라 ±1프레임"은 **데디 서버 지속 발엔 틀렸음.** 클라 세이브드 무브에 `bShotSpawned`(`FLAG_Custom_3`, `WillSpawnShot`으로 "이 프레임에 발사" 예측)를 싣고, 서버는 타이머 틱에 그 무브가 안 왔으면 최대 `MaxShotSynchDelay` 0.2s 발사를 미뤘다가 그 무브의 저장 위치·회전으로 쏜다. 즉 UT는 §5의 "발사를 `FSavedMove`에 싣기"를 연사에 실제로 쓴다. 첫 발만 동기화 없음(`BeginState` `bNetDelayedShot=false`). 안 B는 CMC 결합 없이 오차 0이라 결정 불변 | `UTWeaponStateFiring.cpp:72-93`, `UTCharMovementReplication.cpp:94, :596`, `UTCharacter.cpp:328-397` |
| 09-19 | **어빌리티 배칭 채택** + **정정:** "배칭은 우리 구조에 무의미"는 커스텀 Unreliable RPC 전제였음. TargetData 전송에선 `Single` 3→1이라 유효. `CallServerSetReplicatedTargetData`가 배치 중이면 RPC 대신 `ExistingBatchData->TargetData`에 담는다 | `AbilitySystemComponent_Abilities.cpp:4095-4174`, `CallServerSetReplicatedTargetData` 배치 분기 |
| 09-20 | **정정:** 배칭 "`Single` 3→1"과 "예약 슬롯(`Interval`까지 생존)"이 양립 불가 — End가 타이머 틱에서 일어나 배치 밖. 선택지: 예약을 어빌리티에(2 RPC, `Auto`/`Single` 한 수명 경로) vs 무기에(1 RPC, 부품 하나 더). **어빌리티에, 2 RPC** — RPC 1개 차이는 대역폭상 무시 가능으로 이미 결론, 코드가 적다. 부수: `Single`도 `bReplicateEndAbility=true`(서버 인스턴스는 클라가 끝낸다) | `EPGA_Item_PrimaryUse.cpp:52`, `AbilitySystemComponent_Abilities.cpp:2879 AbilitySpecInputPressed` |
| 09-20 | 배율 변경은 **다음 발부터** — `Bank`+재예약 안 함. 진행 중 간격 한 번(≤ Interval) 늦는 건 체감 불가, 서버 버킷은 `TryTake` 시점 배율이라 종료 시 초과 1발은 `MaxTokens` 안 | 설계 문서 §4-7 대가 |
| 09-21 | **UT식 원점 동기 채택** (사용자 결정 — "안 B의 원점 ±1프레임은 추가 오차다"). 페이로드에 `ClientMoveTimeStamp` 추가, 서버가 무브마다 카메라 위치를 타임스탬프로 기록, `HandleServerFire`가 조회. 히스토리 위치: 처음 CMC → **SSR 컴포넌트로 이동**(같은 날) — UT처럼 "과거 위치"의 주인을 하나로; CMC는 델리게이트 인자만 늘어난다. 저장 시점은 둘 다 `OnMovementUpdated` 동기라 정확도 차이 없음. 미스 시 현재 위치 폴백. 세이브드 무브에 발사 비트를 싣는 것(Source식)은 여전히 안 한다 — 무브는 그대로, 키만 페이로드에 | `CharacterMovementComponent.cpp:9900`(`CurrentClientTimeStamp` 설정) → `:9924 MoveAutonomous` → `OnMovementUpdated`; 클라 `:8743 UpdateTimeStampAndDeltaTime`, `:12548 SetMoveFor TimeStamp`; `UTCharacter.cpp:365-397` |
| 09-20 | 발사 속도 버프는 **무기 스탯이 아니라 캐릭터 `LocalModifiers`** — `WeaponDef` 공유 DataAsset, 버프는 플레이어 소속(무기 교체를 따라감). 무기 고유 영구 보정(부착물 등)은 문서에 없어 층을 만들지 않는다 | CLAUDE.md §2 |

---

## 3. 검증된 사실 (근거가 있는 것만 — 새 결정은 여기서 출발)

- UT 발사 RPC: `ServerStartFire(FireModeNum, FireEventIndex, bClientFired)` `Server, unreliable, WithValidation` + 앱 레벨 재전송 2회(`QueueResendFire`, 0.04s). 서버 원점 `GetFireStartLoc()`=`GetPawnViewLocation()`, 방향 `GetViewRotation()`=컨트롤러 회전. 리와인드 `0.0005 × Clamp(ExactPing − Fudge, 0, MaxPredictionPing)` — 타깃만 되감고 쏘는 사람은 현재값. `UTWeapon.cpp`, `UTPlayerController.cpp:285`.
- 클라 프레임 순서: `TickPlayerInput` → `UpdateRotation` → CMC 틱(`ServerMove` 큐). `PlayerController.cpp PlayerTick`.
- 서버는 원격 오토노머스 프록시를 이동 패킷 사이에 움직이지 않는다. `CharacterMovementComponent.cpp:1739-1754`.
- `ServerMove`가 서버 컨트롤러 회전을 채운다. `CharacterMovementComponent.cpp:9861, :9907`.
- 같은 액터 채널의 RPC는 보낸 순서로 처리. 채널이 다르면 보장 없음. ASC는 PlayerState(`EPPlayerState.cpp:14`), CMC·CombatComponent는 캐릭터.
- `ServerSetReplicatedTargetData`는 Reliable. `AbilitySystemComponent.h:1570`.
- GASShooter/Lyra는 `FHitResult` 통째(히트당 50~80B, 샷건 ~500B README:2472)를 보내고 서버가 검증 없이 믿는다. 엔진 `bHitReplaced`/`ReplaceHitWith` 훅은 Lyra가 안 쓴다.
- 클라 `FNetworkPredictionData_Client_Character::CurrentTimeStamp`는 CMC 틱(`ReplicateMoveToServer`)에서 `+= DeltaTime` 후 그 프레임 무브의 `TimeStamp`가 된다(`:8743`, `:12548`). 발사는 CMC 틱 **전**(`TickPlayerInput`)이므로 발사 순간의 값 = 직전 프레임 무브의 타임스탬프 = 그때 위치를 만든 무브. 4분마다 리셋(`MinTimeBetweenTimeStampResets`, `:803`).
- 서버 `ServerMove_PerformMovement`: `ServerData->CurrentClientTimeStamp = ClientTimeStamp`(`:9900`) → `MoveAutonomous`(`:9924`) → `PerformMovement` → `OnMovementUpdated`. 그 안에서 `GetCurrentNetworkMoveData()->TimeStamp`가 방금 적용한 무브의 타임스탬프. 번들(`ServerMoveDual`)은 Old/New 각각 이 경로를 탄다 — 히스토리는 `NetworkMoveType` 전부 기록해야 한다(SSR 훅은 `NewMove`만).
- `FRPCDoSDetection`: 기본 꺼짐, `[GameNetDriver RPCDoSDetection] bRPCDoSDetection=true`로 켬. `BaseEngine.ini:1865`.

---

## 4. 미결 / 후속

- [x] 설계 문서 `../04_Polish_WeaponFireRate.md` §4를 안 B로 동기화 (2026-09-20 — §4-2 구조, §4-4 코드, §4-5 배칭, §4-7 대가, §6 PIE 11행)
- [x] 구현서 `04_Polish_WeaponFireRate_Implementation.md` 작성 (2026-09-20). Step 1~12 TargetData 전송 → PIE → Step 13 배칭(독립). 구현 시 충돌하면 구현서 → 이 STATUS → 설계 문서 순
- [ ] `UEPAbilitySystemComponent` 신설은 PlayerState 생성 코드가 바뀌는 횡단 변경 — GAS_STATUS·PROJECT_CONTEXT 갱신 대상
- [ ] 스킬 PIE 검증 완료 후 착수
- [ ] 후속 후보(순서 없음): 탄약 예측 / `FireIndex` 재전송 큐(유실 저항) / `HasTokenAvailable` 피크 / `bRPCDoSDetection` / `FireMode::Burst` 카운터 / `CalculateSpread` 디버그 로그·`ApplySpread` 죽은 코드 제거 / `Issue/FireRate_GECooldownPrediction.md` "Ability Batching" 오기 정정 / `DOCS/Mine/LagCompensationFix.md`·포트폴리오의 "클라가 원점·방향을 보낸다" 서술 갱신

---

## 5. 다시 열지 않는 것

- 발마다 어빌리티 활성화(GASShooter 구조) — 연사당 한 번 활성화하는 지금 구조를 유지한다. (배칭 자체는 **채택** — §1. 열지 않는 건 "발마다 활성화"뿐.)
- 클라 히트 결과 전송(Lyra/GASShooter) — SSR이 서버 트레이스를 하므로 클라 히트를 받을 이유가 없고, 받으면 검증 문제만 생긴다.
- 발사를 `FSavedMove`에 싣기(Valve식 — UT도 데디 서버 연사 지속 발에 `FLAG_Custom_3 bShotSpawned`로 쓴다, §2 09-20) — GAS 활성화·커밋·예측 키가 다른 채널에 있어 서버에서 "무브는 왔는데 어빌리티는 아직"이 생긴다. 09-21의 타임스탬프 동기가 무브를 건드리지 않고 같은 결과(원점 오차 0)를 얻는다.
- 원점을 클라가 보내고 검증하기(궤적 검증 등) — 안 받는 쪽이 단순하고 정확. 타임스탬프 동기로 정확도 논거도 사라졌다.
- **안 C (UT식 서버 타이머 페이싱)** — 방향 ±1프레임이 구조적으로 남는다. 연사 RPC 절약은 대역폭상 의미 없고, 연타 큐는 안 B에 예약 한 칸으로 옮겼다. 다시 열려면 "방향 오차를 감수할 이유"가 있어야 한다.
