#pragma once
#include <cstddef>
#include <string>
#include <utility>
#include <vector>


namespace aesp {
    struct Json {
        enum class Type { Null, Bool, Number, String, Array, Object };
        Type type = Type::Null;
        bool boolean = false;
        double number = 0;
        std::string string;
        vector<Json> array;
        vector<pair<std::string, Json>> object;

        bool has(const std::string& key) const;
        const Json* find(const std::string& key) const;
        const Json& at(const std::string& key) const;
        const std::string& str(const std::string& key) const;
        size_t size_value(const std::string& key) const;
    };

    Json parse_json(const std::string& text);
}