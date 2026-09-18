#pragma once

#include "Runtime/Reflection/ReflectedValue.h"
#include "Runtime/Reflection/ReflectionRegistry.h"

namespace noc
{
    enum class FunctionInvokeStatus : uint8_t
    {
        Success = 0,
        InvalidFunction,
        ArgumentCountMismatch,
        InvalidArgument,
        ArgumentTypeMismatch,
        MissingObject,
        UnknownReturnType,
        ReturnDestinationRequired,
        ReturnDestinationAlreadyInitialized,
        CannotConstructReturnValue,
        InvocationFailed
    };

    [[nodiscard]] inline FunctionInvokeStatus InvokeReflectedFunction(
        const ReflectionRegistry& registry,
        const FunctionMetadata& function,
        FunctionInvocationContext& context,
        const ReflectedConstValueView* arguments,
        uint32_t argumentCount,
        IAllocator* returnAllocator,
        OwnedReflectedValue* returnValue)
    {
        if (!function.functionId.IsValid() || !function.invoke)
            return FunctionInvokeStatus::InvalidFunction;

        if (argumentCount != function.parameterCount)
            return FunctionInvokeStatus::ArgumentCountMismatch;

        if (argumentCount > 0 && !arguments)
            return FunctionInvokeStatus::InvalidArgument;

        for (uint32_t i = 0; i < argumentCount; ++i)
        {
            if (!arguments[i].IsValid())
                return FunctionInvokeStatus::InvalidArgument;
            if (arguments[i].typeId != function.parameters[i].typeId)
                return FunctionInvokeStatus::ArgumentTypeMismatch;
        }

        if (HasFlag(function.flags, FunctionFlags::Member))
        {
            if (HasFlag(function.flags, FunctionFlags::Const))
            {
                if (!context.object && !context.mutableObject)
                    return FunctionInvokeStatus::MissingObject;
            }
            else if (!context.mutableObject)
            {
                return FunctionInvokeStatus::MissingObject;
            }
        }

        if (!function.returnTypeId.IsValid())
        {
            return function.invoke(
                context,
                arguments,
                argumentCount,
                ReflectedValueView{})
                ? FunctionInvokeStatus::Success
                : FunctionInvokeStatus::InvocationFailed;
        }

        if (!returnAllocator || !returnValue)
            return FunctionInvokeStatus::ReturnDestinationRequired;

        if (returnValue->IsValid())
            return FunctionInvokeStatus::ReturnDestinationAlreadyInitialized;

        const TypeMetadata* returnType =
            registry.FindType(function.returnTypeId);
        if (!returnType)
            return FunctionInvokeStatus::UnknownReturnType;

        if (!returnValue->InitDefault(*returnAllocator, *returnType))
            return FunctionInvokeStatus::CannotConstructReturnValue;

        if (!function.invoke(
                context,
                arguments,
                argumentCount,
                returnValue->View()))
        {
            returnValue->Clear();
            return FunctionInvokeStatus::InvocationFailed;
        }

        return FunctionInvokeStatus::Success;
    }
}
