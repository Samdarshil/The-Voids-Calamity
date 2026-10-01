// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidValidationReport.h"

class UWorld;

/**
 * Point in the build pipeline after which a validator runs. The order of this
 * enum IS the canonical pipeline order:
 *
 *   Import -> PostImport -> Road -> PostRoad -> District -> PostDistrict ->
 *   Building -> PostBuilding -> Environment -> PostEnvironment -> Lighting ->
 *   PostLighting -> Props -> PostProps -> Metro -> PostMetro -> Final
 *
 * A validator that guards generator X registers for PostX. Data-only
 * validators (no world needed) register for PostImport.
 */
enum class EVoidValidationStage : uint8
{
	PostImport,
	PostRoad,
	PostDistrict,
	PostBuilding,
	PostEnvironment,
	PostLighting,
	PostProps,
	PostMetro,
	Final
};

VOIDWORLDBUILDERCORE_API const TCHAR* VoidValidationStageToString(EVoidValidationStage Stage);

/**
 * Tunables shared by every validator. Plain struct (not a UDeveloperSettings)
 * so validators stay usable from commandlets and automation tests without a
 * CDO. The defaults are deliberately permissive: they exist to catch corrupt
 * or absurd data (NaN, 1e30, 40 km-tall buildings), not to enforce taste.
 */
struct VOIDWORLDBUILDERCORE_API FVoidValidationOptions
{
	/** |x|, |y|, |z| above this is "invalid coordinate". 2.0e7 uu = 200 km; far beyond any city, far below float/double precision trouble. */
	double MaxCoordinateAbs = 2.0e7;

	/** Two road endpoints closer than this are treated as the same junction. Mirrors UVoidRoadGenerationSettings::JunctionToleranceUnits' default (50). */
	double JunctionToleranceUnits = 50.0;

	double MaxBuildingHeightUnits = 200000.0;   // 2 km
	double MaxBuildingExtentUnits = 50000.0;    // 500 m footprint bounding box side
	double MaxRoadWidthUnits = 5000.0;          // 50 m
	double MinSegmentLengthUnits = 1.0;         // shorter consecutive-point spacing is "degenerate"
	double HairpinAngleDegrees = 150.0;         // turn sharper than this between two segments is suspicious
	double AcuteJunctionAngleDegrees = 10.0;    // two roads leaving one junction closer than this overlap
	int32 OverloadedJunctionRoadCount = 6;      // more roads than this at one junction
	double FloatingToleranceUnits = 100.0;      // generated building bounds this far above Z=0 are "floating"
	int32 MaxFootprintCorners = 256;            // O(n^2) self-intersection test is skipped above this
	bool bCheckRoadCrossings = true;
	bool bCheckBuildingRoadConflicts = true;
	bool bCheckBuildingOverlaps = true;

	/** After this many issues with the same code, further ones are counted but not stored (one Info summary is added). Blocking issues still invalidate the report. */
	int32 MaxIssuesPerCode = 200;

	/** Meridian package: missing locked district files downgrade from ERROR to WARNING (partial developer runs). */
	bool bAllowMissingDistrictPackages = false;
};

/** Everything a validator may look at. Pointers are null when that input does not exist yet (e.g. World before generation). */
struct VOIDWORLDBUILDERCORE_API FVoidValidationInput
{
	const FVoidDesignPackage* Package = nullptr;
	UWorld* World = nullptr;

	/** District ids from the Meridian DistrictRegistry, if it was validated. Empty = unknown, reference checks against it are skipped. */
	TSet<FName> KnownDistrictIds;

	/** Generators that have finished (successfully or not) so far in this pipeline run. World validators only expect output from these. */
	TSet<FName> ExecutedGenerators;

	/** File path or description of where Package came from. */
	FString PackageSource;
};

/**
 * FVoidValidationContext
 *
 * Write-side of validation. Adds throttling so a degenerate 1M-road dataset
 * yields a readable report rather than a 1M-line one, and stamps every issue
 * with the current subsystem/source so validators cannot forget to.
 */
class VOIDWORLDBUILDERCORE_API FVoidValidationContext
{
public:
	FVoidValidationContext(FVoidValidationReport& InReport, const FVoidValidationOptions& InOptions, FName InSubsystem = NAME_None, FString InSource = FString());

	const FVoidValidationOptions& Options;

	void SetSubsystem(FName InSubsystem) { Subsystem = InSubsystem; }
	void SetSource(FString InSource) { Source = MoveTemp(InSource); }

	/** The one emit path. Location may be null. */
	void Emit(EVoidValidationSeverity Severity, FName Code, FString Message, FString ObjectId, FString FieldPath, FString SuggestedFix, const FVector* Location = nullptr);

