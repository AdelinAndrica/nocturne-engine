#pragma once
#include <string>
#include <string_view>

namespace noc {

    class TextResource {
    public:
        explicit TextResource(std::string s) : text_(std::move(s)) {}

        std::string_view View() const { return text_; }
        const std::string& Str() const { return text_; }
        size_t Size() const { return text_.size(); }

    private:
        std::string text_;
    };

} // namespace noc