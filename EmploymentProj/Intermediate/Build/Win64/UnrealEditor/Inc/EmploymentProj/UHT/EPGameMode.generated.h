// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Core/EPGameMode.h"

#ifdef EMPLOYMENTPROJ_EPGameMode_generated_h
#error "EPGameMode.generated.h already included, missing '#pragma once' in EPGameMode.h"
#endif
#define EMPLOYMENTPROJ_EPGameMode_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class AEPGameMode **************************************************************
struct Z_Construct_UClass_AEPGameMode_Statics;
EMPLOYMENTPROJ_API UClass* Z_Construct_UClass_AEPGameMode_NoRegister();

#define FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h_15_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAEPGameMode(); \
	friend struct ::Z_Construct_UClass_AEPGameMode_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend EMPLOYMENTPROJ_API UClass* ::Z_Construct_UClass_AEPGameMode_NoRegister(); \
public: \
	DECLARE_CLASS2(AEPGameMode, AGameMode, COMPILED_IN_FLAGS(0 | CLASS_Transient | CLASS_Config), CASTCLASS_None, TEXT("/Script/EmploymentProj"), Z_Construct_UClass_AEPGameMode_NoRegister) \
	DECLARE_SERIALIZER(AEPGameMode)


#define FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h_15_ENHANCED_CONSTRUCTORS \
	/** Deleted move- and copy-constructors, should never be used */ \
	AEPGameMode(AEPGameMode&&) = delete; \
	AEPGameMode(const AEPGameMode&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AEPGameMode); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AEPGameMode); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AEPGameMode) \
	NO_API virtual ~AEPGameMode();


#define FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h_12_PROLOG
#define FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h_15_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h_15_INCLASS_NO_PURE_DECLS \
	FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h_15_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class AEPGameMode;

// ********** End Class AEPGameMode ****************************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
