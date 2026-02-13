# Item 시스템 설계 (확정안: Row + Definition + Instance)

> 기준: Epic 학습 경로 패턴을 따른다.
> `FTableRowBase` + `UItemDefinition(UPrimaryDataAsset)` + `UItemInstance`

---

## 1. 아키텍처 원칙

1. `DataTable Row`는 운영/밸런스 수치의 소스다.
2. `ItemDefinition(DataAsset)`은 에셋/클래스 참조의 소스다.
3. `ItemInstance`는 플레이 중 변하는 상태의 소스다.
4. DB는 플레이어 상태만 저장한다. 게임 규칙 데이터는 저장하지 않는다.

---

## 2. 데이터 3계층

### 2-1. Row (운영 데이터)

```cpp
USTRUCT(BlueprintType)
struct FEPItemRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FName ItemId;

    UPROPERTY(EditAnywhere)
    EEPItemType ItemType;

    UPROPERTY(EditAnywhere)
    int32 MaxStack = 1;

    UPROPERTY(EditAnywhere)
    int32 SlotSize = 1;

    UPROPERTY(EditAnywhere)
    int32 SellPrice = 0;

    UPROPERTY(EditAnywhere)
    bool bQuestItem = false;
};
```

용도:
- 가격, 스택, 슬롯, 확률 테이블, 상점 회전, 드랍 그룹 등

### 2-2. Definition (정적 정의 데이터)

```cpp
UCLASS(BlueprintType)
class UEPItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly)
    FName ItemId;

    UPROPERTY(EditDefaultsOnly)
    FDataTableRowHandle ItemRow;

    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UStaticMesh> WorldMesh;

    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UTexture2D> Icon;
};
```

타입별 확장:
- `UEPWeaponDefinition : UEPItemDefinition`
- `UEPConsumableDefinition : UEPItemDefinition`
- `UEPAmmoDefinition : UEPItemDefinition`

용도:
- 메시, 아이콘, 애니, FX/SFX, Ability 클래스 참조

### 2-3. Instance (런타임 상태)

```cpp
UCLASS(BlueprintType)
class UEPItemInstance : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FGuid InstanceId;

    UPROPERTY()
    FName ItemId;

    UPROPERTY()
    int32 Quantity = 1;

    UPROPERTY()
    int32 CurrentAmmo = 0;

    UPROPERTY()
    float Durability = 100.f;

    UPROPERTY()
    int32 SchemaVersion = 1;
};
```

용도:
- 수량, 탄약, 내구도, 부착물, 랜덤 롤 등 상태 저장

---

## 3. 참조 규칙

1. `ItemId`는 Row/Definition/Instance/DB를 연결하는 공통 키다.
2. Row와 Definition은 서버 시작 시 로드 가능해야 한다.
3. Instance는 Row/Definition을 직접 복제하지 않고 `ItemId + 상태`만 가진다.
4. 월드 Actor(예: Weapon)는 표현체이며 상태 원본이 아니다.

---

## 4. 런타임 책임 분리

| 대상 | 책임 |
|------|------|
| Character | 입력 수집, 이동/시점, 컴포넌트 위임 |
| CombatComponent | 장착/발사/재장전 RPC, 전투 이펙트 |
| InventoryComponent | 슬롯/스택/장착 관리 |
| ItemInstance | 상태 보관 |
| GAS Ability | PrimaryUse/SecondaryUse/Reload/Interact 실행 |

---

## 5. DB 통합 원칙

1. 저장 대상:
- 인벤토리 Instance
- 스태시
- 재화/진행도/거래 기록

2. 저장 금지:
- 아이템 정의(메시/FX/클래스 참조)
- 밸런스/확률/가격표

3. 저장 포맷:
- `ItemId + InstanceState + SchemaVersion`

예시:
```json
{
  "instance_id": "6f5d...-...",
  "item_id": "Weapon_AK74",
  "quantity": 1,
  "current_ammo": 17,
  "durability": 92,
  "schema_version": 3
}
```

---

## 6. 구현 순서

1. `FEPItemRow : FTableRowBase` 도입
2. `UEPItemDefinition` 도입
3. 기존 `UEPWeaponData/UEPItemData`를 Definition 체계로 정리
4. `UEPItemInstance` 도입
5. 인벤토리를 Instance 참조 기반으로 전환
6. Character 전투 로직을 Component/Ability로 이관

---

## 7. 완료 기준

- [ ] Row/Definition/Instance 역할이 코드와 문서에서 일치
- [ ] DB가 `ItemId + InstanceState`만 저장
- [ ] 밸런스 변경이 DataTable 수정만으로 가능
- [ ] FX/SFX/Ability 교체가 Definition 수정만으로 가능
- [ ] Character에서 아이템 세부 로직이 제거됨
