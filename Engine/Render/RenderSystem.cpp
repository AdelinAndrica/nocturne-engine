#include "RenderSystem.h"

#include "Core/Log.h"
#include "Render/DX12/Dx12Renderer.h"

namespace noc
{
	struct RenderSystem::Impl
	{
		Dx12Renderer renderer;
	};

	bool RenderSystem::Init(bool enableDebugLayer)
	{
		if (impl_)
			return true;

		impl_ = new Impl();
		if (!impl_->renderer.Init(enableDebugLayer))
		{
			NOC_LOG_ERROR("Render", "Dx12Renderer::Init failed");
			delete impl_;
			impl_ = nullptr;
			return false;
		}

		NOC_LOG_INFO("Render", "RenderSystem initialized");
		return true;
	}

	void RenderSystem::Shutdown()
	{
		if (!impl_)
			return;

		impl_->renderer.Shutdown();

		delete impl_;
		impl_ = nullptr;

		NOC_LOG_INFO("Render", "RenderSystem shutdown");
	}

	bool RenderSystem::AttachToWindow(void* nativeHwnd, uint32_t clientWidth, uint32_t clientHeight)
	{
		if (!impl_)
			return false;

		return impl_->renderer.AttachToWindow(nativeHwnd, clientWidth, clientHeight);
	}

	void RenderSystem::SetResourceManager(ResourceManager* rm)
	{
		if (!impl_) return;
		impl_->renderer.SetResourceManager(rm);
	}

	void RenderSystem::SetFrameRenderQueue(const RenderQueue* q)
	{
		if (!impl_) return;
		impl_->renderer.SetFrameRenderQueue(q);
	}

	void RenderSystem::BeginFrame()
	{
		if (!impl_)
			return;

		impl_->renderer.BeginFrame();
	}

	void RenderSystem::EndFramePresent()
	{
		if (!impl_)
			return;

		impl_->renderer.EndFramePresent();
	}
}
