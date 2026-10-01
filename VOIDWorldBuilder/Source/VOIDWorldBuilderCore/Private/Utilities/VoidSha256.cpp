// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Utilities/VoidSha256.h"
#include "Misc/FileHelper.h"

namespace VoidSha256Private
{
	static const uint32 K[64] = {
		0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
		0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
		0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
		0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
		0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
		0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
		0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
		0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };

	static inline uint32 Rotr(uint32 X, uint32 N) { return (X >> N) | (X << (32 - N)); }

	static void Compress(uint32 H[8], const uint8* Block)
	{
		uint32 W[64];
		for (int32 i = 0; i < 16; ++i)
		{
			W[i] = (uint32(Block[i * 4]) << 24) | (uint32(Block[i * 4 + 1]) << 16) | (uint32(Block[i * 4 + 2]) << 8) | uint32(Block[i * 4 + 3]);
		}
		for (int32 i = 16; i < 64; ++i)
		{
			const uint32 S0 = Rotr(W[i - 15], 7) ^ Rotr(W[i - 15], 18) ^ (W[i - 15] >> 3);
			const uint32 S1 = Rotr(W[i - 2], 17) ^ Rotr(W[i - 2], 19) ^ (W[i - 2] >> 10);
			W[i] = W[i - 16] + S0 + W[i - 7] + S1;
		}
		uint32 a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
		for (int32 i = 0; i < 64; ++i)
		{
			const uint32 S1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
			const uint32 Ch = (e & f) ^ (~e & g);
			const uint32 T1 = h + S1 + Ch + K[i] + W[i];
			const uint32 S0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
			const uint32 Maj = (a & b) ^ (a & c) ^ (b & c);
			const uint32 T2 = S0 + Maj;
			h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
		}
		H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
	}
}

FString FVoidSha256::HashBytes(const uint8* Data, uint64 NumBytes)
{
	using namespace VoidSha256Private;

	uint32 H[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };

	uint64 Offset = 0;
	while (NumBytes - Offset >= 64)
	{
		Compress(H, Data + Offset);
		Offset += 64;
	}

	// Final block(s): remaining bytes, 0x80, zero pad, 64-bit big-endian bit length.
	uint8 Tail[128] = {};
	const uint64 Rem = NumBytes - Offset;
	for (uint64 i = 0; i < Rem; ++i) { Tail[i] = Data[Offset + i]; }
	Tail[Rem] = 0x80;
	const uint64 TailLen = (Rem + 1 + 8 <= 64) ? 64 : 128;
	const uint64 BitLen = NumBytes * 8ull;
	for (int32 i = 0; i < 8; ++i) { Tail[TailLen - 1 - i] = uint8((BitLen >> (8 * i)) & 0xff); }
	Compress(H, Tail);
	if (TailLen == 128) { Compress(H, Tail + 64); }

	FString Out;
	for (int32 i = 0; i < 8; ++i) { Out += FString::Printf(TEXT("%08x"), H[i]); }
	return Out;
}

bool FVoidSha256::HashFile(const FString& FilePath, FString& OutHexDigest)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *FilePath))
	{
		return false;
	}
	OutHexDigest = HashBytes(Bytes);
	return true;
}
