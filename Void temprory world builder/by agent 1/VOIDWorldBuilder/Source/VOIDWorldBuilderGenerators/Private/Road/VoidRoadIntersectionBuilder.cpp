// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Road/VoidRoadIntersectionBuilder.h"

namespace VoidRoadIntersectionPrivate
{
	struct FEndpointRecord
	{
		int32 RoadIndex = INDEX_NONE;
		bool bIsStart = true;
		FVector Location = FVector::ZeroVector;
	};

	static int32 FindRoot(TArray<int32>& Parent, int32 Index)
	{
		while (Parent[Index] != Index)
		{
			Parent[Index] = Parent[Parent[Index]]; // path halving
			Index = Parent[Index];
		}
		return Index;
	}

	static void Union(TArray<int32>& Parent, int32 A, int32 B)
	{
		const int32 RootA = FindRoot(Parent, A);
		const int32 RootB = FindRoot(Parent, B);
		if (RootA != RootB)
		{
			Parent[RootA] = RootB;
		}
	}
}

TArray<FVoidRoadJunction> FVoidRoadIntersectionBuilder::BuildJunctionGraph(const TArray<FVoidBuiltRoad>& BuiltRoads, float ToleranceUnits)
{
	using namespace VoidRoadIntersectionPrivate;

	TArray<FVoidRoadJunction> Junctions;

	TArray<FEndpointRecord> Endpoints;
	TMap<FName, int32> RoadIdToBuiltIndex;

	for (int32 RoadIndex = 0; RoadIndex < BuiltRoads.Num(); ++RoadIndex)
	{
		RoadIdToBuiltIndex.Add(BuiltRoads[RoadIndex].Spec.Id.Value, RoadIndex);
	}

	for (int32 RoadIndex = 0; RoadIndex < BuiltRoads.Num(); ++RoadIndex)
	{
		const FVoidBuiltRoad& Road = BuiltRoads[RoadIndex];
		if (Road.Spec.RoadType == EVoidRoadType::Roundabout || Road.Spec.bClosedLoop || Road.Points.Num() < 2)
		{
			continue; // Roundabouts have no simple start/end; handled as spur targets below.
		}

		FEndpointRecord Start;
		Start.RoadIndex = RoadIndex;
		Start.bIsStart = true;
		Start.Location = Road.Points[0];
		Endpoints.Add(Start);

		FEndpointRecord End;
		End.RoadIndex = RoadIndex;
		End.bIsStart = false;
		End.Location = Road.Points.Last();
		Endpoints.Add(End);
	}

	const int32 NumEndpoints = Endpoints.Num();
	TArray<int32> Parent;
	Parent.SetNum(NumEndpoints);
	for (int32 Index = 0; Index < NumEndpoints; ++Index)
	{
		Parent[Index] = Index;
	}

	const float ToleranceSq = FMath::Square(ToleranceUnits);

	// Pass 1: coincident-endpoint clustering (2D distance).
	for (int32 A = 0; A < NumEndpoints; ++A)
	{
		for (int32 B = A + 1; B < NumEndpoints; ++B)
		{
			const float DistSq = FVector2D(Endpoints[A].Location - Endpoints[B].Location).SizeSquared();
			if (DistSq <= ToleranceSq)
			{
				Union(Parent, A, B);
			}
		}
	}

	auto FindRoadEndpointIndices = [&Endpoints](int32 RoadIndex, int32& OutStartIdx, int32& OutEndIdx)
	{
		OutStartIdx = INDEX_NONE;
		OutEndIdx = INDEX_NONE;
		for (int32 Index = 0; Index < Endpoints.Num(); ++Index)
		{
			if (Endpoints[Index].RoadIndex == RoadIndex)
			{
				if (Endpoints[Index].bIsStart) { OutStartIdx = Index; }
				else { OutEndIdx = Index; }
			}
		}
	};

	TMultiMap<int32, int32> RoundaboutSpurs; // roundabout BuiltRoads index -> spur endpoint index
	TSet<int32> SpurConsumedEndpoints;

	// Pass 2: explicit ConnectionIds, plus plain coincidence against any roundabout's center.
	for (int32 RoadIndex = 0; RoadIndex < BuiltRoads.Num(); ++RoadIndex)
	{
		const FVoidBuiltRoad& Road = BuiltRoads[RoadIndex];
		if (Road.Spec.RoadType == EVoidRoadType::Roundabout)
		{
			continue;
		}

		int32 ThisStart = INDEX_NONE;
		int32 ThisEnd = INDEX_NONE;
		FindRoadEndpointIndices(RoadIndex, ThisStart, ThisEnd);

		for (const FVoidElementId& ConnectionId : Road.Spec.ConnectionIds)
		{
			const int32* TargetIndexPtr = RoadIdToBuiltIndex.Find(ConnectionId.Value);
			if (!TargetIndexPtr)
			{
				continue; // Unresolvable connection -- FVoidRoadValidator reports this separately.
			}

			const int32 TargetIndex = *TargetIndexPtr;
			const FVoidBuiltRoad& TargetRoad = BuiltRoads[TargetIndex];

			if (TargetRoad.Spec.RoadType == EVoidRoadType::Roundabout)
			{
				if (TargetRoad.Points.Num() > 0)
				{
					const FVector& Center = TargetRoad.Points[0];
					float BestDistSq = TNumericLimits<float>::Max();
					int32 BestEndpoint = INDEX_NONE;

					for (int32 Candidate : { ThisStart, ThisEnd })
					{
						if (Candidate == INDEX_NONE) { continue; }
						const float DistSq = FVector2D(Endpoints[Candidate].Location - Center).SizeSquared();
						if (DistSq < BestDistSq) { BestDistSq = DistSq; BestEndpoint = Candidate; }
					}

					if (BestEndpoint != INDEX_NONE)
					{
						RoundaboutSpurs.Add(TargetIndex, BestEndpoint);
						SpurConsumedEndpoints.Add(BestEndpoint);
					}
				}
				continue;
			}

			int32 TargetStart = INDEX_NONE;
			int32 TargetEnd = INDEX_NONE;
			FindRoadEndpointIndices(TargetIndex, TargetStart, TargetEnd);

			int32 BestA = INDEX_NONE;
			int32 BestB = INDEX_NONE;
			float BestDistSq = TNumericLimits<float>::Max();

			for (int32 CandidateA : { ThisStart, ThisEnd })
			{
				if (CandidateA == INDEX_NONE) { continue; }
				for (int32 CandidateB : { TargetStart, TargetEnd })
				{
					if (CandidateB == INDEX_NONE) { continue; }
					const float DistSq = FVector2D(Endpoints[CandidateA].Location - Endpoints[CandidateB].Location).SizeSquared();
					if (DistSq < BestDistSq) { BestDistSq = DistSq; BestA = CandidateA; BestB = CandidateB; }
				}
			}

			if (BestA != INDEX_NONE && BestB != INDEX_NONE)
			{
				Union(Parent, BestA, BestB);
			}
		}

		// Plain coincidence against any roundabout's center, even without an explicit ConnectionId.
		for (int32 OtherIndex = 0; OtherIndex < BuiltRoads.Num(); ++OtherIndex)
		{
			if (BuiltRoads[OtherIndex].Spec.RoadType != EVoidRoadType::Roundabout || BuiltRoads[OtherIndex].Points.Num() == 0)
			{
				continue;
			}

			const FVector& Center = BuiltRoads[OtherIndex].Points[0];

			for (int32 EndpointIndex : { ThisStart, ThisEnd })
			{
				if (EndpointIndex == INDEX_NONE || SpurConsumedEndpoints.Contains(EndpointIndex))
				{
					continue;
				}

				const float DistSq = FVector2D(Endpoints[EndpointIndex].Location - Center).SizeSquared();
				if (DistSq <= ToleranceSq)
				{
					RoundaboutSpurs.Add(OtherIndex, EndpointIndex);
					SpurConsumedEndpoints.Add(EndpointIndex);
				}
			}
		}
	}

	// Emit one RoundaboutSpur junction per spur endpoint.
	for (const TPair<int32, int32>& SpurPair : RoundaboutSpurs)
	{
		const int32 SpurEndpointIndex = SpurPair.Value;
		const FEndpointRecord& Endpoint = Endpoints[SpurEndpointIndex];
		const FVoidRoadSpec& SpurRoadSpec = BuiltRoads[Endpoint.RoadIndex].Spec;

		FVoidRoadJunction Junction;
		Junction.Location = Endpoint.Location;
		Junction.Type = EVoidRoadJunctionType::RoundaboutSpur;
		Junction.ConnectedRoadIds.Add(SpurRoadSpec.Id);
		Junction.ConnectedRoadIds.Add(BuiltRoads[SpurPair.Key].Spec.Id);
		Junction.PadRadius = FMath::Max(SpurRoadSpec.WidthUnits * 0.5f, 50.0f);
		Junctions.Add(Junction);
	}

	// Group remaining (non-spur-consumed) endpoints by Union-Find root.
	TMap<int32, TArray<int32>> Clusters;
	for (int32 Index = 0; Index < NumEndpoints; ++Index)
	{
		if (SpurConsumedEndpoints.Contains(Index))
		{
			continue;
		}
		Clusters.FindOrAdd(FindRoot(Parent, Index)).Add(Index);
	}

	for (const TPair<int32, TArray<int32>>& ClusterPair : Clusters)
	{
		const TArray<int32>& ClusterEndpoints = ClusterPair.Value;

		TSet<FName> DistinctRoadIds;
		FVector SumLocation = FVector::ZeroVector;
		float MaxWidth = 0.0f;
		TArray<FVoidElementId> ConnectedIds;

		for (int32 EndpointIndex : ClusterEndpoints)
		{
			const FEndpointRecord& Endpoint = Endpoints[EndpointIndex];
			const FVoidRoadSpec& Spec = BuiltRoads[Endpoint.RoadIndex].Spec;

			if (!DistinctRoadIds.Contains(Spec.Id.Value))
			{
				DistinctRoadIds.Add(Spec.Id.Value);
				ConnectedIds.Add(Spec.Id);
			}

			SumLocation += Endpoint.Location;
			MaxWidth = FMath::Max(MaxWidth, Spec.WidthUnits);
		}

		FVoidRoadJunction Junction;
		Junction.Location = SumLocation / static_cast<float>(ClusterEndpoints.Num());
		Junction.ConnectedRoadIds = ConnectedIds;
		Junction.PadRadius = FMath::Max(MaxWidth * 0.5f, 50.0f);

		const int32 DistinctCount = DistinctRoadIds.Num();
		if (DistinctCount <= 1)
		{
			const FEndpointRecord& OnlyEndpoint = Endpoints[ClusterEndpoints[0]];
			const FVoidRoadSpec& OnlySpec = BuiltRoads[OnlyEndpoint.RoadIndex].Spec;
			const bool bIsCulDeSac = OnlySpec.bCulDeSacAtEnd && !OnlyEndpoint.bIsStart;
			Junction.Type = bIsCulDeSac ? EVoidRoadJunctionType::CulDeSac : EVoidRoadJunctionType::DeadEnd;
		}
		else if (DistinctCount == 2)
		{
			Junction.Type = EVoidRoadJunctionType::TwoWayJoin;
		}
		else if (DistinctCount == 3)
		{
			Junction.Type = EVoidRoadJunctionType::TJunction;
		}
		else if (DistinctCount == 4)
		{
			Junction.Type = EVoidRoadJunctionType::FourWayJunction;
		}
		else
		{
			Junction.Type = EVoidRoadJunctionType::Complex;
		}

		Junctions.Add(Junction);
	}

	return Junctions;
}

