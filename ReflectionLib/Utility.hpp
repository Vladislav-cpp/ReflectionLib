#pragma once
#include "pch.h"

class Type;

namespace reflection {

using IniNode = std::vector<std::pair<std::string,std::string>>;
IniNode parse_ini(const std::string& filename);

// Простий XML-вузол
struct XmlNode {
    using Attributes = std::unordered_map<std::string, std::vector<std::string>>;

    std::string NodeName;

    std::string className;
    Attributes attributes;
    std::vector<XmlNode> children;
};

// Парсер: головна функція
XmlNode parse_xml(const std::string& text);

class DataLoader {
public:
    bool load(const std::string& filename);

    bool has(const std::string& key) const {
        return data.find(key) != data.end();
    }

    std::string getString(const std::string& key, const std::string& def = "") const {
        auto it = data.find(key);
        return (it != data.end()) ? it->second : def;
    }

public:
    std::unordered_map<std::string, std::string> data;
};

void* createObjectFromData(Type* type, const DataLoader& loader);

void* build_from_xmlnode(const XmlNode& elem);

}