	void Info(FName Code, FString Message, FString ObjectId = FString(), FString FieldPath = FString(), FString SuggestedFix = FString(), const FVector* Location = nullptr)
	{
		Emit(EVoidValidationSeverity::Info, Code, MoveTemp(Message), MoveTemp(ObjectId), MoveTemp(FieldPath), MoveTemp(SuggestedFix), Location);
	}
	void Warn(FName Code, FString Message, FString ObjectId = FString(), FString FieldPath = FString(), FString SuggestedFix = FString(), const FVector* Location = nullptr)
	{
		Emit(EVoidValidationSeverity::Warning, Code, MoveTemp(Message), MoveTemp(ObjectId), MoveTemp(FieldPath), MoveTemp(SuggestedFix), Location);
	}
	void Error(FName Code, FString Message, FString ObjectId = FString(), FString FieldPath = FString(), FString SuggestedFix = FString(), const FVector* Location = nullptr)
	{
		Emit(EVoidValidationSeverity::Error, Code, MoveTemp(Message), MoveTemp(ObjectId), MoveTemp(FieldPath), MoveTemp(SuggestedFix), Location);
	}
	void Fatal(FName Code, FString Message, FString ObjectId = FString(), FString FieldPath = FString(), FString SuggestedFix = FString(), const FVector* Location = nullptr)
	{
		Emit(EVoidValidationSeverity::Fatal, Code, MoveTemp(Message), MoveTemp(ObjectId), MoveTemp(FieldPath), MoveTemp(SuggestedFix), Location);
	}

	/** Adds one Info per throttled code ("N further VOID.X issues suppressed"). Idempotent; call once when a validator run is finished. */
	void FlushSuppressionSummary();

	FVoidValidationReport& GetReport() { return Report; }

private:
	FVoidValidationReport& Report;
	FName Subsystem;
	FString Source;
	TMap<FName, int32> CodeCounts;
	TMap<FName, int32> SuppressedCounts;
};

/**
 * IVoidValidator
 *
 * Extension point for every subsystem. A generator agent that wants its own
 * output validated implements this and registers it with
 * FVoidValidatorRegistry from its module's StartupModule -- no dependency on
 * the Validation module is needed (this header lives in Core).
 *
 * Contract:
 *  - Validate must be const/stateless and must not spawn, modify or delete anything.
 *  - Must tolerate null Package / World (skip what cannot be checked; say so with an Info).
 *  - Must never crash on malformed data: the whole point is to run on bad data.
 *  - Should be roughly O(N log N) or O(N) in the dataset size. No all-pairs loops.
 */
class VOIDWORLDBUILDERCORE_API IVoidValidator
{
public:
	virtual ~IVoidValidator() = default;

	/** Stable unique id, e.g. "VOID.Package.Roads". Registry rejects duplicates. */
	virtual FName GetValidatorId() const = 0;

	/** Subsystem label stamped on issues this validator emits. */
	virtual FName GetSubsystem() const = 0;

	virtual bool AppliesToStage(EVoidValidationStage Stage) const = 0;

	/** Lower runs first within a stage. Ties are broken by id, so ordering is deterministic. */
	virtual int32 GetOrder() const { return 100; }

	virtual void Validate(EVoidValidationStage Stage, const FVoidValidationInput& Input, FVoidValidationContext& Context) const = 0;
};

/**
 * FVoidValidatorRegistry
 *
 * Mirrors FVoidGeneratorRegistry / FVoidPackageReaderRegistry, with two
 * deliberate differences learned from the audit of those:
 *  1. Duplicate ids are REJECTED and recorded (GetRejectedDuplicateIds), not
 *     silently overwritten -- an overwrite hides an ownership conflict
 *     between two agents' code.
 *  2. Enumeration order is deterministic (stage, order, id) -- TMap
 *     iteration order is not.
 * Publicly constructible so tests can use an isolated instance.
 */
class VOIDWORLDBUILDERCORE_API FVoidValidatorRegistry
{
public:
	static FVoidValidatorRegistry& Get();

	/** @return false (and records the id) if a validator with the same id is already registered; the original stays. */
	bool Register(TSharedRef<IVoidValidator> Validator);
	void Unregister(FName ValidatorId);

	TSharedPtr<IVoidValidator> Find(FName ValidatorId) const;
	TArray<TSharedRef<IVoidValidator>> GetForStage(EVoidValidationStage Stage) const;
	TArray<TSharedRef<IVoidValidator>> GetAll() const;
	TArray<FName> GetRejectedDuplicateIds() const { return RejectedDuplicateIds; }
	int32 Num() const { return Validators.Num(); }

	/** Runs every validator for Stage into Report, in deterministic order. Flushes each validator's throttle summary. */
	void RunStage(EVoidValidationStage Stage, const FVoidValidationInput& Input, const FVoidValidationOptions& Options, FVoidValidationReport& Report) const;

private:
	TMap<FName, TSharedRef<IVoidValidator>> Validators;
	TArray<FName> RejectedDuplicateIds;
};
