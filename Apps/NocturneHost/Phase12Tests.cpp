#include "Phase12CookPack.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Core/Log.h"
#include "Runtime/Engine.h"

// ResourceManager access (exact header names per combined.md conventions)
#include "Resources/ResourceManager.h"
#include "Resources/VirtualFileSystem.h"

// Phase 11 pipeline accessor
#include "Assets/AssetImportPipeline.h"


static bool WriteTextFile_(const std::filesystem::path& p, const std::string& text) {
	std::error_code ec;
	std::filesystem::create_directories(p.parent_path(), ec);
	std::ofstream f(p, std::ios::binary | std::ios::trunc);
	if (!f) return false;
	f.write(text.data(), (std::streamsize)text.size());
	return (bool)f;
}

bool RunPhase12Tests(noc::Engine& engine)
{
	NOC_LOG_INFO("Test", "==============================");
	NOC_LOG_INFO("Test", "Phase 12 — Cooker & Packager Tools Tests");
	NOC_LOG_INFO("Test", "==============================");

	// 1) Create deterministic source asset under Data/Phase12/
	const std::filesystem::path srcPhys =
		std::filesystem::path("..") / ".." / "Data" / "Phase12" / "hello.txt";

	if (!WriteTextFile_(srcPhys, "hello-phase12\n"))
	{
		NOC_LOG_ERROR("Test", "Failed to write source asset: %s", srcPhys.string().c_str());
		return false;
	}

	NOC_LOG_INFO("Test", "Source asset written to: %s",
		std::filesystem::absolute(srcPhys).string().c_str());

	{
		noc::FileHandle vh = engine.VFS().OpenRead("Phase12/hello.txt");
		if (!vh.valid) {
			NOC_LOG_ERROR("Test", "VFS still cannot see Phase12/hello.txt after writing it. Check working directory.");
			return false;
		}
		engine.VFS().Close(vh);
		NOC_LOG_INFO("Test", "VFS can see Phase12/hello.txt");
	}


	// 2) Run Phase 11 importer on vpath
	const std::string vpath = "Phase12/hello.txt";
	if (!engine.Assets().ImportOne(vpath))
	{
		NOC_LOG_ERROR("Test", "ImportOne failed: %s", vpath.c_str());
		return false;
	}

	// 3) Cook + Pack
	noc::tools::Phase12CookPack::CookOptions cookOpt{};
	cookOpt.ddcRoot = "DerivedDataCache";
	cookOpt.cookedRoot = "Cooked";
	cookOpt.cleanCooked = true;

	std::vector<noc::tools::CookFileInfo> cookedFiles;
	std::string err;
	if (!noc::tools::Phase12CookPack::Cook(cookOpt, &cookedFiles, &err))
	{
		NOC_LOG_ERROR("Test", "Cook failed: %s", err.c_str());
		return false;
	}

	noc::tools::Phase12CookPack::PackOptions packOpt{};
	packOpt.cookedRoot = "Cooked";
	packOpt.outZip = "Cooked/NocturneContent.zip";

	err.clear();
	if (!noc::tools::Phase12CookPack::Pack(packOpt, &err))
	{
		NOC_LOG_ERROR("Test", "Pack failed: %s", err.c_str());
		return false;
	}

	// 4) Mount the archive (stored-only zip) into VFS
	if (!engine.VFS().MountArchive("Cooked/NocturneContent.zip"))
	{
		NOC_LOG_ERROR("Test", "MountArchive failed");
		return false;
	}
	NOC_LOG_INFO("Test", "Mounted archive: Cooked/NocturneContent.zip");

	auto EndsWith = [](const std::string& s, const char* suffix) -> bool {
		const size_t n = std::strlen(suffix);
		return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
		};

	auto Contains = [](const std::string& s, const std::string& needle) -> bool {
		return s.find(needle) != std::string::npos;
		};

	auto FileSizeOr0 = [](const std::filesystem::path& p) -> uint64_t {
		std::error_code ec;
		if (!std::filesystem::exists(p, ec)) return 0;
		const auto sz = std::filesystem::file_size(p, ec);
		return ec ? 0 : (uint64_t)sz;
		};


	// 5) Discover the correct cooked artifact for OUR Phase12 hello.txt (do not pick random blobs)
	const std::string want = "Content/Phase12/hello.txt";

	std::string packagedVPath;

	// Prefer the runtime text blob if present (typical Phase11 output convention)
	for (const auto& f : cookedFiles)
	{
		if (Contains(f.relPath, want) && EndsWith(f.relPath, ".txt.bin"))
		{
			packagedVPath = f.relPath;
			break;
		}
	}

	// Fallback to meta (still validates cook->pack->mount->read pipeline deterministically)
	if (packagedVPath.empty())
	{
		for (const auto& f : cookedFiles)
		{
			if (Contains(f.relPath, want) && EndsWith(f.relPath, ".meta.json"))
			{
				packagedVPath = f.relPath;
				break;
			}
		}
	}

	if (packagedVPath.empty())
	{
		NOC_LOG_ERROR("Test", "Did not find cooked artifacts for '%s' in Cook output.", want.c_str());
		NOC_LOG_ERROR("Test", "Cooked file list:");
		for (const auto& f : cookedFiles)
			NOC_LOG_ERROR("Test", "  - %s", f.relPath.c_str());
		return false;
	}

	// --- DDC sanity check ---
	// cooked relPath is "Content/<ddcRel>"
	// so original DDC file is "DerivedDataCache/<ddcRel>"
	const std::string prefix = "Content/";
	std::string ddcRel = packagedVPath;

	if (ddcRel.rfind(prefix, 0) == 0)
		ddcRel = ddcRel.substr(prefix.size());

	const std::filesystem::path ddcBlobPath =
		std::filesystem::path("DerivedDataCache") / std::filesystem::path(ddcRel);

	const uint64_t ddcSize = FileSizeOr0(ddcBlobPath);

	NOC_LOG_INFO("Test", "DDC blob path: %s (bytes=%llu)",
		ddcBlobPath.generic_string().c_str(),
		(unsigned long long)ddcSize);

	if (ddcSize == 0)
	{
		NOC_LOG_ERROR("Test",
			"DDC artifact is empty. This is a Phase 11 importer issue.\n"
			"Expected non-empty runtime data for Phase12/hello.txt.");
		return false;
	}


	// 6) Open/read/close once
	noc::FileHandle h = engine.VFS().OpenRead(packagedVPath);
	if (!h.valid)
	{
		NOC_LOG_ERROR("Test", "VFS OpenRead failed for packaged artifact: %s", packagedVPath.c_str());
		return false;
	}

	const uint64_t sz = engine.VFS().Size(h);
	std::vector<uint8_t> bytes((size_t)sz);

	const size_t got = engine.VFS().Read(h, bytes.data(), bytes.size());
	engine.VFS().Close(h);

	if (got != bytes.size())
	{
		NOC_LOG_ERROR("Test", "VFS read mismatch: got=%zu expected=%zu", got, bytes.size());
		return false;
	}

	NOC_LOG_INFO("Test", "Loaded packaged artifact OK: %s (bytes=%llu)",
		packagedVPath.c_str(), (unsigned long long)sz);

	// 7) Content-specific checks only when appropriate
	if (EndsWith(packagedVPath, ".txt.bin"))
	{
		const std::string loaded((const char*)bytes.data(), (const char*)bytes.data() + bytes.size());
		if (loaded.find("hello-phase12") == std::string::npos)
		{
			NOC_LOG_ERROR("Test", "Packaged text blob did not contain expected marker. vpath=%s",
				packagedVPath.c_str());
			return false;
		}
		NOC_LOG_INFO("Test", "Text marker found in packaged blob.");
	}
	else if (EndsWith(packagedVPath, ".meta.json"))
	{
		const std::string loaded((const char*)bytes.data(), (const char*)bytes.data() + bytes.size());
		// Loose sanity: proves we packed the right meta for hello.txt
		if (loaded.find("hello.txt") == std::string::npos)
		{
			NOC_LOG_ERROR("Test", "Packaged meta did not mention hello.txt. vpath=%s",
				packagedVPath.c_str());
			return false;
		}
		NOC_LOG_INFO("Test", "Meta sanity check OK.");
	}

	NOC_LOG_INFO("Test", "Phase 12 tests PASSED");
	return true;
}


