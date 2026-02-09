#include "MeshFormat.h"
#include <cstring>

namespace noc
{
	static bool ReadU32_(const uint8_t*& p, const uint8_t* end, uint32_t& out)
	{
		if (p + 4 > end) return false;
		memcpy(&out, p, 4);
		p += 4;
		return true;
	}

	bool ParseNocMeshPC(const uint8_t* bytes, size_t size, CpuMeshPC& out, const char*& outErr)
	{
		outErr = nullptr;
		out.vertices.clear();
		out.indices.clear();

		if (!bytes || size < 16)
		{
			outErr = "mesh: too small";
			return false;
		}

		const uint8_t* p = bytes;
		const uint8_t* end = bytes + size;

		char magic[4]{};
		memcpy(magic, p, 4);
		p += 4;

		if (memcmp(magic, "NMSH", 4) != 0)
		{
			outErr = "mesh: bad magic";
			return false;
		}

		uint32_t ver = 0, vc = 0, ic = 0;
		if (!ReadU32_(p, end, ver) || !ReadU32_(p, end, vc) || !ReadU32_(p, end, ic))
		{
			outErr = "mesh: header truncated";
			return false;
		}

		if (ver != 1)
		{
			outErr = "mesh: unsupported version";
			return false;
		}

		const size_t vBytes = (size_t)vc * sizeof(MeshVertexPC);
		const size_t iBytes = (size_t)ic * sizeof(uint16_t);

		if ((size_t)(end - p) < vBytes + iBytes)
		{
			outErr = "mesh: payload truncated";
			return false;
		}

		out.vertices.resize(vc);
		memcpy(out.vertices.data(), p, vBytes);
		p += vBytes;

		out.indices.resize(ic);
		memcpy(out.indices.data(), p, iBytes);
		p += iBytes;

		return true;
	}
}
