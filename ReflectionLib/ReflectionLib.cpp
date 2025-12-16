#include "pch.h"
#include "framework.h"
#include "ReflectionLib.hpp"
#include "Utility.hpp"

namespace reflection {

Reflection Ref{};

//Reflection Ref;

std::string trim_quotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2); // обрізаємо перший і останній символ
    }
    return s; // повертаємо як є, якщо лапки відсутні
}

void Reflection::build_objects_from_xml(std::string path) {
	std::ifstream file(path);
	if( !file.is_open() ) std::cerr << "Failed to open UIConfig.xml\n";

	std::stringstream buffer;
	buffer << file.rdbuf();

	XmlNode root = parse_xml(buffer.str());

// ===== Створення об'єктів через рефлексію =====
	for(auto& elem : root.children[0].children) {
		void* obj = build_from_xmlnode(elem);
		if (obj) {
			m_xUI_Objects.emplace_back( obj, trim_quotes(elem.NodeName) );
			std::cout << "Created object of type: " << elem.className << " -nodeName " <<  elem.NodeName << "\n";
		} else {
			std::cerr << "Failed to create object for element: " << elem.className << " -nodeName " <<  elem.NodeName << "\n";
		}
	}
}


// ================================ AnyValue ==================================

AnyValue::AnyValue(void* p, TypeId i, AnyRefDeleter d) : ptr(p), id(i), deleter(d) {}

AnyValue::AnyValue(AnyValue&& o) noexcept : ptr(o.ptr), id(o.id), deleter(o.deleter) {
	o.ptr = nullptr; o.id = 0; o.deleter = nullptr;
}

AnyValue& AnyValue::operator=(AnyValue&& o) noexcept {
	if (this != &o) {
		reset();
		ptr = o.ptr; id = o.id; deleter = o.deleter;
		o.ptr = nullptr; o.id = 0; o.deleter = nullptr;
	}
	return *this;
}

AnyValue::~AnyValue() { reset(); }

void AnyValue::reset() {
	if (ptr && deleter) deleter(ptr);
	ptr = nullptr; id = 0; deleter = nullptr;
}

AnyRef AnyValue::ref() const { return AnyRef{ ptr, id }; }

// ================================ Utils =====================================

//template<typename T, typename TBase>
//void* rebase_ptr(void* object) {
//	T* d = static_cast<T*>(object);
//	return static_cast<TBase*>(d);
//}

static void* upcast_impl(void* obj, TypeId from_id, TypeId to_id) {
	if (from_id == to_id) return obj;
	auto it = Ref.registry_by_id().find(from_id);
	if (it == Ref.registry_by_id().end()) return nullptr;
	Type* from = it->second;
	for (const auto& b : from->bases) {
		if (!b.base_id || !b.rebase) continue;
		void* as_base = b.rebase(obj);
		if (b.base_id == to_id) return as_base;
		if (void* rec = upcast_impl(as_base, b.base_id, to_id)) return rec;
	}
	return nullptr;
}

static inline void* upcast(void* obj, TypeId obj_id, TypeId want_id) {
	return upcast_impl(obj, obj_id, want_id);
}

// ================================ Type ======================================

//template<typename T>
//Type* get_type() {
//	static Type t;
//	static bool inited = false;
//	if (!inited) {
//		inited = true;
//		t.name = typeid(T).name();
//		t.id = next_type_id();
//		t.constructor = &erased_constructor<T>;
//		if constexpr (!std::is_trivially_destructible_v<T>)
//			t.destructor = &erased_destructor<T>;
//		registry_by_id()[t.id] = &t;
//		id_by_name()[t.name] = t.id;
//	}
//	return &t;
//}

//template<typename T>
//TypeId get_type_id() { return get_type<T>()->id; }

// ================================ Method ====================================

template<typename R>
static AnyValue make_value(R&& r) {
	using T = std::remove_cv_t<std::remove_reference_t<R>>;
	T* p = new T(std::forward<R>(r));
	return AnyValue{ p, get_type_id<T>(), [](void* q) { delete static_cast<T*>(q); } };
}

