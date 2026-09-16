#include "Assets/JsonWriter.h"

#include <iomanip>

namespace noc {

    void JsonWriter::PushScope_(char scope) {
        scopes_.push_back(scope);
        first_.push_back(true);
    }

    void JsonWriter::PopScope_(char scope) {
        if (!scopes_.empty() && scopes_.back() == scope) {
            scopes_.pop_back();
            first_.pop_back();
        }
    }

    void JsonWriter::CommaIfNeeded_() {
        if (first_.empty()) return;
        if (!first_.back()) os_ << ",";
        first_.back() = false;
    }

    // NEW: called by all value writers
    void JsonWriter::BeforeValue_() {
        if (scopes_.empty()) {
            // root scope behaves like a list of values
            CommaIfNeeded_();
            afterKey_ = false;
            return;
        }

        const char scope = scopes_.back();
        if (scope == 'a') {
            // arrays need commas between values
            CommaIfNeeded_();
        }
        else if (scope == 'o') {
            // objects: commas are handled by Key(), NEVER before a value
            // If you want, you can assert(afterKey_) here to catch misuse.
        }

        afterKey_ = false; // once any value is written, we are no longer "just after Key"
    }

    void JsonWriter::WriteEscaped_(std::string_view s) {
        os_ << "\"";
        for (char c : s) {
            switch (c) {
            case '\"': os_ << "\\\""; break;
            case '\\': os_ << "\\\\"; break;
            case '\b': os_ << "\\b"; break;
            case '\f': os_ << "\\f"; break;
            case '\n': os_ << "\\n"; break;
            case '\r': os_ << "\\r"; break;
            case '\t': os_ << "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    os_ << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << (int)((unsigned char)c) << std::dec;
                }
                else {
                    os_ << c;
                }
                break;
            }
        }
        os_ << "\"";
    }

    void JsonWriter::BeginObject() {
        BeforeValue_();
        os_ << "{";
        PushScope_('o');
    }

    void JsonWriter::EndObject() {
        os_ << "}";
        PopScope_('o');
    }

    void JsonWriter::BeginArray() {
        BeforeValue_();
        os_ << "[";
        PushScope_('a');
    }

    void JsonWriter::EndArray() {
        os_ << "]";
        PopScope_('a');
    }

    void JsonWriter::Key(std::string_view k) {
        // Only valid in object scope
        // Commas between key/value pairs are handled here.
        CommaIfNeeded_();
        WriteEscaped_(k);
        os_ << ":";
        afterKey_ = true;
    }

    void JsonWriter::String(std::string_view s) {
        BeforeValue_();
        WriteEscaped_(s);
    }

    void JsonWriter::UInt(uint64_t v) {
        BeforeValue_();
        os_ << v;
    }

    void JsonWriter::Int(int64_t v) {
        BeforeValue_();
        os_ << v;
    }

    void JsonWriter::Bool(bool v) {
        BeforeValue_();
        os_ << (v ? "true" : "false");
    }

    void JsonWriter::Null() {
        BeforeValue_();
        os_ << "null";
    }

    void JsonWriter::KeyString(std::string_view k, std::string_view v) {
        Key(k);
        String(v);
    }

    void JsonWriter::KeyUInt(std::string_view k, uint64_t v) {
        Key(k);
        UInt(v);
    }

    void JsonWriter::KeyInt(std::string_view k, int64_t v) {
        Key(k);
        Int(v);
    }

    void JsonWriter::KeyBool(std::string_view k, bool v) {
        Key(k);
        Bool(v);
    }

} // namespace noc
