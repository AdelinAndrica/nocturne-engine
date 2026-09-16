#include "Assets/Importers/TextImporter.h"

#include <chrono>

#include "Core/Log.h"

namespace noc {

    static uint64_t NowUtcMs_() {
        using namespace std::chrono;
        return (uint64_t)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    bool TextImporter::CanImportExtension(std::string_view extLower) const {
        return extLower == "txt";
    }

    ImportResult TextImporter::Import(const ImportRequest& req) {
        ImportResult r{};
        if (req.sourceVPathNormalized.empty() && req.sourcePhysicalPath.empty()) {
            r.ok = false; r.error = "TextImporter: missing source path";
            return r;
        }

        r.importerId = std::string(Id());
        r.importerVersion = Version();

        // TextImporter doesn't do IO itself; pipeline provides bytes. (Phase 11 contract)
        // Therefore, pipeline will replace this importer with a pipeline-level read step.
        // To keep importer interface simple, we store an empty result here; pipeline will fill from bytes.
        // Design choice: importer assumes pipeline passes file bytes via a different path.
        r.ok = true;
        IntermediateText t{};
        t.text = ""; // filled by pipeline read stage
        r.asset = std::move(t);

        r.metadata.importTimestampUtcMs = NowUtcMs_();
        r.metadata.importerId = r.importerId;
        r.metadata.importerVersion = r.importerVersion;
        r.metadata.optionsHash = 0;

        return r;
    }

} // namespace noc
