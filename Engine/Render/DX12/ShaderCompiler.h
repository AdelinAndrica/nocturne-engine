#pragma once
#include "Dx12Common.h"
#include <string>

namespace noc
{
	class ShaderCompiler
	{
	public:
		// Compile from an in-memory HLSL string (VFS-backed source).
		static bool CompileFromMemory(
			const char* debugName, // used for error messages
			const char* sourceUtf8,
			size_t sourceBytes,
			const char* entry,
			const char* target,
			dx12::ComPtr<ID3DBlob>& outBytecode);

		// Kept from Phase 9 (if you already had it).
		static bool CompileFromFile(
			const wchar_t* filePath,
			const char* entry,
			const char* target,
			dx12::ComPtr<ID3DBlob>& outBytecode);
	};
}
