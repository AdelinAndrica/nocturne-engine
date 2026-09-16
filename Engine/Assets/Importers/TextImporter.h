#pragma once
#include "Assets/Importers/IAssetImporter.h"

namespace noc {

    class TextImporter final : public IAssetImporter {
    public:
        std::string_view Id() const override { return "noc.text"; }
        uint32_t Version() const override { return 1; }

        bool CanImportExtension(std::string_view extLower) const override;
        ImportResult Import(const ImportRequest& req) override;
    };

} // namespace noc
