// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EPItemInstance.generated.h"

class UEPItemDefinition;

UCLASS(BlueprintType)
class EMPLOYMENTPROJ_API UEPItemInstance : public UObject
{
	GENERATED_BODY()
	
public:
	// 인스턴스 고유 ID
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGuid InstanceId;
	
	// Row/Definition 매칭 키
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FName ItemId;
	
	// 수량 (스택)
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 Quantity = 1;
	
	// 직렬화 버전 (DB 마이그레이션용)
	UPROPERTY()
	int32 SchemaVersion = 1;
	
	// 캐시된 Definition 참조 (런타임에 Resolve)
	UPROPERTY(Transient)
	TObjectPtr<UEPItemDefinition> CachedDefinition;
	
	// 팩토리 함수
	static UEPItemInstance* CreateInstance(UObject* Outer, FName InItemId, UEPItemDefinition* InDefinition = nullptr);
	
	// 서브오브젝트 복제 지원 (인벤토리 복제 시 필요)
	virtual bool IsSupportedForNetworking() const override { return true; };
};
