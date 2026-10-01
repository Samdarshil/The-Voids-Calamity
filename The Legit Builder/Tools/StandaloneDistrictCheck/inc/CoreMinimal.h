// Minimal standalone stand-in for the parts of UE Core used by the pure-logic district code.
// Test harness only. NOT part of the plugin.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <optional>
#include <initializer_list>

using int32 = int32_t; using uint32 = uint32_t; using uint8 = uint8_t; using int64 = int64_t;
using TCHAR = char;
#define TEXT(x) x
#define INDEX_NONE (-1)
#define PI 3.14159265358979323846
#define USTRUCT(...)
#define UENUM(...)
#define UPROPERTY(...)
#define UCLASS(...)
#define UFUNCTION(...)
#define GENERATED_BODY()
#define UMETA(...)
#define VOIDWORLDBUILDERCORE_API
#define VOIDWORLDBUILDERIMPORT_API
#define VOIDWORLDBUILDERGENERATORS_API
#define WITH_DEV_AUTOMATION_TESTS 0

struct FString;
struct FName;
struct NAME_None_T {};
static const NAME_None_T NAME_None;

struct FString {
	std::string S;
	FString() {}
	FString(const char* C) : S(C ? C : "") {}
	FString(const std::string& C) : S(C) {}
	static FString Printf(const char* Fmt, ...) {
		char Buf[4096]; va_list A; va_start(A, Fmt); vsnprintf(Buf, sizeof(Buf), Fmt, A); va_end(A); return FString(Buf);
	}
	const char* operator*() const { return S.c_str(); }
	int32 Len() const { return (int32)S.size(); }
	bool IsEmpty() const { return S.empty(); }
	char operator[](int32 I) const { return S[I]; }
	bool Contains(const char* T) const { return S.find(T) != std::string::npos; }
	bool Contains(const FString& T) const { return S.find(T.S) != std::string::npos; }
	FString operator+(const FString& O) const { return FString(S + O.S); }
	FString operator+(const char* O) const { return FString(S + O); }
	FString& operator+=(const FString& O) { S += O.S; return *this; }
	bool operator==(const FString& O) const { return S == O.S; }
	bool operator!=(const FString& O) const { return S != O.S; }
	const FString& ToString() const { return *this; }
};
inline FString operator+(const char* A, const FString& B) { return FString(std::string(A) + B.S); }

struct FName {
	std::string S; bool bNone = true;
	FName() {}
	FName(const NAME_None_T&) {}
	FName(const char* C) : S(C ? C : ""), bNone(false) {}
	FName(const FString& C) : S(C.S), bNone(false) {}
	bool IsNone() const { return bNone; }
	FString ToString() const { return bNone ? FString("None") : FString(S); }
	bool operator==(const FName& O) const { return bNone == O.bNone && S == O.S; }
	bool operator!=(const FName& O) const { return !(*this == O); }
	bool operator<(const FName& O) const { return S < O.S; }
	friend uint32 GetTypeHash(const FName& N) { return (uint32)std::hash<std::string>()(N.S); }
};
inline bool operator==(const FName& A, const NAME_None_T&) { return A.bNone; }
inline bool operator!=(const FName& A, const NAME_None_T&) { return !A.bNone; }
inline bool operator!=(const NAME_None_T&, const FName& A) { return !A.bNone; }

template<typename A, typename B> struct TPair { A Key; B Value; TPair() {} TPair(A K, B V) : Key(K), Value(V) {} };
template<typename T> struct TOptional { bool bSet = false; T V{}; bool IsSet() const { return bSet; } const T& GetValue() const { return V; } TOptional& operator=(const T& X) { V = X; bSet = true; return *this; } };
template<typename T> struct TNumericLimits { static T Max() { return std::numeric_limits<T>::max(); } };
template<typename F> using TFunction = std::function<F>;