//template<auto PtrToMemberFunction>
//AnyValue invoke_func(void* object, std::span<void*> args) {
//	using MT = MethodTraits<PtrToMemberFunction>;
//	using Obj = std::remove_cv_t<typename MT::Class>;
//	using Ret = typename MT::Ret;
//
//	constexpr std::size_t N = std::tuple_size_v<typename MT::ArgsT>;
//	assert(args.size() == N && "wrong arg count");
//
//	Obj* o = static_cast<Obj*>(object);
//
//	auto call = [&]<std::size_t... I>(std::index_sequence<I...>) {
//		if constexpr (std::is_void_v<Ret>) {
//			(o->*PtrToMemberFunction)(*static_cast<std::remove_reference_t<std::tuple_element_t<I, typename MT::ArgsT>>*>(args[I])...);
//			return AnyValue{};
//		} else {
//			Ret r = (o->*PtrToMemberFunction)(*static_cast<std::remove_reference_t<std::tuple_element_t<I, typename MT::ArgsT>>*>(args[I])...);
//			return make_value(std::move(r));
//		}
//	};
//	return call(std::make_index_sequence<N>{});
//}

//template<auto PtrToMethod>
//Method Reflection::make_method(const std::string& name) {
//	using MT = MethodTraits<PtrToMethod>;
//	Method m;
//	m.method_name = name;
//	if constexpr (!std::is_void_v<typename MT::Ret>)
//		m.return_id = get_type_id<typename MT::Ret>();
//	else
//		m.return_id = 0;
//
//	m.param_ids.reserve(std::tuple_size_v<typename MT::ArgsT>);
//	[&]<std::size_t...I>(std::index_sequence<I...>) {
//		(m.param_ids.push_back(get_type_id<std::tuple_element_t<I, typename MT::ArgsT>>()), ...);
//	}(std::make_index_sequence<std::tuple_size_v<typename MT::ArgsT>>{});
//
//	m.invoke = &invoke_func<PtrToMethod>;
//	return m;
//}

AnyValue invoke_on(const Method& m, TypeId method_owner_id,
				   void* obj, TypeId obj_id, std::span<void*> args) {
	if (obj_id != method_owner_id) {
		obj = upcast(obj, obj_id, method_owner_id);
		assert(obj && "upcast failed");
	}
	return m.invoke(obj, args);
}

// ================================ Field =====================================

//template<auto PtrToField>
//AnyRef value_func(void* object) {
//	using FT = FieldTraits<PtrToField>;
//	using C = typename FT::Class;
//	using F = typename FT::FieldT;
//	C* c = static_cast<C*>(object);
//	F* f = &(c->*PtrToField);
//	return AnyRef{ f, get_type_id<F>() };
//}

//template<auto PtrToField>
//Field Reflection::make_field(const std::string& name) {
//	using FT = FieldTraits<PtrToField>;
//	Field f;
//	f.name = name;
//	f.id = get_type_id<typename FT::FieldT>();
//	f.value = &value_func<PtrToField>;
//	return f;
//}

// =============================== Registration ===============================

//template<typename T, typename Base>
//Type::BaseLink make_base() {
//	return Type::BaseLink{ get_type_id<Base>(), &rebase_ptr<T, Base> };
//}

//template<typename T>
//void register_type(	std::initializer_list<Type::BaseLink> bases,
//					std::initializer_list<Field> fields,
//					std::initializer_list<Method> methods) {
//	Type* tp = get_type<T>();
//	tp->bases.insert(tp->bases.end(), bases.begin(), bases.end());
//
//	tp->fields.reserve(tp->fields.size() + fields.size());
//	for (const auto& f : fields) tp->fields.push_back(new Field(f));
//
//	tp->methods.reserve(tp->methods.size() + methods.size());
//	for (const auto& m : methods) tp->methods.push_back(new Method(m));
//
//	tp->size = sizeof(T);
//}

// =============================== Explicit Instantiations ====================

template Type* get_type<int>();
template Type* get_type<double>();
template TypeId get_type_id<int>();
template TypeId get_type_id<double>();


} // namespace reflection
