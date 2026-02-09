#include "ShaderCompiler.h"

namespace noc
{
	bool ShaderCompiler::CompileFromMemory(
		const char* debugName,
		const char* sourceUtf8,
		size_t sourceBytes,
		const char* entry,
		const char* target,
		dx12::ComPtr<ID3DBlob>& outBytecode)
	{
		outBytecode.Reset();

		if (!sourceUtf8 || sourceBytes == 0 || !entry || !target)
			return false;

		UINT flags = 0;
#if defined(_DEBUG)
		flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3DCompile(
			sourceUtf8,
			sourceBytes,
			debugName ? debugName : "noc_shader",
			nullptr,
			nullptr,
			entry,
			target,
			flags,
			0,
			&outBytecode,
			&errors);

		if (FAILED(hr))
		{
			const char* e = errors ? (const char*)errors->GetBufferPointer() : "unknown";
			NOC_LOG_ERROR("Render", "D3DCompile failed (%s:%s/%s): %s", debugName ? debugName : "mem", entry, target, e);
			return false;
		}

		return true;
	}

	bool ShaderCompiler::CompileFromFile(
		const wchar_t* filePath,
		const char* entry,
		const char* target,
		dx12::ComPtr<ID3DBlob>& outBytecode)
	{
		outBytecode.Reset();
		if (!filePath || !entry || !target)
			return false;

		UINT flags = 0;
#if defined(_DEBUG)
		flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		dx12::ComPtr<ID3DBlob> errors;
		HRESULT hr = D3DCompileFromFile(
			filePath,
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entry,
			target,
			flags,
			0,
			&outBytecode,
			&errors);

		if (FAILED(hr))
		{
			const char* e = errors ? (const char*)errors->GetBufferPointer() : "unknown";
			NOC_LOG_ERROR("Render", "D3DCompileFromFile failed: %s", e);
			return false;
		}
		return true;
	}
}
