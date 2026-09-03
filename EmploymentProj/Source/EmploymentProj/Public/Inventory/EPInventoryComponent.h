#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Inventory/EPInventoryTypes.h"
#include "EPInventoryComponent.generated.h"

class AEPPickup;
class UEPItemDefinitionSubsystem;
class UEPCombatComponent;

// 인벤토리 엔트리 배열. FastArray라 바뀐 항목만 델타로 나간다.
USTRUCT()
struct FEPInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	// 모든 엔트리. ParentEntryId로 트리를 이룬다 — 배열 순서에는 의미가 없다(표시 순서는 SortKey).
	UPROPERTY() TArray<FEPInventoryEntry> Items;

	// 이 배열을 든 컴포넌트. 복제 수신 콜백에서 OnInventoryChanged를 쏘려고 든다.
	// 복제하지 않는다 — 생성자의 Entries.Owner = this 가 채운다. 빠뜨리면 클라 UI가 영원히 갱신 안 됨.
	UPROPERTY(NotReplicated) TObjectPtr<UEPInventoryComponent> Owner;

	// DeltaParms : 엔진이 넘기는 델타 직렬화 문맥
	// 반환 : 엔진 규약 — 델타를 썼으면 true
	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms);
	
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters&);
};

// FEPInventoryList가 커스텀 델타 직렬화를 쓴다고 엔진에 알린다. 없으면 배열이 통째로 복제된다.
template<>
struct TStructOpsTypeTraits<FEPInventoryList> : public TStructOpsTypeTraitsBase2<FEPInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true,
   };
};

