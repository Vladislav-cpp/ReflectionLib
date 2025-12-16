#pragma once

#include <span>
#include <vector>
#include <unordered_map>
#include <typeinfo>   // потрібно додати зверху файлу
#include <string>
#include <iostream>
#include <type_traits>

namespace reflection {

using TypeId = std::uint32_t;

struct Type;
struct Field;
struct Method;
struct AnyRef;
struct AnyValue;

	//class Reflection;
	//inline Reflection Ref{};
// ================================ Type ======================================
template<typename T>
TypeId get_type_id() { return get_type<T>()->id; }

//static inline TypeId next_type_id() {
//	static TypeId id = 1;
//	std::cout << " static inline TypeId next_type_id() -> " << id << std::endl;
//	return id++;
//}

inline TypeId next_type_id() {
	static TypeId id = 1;
	//std::cout << "reflection::next_type_id() -> " << id << std::endl;
	return id++;
}
//
template<typename T>
void erased_constructor(void* p) { new (p) T(); }

template<typename T>
void erased_destructor(void* p) { static_cast<T*>(p)->~T(); }

// ================================ Field =====================================

struct Field {
    std::string name;
    TypeId id = 0;                           // ID типу поля
    Type* type = nullptr;                    // Вказівник на Type опис поля
    size_t offset = 0;                       // Зміщення поля в структурі
    using ValueFunc = AnyRef(*)(void*);
    ValueFunc value = nullptr;
//    using ConstructorFunc = void* (*)();     // Опціонально: для побудови типу
//    ConstructorFunc constructor = nullptr;
};

template<auto> struct FieldTraits;

template<typename C, typename F, F C::* Ptr>
struct FieldTraits<Ptr> {
	using ClassT = C;
	using FieldT = F;
	static constexpr auto ptr = Ptr;
};

template<auto PtrToField>
AnyRef value_func(void* object) {
	using FT = FieldTraits<PtrToField>;
	using C = typename FT::ClassT;
	using F = typename FT::FieldT;

	C* c = static_cast<C*>(object);
	F* f = &(c->*PtrToField);
	return AnyRef{ f, get_type_id<F>() };
}

//template<auto PtrToField>
//Field make_field(const std::string& name);

// ================================ Method ====================================

struct Method {
	std::string method_name;
	TypeId return_id = 0;
	std::vector<TypeId> param_ids;

	using InvokeFunc = AnyValue(*)(void*, std::span<void*>);
	InvokeFunc invoke = nullptr;
};

template<auto> struct MethodTraits;

template<typename C, typename R, typename... A, R (C::*Ptr)(A...)>
struct MethodTraits<Ptr> {
	using Class = C;
	using Ret = R;
	using ArgsT = std::tuple<A...>;
	static constexpr auto ptr = Ptr;
};

template<typename C, typename R, typename... A, R (C::*Ptr)(A...) const>
struct MethodTraits<Ptr> {
	using Class = const C;
	using Ret = R;
	using ArgsT = std::tuple<A...>;
	static constexpr auto ptr = Ptr;
};

template<auto PtrToMemberFunction>
AnyValue invoke_func(void* object, std::span<void*> args) {
	using MT = MethodTraits<PtrToMemberFunction>;
	using Obj = std::remove_cv_t<typename MT::Class>;
	using Ret = typename MT::Ret;

	constexpr std::size_t N = std::tuple_size_v<typename MT::ArgsT>;
	assert(args.size() == N && "wrong arg count");

	Obj* o = static_cast<Obj*>(object);

	auto call = [&]<std::size_t... I>(std::index_sequence<I...>) {
		if constexpr (std::is_void_v<Ret>) {
			(o->*PtrToMemberFunction)(*static_cast<std::remove_reference_t<std::tuple_element_t<I, typename MT::ArgsT>>*>(args[I])...);
			return AnyValue{};
		} else {
			Ret r = (o->*PtrToMemberFunction)(*static_cast<std::remove_reference_t<std::tuple_element_t<I, typename MT::ArgsT>>*>(args[I])...);
			return make_value(std::move(r));
		}
	};
	return call(std::make_index_sequence<N>{});
}

//template<auto PtrToMethod>
//Method make_method(const std::string& name);

AnyValue invoke_on(	const Method& m, TypeId method_owner_id,
					void* obj, TypeId obj_id, std::span<void*> args);


// ================================ Registries ================================

class Reflection {
	public:
	Reflection() = default;
	~Reflection() = default;

