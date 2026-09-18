#pragma once

#include "Core/Math/MathTypes.h"
#include "Resources/ResourceHandle.h"
#include "Runtime/Bounds.h"
#include "Runtime/Entity.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/Reflection/ReflectionString.h"

#include <cstdint>

namespace noc
{
    namespace BuiltinTypeIds
    {
        // Design choice (not directly from the book): builtins occupy a
        // reserved high range so Phase 15 component IDs 1..4 remain stable.
        inline constexpr TypeId Bool    { 0x1000000000000001ull };
        inline constexpr TypeId Int8    { 0x1000000000000002ull };
        inline constexpr TypeId Int16   { 0x1000000000000003ull };
        inline constexpr TypeId Int32   { 0x1000000000000004ull };
        inline constexpr TypeId Int64   { 0x1000000000000005ull };
        inline constexpr TypeId UInt8   { 0x1000000000000006ull };
        inline constexpr TypeId UInt16  { 0x1000000000000007ull };
        inline constexpr TypeId UInt32  { 0x1000000000000008ull };
        inline constexpr TypeId UInt64  { 0x1000000000000009ull };
        inline constexpr TypeId Float32 { 0x100000000000000Aull };
        inline constexpr TypeId Float64 { 0x100000000000000Bull };
        inline constexpr TypeId String  { 0x100000000000000Cull };

        inline constexpr TypeId Vec2    { 0x1000000000000101ull };
        inline constexpr TypeId Vec3    { 0x1000000000000102ull };
        inline constexpr TypeId Vec4    { 0x1000000000000103ull };
        inline constexpr TypeId Quat    { 0x1000000000000104ull };
        inline constexpr TypeId Mat4    { 0x1000000000000105ull };
        inline constexpr TypeId AABB    { 0x1000000000000106ull };

        inline constexpr TypeId EntityHandle
            { 0x1000000000000201ull };
        inline constexpr TypeId ResourceHandle
            { 0x1000000000000202ull };

        // Design choice (not directly from the book): resource constraints use
        // reflected marker types rather than ResourceManager implementation
        // enums so reflection remains independent of resource-system internals.
        inline constexpr TypeId MeshResource
            { 0x1000000000000203ull };
    }

    namespace builtin_reflection_detail
    {
        struct MeshResourceMarker
        {
            uint8_t reserved = 0;

            [[nodiscard]] bool operator==(
                const MeshResourceMarker&) const = default;
        };

        inline constexpr PropertyFlags kMathPropertyFlags =
            PropertyFlags::EditorVisible
            | PropertyFlags::Serializable
            | PropertyFlags::ScriptVisible;

        template <typename T>
        [[nodiscard]] inline bool RegisterPrimitive(
            ReflectionRegistry& registry,
            TypeId id,
            const char* name,
            TypeKind kind)
        {
            return registry.RegisterType(
                MakeTypeMetadata<T>(
                    id,
                    name,
                    kind,
                    1,
                    TypeFlags::Serializable
                        | TypeFlags::ScriptVisible));
        }

        [[nodiscard]] inline bool RegisterVec2(ReflectionRegistry& registry)
        {
            const PropertyMetadata properties[] = {
                MakeMemberPropertyMetadata<Vec2, float, &Vec2::x>(
                    MakePropertyId("Nocturne.Vec2.x"), "x",
                    BuiltinTypeIds::Vec2, BuiltinTypeIds::Float32,
                    kMathPropertyFlags),
                MakeMemberPropertyMetadata<Vec2, float, &Vec2::y>(
                    MakePropertyId("Nocturne.Vec2.y"), "y",
                    BuiltinTypeIds::Vec2, BuiltinTypeIds::Float32,
                    kMathPropertyFlags)
            };

            TypeMetadata metadata = MakeTypeMetadata<Vec2>(
                BuiltinTypeIds::Vec2,
                "Nocturne.Vec2",
                TypeKind::Struct,
                1,
                TypeFlags::EditorVisible
                    | TypeFlags::Serializable
                    | TypeFlags::ScriptVisible);
            metadata.properties = properties;
            metadata.propertyCount = 2;
            return registry.RegisterType(metadata);
        }

