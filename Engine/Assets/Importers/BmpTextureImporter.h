#pragma once
#include "Assets/Importers/IAssetImporter.h"

namespace noc {

    class BmpTextureImporter final : public IAssetImporter {
    public:
        std::string_view Id() const override { return "noc.bmp"; }
        uint32_t Version() const override { return 1; }

        bool CanImportExtension(std::string_view extLower) const override;
        ImportResult Import(const ImportRequest& req) override;

        // Pipeline provides bytes (same design choice as TextImporter).
        ImportResult ImportFromMemory(const ImportRequest& req, const uint8_t* bytes, size_t size);
    };

} // namespace noc
