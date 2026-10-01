// Standalone (no Unreal) verification of the engine-independent validation
// algorithms. Build & run:  g++ -std=c++17 -Wall -Wextra -I../../Plugins/VOIDWorldBuilder/Source/VOIDWorldBuilderValidation/Private standalone_tests.cpp -o st && ./st
#include "VoidSha256.h"
#include "VoidValidationMath.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace VoidValidationMath;
static int Failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++Failures; } } while (0)
static std::string Hex(const std::string& S) { uint8_t O[32]; VoidValidationHash::Sha256((const uint8_t*)S.data(), S.size(), O); char B[65]; for (int i = 0; i < 32; ++i) std::snprintf(B + i * 2, 3, "%02x", O[i]); return B; }
int main()
{
	// NIST FIPS 180-4 vectors
	CHECK(Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	CHECK(Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	CHECK(Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
	CHECK(Hex(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
	// padding boundaries: 55, 56, 63, 64, 65 bytes must not crash and must differ
	std::string Prev; for (int n : {55, 56, 63, 64, 65}) { std::string H = Hex(std::string(n, 'x')); CHECK(H != Prev); Prev = H; }
	// geometry
	FV2 A{0,0}, B{10,0}, C{5,-5}, D{5,5};
	FV2 P; CHECK(SegmentsProperlyCross(A,B,C,D,&P)); CHECK(std::fabs(P.X-5)<1e-9 && std::fabs(P.Y)<1e-9);
	CHECK(!SegmentsProperlyCross(A,B,FV2{10,0},FV2{10,5}));          // touching endpoints = junction, not crossing
	CHECK(!SegmentsProperlyCross(A,B,FV2{0,1},FV2{10,1}));           // parallel
	CHECK(!SegmentsProperlyCross(A,B,FV2{2,0},FV2{8,0}));            // collinear overlap
	FV2 Sq[4] = {{0,0},{10,0},{10,10},{0,10}}; FV2 Cw[4] = {{0,0},{0,10},{10,10},{10,0}}; FV2 Bow[4] = {{0,0},{10,10},{10,0},{0,10}};
	CHECK(PolygonSignedArea(Sq,4) == 100.0); CHECK(PolygonSignedArea(Cw,4) == -100.0);
	CHECK(!PolygonSelfIntersects(Sq,4)); CHECK(PolygonSelfIntersects(Bow,4));
	CHECK(PointInPolygon(FV2{5,5},Sq,4)); CHECK(!PointInPolygon(FV2{15,5},Sq,4));
	FV2 Far[4] = {{20,20},{30,20},{30,30},{20,30}}, Over[4] = {{5,5},{15,5},{15,15},{5,15}}, Inside[4] = {{2,2},{4,2},{4,4},{2,4}};
	CHECK(!PolygonsOverlap(Sq,4,Far,4)); CHECK(PolygonsOverlap(Sq,4,Over,4)); CHECK(PolygonsOverlap(Sq,4,Inside,4)); CHECK(PolygonsOverlap(Inside,4,Sq,4));
	CHECK(std::fabs(PointSegmentDistSq(FV2{5,3},A,B)-9.0)<1e-9); CHECK(std::fabs(PointSegmentDistSq(FV2{-3,4},A,B)-25.0)<1e-9);
	CHECK(std::fabs(AngleBetweenDegrees(FV2{1,0},FV2{0,1})-90.0)<1e-9); CHECK(std::fabs(AngleBetweenDegrees(FV2{1,0},FV2{-1,0})-180.0)<1e-9); CHECK(AngleBetweenDegrees(FV2{0,0},FV2{1,0})==0.0);
	CHECK(!IsFinite(FV2{NAN,0})); CHECK(!IsFinite(FV2{0,INFINITY})); CHECK(IsFinite(FV2{1,2}));
	std::printf(Failures ? "%d FAILURE(S)\n" : "ALL STANDALONE TESTS PASSED\n", Failures);
	return Failures ? 1 : 0;
}
