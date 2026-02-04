// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Types/EPTypes.h"

#ifdef EMPLOYMENTPROJ_EPTypes_generated_h
#error "EPTypes.generated.h already included, missing '#pragma once' in EPTypes.h"
#endif
#define EMPLOYMENTPROJ_EPTypes_generated_h

#include "Templates/IsUEnumClass.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Templates/NoDestroy.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Github_UE5_EmploymentProj_EmploymentProj_Source_EmploymentProj_Public_Types_EPTypes_h

// ********** Begin Enum EEPMatchPhase *************************************************************
#define FOREACH_ENUM_EEPMATCHPHASE(op) \
	op(EEPMatchPhase::Waiting) \
	op(EEPMatchPhase::Playing) \
	op(EEPMatchPhase::Ended) 

enum class EEPMatchPhase : uint8;
template<> struct TIsUEnumClass<EEPMatchPhase> { enum { Value = true }; };
template<> EMPLOYMENTPROJ_NON_ATTRIBUTED_API UEnum* StaticEnum<EEPMatchPhase>();
// ********** End Enum EEPMatchPhase ***************************************************************

// ********** Begin Enum EEPItemRarity *************************************************************
#define FOREACH_ENUM_EEPITEMRARITY(op) \
	op(EEPItemRarity::Common) \
	op(EEPItemRarity::Uncommon) \
	op(EEPItemRarity::Rare) \
	op(EEPItemRarity::Legendary) 

enum class EEPItemRarity : uint8;
template<> struct TIsUEnumClass<EEPItemRarity> { enum { Value = true }; };
template<> EMPLOYMENTPROJ_NON_ATTRIBUTED_API UEnum* StaticEnum<EEPItemRarity>();
// ********** End Enum EEPItemRarity ***************************************************************

// ********** Begin Enum EEPFireMode ***************************************************************
#define FOREACH_ENUM_EEPFIREMODE(op) \
	op(EEPFireMode::Single) \
	op(EEPFireMode::Auto) 

enum class EEPFireMode : uint8;
template<> struct TIsUEnumClass<EEPFireMode> { enum { Value = true }; };
template<> EMPLOYMENTPROJ_NON_ATTRIBUTED_API UEnum* StaticEnum<EEPFireMode>();
// ********** End Enum EEPFireMode *****************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
