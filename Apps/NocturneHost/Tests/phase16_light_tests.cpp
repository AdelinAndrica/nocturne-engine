#include "../../NocturneEditor/EditorCommands.h"

#include "Assets/RuntimeFormats/MeshBlob.h"
#include "Core/Log.h"
#include "Core/Memory/Allocator.h"
#include "Runtime/Components/LightComponent.h"
#include "Runtime/Reflection/BuiltinTypes.h"
#include "Runtime/Reflection/ComponentReflection.h"
#include "Runtime/Reflection/FoundationComponents.h"
#include "Runtime/Reflection/LightReflection.h"
#include "Runtime/Reflection/PropertyAccess.h"
#include "Runtime/Reflection/ReflectionRegistry.h"
#include "Runtime/World.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    bool CheckLight(bool condition, const char* message)
    {
        if (!condition)
        {
            NOC_LOG_ERROR("Phase16Light", "%s", message);
            return false;
        }
        return true;
    }
}

bool RunPhase16LightTests()
{
    NOC_LOG_INFO("Phase16Light", "%s", "Light foundation tests begin");

    bool ok = true;
    noc::MallocAllocator allocator;
    noc::ReflectionRegistry reflection;
    noc::World world;

    ok &= CheckLight(
        reflection.Init(allocator, 40),
        "ReflectionRegistry init failed");
    ok &= CheckLight(
        noc::RegisterBuiltinReflectionTypes(reflection)
            && noc::RegisterFoundationComponentReflectionTypes(reflection)
            && noc::RegisterLightReflectionTypes(reflection)
            && reflection.Freeze(),
        "Light reflection registration/freeze failed");
    ok &= CheckLight(
        world.Init(allocator, reflection),
        "World init failed");

    const noc::EntityHandle entity = world.CreateEntity();
    ok &= CheckLight(entity.IsValid(), "Entity creation failed");
    ok &= CheckLight(world.AddTransform(entity), "Transform add failed");
    ok &= CheckLight(
        world.AddLight(entity, noc::LightType::Spot),
        "Spot light add failed");
    ok &= CheckLight(
        !world.AddLight(entity, noc::LightType::Point),
        "Duplicate LightComponent add must fail");

    const noc::LightComponent* light = world.GetLight(entity);
    ok &= CheckLight(
        light && light->type == noc::LightType::Spot,
        "Light type/default state mismatch");

    ok &= CheckLight(
        world.SetLightColor(entity, noc::Vec3{ 1.0f, 0.5f, 0.25f }),
        "Valid light color rejected");
    ok &= CheckLight(
        !world.SetLightColor(entity, noc::Vec3{ -1.0f, 0.5f, 0.25f }),
        "Negative light color accepted");
    ok &= CheckLight(
        world.SetLightIntensity(entity, 7.5f)
            && !world.SetLightIntensity(entity, -0.1f),
        "Light intensity validation failed");
    ok &= CheckLight(
        world.SetLightRange(entity, 12.0f)
            && !world.SetLightRange(entity, 0.0f),
        "Light range validation failed");
    ok &= CheckLight(
        world.SetLightSpotAngles(entity, 0.2f, 0.6f)
            && !world.SetLightSpotAngles(entity, 0.7f, 0.6f)
            && !world.SetLightSpotAngles(entity, 0.2f, 1.5707964f),
        "Spot angle validation failed");

    const noc::TypeId componentTypeId{
        noc::kLightComponentTypeId.value
    };
    const noc::TypeMetadata* componentType =
        reflection.FindType(componentTypeId);
    const noc::TypeMetadata* lightType =
        reflection.FindType(noc::FoundationTypeIds::LightType);

    ok &= CheckLight(
        componentType
            && componentType->kind == noc::TypeKind::Component
            && componentType->componentMetadata,
        "Light component reflection metadata missing");
    ok &= CheckLight(
        lightType
            && lightType->kind == noc::TypeKind::Enum
            && lightType->enumMetadata
            && lightType->enumMetadata->valueCount == 3,
        "LightType enum reflection metadata missing");

    const noc::PropertyMetadata* intensityProperty =
        reflection.FindProperty(
            componentTypeId,
            noc::MakePropertyId("Nocturne.Light.intensity"));
    ok &= CheckLight(
        intensityProperty != nullptr,
        "Reflected light intensity property missing");

    if (componentType
        && componentType->componentMetadata
        && intensityProperty)
    {
        noc::ComponentPropertyRuntimeContext runtime{
            &world,
            entity
        };
        noc::PropertyAccessContext context =
            noc::MakeComponentPropertyAccessContext(
                runtime,
                componentType->componentMetadata);

        const float reflectedIntensity = 3.25f;
        ok &= CheckLight(
            noc::WritePropertyValue(
                *intensityProperty,
                context,
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Float32,
                    &reflectedIntensity })
                == noc::PropertyAccessStatus::Success,
            "Semantic reflected light intensity write failed");

        const float invalidIntensity = -3.0f;
        ok &= CheckLight(
            noc::WritePropertyValue(
                *intensityProperty,
                context,
                noc::ReflectedConstValueView{
                    noc::BuiltinTypeIds::Float32,
                    &invalidIntensity })
                != noc::PropertyAccessStatus::Success,
            "Invalid reflected light intensity write was accepted");

        light = world.GetLight(entity);
        ok &= CheckLight(
            light
                && std::fabs(light->intensity - reflectedIntensity) < 1.0e-5f,
            "Rejected reflected write mutated light state");
    }

    ok &= CheckLight(
        world.DestroyEntity(entity)
            && !world.HasLight(entity),
        "DestroyEntity did not remove LightComponent");

    nocturne::editor::EditorCommandContext commandContext{
        world,
        reflection,
        allocator,
        noc::EntityHandle::Invalid()
    };

    auto verifyPreset =
        [&](nocturne::editor::EditorEntityCreateKind kind,
            const char* name,
            bool expectRenderable,
            bool expectCamera,
            bool expectLight,
            noc::LightType expectedLightType) -> bool
        {
            nocturne::editor::CreateEntityCommand command;
            const noc::ResourceHandle mesh{
                7u,
                3u
            };
            const noc::AABB bounds{
                noc::Vec3{ -1.0f, -1.0f, -1.0f },
                noc::Vec3{ 1.0f, 1.0f, 1.0f }
            };

            if (!command.InitPreset(
                    name,
                    kind,
                    noc::EntityHandle::Invalid(),
                    mesh,
                    bounds)
                || !command.Execute(commandContext))
            {
                return false;
            }

            noc::EntityHandle created = command.CurrentEntity();
            if (!world.IsAlive(created)
                || !world.HasName(created)
                || !world.HasTransform(created)
                || world.HasRenderable(created) != expectRenderable
                || world.HasCamera(created) != expectCamera
                || world.HasLight(created) != expectLight)
            {
                return false;
            }

            if (expectRenderable)
            {
                const noc::RenderableComponent* renderable =
                    world.GetRenderable(created);
                if (!renderable
                    || renderable->mesh != mesh
                    || renderable->localBounds.min.x != -1.0f
                    || renderable->localBounds.max.x != 1.0f)
                {
                    return false;
                }
            }

            if (expectLight)
            {
                const noc::LightComponent* presetLight =
                    world.GetLight(created);
                if (!presetLight
                    || presetLight->type != expectedLightType)
                {
                    return false;
                }
            }

            if (!command.Undo(commandContext)
                || world.IsAlive(created)
                || !command.Redo(commandContext))
            {
                return false;
            }

            created = command.CurrentEntity();
            if (!world.IsAlive(created)
                || world.HasRenderable(created) != expectRenderable
                || world.HasCamera(created) != expectCamera
                || world.HasLight(created) != expectLight)
            {
                return false;
            }

            return command.Undo(commandContext);
        };

    ok &= CheckLight(
        verifyPreset(
            nocturne::editor::EditorEntityCreateKind::Empty,
            "Empty Entity",
            false, false, false,
            noc::LightType::Point),
        "Empty Entity preset/Undo/Redo failed");
    ok &= CheckLight(
        verifyPreset(
            nocturne::editor::EditorEntityCreateKind::StaticMesh,
            "Static Mesh",
            true, false, false,
            noc::LightType::Point),
        "Static Mesh Entity preset/Undo/Redo failed");
    ok &= CheckLight(
        verifyPreset(
            nocturne::editor::EditorEntityCreateKind::Camera,
            "Camera",
            false, true, false,
            noc::LightType::Point),
        "Camera preset/Undo/Redo failed");
    ok &= CheckLight(
        verifyPreset(
            nocturne::editor::EditorEntityCreateKind::DirectionalLight,
            "Directional Light",
            false, false, true,
            noc::LightType::Directional),
        "Directional Light preset/Undo/Redo failed");
    ok &= CheckLight(
        verifyPreset(
            nocturne::editor::EditorEntityCreateKind::PointLight,
            "Point Light",
            false, false, true,
            noc::LightType::Point),
        "Point Light preset/Undo/Redo failed");
    ok &= CheckLight(
        verifyPreset(
            nocturne::editor::EditorEntityCreateKind::SpotLight,
            "Spot Light",
            false, false, true,
            noc::LightType::Spot),
        "Spot Light preset/Undo/Redo failed");

    {
        nocturne::editor::CreateEntityCommand invalidPreset;
        ok &= CheckLight(
            !invalidPreset.InitPreset(
                "Invalid",
                static_cast<
                    nocturne::editor::EditorEntityCreateKind>(255)),
            "Invalid create preset kind was accepted");
    }

    world.Shutdown();
    reflection.Shutdown();

    NOC_LOG_INFO(
        "Phase16Light",
        "Light foundation tests %s",
        ok ? "PASS" : "FAIL");
    return ok;
}


