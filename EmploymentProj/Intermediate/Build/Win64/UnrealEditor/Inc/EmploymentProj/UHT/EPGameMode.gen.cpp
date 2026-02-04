// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Core/EPGameMode.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeEPGameMode() {}

// ********** Begin Cross Module References ********************************************************
EMPLOYMENTPROJ_API UClass* Z_Construct_UClass_AEPGameMode();
EMPLOYMENTPROJ_API UClass* Z_Construct_UClass_AEPGameMode_NoRegister();
ENGINE_API UClass* Z_Construct_UClass_AGameMode();
UPackage* Z_Construct_UPackage__Script_EmploymentProj();
// ********** End Cross Module References **********************************************************

// ********** Begin Class AEPGameMode **************************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_AEPGameMode;
UClass* AEPGameMode::GetPrivateStaticClass()
{
	using TClass = AEPGameMode;
	if (!Z_Registration_Info_UClass_AEPGameMode.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("EPGameMode"),
			Z_Registration_Info_UClass_AEPGameMode.InnerSingleton,
			StaticRegisterNativesAEPGameMode,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_AEPGameMode.InnerSingleton;
}
UClass* Z_Construct_UClass_AEPGameMode_NoRegister()
{
	return AEPGameMode::GetPrivateStaticClass();
}
struct Z_Construct_UClass_AEPGameMode_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \n */" },
#endif
		{ "HideCategories", "Info Rendering MovementReplication Replication Actor Input Movement Collision Rendering HLOD WorldPartition DataLayers Transformation" },
		{ "IncludePath", "Core/EPGameMode.h" },
		{ "ModuleRelativePath", "Public/Core/EPGameMode.h" },
		{ "ShowCategories", "Input|MouseInput Input|TouchInput" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MatchDuration_MetaData[] = {
		{ "Category", "Match" },
		{ "ModuleRelativePath", "Public/Core/EPGameMode.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MinPlayersToStart_MetaData[] = {
		{ "Category", "Match" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// \xec\x9d\xb4\xed\x9b\x84 30\xeb\xb6\x84\xec\x9c\xbc\xeb\xa1\x9c \xec\x88\x98\xec\xa0\x95.\n" },
#endif
		{ "ModuleRelativePath", "Public/Core/EPGameMode.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xec\x9d\xb4\xed\x9b\x84 30\xeb\xb6\x84\xec\x9c\xbc\xeb\xa1\x9c \xec\x88\x98\xec\xa0\x95." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class AEPGameMode constinit property declarations ******************************
	static const UECodeGen_Private::FFloatPropertyParams NewProp_MatchDuration;
	static const UECodeGen_Private::FIntPropertyParams NewProp_MinPlayersToStart;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class AEPGameMode constinit property declarations ********************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AEPGameMode>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_AEPGameMode_Statics

// ********** Begin Class AEPGameMode Property Definitions *****************************************
const UECodeGen_Private::FFloatPropertyParams Z_Construct_UClass_AEPGameMode_Statics::NewProp_MatchDuration = { "MatchDuration", nullptr, (EPropertyFlags)0x0020080000010001, UECodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AEPGameMode, MatchDuration), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MatchDuration_MetaData), NewProp_MatchDuration_MetaData) };
const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_AEPGameMode_Statics::NewProp_MinPlayersToStart = { "MinPlayersToStart", nullptr, (EPropertyFlags)0x0020080000010001, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(AEPGameMode, MinPlayersToStart), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MinPlayersToStart_MetaData), NewProp_MinPlayersToStart_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AEPGameMode_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AEPGameMode_Statics::NewProp_MatchDuration,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AEPGameMode_Statics::NewProp_MinPlayersToStart,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AEPGameMode_Statics::PropPointers) < 2048);
// ********** End Class AEPGameMode Property Definitions *******************************************
UObject* (*const Z_Construct_UClass_AEPGameMode_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AGameMode,
	(UObject* (*)())Z_Construct_UPackage__Script_EmploymentProj,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_AEPGameMode_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_AEPGameMode_Statics::ClassParams = {
	&AEPGameMode::StaticClass,
	"Game",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_AEPGameMode_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_AEPGameMode_Statics::PropPointers),
	0,
	0x009002ACu,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_AEPGameMode_Statics::Class_MetaDataParams), Z_Construct_UClass_AEPGameMode_Statics::Class_MetaDataParams)
};
void AEPGameMode::StaticRegisterNativesAEPGameMode()
{
}
UClass* Z_Construct_UClass_AEPGameMode()
{
	if (!Z_Registration_Info_UClass_AEPGameMode.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_AEPGameMode.OuterSingleton, Z_Construct_UClass_AEPGameMode_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_AEPGameMode.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, AEPGameMode);
AEPGameMode::~AEPGameMode() {}
// ********** End Class AEPGameMode ****************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h__Script_EmploymentProj_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_AEPGameMode, AEPGameMode::StaticClass, TEXT("AEPGameMode"), &Z_Registration_Info_UClass_AEPGameMode, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(AEPGameMode), 2790990193U) },
	};
}; // Z_CompiledInDeferFile_FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h__Script_EmploymentProj_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h__Script_EmploymentProj_1955728146{
	TEXT("/Script/EmploymentProj"),
	Z_CompiledInDeferFile_FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h__Script_EmploymentProj_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Core_EPGameMode_h__Script_EmploymentProj_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
