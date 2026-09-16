#pragma once
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace noc {

    // Minimal JSON writer.
    // Design choice (not directly from the book): hand-rolled writer to avoid external deps.
    class JsonWriter {
    public:
        explicit JsonWriter(std::ostream& os) : os_(os) {}

        void BeginObject();
        void EndObject();

        void BeginArray();
        void EndArray();

        void Key(std::string_view k);

        void String(std::string_view s);
        void UInt(uint64_t v);
        void Int(int64_t v);
        void Bool(bool v);
        void Null();

        // Convenience: "key": "value"
        void KeyString(std::string_view k, std::string_view v);
        void KeyUInt(std::string_view k, uint64_t v);
        void KeyInt(std::string_view k, int64_t v);
        void KeyBool(std::string_view k, bool v);

    private:
        void CommaIfNeeded_();
        void WriteEscaped_(std::string_view s);
        void PushScope_(char scope);
        void PopScope_(char scope);
        void BeforeValue_();


    private:
        bool afterKey_ = false;
        std::ostream& os_;
        std::vector<char> scopes_;
        std::vector<bool> first_;
    };

} // namespace noc
