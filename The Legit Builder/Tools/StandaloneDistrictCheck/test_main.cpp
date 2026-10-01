#include <cstring>
#include "District/VoidDistrictLayoutBuilder.h"
#include "District/VoidDistrictValidator.h"
#include "District/VoidDistrictGeometry.h"
#include "real_data.inc"
#include <iostream>

static void Dump(const FVoidDistrictLayoutPlan& P, bool Verbose)
{
	int E=0,W=0,I=0;
	for (auto& X : P.Report.Issues) {
		const char* Sev = X.Severity==EVoidValidationSeverity::Error?"ERROR":X.Severity==EVoidValidationSeverity::Warning?"WARN":X.Severity==EVoidValidationSeverity::Fatal?"FATAL":"info";
		if (X.Severity==EVoidValidationSeverity::Error||X.Severity==EVoidValidationSeverity::Fatal)E++; else if (X.Severity==EVoidValidationSeverity::Warning)W++; else I++;
		if (Verbose || X.Severity!=EVoidValidationSeverity::Info) printf("  [%s] %s  {%s}\n", Sev, *X.Message, *X.ErrorCode.ToString());
	}
	printf("  -> errors=%d warnings=%d info=%d\n",E,W,I);
}

int main()
{
	FVoidMeridianDataSet D; FillRealMeridian(D);
	FVoidDistrictLayoutInput In; In.Data=&D;
	FVoidDistrictCharacterOverride WZ; WZ.OpenSpaceRatio=0.25; In.CharacterOverrides.Add(FName("white_zones"),WZ);

	FVoidDistrictLayoutPlan Plan = FVoidDistrictLayoutBuilder::Build(In);
	FVoidDistrictValidator::Validate(Plan,D,In.Params);
	printf("== DEFAULT PARAMS on REAL Meridian data\n");
	for (auto& Dist : Plan.Districts) {
		printf("%-15s roads=%3d bld=%3d ps=%2d lm=%2d ports=%d subregions=%d  density-cover=%.2f open=%.2f stories=%d..%d\n", *Dist.Id.ToString(), Dist.Roads.Num(), Dist.Buildings.Num(), Dist.PublicSpaces.Num(), Dist.Landmarks.Num(), Dist.Ports.Num(), Dist.SubRegions.Num(), Dist.Profile.BuildingCoverage, Dist.Profile.OpenSpaceRatio, Dist.Profile.MinStories, Dist.Profile.MaxStories);
	}
	printf("live network roads=%d\n", Plan.LiveNetworkRoads.Num());
	Dump(Plan,false);

	printf("== district order:"); for (auto& Dist: Plan.Districts) printf(" %s",*Dist.Id.ToString()); printf("\n");
	printf("== profile provenance (white_zones):\n"); for (auto& L: Plan.FindDistrict("white_zones")->Profile.Provenance) printf("   %s\n",*L);

	// Determinism: identical second run.
	FVoidDistrictLayoutPlan Plan2 = FVoidDistrictLayoutBuilder::Build(In);
	bool Same = Plan.Districts.Num()==Plan2.Districts.Num();
	for (int i=0; Same && i<Plan.Districts.Num(); ++i) {
		auto&A=Plan.Districts[i]; auto&B=Plan2.Districts[i];
		Same = A.Buildings.Num()==B.Buildings.Num() && A.Roads.Num()==B.Roads.Num();
		for (int j=0; Same && j<A.Buildings.Num(); ++j) Same = A.Buildings[j].Spec.Id==B.Buildings[j].Spec.Id && A.Buildings[j].Center==B.Buildings[j].Center && A.Buildings[j].Spec.HeightUnits==B.Buildings[j].Spec.HeightUnits;
	}
	printf("== determinism (two builds identical): %s\n", Same?"PASS":"FAIL");

	// White zone nodes identical typology?
	{
		auto* W=Plan.FindDistrict("white_zones"); int perNode[8]={0}; double h0=0; bool ident=true; TArray<double> sig[8];
		for (auto& B: W->Buildings){ int n=-1; sscanf(*B.Spec.Id.Value.ToString()+strlen("white_zones.bld.n"),"%d",&n); sig[n].Add(B.Spec.HeightUnits);} 
		for (int n=1;n<4;++n) ident = ident && sig[n].size()==sig[0].size() && std::equal(sig[n].begin(),sig[n].end(),sig[0].begin());
		printf("== white zone nodes identical typology (heights): %s\n", ident?"PASS":"FAIL");
	}

	// Negative tests: validator must catch deliberately-broken plans.
	auto Count=[&](FVoidDistrictLayoutPlan& P,const char* Code){int c=0; for(auto&X:P.Report.Issues) if(X.ErrorCode==FName(Code)) c++; return c;};
	{
		FVoidDistrictLayoutPlan Bad = FVoidDistrictLayoutBuilder::Build(In);
		Bad.FindDistrict("white_zones")->Buildings[0].Center = FVector2D(0,0);                   // outside boundary, on top of Spire tower
		Bad.FindDistrict("olympus_spire")->Roads.Last().Spec.CenterlinePoints[0] = FVector2D(9000,9000); // dangling endpoint
		FVoidDistrictValidator::Validate(Bad,D,In.Params);
		printf("== negative: outside-boundary=%d duplicate/overlap=%d dead-end=%d\n", Count(Bad,"VOID.District.BuildingOutsideBoundary"), Count(Bad,"VOID.District.DuplicateGeometry")+Count(Bad,"VOID.District.BuildingsOverlap"), Count(Bad,"VOID.District.RoadDeadEnd"));
	}
	{
		FVoidDistrictLayoutPlan Bad = FVoidDistrictLayoutBuilder::Build(In);
		auto* Sp=Bad.FindDistrict("olympus_spire"); Sp->PublicSpaces[0].HoleRadius=0; // park/plaza over the tower
		auto* WZd=Bad.FindDistrict("white_zones"); FVoidBox2D B; B.Center=WZd->Buildings[0].Center; B.HalfExtent=FVector2D(200,200);
		WZd->Buildings.Add(WZd->Buildings[0]); WZd->Buildings.Last().Spec.Id=FVoidElementId(FName("dup")); // duplicate geometry
		FVoidDistrictValidator::Validate(Bad,D,In.Params);
		printf("== negative: plaza-over-major-structure=%d duplicate-geometry=%d\n", Count(Bad,"VOID.District.PublicSpaceOverlapsMajorStructure"), Count(Bad,"VOID.District.DuplicateGeometry"));
	}
	{
		FVoidDistrictLayoutInput Wide=In; Wide.Params.ArchivesBoundaryHalfSize=15000; Wide.Params.SeamAngleDegrees=45.0; // ring/node clash?
		FVoidDistrictLayoutPlan P = FVoidDistrictLayoutBuilder::Build(Wide); FVoidDistrictValidator::Validate(P,D,Wide.Params);
		printf("== variant seam angle 45deg (in a node's sector): "); Dump(P,false);
	}
	{
		FVoidDistrictLayoutInput V=In; V.Params.SpokeCount=6; V.Params.EliteTowerCount=6;
		FVoidDistrictLayoutPlan P = FVoidDistrictLayoutBuilder::Build(V); FVoidDistrictValidator::Validate(P,D,V.Params);
		printf("== variant 6 spokes / 6 elite towers: "); Dump(P,false);
	}
	{
		FVoidDistrictLayoutInput V=In; V.Params.SpokeCount=3; V.Params.EliteTowerCount=3;
		FVoidDistrictLayoutPlan P = FVoidDistrictLayoutBuilder::Build(V); FVoidDistrictValidator::Validate(P,D,V.Params);
		printf("== variant 3 spokes: "); Dump(P,false);
	}
	{
		FVoidDistrictLayoutInput V=In; V.Params.BlockSize=1500;
		FVoidDistrictLayoutPlan P = FVoidDistrictLayoutBuilder::Build(V); FVoidDistrictValidator::Validate(P,D,V.Params);
		printf("== variant tiny BlockSize (must error, not crash): "); Dump(P,false);
	}
	{
		FVoidDistrictLayoutPlan Bad = FVoidDistrictLayoutBuilder::Build(In);
		auto* WZd=Bad.FindDistrict("white_zones");
		// wall of a building 100 m tall directly between a plaza and the Spire
		for (auto& B: WZd->Buildings) B.Spec.HeightUnits=6000000;
		auto* Ar=Bad.FindDistrict("sector_0"); Ar->Boundary.Z=0; // Sector 0 at grade
		FVoidDistrictValidator::Validate(Bad,D,In.Params);
		printf("== negative: sector0-at-grade=%d spire-sightline-warning=%d\n", Count(Bad,"VOID.District.Sector0Sightline"), Count(Bad,"VOID.District.SpireSightlineBlocked"));
	}
	return 0;
}
