#include "Dx12PsoCache.h"

namespace noc
{
	ID3D12PipelineState* Dx12PsoCache::Find(const Dx12PsoKey& key) const
	{
		auto it = map_.find(key);
		return (it == map_.end()) ? nullptr : it->second.Get();
	}

	void Dx12PsoCache::Insert(const Dx12PsoKey& key, dx12::ComPtr<ID3D12PipelineState>&& pso)
	{
		map_[key] = std::move(pso);
	}

	void Dx12PsoCache::Clear()
	{
		map_.clear();
	}
}