// 서버 권한 인벤토리. 소유 클라에만 복제된다(COND_OwnerOnly).
// 아이템은 UObject가 아니라 값(FEPInventoryEntry)이고, 배낭·부착물은 ParentEntryId로 표현한다.
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class EMPLOYMENTPROJ_API UEPInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// === 변수 ===
	// === 함수 ===
	UEPInventoryComponent();

	// --- 삽입 ---

	// 컨테이너에 아이템 하나를 넣는다. bFungible이면 기존 엔트리의 Charges에 합산한다.
	//   Container : 넣을 컨테이너의 EntryId. INDEX_NONE이면 본체
	//   ItemId    : DT_Items의 행 이름
	//   InState   : 잔탄·내구도 등 런타임 상태. 값으로 복사된다
	// 반환 : 새 EntryId (합산됐으면 기존 EntryId). 실패는 INDEX_NONE
	//   ★ if(AddItem(...)) 로 검사하지 말 것 — INDEX_NONE(-1)은 truthy다
	int32 AddItem(int32 Container, FName ItemId, const FEPItemState& InState);

	// 서브트리를 통째로 넣는다. EntryId를 전부 새로 발급하고 부모 관계를 재매핑한다.
	// 배낭을 버렸다 되줍는 경로가 이것이다 — AddItem으로 받으면 내용물이 증발한다.
	//   Parent : 목적지 부모의 EntryId. INDEX_NONE이면 본체
	//   SlotId : 루트를 꽂을 슬롯. NAME_None이면 수납(CanFit을 본다),
	//            슬롯 이름이면 CanPlaceInSlot을 보고 CanFit은 건너뛴다(착용은 칸을 안 먹는다)
	//   In     : 전위 순회 배열. In[0]이 루트이고 Parent=INDEX_NONE / SlotId=None / SortKey=0
	//            으로 정규화돼 있어야 한다 — RemoveEntry가 그 모양을 만든다
	// 반환 : 루트의 새 EntryId. 실패는 INDEX_NONE
	int32 AddSubtree(int32 Parent, FName SlotId, const TArray<FEPInventoryEntry>& In);

	// --- 조회 ---

	// 엔트리를 '값으로 복사해' 돌려준다. 포인터를 내보내면 배열 재할당 시 댕글링한다.
	//   EntryId : 찾을 엔트리
	//   Out     : 찾았을 때 채워지는 복사본
	// 반환 : 찾았는가
	bool FindEntry(int32 EntryId, FEPInventoryEntry& Out) const;

	// 그 컨테이너 '안에서' 합칠 수 있는 같은 아이템의 EntryId를 찾는다.
	//   Container : 뒤질 컨테이너. INDEX_NONE이면 본체
	//   ItemId    : 합칠 대상
	// 반환 : 합칠 엔트리. 없으면 INDEX_NONE
	//   ★ Container 인자를 빼면 배낭 속 현금이 본체 현금과 합쳐진다 — 벗는 순간 돈이 딸려 나간다
	int32 FindFungibleEntryId(int32 Container, FName ItemId) const;

	// 그 컨테이너가 쓰고 있는 칸의 합(Sum of SlotSize).
	//   Container : 셀 컨테이너. INDEX_NONE이면 본체
	// ★ 슬롯에 든 것(SlotId != None)은 안 센다 — 이 한 줄이 "칸을 먹는다"의 정의 전체다
	int32 GetUsedSlots(int32 Container) const;

	// 그 컨테이너가 제공하는 칸 수.
	//   Container : 물어볼 컨테이너. INDEX_NONE이면 본체(MaxSlots), 아니면 DT의 ContainerCapacity
	// 반환 : 칸 수. 0이면 컨테이너가 아니다
	int32 GetCapacity(int32 Container) const;

	// 수납 판정의 유일한 지점. GetUsedSlots + SlotSize <= GetCapacity
	//   Container : 넣을 곳
	//   ItemId    : 넣을 아이템 (SlotSize를 DT에서 읽는다)
	bool CanFit(int32 Container, FName ItemId) const;

	// DT의 bFungible — "개체를 구별할 근거가 없다"(현금·탄약상자). 스택이 아니라 Charges 합산이다.
	bool IsFungible(FName ItemId) const;

	// 슬롯 배치 판정의 유일한 지점. MoveEntry의 검사 2·3·4가 이 함수다.
	// 셋 다 (Parent, SlotId, ItemId)의 함수이고 옮기는 엔트리를 보지 않는다.
	//   Parent : 슬롯을 가진 쪽. 몸 슬롯이면 INDEX_NONE, 부착 슬롯이면 그 무기의 EntryId
	//   SlotId : 꽂을 슬롯 이름. NAME_None이면 항상 true (수납은 이 함수의 일이 아니다 → CanFit)
	//   ItemId : 꽂을 아이템 (SlotPriority를 DT에서 읽는다)
	// 반환 : 자격(2) + 정합(3) + 빈자리(4)를 전부 통과했는가
	bool CanPlaceInSlot(int32 Parent, FName SlotId, FName ItemId) const;

	// 그 슬롯에 든 엔트리를 찾는다. 슬롯 조회는 전부 이것 하나로 한다(슬롯이 12개다).
	//   Parent : 슬롯을 가진 쪽. 몸 슬롯이면 INDEX_NONE, 부착이면 그 무기의 EntryId
	//   SlotId : 슬롯 이름
	// 반환 : 든 엔트리. 비었으면 INDEX_NONE
	//   ★ Parent가 선택이 아닌 이유 — Optic은 무기마다 하나씩 있는 슬롯이다
	int32 GetEntryInSlot(int32 Parent, FName SlotId) const;

	// 등에 맨 배낭. 별도 필드가 없다 — SlotId == "Back"이 곧 "이 배낭은 등에 있다"이다.
	int32 GetEquippedBackpack() const { return GetEntryInSlot(INDEX_NONE, TEXT("Back")); }

	// 지금 손에 든 것. ActiveHotbarIndex -> 슬롯 이름 -> 엔트리 순으로 푼다.
	// 파생 게터다(필드가 아니다) — RemoveSelf 뒤에 부르면 INDEX_NONE이 나온다.
	int32 GetEquippedEntryId() const;

	// UI 순회용 읽기 전용 뷰. 반환 참조는 '그 프레임 안에서만' 유효하다 — 순회 중 AddItem 금지.
	const TArray<FEPInventoryEntry>& GetEntries() const { return Entries.Items; }

	// 그 컨테이너의 '수납' 아이템을 SortKey 오름차순으로. 클라(그리기)와 서버가 같은 함수를 쓴다.
	//   Container : 볼 컨테이너. INDEX_NONE이면 본체
	// 반환 : EntryId 목록. 슬롯에 든 것은 빠진다. 동률은 EntryId로 깨서 결정성을 준다
	//   ★ 키 공간이 아니다 — 다음 SortKey를 구할 때 이걸 쓰면 안 된다(KeySpace_* 를 쓴다)
	TArray<int32> GetSortedContents(int32 Container) const;

	// --- 수정 ---

	// Charges를 고치는 유일한 지점. 0으로 클램프하고 MarkItemDirty까지 여기서 한다.
	//   EntryId    : 고칠 엔트리
	//   NewCharges : 새 값(대입). 음수면 0으로 클램프
	// 쓰이는 곳 : 잔탄 write-back — 본질적으로 '대입'이다
	void SetEntryCharges(int32 EntryId, int32 NewCharges);

	// 현재 Charges에 Delta를 더한다. Set에 위임한다.
	//   EntryId : 고칠 엔트리
	//   Delta   : 음수면 차감. ConsumeCharges는 만들지 않는다 — AddEntryCharges(Id, -N)이다
	// 쓰이는 곳 : bFungible 합치기, 소모품 사용, 재장전 소비
	void AddEntryCharges(int32 EntryId, int32 Delta);

	// ParentEntryId + SlotId를 고치는 유일한 지점.
	// 장착·해제·드래그·컨테이너 간 이동·부착이 전부 이 함수 하나다(바뀌는 필드가 둘뿐이라).
	//   EntryId   : 옮길 엔트리
	//   NewParent : 목적지 부모. INDEX_NONE이면 본체
	//   NewSlotId : 목적지 슬롯. NAME_None이면 수납
	// 반환 : 검사 0~6을 전부 통과하고 실제로 옮겼는가
	//   검사 0 제자리 / 1 존재 / 2·3·4 CanPlaceInSlot / 5 CanFit(수납일 때만) / 6 사이클
	//   ★ 통째 대입 금지 — ReplicationID가 리셋되어 수신 측에 '삭제+추가'로 보인다
	//   ★ 부모가 바뀌면 SortKey를 목적지 맨 뒤로 재발급한다. 키는 '재부모 전에' 구한다
	bool MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId);

	// 엔트리와 그 자식 전부를 제거한다. 장착 중이면 잔탄 write-back까지 스스로 한다.
	//   EntryId    : 제거할 서브트리의 루트
	//   OutRemoved : null이 아니면 제거된 서브트리를 '전위 순회'로 담는다.
	//                [0]이 루트이고 Parent=INDEX_NONE / SlotId=None / SortKey=0 으로 정규화된다.
	//                자식은 원래 SortKey를 들고 가서, 되주우면 내용물 순서가 살아난다
	// 반환 : 제거했는가 (권한 없음 / 없는 엔트리면 false)
	//   ★ 스냅샷을 얻는 유일한 방법이 제거하는 것이다 — 순서를 뒤집는 게 문법적으로 불가능하다
	bool RemoveEntry(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved = nullptr);

	// 획득 (1)단계. In[0]의 SlotPriority를 순회해 '첫 빈 슬롯'에 서브트리째 넣는다.
	//   In : AddSubtree와 같은 모양의 전위 순회 배열
	// 반환 : 루트의 새 EntryId. 빈 슬롯이 없으면 INDEX_NONE ((2)단계로 넘어간다)
	//   ★ 본체를 경유하지 않는다 — 경유하면 CanFit에 걸려 "등이 비었는데 배낭을 못 맨다"
	//   ★ 배낭 전용이 아니다. Step 03에서는 배낭 행의 ["Back"] 하나만 돌 뿐이다
	int32 TryAutoEquip(const TArray<FEPInventoryEntry>& In);

	// 획득 (2)단계. 어느 컨테이너부터 볼 것인가.
	// 반환 : [INDEX_NONE(본체), 외투, 상의, 하의, 배낭 ...] 순의 EntryId 목록
	//   ★ 맨 앞의 본체는 직접 붙인다 — 설정의 ContainerOrder에는 본체가 없다(본체는 슬롯이 아니다)
	TArray<int32> GetInsertionOrder() const;

	// 상태 변경 RPC의 게이트. 죽음·시전 확인이 여기 한 곳에 있다.
	// 규칙은 "모든 Server_* 의 첫 줄"이다.
	bool CanMutateInventory() const;

	// 아이템을 발밑에 버린다. 배낭이면 내용물이 통째로 픽업에 실린다.
	//   EntryId : 버릴 엔트리. 클라는 자기가 받은 번호를 '지목'만 한다
	//   ★ 스폰이 제거보다 먼저다 — 뒤집으면 스폰 실패 시 서브트리가 통째로 증발한다
	//   ★ 배낭 '벗기'도 이 함수다. 본체로 옮기면 본체 적재량에 걸린다(본체는 최종 0칸)
	UFUNCTION(Server, Reliable)
	void Server_DropItem(int32 EntryId);

	// 같은 컨테이너 '안에서' 자리만 바꾼다. 부모도 슬롯도 용량도 안 건드린다.
	//   EntryId     : 옮길 엔트리
	//   PrevEntryId : 이 엔트리 '바로 뒤'에 놓는다. INDEX_NONE이면 맨 앞
	//   ★ 인덱스가 아니라 이웃을 받는다 — 클라와 서버 목록이 한 칸 어긋나도 정확하다
	//   ★ 정상 클라에서는 실패할 수 없는 연산이다(그래서 UI가 낙관적으로 먼저 그려도 된다)
	//   RPC가 아니다. Server_ReorderEntry(외부 표면)는 Step 04-B에서 연다
	void ReorderEntry(int32 EntryId, int32 PrevEntryId);

	// 인벤토리가 바뀌었다는 알림. '수신 1회당 1회' 나간다(항목마다가 아니다).
	// 클라는 PostReplicatedReceive가, 서버는 FScopedInventoryNotify가 쏜다. Step 04 UI가 구독한다.
	DECLARE_MULTICAST_DELEGATE(FOnInventoryChanged);
	FOnInventoryChanged OnInventoryChanged;