template<typename T>
struct TArray : std::vector<T> {
	using std::vector<T>::vector;
	TArray() {}
	TArray(std::initializer_list<T> L) : std::vector<T>(L) {}
	int32 Num() const { return (int32)this->size(); }
	T& Add(const T& X) { this->push_back(X); return this->back(); }
	T& Add(T&& X) { this->push_back(std::move(X)); return this->back(); }
	T& AddDefaulted_GetRef() { this->emplace_back(); return this->back(); }
	void Reserve(int32 N) { this->reserve(N); }
	void SetNum(int32 N) { this->resize(N); }
	void Init(const T& V, int32 N) { this->assign(N, V); }
	void Reset() { this->clear(); }
	T& Last() { return this->back(); }
	const T& Last() const { return this->back(); }
	T Pop() { T X = this->back(); this->pop_back(); return X; }
	void Append(const TArray<T>& O) { this->insert(this->end(), O.begin(), O.end()); }
	template<typename P> T* FindByPredicate(P Pred) { for (auto& X : *this) if (Pred(X)) return &X; return nullptr; }
	template<typename P> const T* FindByPredicate(P Pred) const { for (auto& X : *this) if (Pred(X)) return &X; return nullptr; }
	template<typename P> bool ContainsByPredicate(P Pred) const { for (auto& X : *this) if (Pred(X)) return true; return false; }
	bool Contains(const T& V) const { for (auto& X : *this) if (X == V) return true; return false; }
	template<typename P> void Sort(P Pred) { std::stable_sort(this->begin(), this->end(), Pred); }
	void Sort() { std::sort(this->begin(), this->end()); }
	bool IsValidIndex(int32 I) const { return I >= 0 && I < Num(); }
};
template<typename T> struct TSet { std::set<T> S; bool Contains(const T& X) const { return S.count(X) > 0; } void Add(const T& X) { S.insert(X); } };
template<typename K, typename V> struct TMap { std::map<K, V> M; V& Add(const K& k, const V& v) { return M[k] = v; } V* Find(const K& k) { auto it = M.find(k); return it == M.end() ? nullptr : &it->second; } const V* Find(const K& k) const { auto it = M.find(k); return it == M.end() ? nullptr : &it->second; } bool Contains(const K& k) const { return M.count(k) > 0; } };

struct FVector2D {
	double X = 0, Y = 0;
	FVector2D() {} FVector2D(double x, double y) : X(x), Y(y) {}
	static const FVector2D ZeroVector;
	bool operator==(const FVector2D& O) const { return X == O.X && Y == O.Y; }
};
inline const FVector2D FVector2D::ZeroVector(0, 0);
struct FVector {
	double X = 0, Y = 0, Z = 0;
	FVector() {} FVector(double x, double y, double z) : X(x), Y(y), Z(z) {}
	static const FVector ZeroVector;
	FVector operator+(const FVector& O) const { return FVector(X + O.X, Y + O.Y, Z + O.Z); }
};
inline const FVector FVector::ZeroVector(0, 0, 0);
struct FLinearColor { float R, G, B, A; };

namespace FMath {
	template<typename T> T Min(T A, T B) { return A < B ? A : B; }
	template<typename T> T Max(T A, T B) { return A > B ? A : B; }
	inline int32 Max(int32 A, int32 B) { return A > B ? A : B; }
	template<typename T> T Clamp(T V, T Lo, T Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	inline double Abs(double V) { return std::fabs(V); }
	inline int32 Abs(int32 V) { return V < 0 ? -V : V; }
	inline double Sqrt(double V) { return std::sqrt(V); }
	inline double Cos(double V) { return std::cos(V); }
	inline double Sin(double V) { return std::sin(V); }
	inline double Atan2(double Y, double X) { return std::atan2(Y, X); }
	inline double Fmod(double A, double B) { return std::fmod(A, B); }
	inline double Square(double V) { return V * V; }
	inline int32 RoundToInt(float V) { return (int32)std::lround(V); }
	inline int32 CeilToInt(float V) { return (int32)std::ceil(V); }
	inline double RadiansToDegrees(double R) { return R * 180.0 / PI; }
}
#define UE_LOG(...)
#include <limits>
#define MoveTemp std::move