bool RunPhase16PrimitiveAssetTests()
{
    NOC_LOG_INFO(
        "Phase16Primitive",
        "%s",
        "Built-in primitive asset tests begin");

    namespace fs = std::filesystem;

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "Data"
#endif

    struct PrimitiveExpectation
    {
        const char* relativePath;
        uint32_t vertexCount;
        uint32_t indexCount;
        noc::Vec3 expectedMin;
        noc::Vec3 expectedMax;
    };

    const PrimitiveExpectation expectations[] = {
        {
            "Meshes/Primitives/plane.nmsh",
            4u,
            6u,
            { -1.0f, 0.0f, -1.0f },
            { 1.0f, 0.0f, 1.0f }
        },
        {
            "Meshes/Primitives/cube.nmsh",
            24u,
            36u,
            { -1.0f, -1.0f, -1.0f },
            { 1.0f, 1.0f, 1.0f }
        },
        {
            "Meshes/Primitives/sphere.nmsh",
            561u,
            2880u,
            { -1.0f, -1.0f, -1.0f },
            { 1.0f, 1.0f, 1.0f }
        },
        {
            "Meshes/Primitives/cylinder.nmsh",
            134u,
            384u,
            { -1.0f, -1.0f, -1.0f },
            { 1.0f, 1.0f, 1.0f }
        }
    };

    bool ok = true;
    constexpr float kBoundsEpsilon = 1.0e-4f;

    for (const PrimitiveExpectation& expected : expectations)
    {
        const fs::path path =
            fs::path(NOC_CONTENT_ROOT)
            / fs::path(expected.relativePath);

        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            NOC_LOG_ERROR(
                "Phase16Primitive",
                "Failed to open built-in primitive '%s'",
                expected.relativePath);
            ok = false;
            continue;
        }

        std::vector<uint8_t> bytes{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()
        };

        noc::IntermediateMesh mesh;
        std::string error;
        if (!noc::ReadMeshBlob(
                bytes.data(),
                bytes.size(),
                &mesh,
                &error))
        {
            NOC_LOG_ERROR(
                "Phase16Primitive",
                "Failed to decode built-in primitive '%s': %s",
                expected.relativePath,
                error.c_str());
            ok = false;
            continue;
        }

        const uint32_t vertexCount =
            static_cast<uint32_t>(
                mesh.positions.size() / 3u);
        const uint32_t indexCount =
            static_cast<uint32_t>(
                mesh.indices.size());

        ok &= CheckLight(
            vertexCount == expected.vertexCount,
            "Primitive vertex-count regression");
        ok &= CheckLight(
            indexCount == expected.indexCount,
            "Primitive index-count regression");
        ok &= CheckLight(
            mesh.normals.size()
                == static_cast<size_t>(vertexCount) * 3u,
            "Primitive normals missing");
        ok &= CheckLight(
            mesh.tangents.size()
                == static_cast<size_t>(vertexCount) * 4u,
            "Primitive tangents missing");
        ok &= CheckLight(
            mesh.uvs.size()
                == static_cast<size_t>(vertexCount) * 2u,
            "Primitive UVs missing");
        ok &= CheckLight(
            mesh.submeshes.size() == 1u,
            "Primitive must contain one submesh");

        if (vertexCount == 0u)
            continue;

        noc::Vec3 minimum{
            mesh.positions[0],
            mesh.positions[1],
            mesh.positions[2]
        };
        noc::Vec3 maximum = minimum;

        bool finite = true;
        for (uint32_t vertex = 0;
             vertex < vertexCount;
             ++vertex)
        {
            const noc::Vec3 p{
                mesh.positions[vertex * 3u + 0u],
                mesh.positions[vertex * 3u + 1u],
                mesh.positions[vertex * 3u + 2u]
            };
            finite = finite
                && std::isfinite(p.x)
                && std::isfinite(p.y)
                && std::isfinite(p.z);
            minimum.x = (std::min)(minimum.x, p.x);
            minimum.y = (std::min)(minimum.y, p.y);
            minimum.z = (std::min)(minimum.z, p.z);
            maximum.x = (std::max)(maximum.x, p.x);
            maximum.y = (std::max)(maximum.y, p.y);
            maximum.z = (std::max)(maximum.z, p.z);
        }

        ok &= CheckLight(
            finite,
            "Primitive contains non-finite positions");
        ok &= CheckLight(
            std::fabs(minimum.x - expected.expectedMin.x)
                    <= kBoundsEpsilon
                && std::fabs(minimum.y - expected.expectedMin.y)
                    <= kBoundsEpsilon
                && std::fabs(minimum.z - expected.expectedMin.z)
                    <= kBoundsEpsilon
                && std::fabs(maximum.x - expected.expectedMax.x)
                    <= kBoundsEpsilon
                && std::fabs(maximum.y - expected.expectedMax.y)
                    <= kBoundsEpsilon
                && std::fabs(maximum.z - expected.expectedMax.z)
                    <= kBoundsEpsilon,
            "Primitive bounds regression");

        bool indicesValid = true;
        for (const uint32_t index : mesh.indices)
            indicesValid = indicesValid && index < vertexCount;
        ok &= CheckLight(
            indicesValid,
            "Primitive contains out-of-range indices");
    }

    NOC_LOG_INFO(
        "Phase16Primitive",
        "Built-in primitive asset tests %s",
        ok ? "PASS" : "FAIL");
    return ok;
}