protected:
	// 복제 등록. 셋 다 COND_OwnerOnly다 — 조건을 빼면 모든 클라가 남의 가방을 받는다(치트).
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 모든 엔트리. 이 컴포넌트의 유일한 상태다.
	UPROPERTY(Replicated) FEPInventoryList Entries;

	// 본체 용량. 최종값은 0이다 — 수납은 착용 컨테이너(상의·하의·배낭)에서만 나온다.
	// 지금 10은 테스트값. 0을 넣으면 CanFit이 항상 거짓이라 나머지 코드가 그대로 돈다.
	UPROPERTY(Replicated) int32 MaxSlots = 10;

	// SlotId로 표현되지 않는 유일한 상태 — "핫바 1~4 중 지금 어느 쪽을 들었나".
	// 가리키는 것이 엔트리가 아니라 '슬롯'이라 죽은 번호가 생길 문법이 없다. 세팅은 Step 05.
	UPROPERTY(Replicated) int32 ActiveHotbarIndex = INDEX_NONE;

private:
	// RemoveEntry의 재귀 본체. bIsRoot를 밖에서 넘길 문법이 없어야 계약이 지켜진다.
	//   EntryId    : 제거할 엔트리
	//   OutRemoved : 스냅샷을 담을 배열(null 가능)
	//   bIsRoot    : true면 스냅샷의 Parent / SlotId / SortKey를 버린다(목적지 체계로 들어가므로).
	//                false면 그대로 보존한다 — 그래야 되주울 때 내용물 순서가 산다
	// 순서를 바꾸지 말 것: (1)write-back -> (2)스냅샷 -> (3)자신 제거 -> (4)자식 재귀
	bool RemoveEntryInternal(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved, bool bIsRoot);

	// ParentId를 부모로 둔 엔트리를 전부 제거한다.
	//   ParentId : 방금 제거된 부모의 EntryId
	// 자식 목록을 먼저 뜬다 — 순회 도중 배열이 바뀐다.
	void RemoveChildrenRecursive(int32 ParentId, TArray<FEPInventoryEntry>* OutRemoved);

	// 배열에서 빼고 MarkArrayDirty. 배열 축소의 유일한 지점.
	void RemoveSelf(int32 EntryId);

	// 그 번호의 엔트리가 있는가. 조작된 요청을 거르는 데 쓴다.
	bool ContainsEntry(int32 EntryId) const;

	// 번호 발급 + 삽입 + MarkItemDirty의 유일한 지점.
	// 칸 검사도 bFungible 합치기도 하지 않는다 — 그건 호출자(AddItem) 몫이다.
	//   Parent : 부모 EntryId. INDEX_NONE이면 본체
	//   ItemId : DT 행 이름
	//   State  : 런타임 상태(값 복사)
	//   SlotId : 슬롯이면 그 이름, 수납이면 NAME_None
	// 반환 : 새로 발급한 EntryId
	//   ★ SortKey도 여기서 발급한다(형제 맨 뒤). '배열에 넣기 전에' 구한다
	int32 InsertEntry(int32 Parent, FName ItemId, const FEPItemState& State, FName SlotId);

	// SortKey를 고치는 유일한 지점. MarkItemDirty가 여기 한 곳에 있다.
	//   EntryId : 고칠 엔트리
	//   NewKey  : 새 정렬 키. 음수도 0도 유효한 값이다
	void AssignSortKey(int32 EntryId, int32 NewKey);

	// 형제 키를 0, Step, 2*Step... 으로 다시 깐다. 현재 순서는 유지하고 간격만 벌린다.
	// 이분 고갈(같은 틈에 ~16회) 또는 경계 접근에서만 돈다 — 죽은 코드가 아니다.
	//   Container : 다시 깔 부모. 슬롯에 든 형제도 포함한다(키 공간은 부모 전체다)
	void RenormalizeSortKeys(int32 Container);

	// 그 부모의 최대 키 + Step. 새 엔트리를 형제 맨 뒤에 놓는다.
	// 키 발급의 유일한 지점이라 상한 가드도 여기 하나면 된다(그래서 const가 아니다).
	//   Container : 부모 EntryId. 형제가 없으면 0
	// ★ 이하 KeySpace_ 셋은 '부모 전체'를 본다 — GetSortedContents(표시 목록)를 쓰면 동률이 난다
	int32 KeySpace_NextAtEnd(int32 Container);

	// 그 부모의 최소 키. 맨 앞으로 보낼 때 여기서 Step을 뺀다(키가 음수로 내려간다).
	int32 KeySpace_Min(int32 Container) const;

	// Key보다 큰 것 중 가장 작은 키를 찾는다. 이분 삽입의 오른쪽 경계다.
	//   Container : 볼 부모
	//   Key       : 이 값보다 커야 한다
	//   Exclude   : 제외할 EntryId (자기 자신)
	//   OutKey    : 찾았을 때 채워지는 키
	// 반환 : 찾았는가 — 못 찾으면 "맨 뒤"라는 뜻이다
	//   ★ 실패를 INDEX_NONE으로 돌려주면 안 된다. SortKey는 -1도 도달 가능한 값이다
	//   ※ 문서상 반환형은 bool이다 (현재 int32)
	bool KeySpace_NextAbove(int32 Container, int32 Key, int32 Exclude, int32& OutKey) const;

	// 그 엔트리의 SortKey를 읽는다.
	//   EntryId : 읽을 엔트리
	//   OutKey  : 찾았을 때 채워지는 키
	// 반환 : 찾았는가 (※ 문서상 bool. "없으면 0"도 센티널로 쓸 수 없다 — 0은 정상 키다)
	int32 KeyOf(int32 EntryId, int32& OutKey) const;

	// ReorderEntry의 재귀 본체. bRetry를 밖에서 넘길 문법이 없어야 종료가 보장된다.
	//   bRetry : 재정규화 후의 재시도인가. true인데 또 자리가 없으면 ensure로 드러내고 멈춘다
	void ReorderEntryInternal(int32 EntryId, int32 PrevEntryId, bool bRetry);

	static constexpr int32 SortKeyStep = 1<<16;              // 키 간격. 사이에 ~16회 끼울 수 있다
	static constexpr int32 SortKeyGuard = SortKeyStep * 4;   // int32 경계까지 남겨둘 여유

	// 캐릭터 전방 100cm + 바닥 트레이스. 막혀 있으면 발밑으로 폴백한다.
	// 반환 : 스폰된 픽업. 실패면 nullptr
	AEPPickup* SpawnPickupInFront() const;

	// 아이템 정의 서브시스템. 멤버로 캐시하지 않는다 — "언제까지 유효한가"를 만들지 않으려고.
	const UEPItemDefinitionSubsystem* Defs() const;

	int32 NextEntryId = 1;   // 서버 전용. 복제하지 않는다. 재번호되지 않는 발급기다
	int32 NotifyDepth = 0;   // 중간 알림을 막는 스코프 가드의 깊이. 0이 될 때 한 번 Broadcast

	friend struct FScopedInventoryNotify;   // 정의는 EPInventoryComponent.cpp 상단
};