	public:
	inline std::unordered_map<TypeId, Type*>& registry_by_id() { return m_xRegisters_TypeId_Type; }
	inline std::unordered_map<std::string, TypeId>& id_by_name() { return m_xRegisters_Name_TypeId; }

	public:
	std::unordered_map<TypeId, Type*> m_xRegisters_TypeId_Type;
	std::unordered_map<std::string, TypeId> m_xRegisters_Name_TypeId;

	using ParseFunc = void(*)(void*, const std::vector<std::string>&);
	std::unordered_map<TypeId, ParseFunc> m_xTypeParsers;

	public:
	std::vector< std::pair<void*, std::string> > m_xUI_Objects;

	public:
	// Реєстрація парсера для типу
	template<typename T>
	void register_type_parser(ParseFunc func) {
		m_xTypeParsers[ get_type_id<T>() ] = func;
	}

	// Виклик парсера
	bool try_parse(TypeId id, void* ptr, const std::vector<std::string>& values) const {
		auto it = m_xTypeParsers.find(id);

		if( it == m_xTypeParsers.end() ) {
			std::string messeg {"Reflection error: not found type id "};
			std::cerr << messeg << id << "\n";
			throw std::runtime_error( messeg + std::to_string(id) );
			return false;
		}

		it->second(ptr, values);
		return true;
	}

	void build_objects_from_xml(std::string path);

	public: // Field
	template<auto PtrToField>
	Field make_field(const std::string& name) {
		using FT = FieldTraits<PtrToField>;
		using ClassT = typename FT::ClassT;
		using FieldT = typename FT::FieldT;


		Field f;
		f.name = name;
		f.id = get_type_id<FieldT>();
		f.type = get_type<FieldT>();
		//f.offset = offsetof(ClassT, PtrToField);
		f.offset = reinterpret_cast<size_t>(
			&(reinterpret_cast<ClassT const volatile*>(0)->*PtrToField)
		);
		f.value = &value_func<PtrToField>;

		//f.constructor = []() -> void* {
		//	void* mem = ::operator new(sizeof(FieldT));
		//	get_type<FieldT>()->constructor(mem);
		//	return mem;
		//};

		return f;
	}

	public: // Method
	template<auto PtrToMethod>
	Method make_method(const std::string& name) {
		using MT = MethodTraits<PtrToMethod>;
		Method m;
		m.method_name = name;
		if constexpr (!std::is_void_v<typename MT::Ret>)
			m.return_id = get_type_id<typename MT::Ret>();
		else
			m.return_id = 0;

		m.param_ids.reserve(std::tuple_size_v<typename MT::ArgsT>);
		[&]<std::size_t...I>(std::index_sequence<I...>) {
			(m.param_ids.push_back(get_type_id<std::tuple_element_t<I, typename MT::ArgsT>>()), ...);
		}(std::make_index_sequence<std::tuple_size_v<typename MT::ArgsT>>{});

		m.invoke = &invoke_func<PtrToMethod>;
		return m;
	}

};

extern Reflection Ref;

#define REFLECTION_FRIEND					\
	public:									\
	friend class ::reflection::Reflection;	\
	void static technical_work();


// ============================ AnyRef / AnyValue =============================

struct AnyRef {
	void*	ptr = nullptr;
	TypeId	id = 0;
};

using AnyRefDeleter = void(*)(void*);

struct AnyValue {
	void*	ptr = nullptr;
	TypeId	id = 0;
	AnyRefDeleter deleter = nullptr;

	AnyValue() = default;
	AnyValue(void* p, TypeId i, AnyRefDeleter d);
	AnyValue(const AnyValue&) = delete;
	AnyValue& operator=(const AnyValue&) = delete;
	AnyValue(AnyValue&& o) noexcept;
	AnyValue& operator=(AnyValue&& o) noexcept;
	~AnyValue();

	void reset();
	AnyRef ref() const;
};

// ================================ Type ======================================

struct Type {
	struct BaseLink {
		TypeId base_id = 0;
		void* (*rebase)(void* derived_ptr) = nullptr;
	};

	std::string name;
	TypeId id = 0;
	size_t size = 0;