        [[nodiscard]] inline bool RegisterVec3(ReflectionRegistry& registry)
        {
            const PropertyMetadata properties[] = {
                MakeMemberPropertyMetadata<Vec3, float, &Vec3::x>(
                    MakePropertyId("Nocturne.Vec3.x"), "x",
                    BuiltinTypeIds::Vec3, BuiltinTypeIds::Float32,
                    kMathPropertyFlags),
                MakeMemberPropertyMetadata<Vec3, float, &Vec3::y>(
                    MakePropertyId("Nocturne.Vec3.y"), "y",
                    BuiltinTypeIds::Vec3, BuiltinTypeIds::Float32,
                    kMathPropertyFlags),
                MakeMemberPropertyMetadata<Vec3, float, &Vec3::z>(
                    MakePropertyId("Nocturne.Vec3.z"), "z",
                    BuiltinTypeIds::Vec3, BuiltinTypeIds::Float32,
                    kMathPropertyFlags)
            };

            TypeMetadata metadata = MakeTypeMetadata<Vec3>(
                BuiltinTypeIds::Vec3,
                "Nocturne.Vec3",
                TypeKind::Struct,
                1,
                TypeFlags::EditorVisible
                    | TypeFlags::Serializable
                    | TypeFlags::ScriptVisible);
            metadata.properties = properties;
            metadata.propertyCount = 3;
            return registry.RegisterType(metadata);
        }

        [[nodiscard]] inline bool RegisterVec4(ReflectionRegistry& registry)
        {
            const PropertyMetadata properties[] = {
                MakeMemberPropertyMetadata<Vec4, float, &Vec4::x>(
                    MakePropertyId("Nocturne.Vec4.x"), "x",
                    BuiltinTypeIds::Vec4, BuiltinTypeIds::Float32,
                    kMathPropertyFlags),
                MakeMemberPropertyMetadata<Vec4, float, &Vec4::y>(
                    MakePropertyId("Nocturne.Vec4.y"), "y",
                    BuiltinTypeIds::Vec4, BuiltinTypeIds::Float32,
                    kMathPropertyFlags),
                MakeMemberPropertyMetadata<Vec4, float, &Vec4::z>(
                    MakePropertyId("Nocturne.Vec4.z"), "z",
                    BuiltinTypeIds::Vec4, BuiltinTypeIds::Float32,
                    kMathPropertyFlags),
                MakeMemberPropertyMetadata<Vec4, float, &Vec4::w>(
                    MakePropertyId("Nocturne.Vec4.w"), "w",
                    BuiltinTypeIds::Vec4, BuiltinTypeIds::Float32,
                    kMathPropertyFlags)
            };

            TypeMetadata metadata = MakeTypeMetadata<Vec4>(
                BuiltinTypeIds::Vec4,
                "Nocturne.Vec4",
                TypeKind::Struct,
                1,
                TypeFlags::EditorVisible
                    | TypeFlags::Serializable
                    | TypeFlags::ScriptVisible);
            metadata.properties = properties;
            metadata.propertyCount = 4;
            return registry.RegisterType(metadata);
        }

        [[nodiscard]] inline bool RegisterQuat(ReflectionRegistry& registry)
        {
            const PropertyMetadata properties[] = {
                MakeMemberPropertyMetadata<Quat, float, &Quat::x>(
                    MakePropertyId("Nocturne.Quat.x"), "x",
                    BuiltinTypeIds::Quat, BuiltinTypeIds::Float32,
                    kMathPropertyFlags),
                MakeMemberPropertyMetadata<Quat, float, &Quat::y>(
                    MakePropertyId("Nocturne.Quat.y"), "y",
                    BuiltinTypeIds::Quat, BuiltinTypeIds::Float32,
                    kMathPropertyFlags),
                MakeMemberPropertyMetadata<Quat, float, &Quat::z>(
                    MakePropertyId("Nocturne.Quat.z"), "z",
                    BuiltinTypeIds::Quat, BuiltinTypeIds::Float32,
                    kMathPropertyFlags),
                MakeMemberPropertyMetadata<Quat, float, &Quat::w>(
                    MakePropertyId("Nocturne.Quat.w"), "w",
                    BuiltinTypeIds::Quat, BuiltinTypeIds::Float32,
                    kMathPropertyFlags)
            };

            TypeMetadata metadata = MakeTypeMetadata<Quat>(
                BuiltinTypeIds::Quat,
                "Nocturne.Quat",
                TypeKind::Struct,
                1,
                TypeFlags::EditorVisible
                    | TypeFlags::Serializable
                    | TypeFlags::ScriptVisible);
            metadata.properties = properties;
            metadata.propertyCount = 4;
            return registry.RegisterType(metadata);
        }

