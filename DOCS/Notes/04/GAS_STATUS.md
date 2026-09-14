# GAS 마이그레이션 전체 진행 상황

> 세션 시작 시 이 파일을 먼저 읽을 것.
> 현재 단계 확인 후 해당 단계의 STATUS 파일을 추가로 읽는다.

---

## 진행 상황

- [x] 04_GAS_01 Foundation (ASC + AttributeSet)
- [x] 04_GAS_02 DamagePipeline
- [x] 04_GAS_03 PrimaryUse
- [x] 04_GAS_04 Reload (PIE 검수 완료) — `04_GAS_04_Reload_STATUS.md`
- [x] 04_GAS_05 WeaponDecals (PIE 검수 완료) — `04_GAS_05_WeaponDecals_STATUS.md`
- [x] 04_GAS_05 Spread CDF (PIE 검수 완료) — `04_GAS_05_Spread_STATUS.md`
- [x] 04_GAS_06 HitZoneDamage (PIE 검수 완료) — `04_GAS_06_HitZoneDamage_STATUS.md`
- [x] 04_GAS_07 Skills (Step 0~8 완료, `UEPGA_Skill_Base` 마이그레이션 완료, PIE 검수 완료) — `04_GAS_07_Skills_STATUS.md`
- [x] 04_GAS_08 HUD (4-상태 슬롯 + 중앙 게이지 2종 + `IEPGaugeVisual` 분리 완료, PIE 검수 완료) — `04_GAS_08_HUD_STATUS.md`

**GAS 마이그레이션 완료 (2026-07-26).** 다음 작업은 GAS 밖 — `DOCS/DOCS.md` §5 실행 순서 참조.

---

## 레거시 제거 확인 (2026-07-26 grep 검증 — 전부 제거됨)

| 항목 | 상태 |
|---|---|
| `Server_Fire` / `Server_Reload` RPC | 없음 |
| `AEPCharacter::HP` / `TakeDamage()` | 없음 |
| `AEPWeapon::CurrentAmmo` / `StartReload` / `FinishReload` | 없음 (`UEPWeaponInstance::CurrentAmmo`가 새 위치) |
| `BoneDamageMultiplierMap` | 없음 (HitZone 태그 시스템으로 대체) |
| `EPGA_Item_Reload` authority 가드 누락 | 수정됨 (`EPGA_Item_Reload.cpp:65`) |
| 스태미나 (Stamina / SprintDrain / StaminaRegen / State.Dashing) | 없음 |

---

## 남은 이슈 (GAS 내부 — 전부 경미)

| 위치 | 내용 | 상세 |
|---|---|---|
| `EPGA_Skill_Base.cpp:85` | 피격 중단인데 `bWasCancelled = false` | `04_GAS_07_Skills_STATUS.md` |
| `EPGA_Skill_ShieldOn.h:31` | `ShieldCooldown = 50.f` vs GAME.md 스펙 30초 | `04_GAS_07_Skills_STATUS.md` |
| `EPSkillSlotWidget.cpp:143-149` | 상태 변화 없이도 `ApplyState` 호출 → 불필요한 캐시 리셋 | `04_GAS_08_HUD_STATUS.md` |
| 명명 | `UEPCastGaugeWidget`이 방벽 지속시간에도 쓰여 이름이 부정확 (개명은 CoreRedirects 필요) | `04_GAS_08_HUD_STATUS.md` |

---

## 세션 시작 템플릿

```
@GAS_STATUS.md @04_GAS_0X_XXX_STATUS.md
Step X 진행.
```
