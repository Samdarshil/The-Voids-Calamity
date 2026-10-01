// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Interfaces/IVoidValidator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoidTest
{
	inline int32 CountCode(const FVoidValidationReport& R, const TCHAR* Code)
	{
		int32 N = 0;
		for (const FVoidValidationIssue& I : R.Issues) { N += (I.ErrorCode == FName(Code)) ? 1 : 0; }
		return N;
	}
	inline bool HasCode(const FVoidValidationReport& R, const TCHAR* Code) { return CountCode(R, Code) > 0; }
	inline bool HasCodeAt(const FVoidValidationReport& R, const TCHAR* Code, EVoidValidationSeverity Sev)
	{
		for (const FVoidValidationIssue& I : R.Issues) { if (I.ErrorCode == FName(Code) && I.Severity == Sev) { return true; } }
		return false;
	}

	inline FVoidRoadSpec MakeRoad(const TCHAR* Id, std::initializer_list<FVector2D> Points, float Width = 600.0f)
	{
		FVoidRoadSpec R;
		R.Id = FVoidElementId(FName(Id));
		R.CenterlinePoints = TArray<FVector2D>(Points);
		R.WidthUnits = Width;
		return R;
	}

	inline FVoidBuildingSpec MakeBuilding(const TCHAR* Id, std::initializer_list<FVector2D> Corners, float Height = 1000.0f)
	{
		FVoidBuildingSpec B;
		B.Id = FVoidElementId(FName(Id));
		B.FootprintCorners = TArray<FVector2D>(Corners);
		B.HeightUnits = Height;
		B.BuildingType = TEXT("Test");
		return B;
	}

	inline FVoidDesignPackage MakePackage()
	{
		FVoidDesignPackage P;
		P.Metadata.SourceDocumentName = TEXT("test");
		P.Metadata.bIsApproved = true;
		P.District.DistrictId = FVoidElementId(TEXT("test_district"));
		return P;
	}

	/** Runs one validator's static Run() with a fresh valid report. */
	template <typename TValidator>
	FVoidValidationReport Run(const FVoidDesignPackage& Package, FVoidValidationOptions Options = FVoidValidationOptions(), const TSet<FName>& KnownDistricts = TSet<FName>())
	{
		FVoidValidationReport Report;
		Report.bIsValid = true;
		FVoidValidationInput Input;
		Input.Package = &Package;
		Input.KnownDistrictIds = KnownDistricts;
		FVoidValidationContext Ctx(Report, Options, TEXT("Test"));
		TValidator::Run(Package, Input, Ctx);
		Ctx.FlushSuppressionSummary();
		return Report;
	}
}

#endif
