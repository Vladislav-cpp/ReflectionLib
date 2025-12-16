#include "pch.h"
#include "Utility.hpp"
#include "ReflectionLib.hpp"


namespace reflection {
// -------------------------------------------------------------------------------------------------------------------------------
//                                                parse_INI begin
// -------------------------------------------------------------------------------------------------------------------------------

static inline std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t\r\n");
	if(a == std::string::npos) return "";
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

IniNode parse_ini(const std::string& filename) {
	IniNode result;
	std::ifstream file(filename);
	if(!file.is_open()) throw std::runtime_error("Cannot open ini file: " + filename);

	std::string line;
	while(std::getline(file, line)) {

		line = trim(line);

		// Порожні строки або коментарі #
		if(line.empty() || line[0] == '#') continue;

		// Шукаємо =
		size_t eq = line.find('=');
		if(eq == std::string::npos) continue; // або помилка

		std::string key = trim(line.substr(0, eq));
		std::string value = trim(line.substr(eq + 1));

		result.emplace_back(key, value);
	}

	return result;
}

// -------------------------------------------------------------------------------------------------------------------------------
//                                                parse_INI end
// -------------------------------------------------------------------------------------------------------------------------------


// -------------------------------------------------------------------------------------------------------------------------------
//                                                parse_xml begin
// -------------------------------------------------------------------------------------------------------------------------------

// <head tmp=0... />
// or
// <head> tmp={"-1", "10"} </head> // std::pair<int> - {}

struct DataKey {

    // Макрос для генерації поля + конструктора
#define AddDataType(Type, Name) \
    Type Data##Name; \
    DataKey(Type t) : Data##Name(t) {}

    // Цілі числа
    AddDataType(int, int)
    AddDataType(short, short)
    AddDataType(long, long)
    AddDataType(long long, llong)

    // Від’ємні/беззнакові числа
    AddDataType(unsigned int, uint)
    AddDataType(unsigned short, ushort)
    AddDataType(unsigned long, ulong)
    AddDataType(unsigned long long, ullong)

    // Дробові числа
    AddDataType(float, float)
    AddDataType(double, double)
    AddDataType(long double, ldouble)

    // Логічний тип
    AddDataType(bool, bool)

    // Символи
    AddDataType(char, char)
    AddDataType(unsigned char, ushar)
    AddDataType(wchar_t, wchar_t)

    // Рядки
    std::string dataString;
    DataKey(std::string t) : dataString(t) {};

