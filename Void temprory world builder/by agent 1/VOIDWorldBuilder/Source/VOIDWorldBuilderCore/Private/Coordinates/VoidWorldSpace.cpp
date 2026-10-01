// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Coordinates/VoidWorldSpace.h"

FVoidWorldSpaceConfig FVoidWorldSpaceConfig::FromSettings(const UVoidWorldBuilderSettings* Settings)
{
	FVoidWorldSpaceConfig Out;
	if (Settings)
	{
		Out.SourceUnitsToUU = Settings->SourceUnitsToUnrealUnits;
		Out.Axis = Settings->SourceAxisConvention;
		Out.WorldOriginOffsetUU = Settings->WorldOriginOffsetUU;
		Out.MaxAbsCoordinateUU = Settings->MaxAbsCoordinateUU;
	}
	return Out;
}

FVoidWorldSpace FVoidWorldSpace::FromSettings()
{
	return FVoidWorldSpace(FVoidWorldSpaceConfig::FromSettings(UVoidWorldBuilderSettings::Get()));
}

bool FVoidWorldSpace::IsPositionAcceptable(const FVector& Position, FString* OutError) const
{
	if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) || !FMath::IsFinite(Position.Z))
	{
		if (OutError) { *OutError = TEXT("Position contains a non-finite (NaN/Inf) component."); }
		return false;
	}

	const double Limit = Config.MaxAbsCoordinateUU;
	if (FMath::Abs(Position.X) > Limit || FMath::Abs(Position.Y) > Limit || FMath::Abs(Position.Z) > Limit)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("Position (%.1f, %.1f, %.1f) exceeds the +/-%.0f UU coordinate limit."),
				Position.X, Position.Y, Position.Z, Limit);
		}
		return false;
	}
	return true;
}

bool FVoidWorldSpace::TryFromSource2D(const FVector2D& Source, double SourceElevation, FVector& OutWorld, FString* OutError) const
{
	if (!FMath::IsFinite(Source.X) || !FMath::IsFinite(Source.Y) || !FMath::IsFinite(SourceElevation))
	{
		if (OutError) { *OutError = TEXT("Source coordinate contains a non-finite (NaN/Inf) component."); }
		return false;
	}

	const double Y = (Config.Axis == EVoidSourceAxisConvention::FlipY) ? -Source.Y : Source.Y;
	const FVector World = FVector(Source.X, Y, SourceElevation) * Config.SourceUnitsToUU + Config.WorldOriginOffsetUU;

	if (!IsPositionAcceptable(World, OutError))
	{
		return false;
	}

	OutWorld = World;
	return true;
}

bool FVoidWorldSpace::TryFromPolar(double RadiusUU, double AzimuthDegrees, double ElevationUU, FVector& OutWorld, FString* OutError) const
{
	if (!FMath::IsFinite(RadiusUU) || !FMath::IsFinite(AzimuthDegrees) || !FMath::IsFinite(ElevationUU) || RadiusUU < 0.0)
	{
		if (OutError) { *OutError = TEXT("Polar coordinate is non-finite or has a negative radius."); }
		return false;
	}

	const FVector2D Dir = AzimuthToDirection2D(AzimuthDegrees);
	const FVector World = FVector(Dir.X * RadiusUU, Dir.Y * RadiusUU, ElevationUU) + Config.WorldOriginOffsetUU;

	if (!IsPositionAcceptable(World, OutError))
	{
		return false;
	}

	OutWorld = World;
	return true;
}

FVector2D FVoidWorldSpace::ToSource2D(const FVector& World) const
{
	const FVector Local = (World - Config.WorldOriginOffsetUU) / Config.SourceUnitsToUU;
	return FVector2D(Local.X, (Config.Axis == EVoidSourceAxisConvention::FlipY) ? -Local.Y : Local.Y);
}

FVector2D FVoidWorldSpace::AzimuthToDirection2D(double AzimuthDegrees)
{
	const double Rad = FMath::DegreesToRadians(AzimuthDegrees);
	return FVector2D(FMath::Cos(Rad), FMath::Sin(Rad));
}
