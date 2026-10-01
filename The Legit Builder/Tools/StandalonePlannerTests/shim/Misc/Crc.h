#pragma once
#include "CoreMinimal.h"
struct FCrc { static uint32 StrCrc32(const char* s){ uint32 h=2166136261u; while(*s){h^=(uint8)*s++;h*=16777619u;} return h; } };
