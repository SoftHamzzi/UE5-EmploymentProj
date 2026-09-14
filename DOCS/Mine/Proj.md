# 투사체 Hit Registration 설계 가이드

히트스캔(`EPServerSideRewindComponent`)에서 다루지 못한 **물리 투사체의 히트 렉 문제**를 다룬다.

---

## 1. 왜 투사체 Hit Reg이 히트스캔보다 어려운가

히트스캔은 발사 순간 = 판정 순간이라 **리와인드 1회**로 끝난다.

```
발사 → ClientFireTime으로 타겟 리와인드 → LineTrace → 판정 끝
```

투사체는 시간이 지나면서 여러 문제가 겹친다.

| 문제 | 설명 |
|---|---|
| 스폰 위치 지연 | 서버가 RPC를 받은 시점은 이미 RTT/2만큼 늦음 |
| 비행 중 타겟 이동 | 투사체가 날아가는 동안 타겟도 계속 움직임 |
| 클라이언트 화면과 서버 불일치 | 클라이언트가 본 타겟 위치는 서버보다 RTT만큼 과거 |
| RTT 이전 충돌 케이스 | 투사체 속도가 빠르면 RPC 도착 전에 이미 맞았어야 하는 상황 |
| 리드샷 vs 래그 보상 충돌 | 충돌 시점 리와인드 시 리드샷 게임플레이 붕괴 |

---

## 2. 현업 게임별 처리 방식

### Minecraft (래그 보상 없음)
가장 단순한 구현. 래그 보상이 전혀 없다.

```
클라이언트 발사 요청 → 서버 현재 시간 기준으로 화살 스폰
→ 매 틱(20틱/s) 현재→다음 위치 LineTrace → 충돌 시 데미지
```

- 타겟 리와인드 없음, 스폰 위치 보정 없음
- 서버 현재 틱 기준 타겟 위치로만 판정
- 고핑 환경에서 클라이언트 화면에 맞아도 서버에서 빗나가는 현상 그대로 발생
- PvP에서 활이 "핑 게임"이 되는 이유

### Apex Legends (서버 권위 + 매 틱 리와인드)
Respawn 공식 블로그 + Nicola Geretti 역공학 분석 기준.

**발사 시점 처리:**
```
Server_Fire 수신 (ClientFireTime 포함)
→ 투사체를 ClientFireTime 기준으로 RTT/2만큼 앞 위치에서 스폰
→ 이후 서버가 정방향 물리 시뮬레이션
```

**비행 중 처리 (매 투사체 틱):**
```
투사체 이동
→ 타겟을 "클라이언트 발사 시점에 봤던 위치"로 리와인드
→ 충돌 체크
→ 타겟 복원
```

**충돌 시점 리와인드를 쓰지 않는 이유:**
리드샷(선행 조준) 게임플레이가 붕괴된다.
도망치는 적 앞을 겨냥해서 쐈는데, 충돌 시점에 리와인드하면
그 위치에 적이 없었던 것으로 판정될 수 있다.

**rewindTime 계산 (Geretti 분석):**
```
rewindTime = inputQueueDelay + interpolationBackTime + clientLatency - clientFiringDelay
```
최대값 캡이 있어 극단적 고핑 플레이어의 무제한 리와인드를 막는다.

### MechWarrior Online (비이터레이티브 선 트레이스 근사)
빠른 투사체 + 높은 RTT 케이스(RTT 이전에 이미 맞았어야 하는 상황)를 가장 투명하게 공개한 사례.

매 틱 시뮬레이션 대신 **리와인드 구간 전체를 단일 LineTrace로 근사**한다.

```
리와인드 구간 (Rewind Period):
  투사체를 점, 궤적을 직선으로 가정
  타겟을 리와인드된 위치로 고정
  투사체 속도 → 타겟 기준 상대속도로 변환
  → 단일 LineTrace로 리와인드 구간 전체 커버

히트 시점 계산:
  충돌 위치 비율(hit time) 계산
  → 수정된 ping으로 타겟 리와인드 → 최종 확인 판정

동기화 구간 (Synchronized Period):
  리와인드 구간 이후 정방향 물리 시뮬레이션
```

메크처럼 히트박스가 크고 투사체가 직선 탄도인 경우에 허용 가능한 근사.

---

## 3. Predicted Projectile Path 방식

클라이언트는 **발사 시점, 위치, 방향만 전송**하고 서버가 무기 정보를 기반으로 경로를 직접 계산하는 방식.
타겟 리와인드 없이 서버 현재 상태 기준으로만 판정한다.

### 처리 흐름

```
1. 클라이언트: 발사 시점(ClientFireTime) + 위치 + 방향 전송

2. 서버: 무기 스펙(속도, 중력, 탄도)으로 predicted path 계산
   → ClientFireTime 기준으로 현재까지 날아갔어야 할 구간(리와인드 구간)을
     LineTrace 하나로 즉시 체크

3. 리와인드 구간에서 충돌 없으면:
   → 투사체를 현재 위치에 스폰, 정방향 시뮬레이션 시작

4. 이후 타겟 CMC 업데이트가 서버에 도달할 때마다:
   → 서버의 현재 투사체 위치와 타겟 현재 위치로 충돌 체크
   → 타겟 리와인드 불필요 (투사체와 타겟 모두 서버 현재 상태 기준)
```

### 타겟 리와인드가 필요 없는 이유

