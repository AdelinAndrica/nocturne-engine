#include "Assets/Importers/AssetImporterRegistry.h"

namespace noc {

    bool AssetImporterRegistry::Register(std::unique_ptr<IAssetImporter> importer) {
        if (!importer) return false;
        importers_.push_back(std::move(importer));
        return true;
    }

    IAssetImporter* AssetImporterRegistry::FindForExtension(std::string_view extLower) const {
        for (auto& it : importers_) {
            if (it && it->CanImportExtension(extLower)) return it.get();
        }
        return nullptr;
    }

} // namespace noc