FVoidRoadMeshSection FVoidRoadIntersectionBuilder::BuildJunctionPad(const FVoidRoadJunction& Junction, const FLinearColor& VertexColor, int32 NumSegments)
{
	FVoidRoadMeshSection Section;

	if (Junction.PadRadius <= 0.0f || NumSegments < 3)
	{
		return Section;
	}

	TArray<FVector> RingPoints;
	RingPoints.Reserve(NumSegments);
	for (int32 Index = 0; Index < NumSegments; ++Index)
	{
		const float Angle = (2.0f * PI * Index) / static_cast<float>(NumSegments);
		RingPoints.Add(Junction.Location + FVector(Junction.PadRadius * FMath::Cos(Angle), Junction.PadRadius * FMath::Sin(Angle), 0.0f));
	}

	for (int32 Index = 0; Index < NumSegments; ++Index)
	{
		const FVector& A = Junction.Location;
		const FVector& B = RingPoints[Index];
		const FVector& C = RingPoints[(Index + 1) % NumSegments];

		FVector Normal = FVector::CrossProduct(B - A, C - A);
		Normal = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
		const FProcMeshTangent Tangent((B - A).GetSafeNormal(), false);

		const int32 BaseIndex = Section.Vertices.Num();
		Section.Vertices.Add(A);
		Section.Vertices.Add(B);
		Section.Vertices.Add(C);
		Section.Normals.Add(Normal);
		Section.Normals.Add(Normal);
		Section.Normals.Add(Normal);
		Section.UVs.Add(FVector2D(0.5f, 0.5f));
		Section.UVs.Add(FVector2D(0.5f + 0.5f * FMath::Cos((2.0f * PI * Index) / NumSegments), 0.5f + 0.5f * FMath::Sin((2.0f * PI * Index) / NumSegments)));
		Section.UVs.Add(FVector2D(0.5f + 0.5f * FMath::Cos((2.0f * PI * (Index + 1)) / NumSegments), 0.5f + 0.5f * FMath::Sin((2.0f * PI * (Index + 1)) / NumSegments)));
		Section.VertexColors.Add(VertexColor);
		Section.VertexColors.Add(VertexColor);
		Section.VertexColors.Add(VertexColor);
		Section.Tangents.Add(Tangent);
		Section.Tangents.Add(Tangent);
		Section.Tangents.Add(Tangent);

		Section.Triangles.Add(BaseIndex);
		Section.Triangles.Add(BaseIndex + 1);
		Section.Triangles.Add(BaseIndex + 2);
	}

	return Section;
}