    // Пари
    std::pair<int, int> dataPairInt;
    DataKey(std::pair<int, int> t) : dataPairInt(t) {};
#undef AddDataType
};

static void skipWhitespace(std::string& src, size_t& pos) {
    while( pos < src.size() && std::isspace( (unsigned char)src[pos]) ) pos++;
}

//              std::isspace()
// A standard white-space character:
// Space            ( 0x20, ' '  ),
// Form feed        ( 0x0c, '\f' ),
// Line feed        ( 0x0a, '\n' ),
// Carriage return  ( 0x0d, '\r' ),
// Horizontal tab   ( 0x09, '\t' ),
// Vertical tab     ( 0x0b, '\v' )

//              std::isalnum
// following characters are alphanumeric:
// digits (0123456789)
// uppercase letters (ABCDEFGHIJKLMNOPQRSTUVWXYZ)
// lowercase letters (abcdefghijklmnopqrstuvwxyz)

//static bool startsWith(const std::string& src) {
//    return src.compare(pos, src.size(), src) == 0;
//}

static std::string parseNodeName(std::string& src, size_t& pos, bool& isPairedTag) {
    // 1. Пропустити пробіли
    skipWhitespace(src, pos);

    // 2. Очікуємо символ '<'
    if(pos >= src.size() || src[pos] != '<') {
        throw std::runtime_error("Expected '<' at node start");
    }

    pos++; // пропускаємо '<'

    // 3. Перевірка на '</' — це не початок, це вже кінець
    if(pos < src.size() && src[pos] == '/') {
        throw std::runtime_error("Unexpected closing tag '</' while parsing node name");
    }

    // 4. Зчитуємо ім'я (алфанумерик або '_')
    size_t start = pos;
    while (pos < src.size() && (std::isalnum((unsigned char)src[pos]) || src[pos] == '_')) {
        pos++;
    }

    std::string name = src.substr(start, pos - start);

    // 5. Пропускаємо пробіли після імені
    skipWhitespace(src, pos);

    // 6. Очікуємо звичайне закриття початку тегу '>'
    if(pos < src.size() && src[pos] == '>') {
        pos++; // пропускаємо '>'
        isPairedTag = true;
    }

    skipWhitespace(src, pos);

    return name;
}

static std::string parseKey(std::string& src,size_t& pos) {
    size_t start = pos;

    while( pos < src.size() && src[pos] != '=' ) pos++;

    return src.substr(start, pos-start);
}

// -> = " this "
static std::vector<std::string> parseValue(std::string& src,size_t& pos) {
    std::vector<std::string> result;

    size_t start = pos;
    if( src[pos]=='=' ) pos++;
    else throw std::runtime_error("Expected '=' after closing tag name");

    auto getNumber = [](std::string& src,size_t& pos) ->std::string {
        std::string value;

        while( pos < src.size() && ( isdigit(src[pos]) || src[pos] == '-' || src[pos] == '+' || src[pos] == '.' ) ) {
            value+= src[pos];
            pos++;
        }
        return value;
    };

    auto getString = [](std::string& src,size_t& pos) ->std::string {
        std::string value;
        //value+= src[pos]; "
        pos++;

        while( pos < src.size() && src[pos] !=  '"' ) {
            value+= src[pos];
            pos++;
        }

        // value+= src[pos]; "
        return value;
    };

    auto tmpPos = pos;

    if(src[pos]=='{' && src[++tmpPos]=='"') {
        ++pos;
        while( src[pos] != '}' ) {
            result.emplace_back( getString(src, pos) );

            /// skip ", 
            if(src[pos++]!='"') throw std::runtime_error("Expected \" "); 
            if(src[pos]==',') { ++pos; skipWhitespace(src, pos); continue; }
            if(src[pos]=='}') { ++pos; break; }
            skipWhitespace(src, pos);
            if(src[pos]!='"') throw std::runtime_error("Expected \" ");
        }

    } else if(src[pos]=='{') {
        ++pos;
        result.emplace_back( getNumber(src, pos) );
        skipWhitespace(src, pos);

        if( src[pos++]!=',' ) throw std::runtime_error("Expected ','");
        skipWhitespace(src, pos);

        result.emplace_back( getNumber(src, pos) );

        if(src[pos++]!='}') throw std::runtime_error("Expected '}'");
    } else if( src[pos]== '"' ) {

       result.emplace_back( getString(src, pos) );

       if(src[pos++]!='"') throw std::runtime_error("Expected \" ");

    } else {
        result.emplace_back( getNumber(src, pos) );
    }

    return result;
}

// isPairedTag == true -> begin <name> ,end </name>
// isPairedTag == false -> begin <name ,end />
bool IsNodeEnd(std::string& src, size_t& pos, const std::string name, bool isPairedTag) {
    skipWhitespace(src, pos);

    // Для void-тегів <node />
    if(!isPairedTag) {
        if(src[pos] == '/' && pos + 1 < src.size() && src[pos + 1] == '>') {
            pos += 2;
            return true;
            std::cout << "close tag - " << name << '\t';
        }
        return false;
    }

    // Очікуємо </name>
    if(src[pos] == '<' && pos + 1 < src.size() && src[pos + 1] == '/') {
        pos += 2; // пропускаємо '</'

        // Зчитуємо ім'я
        size_t start = pos;
        while(pos < src.size() && std::isalnum((unsigned char)src[pos])) pos++;
        std::string foundName = src.substr(start, pos - start);

        if(foundName != name) {
            std::cerr <<"Mismatched closing tag: expected </" + name + "> but got </" + foundName + ">"; 
            throw std::runtime_error("Mismatched closing tag: expected </" + name + "> but got </" + foundName + ">");
        }

        skipWhitespace(src, pos);
        if(src[pos] == '>') {
            pos++; // пропускаємо '>'
            return true;
        } else {
            throw std::runtime_error("Expected '>' after closing tag name");
        }
    }

    return false;
}

static void parseNode(std::string& src,size_t& pos, std::vector<XmlNode>& children) {
    skipWhitespace(src, pos);
    if( pos >= src.size() ) return;

    bool isPairedTag = false;
    auto Name = parseNodeName(src, pos, isPairedTag); // begin name Node <Test> ( PairedTag ) or <test .... ( VoidTag )
    if( Name.empty() ) return;

    XmlNode node;
    node.className = Name;

    while( pos < src.size() && !IsNodeEnd(src, pos, Name, isPairedTag) ) {

            if( IsNodeEnd(src, pos, Name, isPairedTag) ) return; // node end </Test>
            else if( (unsigned char)src[pos]=='<' ) { // new  injection node
                parseNode(src, pos, node.children);
                continue;
            }

            std::string key = parseKey(src, pos);

            std::vector<std::string> value = parseValue(src, pos);
            if(key == "NodeName") node.NodeName = value[0];
            else node.attributes[ key ] = std::move(value);
    }

    children.emplace_back( std::move(node) );
}

XmlNode parse_xml(const std::string& text) {
    std::string src = text;
    size_t pos = 0;

    XmlNode node;
    node.className = "Base";

    while( pos < src.size() ) parseNode(src, pos, node.children);

    return node;
}
// -------------------------------------------------------------------------------------------------------------------------------
//                                                          parse_xml end
// -------------------------------------------------------------------------------------------------------------------------------


std::string typeid_from_id(TypeId tid) {
    auto& vec_id = Ref.registry_by_id();

    if( auto it = vec_id.find(tid); it != vec_id.end() ) return it->second->name;
    return {};
}

template<typename T>
T convert_string(const std::string& str) {
    if      constexpr (std::is_same_v<T, int>)          return std::stoi(str);
    else if constexpr (std::is_same_v<T, float>)        return std::stof(str);
    else if constexpr (std::is_same_v<T, double>)       return std::stod(str);
    else if constexpr (std::is_same_v<T, std::string>)  return str;
    else if constexpr (std::is_same_v<T, bool>)         return str == "true";
    else                                                static_assert(!sizeof(T*), "Unsupported type in convert_string");
}

// Шаблонна версія AnyValue
template<typename T>
AnyValue make_any_from_string(const std::string& str) {
    T* val = new T(convert_string<T>(str));
    return AnyValue{ val, get_type_id<T>(), [](void* p){ delete static_cast<T*>(p); } };
}

// Функція для виклику по TypeId
AnyValue convert_string_to_type(const std::string& str, TypeId tid) {
    auto try_type = [&](auto dummy) -> AnyValue {
        using T = decltype(dummy);
        return make_any_from_string<T>(str);
    };

    if(tid == get_type_id<int>())          return try_type(int{});
    if(tid == get_type_id<float>())        return try_type(float{});
    if(tid == get_type_id<double>())       return try_type(double{});
    if(tid == get_type_id<std::string>())  return try_type(std::string{});
    if(tid == get_type_id<bool>())         return try_type(bool{});

    return AnyValue{};
}

// Фабрика об'єктів з XmlNode
void* build_from_xmlnode(const XmlNode& elem) {
    // 1?? Отримуємо тип по імен
    auto& idMap =  Ref.id_by_name();
    auto tid_it = idMap.find(elem.className);

    if( tid_it == idMap.end() ) {
        std::string messeg {"Reflection error: not found type id "};
        std::cerr << messeg << elem.className << "\n";
        throw std::runtime_error( messeg + elem.className );
        return nullptr; 
    }

    TypeId tid = tid_it->second;
    Type* type = Ref.registry_by_id()[tid];

    if( !type ) {
        std::string messeg {"Reflection error: not found type id "};
        std::cerr << messeg << tid << "\n";
        throw std::runtime_error( messeg + std::to_string(tid) );
        return nullptr;
    }

    // 2?? Створюємо порожній об'єкт
    void* obj = ::operator new(type->size);
    type->constructor(obj);

    std::cout << "constructor type  - " << type->name << "; id - "  << type->id << "; size - " << type->size << "\n\n";

    // 3?? Автоматичне заповнення полів
    for(Field* f : type->fields) {
        auto it = elem.attributes.find(f->name);
        if(it != elem.attributes.end()) {
            AnyRef ref = f->value(obj);

            // Автоматичне приведення: int, float, string, double, bool
            if     ( ref.id == get_type_id<int>()         )  *static_cast<int*>(         ref.ptr) = std::stoi(it->second.front());
            else if( ref.id == get_type_id<float>()       )  *static_cast<float*>(       ref.ptr) = std::stof(it->second.front());
            else if( ref.id == get_type_id<double>()      )  *static_cast<double*>(      ref.ptr) = std::stod(it->second.front());
            else if( ref.id == get_type_id<std::string>() )  *static_cast<std::string*>( ref.ptr) = it->second.front();
            else if( ref.id == get_type_id<bool>()        )  *static_cast<bool*>(        ref.ptr) = (it->second.front() == "true");
            else if( f->type->constructor ) {
                // Створюємо об’єкт типу поля (наприклад sf::Vector2i)
                //void* fieldObj = f->constructor();

                void* fieldObj = ::operator new(f->type->size);
                f->type->constructor(fieldObj);

                // Пробуємо знайти парсер для цього типу
                if( Ref.try_parse(f->id, fieldObj, it->second) ) {
                    // Якщо парсер є — копіюємо дані у поле
                    std::memcpy(ref.ptr, fieldObj, f->type->size);
                } else {

                    assert(false && "❌ No parser or deserialization defined for this field type!");
                    // Якщо парсера немає — можливо, це вкладений об’єкт (клас)
                    // Можна викликати рекурсивну десеріалізацію:
                    // deserializeObject(fieldObj, elem.child(f->name));
                }

                // Звільняємо тимчасову пам’ять (якщо конструктор виділяв через ::operator new)
                ::operator delete(fieldObj);
            }
        }
    }

    // 4?? Виклик метода ініціалізації, якщо є SetData (опціонально)
    //for (reflection::Method* m : type->methods) {
    //    if(m->method_name == "SetData1") {
    //        std::vector<void*> args;

    //        // генеруємо args автоматично по типах
    //        for(reflection::TypeId pid : m->param_ids) {
    //            auto it = elem.attributes.find(typeid_from_id(pid)); // функція яка мапить id->ім’я атрибута
    //            if(it != elem.attributes.end()) {
    //                //void* val = convert_string_to_type(it->second, pid).ptr; // універсальна функція конвертації
    //                reflection::AnyValue any_val = convert_string_to_type(it->second, pid);
    //                void* val = any_val.ptr;
    //                args.push_back(val);
    //            }
    //        }

    //        invoke_on(*m, tid, obj, tid, args);

    //        // очищення тимчасових значень, якщо треба
    //    }
    //}

    // 5?? Рекурсія для дочірніх елементів
    for(auto& child : elem.children) {
        void* child_obj = build_from_xmlnode(child);

        // тут можна прикріпити child_obj до obj, якщо тип підтримує дочірні
        // наприклад через поле "children" або метод AddChild
    }

    return obj;
}

#include <memory>
void* createObjectFromData(Type* type, const DataLoader& loader) {
    if (!type) return nullptr;

    // 1️⃣ Створюємо порожній об'єкт
    void* obj = ::operator new(type->size);

    // 2️⃣ Викликаємо конструктор
    type->constructor(obj);

    // 3️⃣ Автоматичне заповнення полів через reflection
    for (Field* f : type->fields) {
        auto it = loader.data.find(f->name);
        if (it != loader.data.end()) {
            AnyRef ref = f->value(obj);

            const std::string& value = it->second;

            if      (ref.id == get_type_id<int>())          *static_cast<int*>(ref.ptr) = std::stoi(value);
            else if (ref.id == get_type_id<float>())        *static_cast<float*>(ref.ptr) = std::stof(value);
            else if (ref.id == get_type_id<std::string>())  *static_cast<std::string*>(ref.ptr) = value;
            else if (ref.id == get_type_id<double>())       *static_cast<double*>(ref.ptr) = std::stod(value);
            else if (ref.id == get_type_id<bool>())         *static_cast<bool*>(ref.ptr) = (value == "true");
            else
                std::cerr << "Unsupported field type for: " << f->name << "\n";
        }
    }

    return obj;
}

bool DataLoader::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config: " << filename << "\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string key, eq, value;
        if (iss >> key >> eq >> value && eq == "=") {
            data[key] = value;
        }
    }
    return true;
}

}


