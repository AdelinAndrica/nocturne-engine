#pragma once
#include <memory>
#include <string_view>
#include <vector>

#include "Assets/Importers/IAssetImporter.h"

namespace noc {

    class AssetImporterRegistry {
    public:
        bool Register(std::unique_ptr<IAssetImporter> importer);

        // selects by extension (".obj" etc; caller passes "obj")
        IAssetImporter* FindForExtension(std::string_view extLower) const;

    private:
        std::vector<std::unique_ptr<IAssetImporter>> importers_;
    };

} // namespace noc
