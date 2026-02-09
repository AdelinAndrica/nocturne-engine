#pragma once
#include "Dx12Common.h"
#include <cstdint>
#include <unordered_map>

namespace noc
{
	struct Dx12PsoKey
	{
		const void* vs = nullptr;
		const void* ps = nullptr;
		ID3D12RootSignature* rootSig = nullptr;
		DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		uint64_t inputLayoutHash = 0;

		bool operator==(const Dx12PsoKey& o) const
		{
			return vs == o.vs && ps == o.ps && rootSig == o.rootSig && rtvFormat == o.rtvFormat && inputLayoutHash == o.inputLayoutHash;
		}
	};

	struct Dx12PsoKeyHash
	{
		size_t operator()(const Dx12PsoKey& k) const noexcept
		{
			size_t h = 1469598103934665603ull;
			auto mix = [&](size_t v) { h ^= v; h *= 1099511628211ull; };
			mix((size_t)k.vs);
			mix((size_t)k.ps);
			mix((size_t)k.rootSig);
			mix((size_t)k.rtvFormat);
			mix((size_t)k.inputLayoutHash);
			return h;
		}
	};

	class Dx12PsoCache
	{
	public:
		ID3D12PipelineState* Find(const Dx12PsoKey& key) const;
		void Insert(const Dx12PsoKey& key, dx12::ComPtr<ID3D12PipelineState>&& pso);
		void Clear();

	private:
		std::unordered_map<Dx12PsoKey, dx12::ComPtr<ID3D12PipelineState>, Dx12PsoKeyHash> map_;
	};
}
