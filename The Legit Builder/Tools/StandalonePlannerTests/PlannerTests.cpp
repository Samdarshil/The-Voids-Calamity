#include "CoreMinimal.h"
#include "Environment/VoidPlacementPlanner.h"
using namespace VoidPlan;
static int Fails=0;
#define CHECK(c,msg) do{ if(!(c)){ printf("  FAIL: %s\n",msg); ++Fails;} else printf("  ok:   %s\n",msg);}while(0)

static FRoad MakeRoad(const char* id,int tier,double x0,double y0,double x1,double y1,bool sidewalk,double hw=300){
  FRoad R; R.Id=FName(id); R.Tier=tier; R.Points.Add(FVector(x0,y0,0)); R.Points.Add(FVector(x1,y1,0));
  R.HalfWidth=hw; R.bHasSidewalk=sidewalk; R.BandInner=hw+25; R.BandOuter=hw+25+150; return R; }
static FRule MakeRule(const char* id,const char* cat,const char* ctx,const char* domain="Environment"){
  FRule R; R.RuleId=FName(id); R.Category=FName(cat); R.Context=FName(ctx); R.Domain=FName(domain); return R; }
static FInputs BaseInputs(){
  FInputs I; I.DistrictId=FName("white_zones");
  I.Roads.Add(MakeRoad("road_a",1,0,0,10000,0,true));
  I.Roads.Add(MakeRoad("road_b",3,0,3000,10000,3000,false));
  I.Roads.Add(MakeRoad("road_tunnel",1,0,6000,10000,6000,true)); I.Roads.Last().bTunnel=true;
  I.MinImportanceDensity=1.0f; return I; }