FVoidRoadMeshSection FVoidRoadIntersectionBuilder::BuildCrosswalkStripe(
	const FVector& JunctionCenter,
	const FVector2D& ApproachDirection2D,
	float RoadWidth,
	float StripeZoneLength,
	const FLinearColor& StripeColorA,
	const FLinearColor& StripeColorB,
	int32 NumStripes)
{
	FVoidRoadMeshSection Section;

	if (RoadWidth <= 0.0f || StripeZoneLength <= 0.0f || NumStripes < 1)
	{
		return Section;
	}

	FVector2D Direction = ApproachDirection2D.IsNearlyZero() ? FVector2D(1.0f, 0.0f) : ApproachDirection2D.GetSafeNormal();
	const FVector2D Perpendicular(Direction.Y, -Direction.X);

	const float StripeSpacing = StripeZoneLength / static_cast<float>(NumStripes);
	const float StripeDepth = StripeSpacing * 0.6f; // leaves a gap between stripes for the classic crosswalk look
	const float HalfWidth = RoadWidth * 0.5f;
	constexpr float ZLift = 1.0f; // avoid z-fighting with the road surface directly beneath

	for (int32 StripeIndex = 0; StripeIndex < NumStripes; ++StripeIndex)
	{
		const float NearDist = StripeIndex * StripeSpacing;
		const float FarDist = NearDist + StripeDepth;

		const FVector2D NearCenter = FVector2D(JunctionCenter.X, JunctionCenter.Y) + Direction * NearDist;
		const FVector2D FarCenter = FVector2D(JunctionCenter.X, JunctionCenter.Y) + Direction * FarDist;

		const FVector NearLeft(NearCenter.X + Perpendicular.X * -HalfWidth, NearCenter.Y + Perpendicular.Y * -HalfWidth, JunctionCenter.Z + ZLift);
		const FVector NearRight(NearCenter.X + Perpendicular.X * HalfWidth, NearCenter.Y + Perpendicular.Y * HalfWidth, JunctionCenter.Z + ZLift);
		const FVector FarRight(FarCenter.X + Perpendicular.X * HalfWidth, FarCenter.Y + Perpendicular.Y * HalfWidth, JunctionCenter.Z + ZLift);
		const FVector FarLeft(FarCenter.X + Perpendicular.X * -HalfWidth, FarCenter.Y + Perpendicular.Y * -HalfWidth, JunctionCenter.Z + ZLift);

		const FLinearColor& StripeColor = (StripeIndex % 2 == 0) ? StripeColorA : StripeColorB;
		const FProcMeshTangent Tangent(Direction.X, Direction.Y, 0.0f);

		const int32 BaseIndex = Section.Vertices.Num();
		Section.Vertices.Add(NearLeft);
		Section.Vertices.Add(NearRight);
		Section.Vertices.Add(FarRight);
		Section.Vertices.Add(FarLeft);

		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			Section.Normals.Add(FVector::UpVector);
			Section.VertexColors.Add(StripeColor);
			Section.Tangents.Add(Tangent);
		}

		Section.UVs.Add(FVector2D(0.0f, 0.0f));
		Section.UVs.Add(FVector2D(1.0f, 0.0f));
		Section.UVs.Add(FVector2D(1.0f, 1.0f));
		Section.UVs.Add(FVector2D(0.0f, 1.0f));

		Section.Triangles.Add(BaseIndex);
		Section.Triangles.Add(BaseIndex + 1);
		Section.Triangles.Add(BaseIndex + 2);
		Section.Triangles.Add(BaseIndex);
		Section.Triangles.Add(BaseIndex + 2);
		Section.Triangles.Add(BaseIndex + 3);
	}

	return Section;
}
