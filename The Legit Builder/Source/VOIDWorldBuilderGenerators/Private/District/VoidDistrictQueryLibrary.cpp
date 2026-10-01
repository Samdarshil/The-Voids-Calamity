// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "District/VoidDistrictQueryLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Algo/Sort.h"

namespace
{
	UWorld* WorldOf(const UObject* Context)
	{
		return Context ? Context->GetWorld() : nullptr;
	}
}

AVoidMeridianRootActor* UVoidDistrictQueryLibrary::GetMeridianRoot(const UObject* WorldContextObject)
{
	if (UWorld* World = WorldOf(WorldContextObject))
	{
		for (TActorIterator<AVoidMeridianRootActor> It(World); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

AVoidDistrictActor* UVoidDistrictQueryLibrary::FindDistrict(const UObject* WorldContextObject, FName DistrictId)
{
	if (UWorld* World = WorldOf(WorldContextObject))
	{
		for (TActorIterator<AVoidDistrictActor> It(World); It; ++It)
		{
			if (It->DistrictId == DistrictId)
			{
				return *It;
			}
		}
	}
	return nullptr;
}

TArray<AVoidDistrictLandmarkActor*> UVoidDistrictQueryLibrary::GetLandmarks(const UObject* WorldContextObject, FName DistrictId, bool bSkylineOnly)
{
	TArray<AVoidDistrictLandmarkActor*> Result;
	if (UWorld* World = WorldOf(WorldContextObject))
	{
		for (TActorIterator<AVoidDistrictLandmarkActor> It(World); It; ++It)
		{
			AVoidDistrictLandmarkActor* Landmark = *It;
			if (!DistrictId.IsNone() && Landmark->DistrictId != DistrictId) { continue; }
			if (bSkylineOnly && Landmark->VisibilityTier < 0) { continue; }
			Result.Add(Landmark);
		}
	}

	Algo::Sort(Result, [](const AVoidDistrictLandmarkActor* PA, const AVoidDistrictLandmarkActor* PB)
	{
		const int32 TierA = PA->VisibilityTier < 0 ? MAX_int32 : PA->VisibilityTier;
		const int32 TierB = PB->VisibilityTier < 0 ? MAX_int32 : PB->VisibilityTier;
		if (TierA != TierB) { return TierA < TierB; }
		return PA->InstanceId.LexicalLess(PB->InstanceId);
	});
	return Result;
}

AVoidDistrictLandmarkActor* UVoidDistrictQueryLibrary::FindLandmark(const UObject* WorldContextObject, FName LandmarkRegistryId)
{
	const TArray<AVoidDistrictLandmarkActor*> All = GetLandmarks(WorldContextObject, NAME_None, false);
	for (AVoidDistrictLandmarkActor* Landmark : All)
	{
		if (Landmark->LandmarkId == LandmarkRegistryId)
		{
			return Landmark;
		}
	}
	return nullptr;
}

bool UVoidDistrictQueryLibrary::GetDistrictProfile(const UObject* WorldContextObject, FName DistrictId, FVoidDistrictProfileData& OutProfile)
{
	if (const AVoidDistrictActor* District = FindDistrict(WorldContextObject, DistrictId))
	{
		OutProfile = District->Profile;
		return true;
	}
	return false;
}

TArray<FVoidDistrictPortInfo> UVoidDistrictQueryLibrary::GetPorts(const UObject* WorldContextObject, FName DistrictId)
{
	if (const AVoidDistrictActor* District = FindDistrict(WorldContextObject, DistrictId))
	{
		return District->Ports;
	}
	return TArray<FVoidDistrictPortInfo>();
}