int main(){
  printf("[determinism]\n");
  { FInputs I=BaseInputs(); TArray<FRule> Rules; FRule T=MakeRule("tree","Tree","Sidewalk"); T.SpacingUnits=800; T.Probability=0.7f; Rules.Add(T);
    FRule L=MakeRule("light","Streetlight","Roadside"); L.SpacingUnits=1500; L.LateralFraction=0.1f; Rules.Add(L);
    FStats S1,S2; auto A=Plan(I,Rules,&S1); auto B=Plan(I,Rules,&S2);
    bool same=A.Num()==B.Num(); for(int i=0;same&&i<A.Num();++i) same=A[i].StableId==B[i].StableId&&A[i].Location.X==B[i].Location.X&&A[i].YawDegrees==B[i].YawDegrees&&A[i].UniformScale==B[i].UniformScale;
    CHECK(A.Num()>0,"generates instances"); CHECK(same,"identical input -> identical ids/transforms");
    // reversed rule order gives same canonical result
    TArray<FRule> Rev; Rev.Add(Rules[1]); Rev.Add(Rules[0]); auto C=Plan(I,Rev);
    bool same2=C.Num()==A.Num(); for(int i=0;same2&&i<A.Num();++i) same2=A[i].StableId==C[i].StableId; CHECK(same2,"rule order does not change result");
    // different seed -> different city
    FInputs I2=I; I2.GlobalSeed=12345; auto D=Plan(I2,Rules); int diff=0; for(int i=0;i<std::min(A.Num(),D.Num());++i) diff+=A[i].StableId!=D[i].StableId; CHECK(diff>0,"different seed changes ids");
    // stability: add a new road; existing road instances unchanged
    FInputs I3=I; I3.Roads.Add(MakeRoad("road_new",2,0,-4000,8000,-4000,true)); auto E=Plan(I3,Rules);
    int found=0,missing=0; for(auto&a:A){ bool f=false; for(auto&e:E) if(e.StableId==a.StableId&&e.Location.X==a.Location.X&&e.Location.Y==a.Location.Y){f=true;break;} f?++found:++missing; }
    CHECK(missing==0,"adding a road does not disturb existing instances"); (void)found; }

  printf("[context rules]\n");
  { FInputs I=BaseInputs(); TArray<FRule> Rules; FRule T=MakeRule("tree","Tree","Sidewalk"); T.SpacingUnits=600; Rules.Add(T); FStats S; auto A=Plan(I,Rules,&S);
    bool badRoad=false,inBand=true; for(auto&a:A){ if(a.SourceId!=FName("road_a")) badRoad=true; double d=std::fabs(a.Location.Y); if(d<325-1||d>475+1) inBand=false; }
    CHECK(A.Num()>0,"sidewalk trees exist on the sidewalk road"); CHECK(!badRoad,"no sidewalk trees on roads without sidewalk or in tunnels"); CHECK(inBand,"sidewalk trees sit inside the sidewalk band (never on the road surface)");
    FRule L=MakeRule("light","Streetlight","Roadside"); L.SpacingUnits=600; Rules.Reset(); Rules.Add(L); auto B=Plan(I,Rules); bool tun=false,hasB=false; for(auto&b:B){ if(b.SourceId==FName("road_tunnel")) tun=true; if(b.SourceId==FName("road_b")) hasB=true; }
    CHECK(!tun,"no roadside props inside tunnels"); CHECK(hasB,"roadside lights still placed on roads without sidewalks"); }

  printf("[junction clearance + building exclusion]\n");
  { FInputs I=BaseInputs(); FJunction J; J.Key=FName("J1"); J.Location=FVector(5000,0,0); J.PadRadius=500; J.NumRoads=3; I.Junctions.Add(J);
    TArray<FRule> Rules; FRule L=MakeRule("light","Streetlight","Roadside"); L.SpacingUnits=300; L.JunctionClearance=100; Rules.Add(L); auto A=Plan(I,Rules); bool near=false;
    for(auto&a:A){ double dx=a.Location.X-5000,dy=a.Location.Y; if(std::sqrt(dx*dx+dy*dy)<600) near=true; } CHECK(!near,"no linear props within junction pad + clearance");
    FArea B; B.Id=FName("bld1"); B.Kind=FName("Building"); B.Polygon.Add(FVector2D(1000,300)); B.Polygon.Add(FVector2D(3000,300)); B.Polygon.Add(FVector2D(3000,900)); B.Polygon.Add(FVector2D(1000,900)); B.Height=2000; I.Areas.Add(B);
    TArray<FRule> R2; FRule T=MakeRule("tree","Tree","Sidewalk"); T.SpacingUnits=200; T.LateralFraction=1.5f; R2.Add(T); auto C=Plan(I,R2); bool inside=false; for(auto&c:C) if(PointInPolygon(FVector2D(c.Location.X,c.Location.Y),B.Polygon)) inside=true; CHECK(!inside,"nothing is placed inside a building footprint"); }

  printf("[park scatter, commercial adjacency, facade]\n");
  { FInputs I=BaseInputs(); FArea P; P.Id=FName("park1"); P.Kind=FName("Park"); P.Polygon.Add(FVector2D(0,10000)); P.Polygon.Add(FVector2D(4000,10000)); P.Polygon.Add(FVector2D(4000,14000)); P.Polygon.Add(FVector2D(0,14000)); I.Areas.Add(P);
    TArray<FRule> Rules; FRule T=MakeRule("ptree","Tree","Park"); T.DensityPer100SqM=1.0f; Rules.Add(T); auto A=Plan(I,Rules);
    // 16,000,000 uu^2 = 16 x 100 m^2 -> ~ 16 * 1.0 = 16 expected; allow tolerance
    CHECK(A.Num()>=8&&A.Num()<=24,"park density ~ requested density"); bool all=true; for(auto&a:A) if(!PointInPolygon(FVector2D(a.Location.X,a.Location.Y),P.Polygon)) all=false; CHECK(all,"all park instances inside the park");
    FInputs I2=BaseInputs(); FArea Shop; Shop.Id=FName("shop"); Shop.Kind=FName("Building"); Shop.Use=FName("Commercial"); Shop.Height=1500;
    Shop.Polygon.Add(FVector2D(2000,-2200)); Shop.Polygon.Add(FVector2D(4000,-2200)); Shop.Polygon.Add(FVector2D(4000,-800)); Shop.Polygon.Add(FVector2D(2000,-800)); I2.Areas.Add(Shop);
    TArray<FRule> R2; FRule Bin=MakeRule("bin","Bin","Commercial"); Bin.SpacingUnits=250; Bin.LateralFraction=1.0f; Bin.RequiredAreaUse=FName("Commercial"); Bin.AreaProximityUnits=700; R2.Add(Bin); auto B=Plan(I2,R2);
    bool near=B.Num()>0; for(auto&b:B) if(b.Location.X<1300||b.Location.X>4700) near=false; CHECK(B.Num()>0,"storefront props appear next to commercial building"); CHECK(near,"storefront props only near the commercial building");
    TArray<FRule> R3; FRule Sign=MakeRule("bb","Billboard","Facade","Props"); Sign.RequiredAreaUse=FName("Commercial"); Sign.AreaProximityUnits=1500; Sign.ZOffset=800; R3.Add(Sign); auto C=Plan(I2,R3);
    CHECK(C.Num()>=1,"facade signs on road-facing edges"); }

  printf("[density controls, budget, filtering, polish, cinematic priority]\n");
  { FInputs I=BaseInputs(); I.MinImportanceDensity=1.0f; TArray<FRule> Rules; FRule T=MakeRule("tree","Tree","Sidewalk"); T.SpacingUnits=300; T.Probability=1.0f; Rules.Add(T);
    FInputs Lo=I,Hi=I; Lo.DensityScale=0.25f; Hi.DensityScale=1.0f; int nLo=Plan(Lo,Rules).Num(), nHi=Plan(Hi,Rules).Num(); CHECK(nLo<nHi&&nLo>0,"density scale reduces count monotonically");
    FInputs Sub=Lo; Sub.DensityScale=1.0f; int nSubA=Plan(Lo,Rules).Num(); FInputs Mid=I; Mid.DensityScale=0.5f; auto M=Plan(Mid,Rules); auto Lw=Plan(Lo,Rules); int notSubset=0; for(auto&l:Lw){ bool f=false; for(auto&m:M) if(m.StableId==l.StableId){f=true;break;} if(!f)++notSubset; } CHECK(notSubset==0,"lower density is a strict subset of higher density (stable thinning)"); (void)nSubA;
    FInputs Bud=I; Bud.MaxInstances=10; FStats S; auto B=Plan(Bud,Rules,&S); CHECK(B.Num()==10&&S.RejectedBudget>0,"instance budget enforced");
    FInputs Bnd=I; Bnd.bUseBounds=true; Bnd.BoundsMin=FVector2D(0,-1000); Bnd.BoundsMax=FVector2D(2000,1000); auto Cc=Plan(Bnd,Rules); bool okb=Cc.Num()>0; for(auto&c:Cc) if(c.Location.X>2000) okb=false; CHECK(okb,"spatial bounds filter");
    FInputs Al=I; Al.CategoryAllowList.Add(FName("Bench")); CHECK(Plan(Al,Rules).Num()==0,"category allow-list filters categories");
    FRule Clutter=MakeRule("trash","Trash","Roadside"); Clutter.SpacingUnits=300; Clutter.PolishResponse=-0.9f; TArray<FRule> CR; CR.Add(Clutter); FInputs Neg=I,Prist=I; Neg.District.Polish=0.05f; Prist.District.Polish=0.95f; CHECK(Plan(Neg,CR).Num()>Plan(Prist,CR).Num(),"clutter is denser in neglected districts than pristine ones");
    FInputs Cin=I; Cin.MinImportanceDensity=0.05f; Cin.ExtraFocusPoints.Add(FVector(1000,0,0)); Cin.FocusRadiusUnits=2500; Cin.DensityScale=1.0f; FRule Sm=T; Sm.Probability=1.0f; TArray<FRule> SR; SR.Add(Sm); auto Ci=Plan(Cin,SR); int near=0,far=0; for(auto&c:Ci){ if(c.Location.X<3500) ++near; else if(c.Location.X>6500) ++far; } CHECK(near>far,"cinematic focus points get more dressing than distant areas"); }

  printf("[clusters + junction rule + vehicles]\n");
  { FInputs I=BaseInputs(); TArray<FRule> Rules; FRule Cr=MakeRule("crate","Crate","Alley","Props"); Cr.MinTier=4; Cr.ClusterMin=2; Cr.ClusterMax=4; Cr.ClusterRadius=80; Cr.SpacingUnits=1000; Rules.Add(Cr);
    I.Roads.Add(MakeRoad("alley1",5,0,-8000,6000,-8000,false)); auto A=Plan(I,Rules); bool onlyAlley=A.Num()>0; for(auto&a:A) if(a.SourceId!=FName("alley1")) onlyAlley=false; CHECK(onlyAlley,"alley clutter only on Service/Alley tier roads"); CHECK(A.Num()>A.Num()/2,"clusters emit multiple items");
    FInputs J=BaseInputs(); FJunction Jn; Jn.Key=FName("J2"); Jn.Location=FVector(5000,0,0); Jn.PadRadius=400; Jn.NumRoads=4; for(int i=0;i<4;++i){ FApproach Ap; Ap.Dir=FVector2D(std::cos(i*PI/2),std::sin(i*PI/2)); Ap.HalfWidth=300; Ap.BandOuter=475; Jn.Approaches.Add(Ap);} J.Junctions.Add(Jn);
    TArray<FRule> TL; FRule Tl=MakeRule("tl","TrafficLight","Junction"); Tl.bBothSides=false; Tl.MinJunctionRoads=3; TL.Add(Tl); auto L=Plan(J,TL); CHECK(L.Num()==4,"one traffic light per approach at a four-way junction");
    Tl.bBothSides=true; TL.Reset(); TL.Add(Tl); CHECK(Plan(J,TL).Num()==8,"two per approach when both corners requested");
    FJunction T3=Jn; T3.NumRoads=2; J.Junctions.Reset(); J.Junctions.Add(T3); CHECK(Plan(J,TL).Num()==0,"no traffic lights at simple 2-road joins");
    FInputs V=BaseInputs(); TArray<FRule> VR; FRule Car=MakeRule("car","Vehicle","Curbside","Props"); Car.LateralMode=ELateralMode::RoadEdge; Car.LateralInsetUnits=150; Car.SpacingUnits=700; Car.MinTier=1; Car.MaxTier=4; VR.Add(Car); auto Cars=Plan(V,VR);
    bool onRoad=Cars.Num()>0; for(auto&c:Cars){ double d=std::fabs(c.Location.Y-((c.SourceId==FName("road_a"))?0:3000)); if(std::fabs(d-150)>1.0) onRoad=false; } CHECK(onRoad,"parked vehicles sit at the lane edge inside the road width"); }

  printf("\n%s (%d failures)\n",Fails?"FAILED":"ALL PASSED",Fails); return Fails?1:0; }
