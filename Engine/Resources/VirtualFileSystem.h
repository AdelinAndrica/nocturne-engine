#pragma once

#include "FileHandle.h"
#include "IFileMount.h"

#include "Core/Memory/Allocator.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace noc
{
    class IFileMount;

    /**
     * @brief Maps normalized virtual asset paths onto mounted physical storage.
     *
     * VirtualFileSystem lets runtime code ask for a path such as
     * @c "Meshes/triangle.nmsh" without knowing whether the bytes come from a loose
     * development directory or an archive.
     *
     * @par When to use
     * Use VFS for runtime file access that belongs to the engine content namespace.
     * ResourceManager builds its loading path on top of this class.
     *
     * @par Do not use for
     * Do not scatter absolute machine-specific paths through gameplay/editor code.
     * Physical root selection is application boot policy.
     *
     * @par Mount priority
     * OpenRead() searches mounts in reverse order. Therefore a mount added later
     * has higher priority and may override the same virtual path from an earlier mount.
     *
     * @par Ownership
     * VFS owns the mount objects stored inside it. FileHandle values are borrowed
     * access tokens into those mounts and should be closed with Close().
     *
     * @par Threading
     * No public general thread-safety contract is declared by VirtualFileSystem.
     * ResourceManager currently uses it from load jobs according to Nocturne's
     * resource pipeline; arbitrary concurrent caller usage should not be assumed safe.
     *
     * @see ResourceManager
     * @ingroup resources
     */
    class VirtualFileSystem
    {
    public:
        /**
         * @brief Constructs an empty VFS with no mounted storage providers.
         *
         * Mounts are added explicitly through MountLooseDirectory()/MountArchive().
         */
        VirtualFileSystem() = default;

        /** @brief Destroys the VFS and its owned mount objects. */
        ~VirtualFileSystem();

        /**
         * @brief Adds a loose-directory mount.
         *
         * @param physicalRootUtf8 Physical directory root.
         * @return false only for a null/empty path in this API layer; otherwise the
         * mount object is appended.
         *
         * @note Later mounts have higher lookup priority.
         */
        bool MountLooseDirectory(const char* physicalRootUtf8);

        /**
         * @brief Adds an indexed archive mount.
         *
         * @param archivePathUtf8 Physical archive path.
         * @return true when the archive index is built and the mount is appended;
         * false for invalid input or archive-index failure.
         */
        bool MountArchive(const char* archivePathUtf8);

        /**
         * @brief Opens one virtual path for reading.
         *
         * The path is normalized to Nocturne's relative virtual namespace, then
         * mounts are searched from newest to oldest.
         *
         * @param virtualPath Virtual content path.
         * @return Valid FileHandle on success; an invalid handle if normalization
         * fails or no mount contains the file.
         *
         * @see Close
         */
        FileHandle OpenRead(std::string_view virtualPath);

        /**
         * @brief Closes a VFS file handle and clears its state.
         *
         * Invalid/already-closed handles are ignored, making repeated Close() calls
         * defensive no-ops.
         *
         * @param h Handle to close. The passed value is invalidated in place.
         */
        void Close(FileHandle& h);

        /**
         * @brief Reads up to @p bytes bytes from the current handle cursor.
         *
         * @param h Open handle.
         * @param dst Destination buffer owned by the caller.
         * @param bytes Requested byte count.
         * @return Number of bytes read; 0 for an invalid handle or no progress.
         */
        size_t Read(FileHandle& h, void* dst, size_t bytes);

        /**
         * @brief Returns the size of an open file.
         * @return File size in bytes, or 0 for an invalid handle.
         *
         * @note A legitimate empty file also has size 0.
         */
        uint64_t Size(const FileHandle& h) const;

        /** @brief Returns the number of currently mounted storage providers. */
        size_t MountCount() const { return mounts_.size(); }

        /**
         * @brief Reads an entire virtual file into allocator-owned memory.
         *
         * @param virtualPath Virtual content path.
         * @param outSize Receives the byte count; set to 0 on failure or empty file.
         * @param alloc Allocator used for the returned buffer.
         * @return Allocated byte buffer on non-empty success; nullptr on failure.
         *
         * @warning Empty files are treated as valid but currently also return
         * nullptr with outSize == 0. Do not use the pointer alone to distinguish an
         * empty file from failure.
         *
         * @par Ownership
         * On non-null success the caller owns the returned allocation and must call
         * @c alloc.Deallocate(returnedPointer).
         */
        uint8_t* ReadAllBytes(const char* virtualPath, size_t& outSize, IAllocator& alloc);

        /**
         * @brief Reads an entire virtual file into a null-terminated UTF-8 buffer.
         *
         * @param virtualPath Virtual content path.
         * @param alloc Allocator used for the returned text.
         * @param outText Receives the allocated null-terminated buffer on success;
         * set to nullptr on failure.
         * @return true on success, including an empty file; false on failure.
         *
         * @par Ownership
         * Caller owns @p outText on success and must release it with
         * @c alloc.Deallocate(outText).
         */
        bool ReadAllText(const char* virtualPath, IAllocator& alloc, char*& outText);

    private:
        std::vector<std::unique_ptr<IFileMount>> mounts_;
    };
}