	std::vector<BaseLink> bases;
	std::vector<Field*> fields;
	std::vector<Method*> methods;

	using ConstructorFunc = void(*)(void*);
	using DestructorFunc = void(*)(void*);

	ConstructorFunc constructor = nullptr;
	DestructorFunc destructor = nullptr;
};

template<typename T>
Type* get_type() {
	using U = std::remove_reference_t<T>;
	static Type t;
	static bool inited = false;

	if( !inited ) {
		inited = true;

		std::string raw = typeid(U).name();
		// видаляємо "class " або "struct " якщо вони є
		constexpr std::string_view class_kw = "class ";
		constexpr std::string_view struct_kw = "struct ";

		if (raw.rfind(class_kw, 0) == 0)		raw.erase(0, class_kw.size());
		else if (raw.rfind(struct_kw, 0) == 0)	raw.erase(0, struct_kw.size());

		t.name = std::move(raw);
		t.id = next_type_id();
	
		//static_assert(std::is_default_constructible_v<T>,	"Type T must be default constructible to use reflection");
		t.constructor = &erased_constructor<U>;

		if constexpr (!std::is_trivially_destructible_v<U>)	t.destructor = &erased_destructor<U>;
		//Ref.m_xRegisters_TypeId_Type[t.id] = &t;
		//Ref.m_xRegisters_Name_TypeId[t.name] = t.id;
	}

	return &t;
}
// ================================ Utils =====================================

template<typename T, typename TBase>
void* rebase_ptr(void* object) {
	T* d = static_cast<T*>(object);
	return static_cast<TBase*>(d);
}

// =============================== Registration ===============================

template<typename T, typename Base>
Type::BaseLink make_base() {
	return Type::BaseLink{ get_type_id<Base>(), &rebase_ptr<T, Base> };
}

template<typename T>
void register_type(	std::initializer_list<Type::BaseLink> bases,
					std::initializer_list<Field> fields,
					std::initializer_list<Method> methods) {
	Type* tp = get_type<T>();
	tp->bases.insert(tp->bases.end(), bases.begin(), bases.end());

	tp->fields.reserve(tp->fields.size() + fields.size());
	for (const auto& f : fields) tp->fields.push_back(new Field(f));

	tp->methods.reserve(tp->methods.size() + methods.size());
	for (const auto& m : methods) tp->methods.push_back(new Method(m));

	tp->size = sizeof(T);

	Ref.m_xRegisters_TypeId_Type[ tp->id ]	= tp;
	Ref.m_xRegisters_Name_TypeId[ tp->name ]= tp->id;
}

// =============================== Macros =====================================

#define REFLECT_FIELD(C, MEMBER)		Ref.make_field<&C::MEMBER>(#MEMBER)
#define REFLECT_METHOD(C, METHOD)		Ref.make_method<&C::METHOD>(#METHOD)

#define BASES_WRAPPER(...)   std::initializer_list<Type::BaseLink>{ __VA_ARGS__ }
#define FIELDS_WRAPPER(...)  std::initializer_list<Field>{ __VA_ARGS__ }
#define METHODS_WRAPPER(...) std::initializer_list<Method>{ __VA_ARGS__ }

#define REFLECT_TYPE_IMPL(Derived, BASES, FIELDS, METHODS)						\
	static bool _reflect_##Derived = [](){										\
		reflection::register_type<Derived>(BASES, FIELDS, METHODS);				\
		return true;															\
	}();

#define WRAPPER(...) { {__VA_ARGS__} }

//#define REFLECT_TYPE1(DerivedClass, BASES, FIELDS, METHODS)						\
//	void DerivedClass::technical_work() {										\
//	using namespace reflection;													\
//	REFLECT_TYPE_IMPL(DerivedClass, WRAPPER(BASES), WRAPPER(FIELDS), WRAPPER(METHODS)) }
//	REFLECT_TYPE_IMPL(DerivedClass, BASES, FIELDS, METHODS) }




#define REFLECT_TYPE(DerivedClass, BASES, FIELDS, METHODS) \
void DerivedClass::technical_work() { \
	using namespace reflection; \
	REFLECT_TYPE_IMPL(DerivedClass, \
		BASES, \
		FIELDS, \
		METHODS) \
}


} // namespace reflection
