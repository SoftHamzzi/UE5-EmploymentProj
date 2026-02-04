// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Types/EPTypes.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeEPTypes() {}

// ********** Begin Cross Module References ********************************************************
EMPLOYMENTPROJ_API UEnum* Z_Construct_UEnum_EmploymentProj_EEPFireMode();
EMPLOYMENTPROJ_API UEnum* Z_Construct_UEnum_EmploymentProj_EEPItemRarity();
EMPLOYMENTPROJ_API UEnum* Z_Construct_UEnum_EmploymentProj_EEPMatchPhase();
UPackage* Z_Construct_UPackage__Script_EmploymentProj();
// ********** End Cross Module References **********************************************************

// ********** Begin Enum EEPMatchPhase *************************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EEPMatchPhase;
static UEnum* EEPMatchPhase_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EEPMatchPhase.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EEPMatchPhase.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_EmploymentProj_EEPMatchPhase, (UObject*)Z_Construct_UPackage__Script_EmploymentProj(), TEXT("EEPMatchPhase"));
	}
	return Z_Registration_Info_UEnum_EEPMatchPhase.OuterSingleton;
}
template<> EMPLOYMENTPROJ_NON_ATTRIBUTED_API UEnum* StaticEnum<EEPMatchPhase>()
{
	return EEPMatchPhase_StaticEnum();
}
struct Z_Construct_UEnum_EmploymentProj_EEPMatchPhase_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "Ended.DisplayName", "Ended" },
		{ "Ended.Name", "EEPMatchPhase::Ended" },
		{ "ModuleRelativePath", "Public/Types/EPTypes.h" },
		{ "Playing.DisplayName", "Playing" },
		{ "Playing.Name", "EEPMatchPhase::Playing" },
		{ "Waiting.DisplayName", "Waiting" },
		{ "Waiting.Name", "EEPMatchPhase::Waiting" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EEPMatchPhase::Waiting", (int64)EEPMatchPhase::Waiting },
		{ "EEPMatchPhase::Playing", (int64)EEPMatchPhase::Playing },
		{ "EEPMatchPhase::Ended", (int64)EEPMatchPhase::Ended },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_EmploymentProj_EEPMatchPhase_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_EmploymentProj_EEPMatchPhase_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_EmploymentProj,
	nullptr,
	"EEPMatchPhase",
	"EEPMatchPhase",
	Z_Construct_UEnum_EmploymentProj_EEPMatchPhase_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_EmploymentProj_EEPMatchPhase_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_EmploymentProj_EEPMatchPhase_Statics::Enum_MetaDataParams), Z_Construct_UEnum_EmploymentProj_EEPMatchPhase_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_EmploymentProj_EEPMatchPhase()
{
	if (!Z_Registration_Info_UEnum_EEPMatchPhase.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EEPMatchPhase.InnerSingleton, Z_Construct_UEnum_EmploymentProj_EEPMatchPhase_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EEPMatchPhase.InnerSingleton;
}
// ********** End Enum EEPMatchPhase ***************************************************************

// ********** Begin Enum EEPItemRarity *************************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EEPItemRarity;
static UEnum* EEPItemRarity_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EEPItemRarity.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EEPItemRarity.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_EmploymentProj_EEPItemRarity, (UObject*)Z_Construct_UPackage__Script_EmploymentProj(), TEXT("EEPItemRarity"));
	}
	return Z_Registration_Info_UEnum_EEPItemRarity.OuterSingleton;
}
template<> EMPLOYMENTPROJ_NON_ATTRIBUTED_API UEnum* StaticEnum<EEPItemRarity>()
{
	return EEPItemRarity_StaticEnum();
}
struct Z_Construct_UEnum_EmploymentProj_EEPItemRarity_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "Common.Name", "EEPItemRarity::Common" },
		{ "Legendary.Name", "EEPItemRarity::Legendary" },
		{ "ModuleRelativePath", "Public/Types/EPTypes.h" },
		{ "Rare.Name", "EEPItemRarity::Rare" },
		{ "Uncommon.Name", "EEPItemRarity::Uncommon" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EEPItemRarity::Common", (int64)EEPItemRarity::Common },
		{ "EEPItemRarity::Uncommon", (int64)EEPItemRarity::Uncommon },
		{ "EEPItemRarity::Rare", (int64)EEPItemRarity::Rare },
		{ "EEPItemRarity::Legendary", (int64)EEPItemRarity::Legendary },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_EmploymentProj_EEPItemRarity_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_EmploymentProj_EEPItemRarity_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_EmploymentProj,
	nullptr,
	"EEPItemRarity",
	"EEPItemRarity",
	Z_Construct_UEnum_EmploymentProj_EEPItemRarity_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_EmploymentProj_EEPItemRarity_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_EmploymentProj_EEPItemRarity_Statics::Enum_MetaDataParams), Z_Construct_UEnum_EmploymentProj_EEPItemRarity_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_EmploymentProj_EEPItemRarity()
{
	if (!Z_Registration_Info_UEnum_EEPItemRarity.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EEPItemRarity.InnerSingleton, Z_Construct_UEnum_EmploymentProj_EEPItemRarity_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EEPItemRarity.InnerSingleton;
}
// ********** End Enum EEPItemRarity ***************************************************************

// ********** Begin Enum EEPFireMode ***************************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EEPFireMode;
static UEnum* EEPFireMode_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EEPFireMode.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EEPFireMode.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_EmploymentProj_EEPFireMode, (UObject*)Z_Construct_UPackage__Script_EmploymentProj(), TEXT("EEPFireMode"));
	}
	return Z_Registration_Info_UEnum_EEPFireMode.OuterSingleton;
}
template<> EMPLOYMENTPROJ_NON_ATTRIBUTED_API UEnum* StaticEnum<EEPFireMode>()
{
	return EEPFireMode_StaticEnum();
}
struct Z_Construct_UEnum_EmploymentProj_EEPFireMode_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "Auto.Name", "EEPFireMode::Auto" },
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/Types/EPTypes.h" },
		{ "Single.Name", "EEPFireMode::Single" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EEPFireMode::Single", (int64)EEPFireMode::Single },
		{ "EEPFireMode::Auto", (int64)EEPFireMode::Auto },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_EmploymentProj_EEPFireMode_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_EmploymentProj_EEPFireMode_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_EmploymentProj,
	nullptr,
	"EEPFireMode",
	"EEPFireMode",
	Z_Construct_UEnum_EmploymentProj_EEPFireMode_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_EmploymentProj_EEPFireMode_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_EmploymentProj_EEPFireMode_Statics::Enum_MetaDataParams), Z_Construct_UEnum_EmploymentProj_EEPFireMode_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_EmploymentProj_EEPFireMode()
{
	if (!Z_Registration_Info_UEnum_EEPFireMode.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EEPFireMode.InnerSingleton, Z_Construct_UEnum_EmploymentProj_EEPFireMode_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EEPFireMode.InnerSingleton;
}
// ********** End Enum EEPFireMode *****************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Types_EPTypes_h__Script_EmploymentProj_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EEPMatchPhase_StaticEnum, TEXT("EEPMatchPhase"), &Z_Registration_Info_UEnum_EEPMatchPhase, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3616593433U) },
		{ EEPItemRarity_StaticEnum, TEXT("EEPItemRarity"), &Z_Registration_Info_UEnum_EEPItemRarity, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3837604370U) },
		{ EEPFireMode_StaticEnum, TEXT("EEPFireMode"), &Z_Registration_Info_UEnum_EEPFireMode, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1268594888U) },
	};
}; // Z_CompiledInDeferFile_FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Types_EPTypes_h__Script_EmploymentProj_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Types_EPTypes_h__Script_EmploymentProj_3704023480{
	TEXT("/Script/EmploymentProj"),
	nullptr, 0,
	nullptr, 0,
	Z_CompiledInDeferFile_FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Types_EPTypes_h__Script_EmploymentProj_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Types_EPTypes_h__Script_EmploymentProj_Statics::EnumInfo),
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
