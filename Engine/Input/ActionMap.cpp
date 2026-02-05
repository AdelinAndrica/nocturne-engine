#include "ActionMap.h"
#include "InputSystem.h"
#include <cmath>

namespace noc
{
	void ActionMap::Clear()
	{
		bindingCount_ = 0;
		valueCount_ = 0;
	}

	bool ActionMap::Bind(const Binding& b)
	{
		if (bindingCount_ >= kMaxBindings)
			return false;
		bindings_[bindingCount_++] = b;
		return true;
	}

	bool ActionMap::Rebind(ActionId action, SourceType type, const Binding& replacement)
	{
		for (uint32_t i = 0; i < bindingCount_; ++i)
		{
			if (bindings_[i].action == action && bindings_[i].type == type)
			{
				bindings_[i] = replacement;
				return true;
			}
		}
		return false;
	}

	void ActionMap::Update(const InputSystem& input)
	{
		// Clear cached values.
		valueCount_ = 0;

		for (uint32_t i = 0; i < bindingCount_; ++i)
		{
			const Binding& b = bindings_[i];
			if (b.action == InvalidAction)
				continue;

			float v = 0.0f;

			switch (b.type)
			{
			case SourceType::DigitalKey:
			case SourceType::DigitalMouseButton:
			{
				const bool down = input.IsDown(b.key);
				v = down ? 1.0f : 0.0f;
				break;
			}

			case SourceType::AnalogMouseDeltaX:
			{
				const MouseDelta md = input.GetMouseDelta();
				v = static_cast<float>(md.dx) * b.scale;
				break;
			}
			case SourceType::AnalogMouseDeltaY:
			{
				const MouseDelta md = input.GetMouseDelta();
				v = static_cast<float>(md.dy) * b.scale;
				break;
			}
			case SourceType::AnalogMouseWheel:
			{
				v = static_cast<float>(input.GetWheelDelta()) * b.scale;
				break;
			}
			default:
				break;
			}

			if (b.negate)
				v = -v;

			// Accumulate contributions (e.g., W + S bindings).
			const float prev = GetValue_(b.action);
			SetValue_(b.action, prev + v);
		}

		// Optional: clamp near-zero to 0 for stability
		for (uint32_t i = 0; i < valueCount_; ++i)
		{
			if (std::fabs(values_[i].value) < 1e-6f)
				values_[i].value = 0.0f;
		}
	}

	float ActionMap::GetActionValue(ActionId action) const
	{
		return GetValue_(action);
	}

	void ActionMap::SetValue_(ActionId action, float v)
	{
		for (uint32_t i = 0; i < valueCount_; ++i)
		{
			if (values_[i].action == action)
			{
				values_[i].value = v;
				return;
			}
		}

		if (valueCount_ < kMaxActionsCached)
		{
			values_[valueCount_++] = ActionValue{ action, v };
		}
	}

	float ActionMap::GetValue_(ActionId action) const
	{
		for (uint32_t i = 0; i < valueCount_; ++i)
		{
			if (values_[i].action == action)
				return values_[i].value;
		}
		return 0.0f;
	}
}
