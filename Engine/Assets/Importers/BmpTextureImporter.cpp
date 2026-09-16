#include "Assets/Importers/BmpTextureImporter.h"

#include <chrono>
#include <cstring>

namespace noc {

#pragma pack(push, 1)
    struct BmpFileHeader {
        uint16_t bfType;      // 'BM'
        uint32_t bfSize;
        uint16_t bfReserved1;
        uint16_t bfReserved2;
        uint32_t bfOffBits;
    };
    struct BmpInfoHeader {
        uint32_t biSize;      // 40
        int32_t  biWidth;
        int32_t  biHeight;
        uint16_t biPlanes;
        uint16_t biBitCount;  // 24 or 32
        uint32_t biCompression; // 0 = BI_RGB
        uint32_t biSizeImage;
        int32_t  biXPelsPerMeter;
        int32_t  biYPelsPerMeter;
        uint32_t biClrUsed;
        uint32_t biClrImportant;
    };
#pragma pack(pop)

    static uint64_t NowUtcMs_() {
        using namespace std::chrono;
        return (uint64_t)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    bool BmpTextureImporter::CanImportExtension(std::string_view extLower) const {
        return extLower == "bmp";
    }

    ImportResult BmpTextureImporter::Import(const ImportRequest& req) {
        ImportResult r{};
        r.ok = true;
        r.importerId = std::string(Id());
        r.importerVersion = Version();
        r.metadata.importTimestampUtcMs = NowUtcMs_();
        r.metadata.importerId = r.importerId;
        r.metadata.importerVersion = r.importerVersion;
        return r;
    }

    ImportResult BmpTextureImporter::ImportFromMemory(const ImportRequest& req, const uint8_t* bytes, size_t size) {
        ImportResult r{};
        r.importerId = std::string(Id());
        r.importerVersion = Version();

        if (!bytes || size < sizeof(BmpFileHeader) + sizeof(BmpInfoHeader)) {
            r.ok = false; r.error = "BmpImporter: file too small";
            return r;
        }

        BmpFileHeader fh{};
        std::memcpy(&fh, bytes, sizeof(fh));
        if (fh.bfType != 0x4D42) { // 'BM'
            r.ok = false; r.error = "BmpImporter: not BM";
            return r;
        }

        BmpInfoHeader ih{};
        std::memcpy(&ih, bytes + sizeof(fh), sizeof(ih));
        if (ih.biSize != 40 || ih.biPlanes != 1) {
            r.ok = false; r.error = "BmpImporter: unsupported header";
            return r;
        }
        if (ih.biCompression != 0) {
            r.ok = false; r.error = "BmpImporter: compressed BMP not supported";
            return r;
        }
        if (ih.biBitCount != 24 && ih.biBitCount != 32) {
            r.ok = false; r.error = "BmpImporter: only 24/32-bit BMP supported";
            return r;
        }

        const int w = ih.biWidth;
        const int hSigned = ih.biHeight;
        const int h = (hSigned < 0) ? -hSigned : hSigned;
        const bool topDown = (hSigned < 0);

        if (w <= 0 || h <= 0) {
            r.ok = false; r.error = "BmpImporter: invalid dimensions";
            return r;
        }

        const uint32_t bpp = ih.biBitCount / 8;
        const uint32_t rowBytesNoPad = (uint32_t)w * bpp;
        const uint32_t rowPad = (4 - (rowBytesNoPad % 4)) % 4;
        const uint32_t rowStride = rowBytesNoPad + rowPad;

        const uint32_t pixelOffset = fh.bfOffBits;
        if (pixelOffset >= size) {
            r.ok = false; r.error = "BmpImporter: bad pixel offset";
            return r;
        }
        if ((uint64_t)pixelOffset + (uint64_t)rowStride * (uint64_t)h > size) {
            r.ok = false; r.error = "BmpImporter: truncated pixel data";
            return r;
        }

        IntermediateTexture tex{};
        tex.format = IntermediateTextureFormat::BGRA8_UNorm;
        tex.colorSpace = req.options.forceLinearColor ? IntermediateColorSpace::Linear : IntermediateColorSpace::SRGB;

        IntermediateMip mip{};
        mip.width = (uint32_t)w;
        mip.height = (uint32_t)h;
        mip.pixels.resize((size_t)w * (size_t)h * 4);

        // Convert BGR/BGRA to BGRA8
        for (int y = 0; y < h; ++y) {
            const int srcY = topDown ? y : (h - 1 - y);
            const uint8_t* src = bytes + pixelOffset + (size_t)srcY * rowStride;
            uint8_t* dst = mip.pixels.data() + (size_t)y * (size_t)w * 4;

            for (int x = 0; x < w; ++x) {
                const uint8_t B = src[x * bpp + 0];
                const uint8_t G = src[x * bpp + 1];
                const uint8_t R = src[x * bpp + 2];
                const uint8_t A = (bpp == 4) ? src[x * bpp + 3] : 255;

                dst[x * 4 + 0] = B;
                dst[x * 4 + 1] = G;
                dst[x * 4 + 2] = R;
                dst[x * 4 + 3] = A;
            }
        }

        tex.mips.push_back(std::move(mip));
        r.asset = std::move(tex);
        r.ok = true;

        r.metadata.importTimestampUtcMs = NowUtcMs_();
        r.metadata.importerId = r.importerId;
        r.metadata.importerVersion = r.importerVersion;

        return r;
    }

} // namespace noc