static std::string src;
static size_t pos;

static void skipWhitespace() {
    while (pos < src.size() && std::isspace((unsigned char)src[pos])) pos++;
}

//static bool startsWith(const std::string& s) {
//    return src.compare(pos, s.size(), s) == 0;
//}

static std::string parseName() {
    size_t start = pos;
    while (pos < src.size() && (std::isalnum((unsigned char)src[pos]) || src[pos]=='_'))
        pos++;
    return src.substr(start, pos-start);
}

static std::string parseQuotedValue() {
    if (src[pos] != '"') return {};
    pos++;
    size_t start = pos;
    while (pos < src.size() && src[pos] != '"') pos++;
    std::string val = src.substr(start, pos-start);
    if (pos < src.size()) pos++; // пропустити "
    return val;
}

/*
static XmlNode parseNode() {
    XmlNode node;

    if (src[pos] != '<') return node;
    pos++;
    node.name = parseName();

    // атрибути
    skipWhitespace();
    while (pos < src.size() && src[pos] != '>' && src[pos] != '/') {
        std::string key = parseName();
        skipWhitespace();
        if (pos < src.size() && src[pos] == '=') {
            pos++;
            skipWhitespace();
            std::string val = parseQuotedValue();
            node.attributes[key] = val;
        }
        skipWhitespace();
    }

    if (pos < src.size() && src[pos] == '/') { // <tag .../>
        while (pos < src.size() && src[pos] != '>') pos++;
        if (pos < src.size()) pos++;
        return node;
    }

    if (pos < src.size()) pos++; // '>'

    // діти
    skipWhitespace();
    while (pos < src.size() && !startsWith("</")) {
        if (src[pos] == '<') {
            node.children.push_back(parseNode());
        } else {
            pos++; // текст ігноруємо
        }
        skipWhitespace();
    }

    // закриваючий тег
    if (startsWith("</")) {
        pos += 2; // </
        parseName();
        while (pos < src.size() && src[pos] != '>') pos++;
        if (pos < src.size()) pos++;
    }

    return node;
}

XmlNode parse_xml(const std::string& text) {
    src = text;
    pos = 0;
    skipWhitespace();
    return parseNode();
}
*/