        [[nodiscard]] inline bool RegisterAabb(ReflectionRegistry& registry)
        {
            const PropertyMetadata properties[] = {
                MakeMemberPropertyMetadata<AABB, Vec3, &AABB::min>(
                    MakePropertyId("Nocturne.AABB.min"), "min",
                    BuiltinTypeIds::AABB, BuiltinTypeIds::Vec3,
                    kMathPropertyFlags),
                MakeMemberPropertyMetadata<AABB, Vec3, &AABB::max>(
                    MakePropertyId("Nocturne.AABB.max"), "max",
                    BuiltinTypeIds::AABB, BuiltinTypeIds::Vec3,
                    kMathPropertyFlags)
            };

            TypeMetadata metadata = MakeTypeMetadata<AABB>(
                BuiltinTypeIds::AABB,
                "Nocturne.AABB",
                TypeKind::Struct,
                1,
                TypeFlags::EditorVisible
                    | TypeFlags::Serializable
                    | TypeFlags::ScriptVisible);
            metadata.properties = properties;
            metadata.propertyCount = 2;
            return registry.RegisterType(metadata);
        }
    }

    [[nodiscard]] inline bool RegisterBuiltinReflectionTypes(
        ReflectionRegistry& registry)
    {
        using namespace builtin_reflection_detail;

        if (!RegisterPrimitive<bool>(
                registry, BuiltinTypeIds::Bool,
                "Nocturne.Bool", TypeKind::Bool)
            || !RegisterPrimitive<int8_t>(
                registry, BuiltinTypeIds::Int8,
                "Nocturne.Int8", TypeKind::SignedInteger)
            || !RegisterPrimitive<int16_t>(
                registry, BuiltinTypeIds::Int16,
                "Nocturne.Int16", TypeKind::SignedInteger)
            || !RegisterPrimitive<int32_t>(
                registry, BuiltinTypeIds::Int32,
                "Nocturne.Int32", TypeKind::SignedInteger)
            || !RegisterPrimitive<int64_t>(
                registry, BuiltinTypeIds::Int64,
                "Nocturne.Int64", TypeKind::SignedInteger)
            || !RegisterPrimitive<uint8_t>(
                registry, BuiltinTypeIds::UInt8,
                "Nocturne.UInt8", TypeKind::UnsignedInteger)
            || !RegisterPrimitive<uint16_t>(
                registry, BuiltinTypeIds::UInt16,
                "Nocturne.UInt16", TypeKind::UnsignedInteger)
            || !RegisterPrimitive<uint32_t>(
                registry, BuiltinTypeIds::UInt32,
                "Nocturne.UInt32", TypeKind::UnsignedInteger)
            || !RegisterPrimitive<uint64_t>(
                registry, BuiltinTypeIds::UInt64,
                "Nocturne.UInt64", TypeKind::UnsignedInteger)
            || !RegisterPrimitive<float>(
                registry, BuiltinTypeIds::Float32,
                "Nocturne.Float32", TypeKind::FloatingPoint)
            || !RegisterPrimitive<double>(
                registry, BuiltinTypeIds::Float64,
                "Nocturne.Float64", TypeKind::FloatingPoint))
        {
            return false;
        }

        TypeMetadata stringMetadata =
            MakeTypeMetadata<ReflectionString>(
                BuiltinTypeIds::String,
                "Nocturne.String",
                TypeKind::String,
                1,
                TypeFlags::Serializable
                    | TypeFlags::ScriptVisible);
        stringMetadata.lifecycle =
            MakeReflectionStringLifecycleOperations();

        if (!registry.RegisterType(stringMetadata)
            || !RegisterVec2(registry)
            || !RegisterVec3(registry)
            || !RegisterVec4(registry)
            || !RegisterQuat(registry))
        {
            return false;
        }

        // Design choice (not directly from the book): Mat4 storage is an
        // implementation detail, so generic authoring does not expose 16 raw
        // matrix elements as properties.
        if (!registry.RegisterType(
                MakeTypeMetadata<Mat4>(
                    BuiltinTypeIds::Mat4,
                    "Nocturne.Mat4",
                    TypeKind::Opaque,
                    1,
                    TypeFlags::Serializable))
            || !RegisterAabb(registry)
            || !registry.RegisterType(
                MakeTypeMetadata<EntityHandle>(
                    BuiltinTypeIds::EntityHandle,
                    "Nocturne.EntityHandle",
                    TypeKind::EntityReference,
                    1,
                    TypeFlags::ScriptVisible
                        | TypeFlags::Transient))
            || !registry.RegisterType(
                MakeTypeMetadata<ResourceHandle>(
                    BuiltinTypeIds::ResourceHandle,
                    "Nocturne.ResourceHandle",
                    TypeKind::ResourceReference,
                    1,
                    TypeFlags::Serializable
                        | TypeFlags::ScriptVisible))
            || !registry.RegisterType(
                MakeTypeMetadata<
                    builtin_reflection_detail::MeshResourceMarker>(
                        BuiltinTypeIds::MeshResource,
                        "Nocturne.Resource.Mesh",
                        TypeKind::Opaque,
                        1)))
        {
            return false;
        }

        return true;
    }
}
