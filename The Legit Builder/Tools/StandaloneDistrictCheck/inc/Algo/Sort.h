#pragma once
#include "CoreMinimal.h"
namespace Algo { template<class R, class P> void Sort(R& r, P p) { std::stable_sort(r.begin(), r.end(), p); } }
