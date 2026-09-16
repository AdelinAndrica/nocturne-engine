#pragma once
#include "Assets/Importers/IAssetImporter.h"

namespace noc {

    class ObjMeshImporter final : public IAssetImporter {
    public:
        std::string_view Id() const override { return "noc.obj"; }
        uint32_t Version() const override { return 1; }

        bool CanImportExtension(std::string_view extLower) const override;
        ImportResult Import(const ImportRequest& req) override;

        ImportResult ImportFromMemory(const ImportRequest& req, const uint8_t* bytes, size_t size);
    };

} // namespace noc