투사체는 서버에서 정방향으로 날아가고 있고, 타겟도 서버에서 CMC로 실시간 움직이고 있다.
두 객체 모두 서버 현재 시간 기준이므로 현재 위치끼리 충돌 체크하면 충분하다.

타겟 리와인드가 필요한 건 히트스캔처럼 **"발사 순간 클라이언트 화면 기준"** 으로 판정해야 할 때다.
투사체는 실제로 날아가서 맞는 구조이므로 서버 현재 상태 기준 판정이 올바르다.

### 장점

- 매 틱 리와인드 불필요 → 연산 비용 낮음
- 리와인드 구간은 LineTrace 1번으로 처리 (MWO 방식과 동일)
- 클라이언트가 경로를 보내지 않으므로 치팅 방지에 유리
- CMC 업데이트 시점에만 체크하면 되므로 구현 단순

### 한계

- 투사체가 타겟 사이를 빠르게 지나쳐도 CMC 업데이트 사이에 놓치는 경우 발생 가능
  → 투사체 틱을 충분히 빠르게 설정하거나 Sweep으로 보완 필요

---

## 4. 타임스탬프 방식의 한계 (snapnet.dev 분석)

`ServerNow - 평균핑`으로 타임스탬프를 계산하는 기존 방식의 문제:

**패킷 지터:**
평균은 맞을 수 있어도 매 발사마다 거의 항상 틀림.
마지막으로 받은 서버 업데이트가 어느 서버 시간에 해당하는지 알 수 없음.

**클라이언트 보간/보외삽:**
원격 캐릭터는 서버 데이터를 그대로 표시하지 않고 보간하거나 앞으로 예측해서 표시한다.
결과적으로 클라이언트 화면에 보이는 위치가 **서버에 실제로 존재했던 적 없는 위치**일 수 있다.
이 경우 어떤 타임스탬프로 리와인드해도 정확히 일치하는 스냅샷이 없다.

**테스트 데이터 (일반 네트워크 환경):**
| 발사 | 리와인드 적용 오차 | 리와인드 없음 오차 |
|---|---|---|
| 1 | 14.56 units | 93.49 units |
| 2 | 22.97 units | 48.84 units |
| 3 | 1.73 units | 88.00 units |

오차에 상한이 없다는 것이 핵심 문제.

**이 프로젝트의 개선:**
`CMC::OnMovementUpdated` 델리게이트 + `PostPhysics` pending 패턴으로
실제 서버 물리 업데이트 시점에 스냅샷을 저장하여 보간 문제를 완화.
(자세한 내용: `memory/project_ssr_fix.md`)

---

## 5. 넷코드 아키텍처와 투사체

| 아키텍처 | 투사체 처리 | 대표 게임 |
|---|---|---|
| Lockstep | 모든 클라이언트 동일 시뮬레이션 | RTS |
| Rollback | 예측 + 불일치 시 롤백 | 격투게임 |
| Snapshot Interpolation | 서버 권위 + 클라이언트 코스메틱 + backwards reconciliation | Apex, CoD, CS |
| Tribes | 부분 업데이트 + 보외삽 | MMO, 대규모 배틀로얄 |

경쟁 FPS에서 투사체는 **Snapshot Interpolation + backwards reconciliation** 조합이 표준.

---

## 6. 이 프로젝트 현재 상태

| 항목 | 상태 | 비고 |
|---|---|---|
| 히트스캔 SSR | ✅ 완료 | `UEPServerSideRewindComponent` |
| 투사체 스폰 위치 보정 | ❌ 미구현 | `ClientFireTime` 기반 RTT/2 보정 필요 |
| 투사체 비행 중 타겟 리와인드 | ❌ 미구현 | 매 틱 SSR 히스토리 보간 필요 |
| 코스메틱 투사체 (클라이언트) | ✅ 완료 | `SetCosmeticOnly()` |
| 코스메틱 투사체 (다른 클라이언트) | ✅ 완료 | `Multicast_SpawnCosmeticProjectile` |

투사체 Hit Reg 완성을 위해 필요한 작업 (Predicted Projectile Path 방식 기준):
1. `HandleProjectileFire`에서 `ClientFireTime` 기반으로 리와인드 구간 LineTrace 체크
2. 충돌 없으면 현재 위치에 투사체 스폰 후 정방향 시뮬레이션
3. `AEPProjectile::OnProjectileHit`에서 서버 현재 상태 기준으로 데미지 판정 (타겟 리와인드 불필요)

---

## 참고 자료

- [What Makes Apex Tick (Respawn/EA 공식)](https://www.ea.com/games/apex-legends/news/servers-netcode-developer-deep-dive)
- [Netcode Series Part 4: Projectiles — Nicola Geretti](https://medium.com/@geretti/netcode-series-part-4-projectiles-96427ac53633)
- [Lag Compensating Weapons in MechWarrior Online — GameDeveloper.com](https://www.gamedeveloper.com/programming/why-making-multiplayer-games-is-hard-lag-compensating-weapons-in-mechwarrior-online)
- [Performing Lag Compensation in UE5 — snapnet.dev](https://snapnet.dev/blog/performing-lag-compensation-in-unreal-engine-5/)
- [Netcode Architectures Part 3: Snapshot Interpolation — snapnet.dev](https://snapnet.dev/blog/netcode-architectures-part-3-snapshot-interpolation/)
