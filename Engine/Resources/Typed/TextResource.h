#pragma once
#include <string>
#include <string_view>

namespace noc {

    /**
     * @brief Decoded runtime text resource owned by ResourceManager.
     *
     * TextResource stores the full decoded text in an owned std::string.
     * Callers receive a borrowed const TextResource* from ResourceManager::GetText().
     *
     * @warning Do not delete the TextResource pointer returned by ResourceManager.
     *
     * @ingroup resources
     */
    class TextResource {
    public:
        /** @brief Constructs the resource by taking ownership of the supplied string value. */
        explicit TextResource(std::string s) : text_(std::move(s)) {}

        /**
         * @brief Returns a lightweight read-only view of the stored text.
         * @warning The view becomes invalid when this TextResource is destroyed.
         */
        std::string_view View() const { return text_; }

        /**
         * @brief Returns the underlying owned std::string as read-only reference.
         * @warning The reference becomes invalid when this TextResource is destroyed.
         */
        const std::string& Str() const { return text_; }

        /** @brief Returns text size in bytes, excluding the string terminator. */
        size_t Size() const { return text_.size(); }

    private:
        std::string text_;
    };

} // namespace noc
