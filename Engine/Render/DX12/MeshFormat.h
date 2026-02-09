#pragma once
#include <cstdint>
#include <vector>

namespace noc
{
	// Minimal binary mesh format for Phase 10:
	// Header:
	//   char     magic[4] = "NMSH"
	//   uint32   version  = 1
	//   uint32   vertexCount
	//   uint32   indexCount
	// Vertex:
	//   float3 position
	//   float4 color
	// Indices:
	//   uint16 indexCount entries
	struct MeshVertexPC
	{
		float px, py, pz;
		float r, g, b, a;
	};

	struct CpuMeshPC
	{
		std::vector<MeshVertexPC> vertices;
		std::vector<uint16_t> indices;
	};

	// Returns false on parse error.
	bool ParseNocMeshPC(const uint8_t* bytes, size_t size, CpuMeshPC& out, const char*& outErr);
}
