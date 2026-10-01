// Minimal UE API shim so the pure planner can be compiled + tested outside the editor. NOT part of the deliverable plugin.
#pragma once
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define VOIDWORLDBUILDERGENERATORS_API
#define TEXT(x) x
typedef char TCHAR; typedef int32_t int32; typedef int64_t int64; typedef uint8_t uint8; typedef uint32_t uint32; typedef uint64_t uint64;
static constexpr double PI = 3.14159265358979323846;
template<class F> using TFunction = std::function<F>;
struct FString { std::string S; FString(){} FString(const char* c):S(c){} FString(std::string s):S(s){}
  const char* operator*() const { return S.c_str(); } FString ToLower() const { std::string t=S; for(auto&c:t)c=(char)tolower(c); return FString(t);} 
  bool operator==(const FString&o)const{return S==o.S;} };
struct FName { std::string S; FName():S(""){} FName(const char* c):S(c){} FString ToString() const { return FString(S);} bool IsNone() const { return S.empty(); }
  bool operator==(const FName&o)const{return S==o.S;} bool operator!=(const FName&o)const{return S!=o.S;} bool operator<(const FName&o)const{return S<o.S;} };
template<class T> struct TArray { std::vector<T> V;
  void Add(const T& t){V.push_back(t);} int32 Num() const {return (int32)V.size();} T& operator[](int32 i){return V[i];} const T& operator[](int32 i) const {return V[i];}
  void Reserve(int32 n){V.reserve(n);} void Reset(){V.clear();} T& Last(){return V.back();} const T& Last() const {return V.back();} void SetNum(int32 n){V.resize(n);} 
  bool Contains(const T& t) const { return std::find(V.begin(),V.end(),t)!=V.end(); }
  template<class P> void Sort(P p){ std::sort(V.begin(),V.end(),p);} auto begin(){return V.begin();} auto end(){return V.end();} auto begin() const {return V.begin();} auto end() const {return V.end();} };
template<class K,class V> struct TMap { std::map<K,V> M; V* Find(const K&k){auto i=M.find(k);return i==M.end()?nullptr:&i->second;} const V* Find(const K&k) const {auto i=M.find(k);return i==M.end()?nullptr:&i->second;}
  V& FindOrAdd(const K&k){return M[k];} void Add(const K&k,const V&v){M[k]=v;} auto begin() const {return M.begin();} auto end() const {return M.end();} };
struct FVector2D { double X=0,Y=0; FVector2D(){} FVector2D(double x,double y):X(x),Y(y){} static const FVector2D ZeroVector;
  FVector2D operator+(const FVector2D&o)const{return {X+o.X,Y+o.Y};} FVector2D operator-(const FVector2D&o)const{return {X-o.X,Y-o.Y};}
  FVector2D operator*(double s)const{return {X*s,Y*s};} FVector2D operator/(double s)const{return {X/s,Y/s};}
  FVector2D& operator+=(const FVector2D&o){X+=o.X;Y+=o.Y;return *this;} FVector2D& operator/=(double s){X/=s;Y/=s;return *this;} };
inline const FVector2D FVector2D::ZeroVector(0,0);
struct FVector { double X=0,Y=0,Z=0; FVector(){} FVector(double x,double y,double z):X(x),Y(y),Z(z){} static const FVector ZeroVector;
  FVector operator+(const FVector&o)const{return {X+o.X,Y+o.Y,Z+o.Z};} FVector operator-(const FVector&o)const{return {X-o.X,Y-o.Y,Z-o.Z};}
  FVector operator*(double s)const{return {X*s,Y*s,Z*s};} FVector& operator+=(const FVector&o){X+=o.X;Y+=o.Y;Z+=o.Z;return *this;}
  static double Dist(const FVector&a,const FVector&b){return std::sqrt((a.X-b.X)*(a.X-b.X)+(a.Y-b.Y)*(a.Y-b.Y)+(a.Z-b.Z)*(a.Z-b.Z));} };
inline const FVector FVector::ZeroVector(0,0,0);
struct FMath {
 template<class T> static T Clamp(T v,T a,T b){return v<a?a:(v>b?b:v);} template<class T> static T Max(T a,T b){return a>b?a:b;} template<class T> static T Min(T a,T b){return a<b?a:b;}
 template<class T> static T Lerp(T a,T b,T t){return a+(b-a)*t;} static double Sqrt(double x){return std::sqrt(x);} static double Atan2(double y,double x){return std::atan2(y,x);}
 static double RadiansToDegrees(double r){return r*180.0/PI;} static double Cos(double x){return std::cos(x);} static double Sin(double x){return std::sin(x);}
 static int32 CeilToInt(double x){return (int32)std::ceil(x);} static int32 FloorToInt(double x){return (int32)std::floor(x);} };
