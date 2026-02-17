// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/EPWeaponAnimInstance.h"
#include "Animation/EPAnimInstance.h"

void UEPWeaponAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	// 메인 AnimBP에서 변수 복사
	// GetLinkedAnimLayerInstanceByGroup 또는 GetOwningComponent로 메인 인스턴스 접근
	USkeletalMeshComponent* MeshComp = GetOwningComponent();
	if (!MeshComp) return;
	
	UEPAnimInstance* MainAnim = Cast<UEPAnimInstance>(MeshComp->GetAnimInstance());
	if (!MainAnim) return;
	
	// MainAnim의 Getter를 통해 복사 (Getter 추가 필요)
	// 또는 Property Access를 AnimBP에서 직접 사용
}
