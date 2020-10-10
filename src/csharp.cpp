#define LUMIX_NO_CUSTOM_CRT
#include "csharp.h"
#include "animation/animation.h"
#include "animation/animation_scene.h"
#include "audio/audio_scene.h"
#include "engine/crc32.h"
#include "engine/engine.h"
#include "engine/flag_set.h"
#include "engine/geometry.h"
#include "engine/hash_map.h"
#include "engine/input_system.h"
#include "engine/log.h"
#include "engine/path.h"
#include "engine/prefab.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/serializer.h"
#include "engine/universe.h"
#include "gui/gui_scene.h"
#include "helpers.h"
#include "imgui/imgui.h"
#include "navigation/navigation_scene.h"
#include "physics/physics_scene.h"
#include "renderer/render_scene.h"
#include "renderer/renderer.h"
#include <mono/metadata/exception.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/threads.h>

#pragma comment(lib, "mono-2.0-sgen.lib")


namespace Lumix {


static const ComponentType CSHARP_SCRIPT_TYPE = Reflection::getComponentType("csharp_script");


static MonoString* GetStringProperty(const char* propertyName, MonoClass* classType, MonoObject* classObject) {
	MonoProperty* messageProperty;
	MonoMethod* messageGetter;
	MonoString* messageString;

	messageProperty = mono_class_get_property_from_name(classType, propertyName);
	messageGetter = mono_property_get_get_method(messageProperty);
	messageString = (MonoString*)mono_runtime_invoke(messageGetter, classObject, NULL, NULL);
	return messageString;
}


static void handleException(MonoObject* exc) {
	if (!exc) return;

	MonoClass* exception_class = mono_object_get_class(exc);
	MonoType* exception_type = mono_class_get_type(exception_class);
	MonoStringHolder type_name = mono_type_get_name(exception_type);
	MonoStringHolder message = GetStringProperty("Message", exception_class, exc);
	MonoStringHolder source = GetStringProperty("Source", exception_class, exc);
	MonoStringHolder stack_trace = GetStringProperty("StackTrace", exception_class, exc);
	MonoStringHolder target_site = GetStringProperty("TargetSite", exception_class, exc);
	if (message.isValid()) logError("C#") << (const char*)message;
	if (source.isValid()) logError("C#") << (const char*)source;
	if (stack_trace.isValid()) logError("C#") << (const char*)stack_trace;
	if (target_site.isValid()) logError("C#") << (const char*)target_site;
}


void getCSharpName(const char* in_name, StaticString<128>& class_name) {
	char* out = class_name.data;
	const char* in = in_name;
	bool to_upper = true;
	while (*in && out - class_name.data < lengthOf(class_name.data) - 1) {
		if (*in == '_' || *in == ' ' || *in == '-') {
			to_upper = true;
			++in;
			continue;
		}

		if (*in == '(') {
			to_upper = true;
			while (*in && *in != ')') ++in;
			if (*in == ')') ++in;
			continue;
		}

		if (to_upper) {
			*out = *in >= 'a' && *in <= 'z' ? *in - 'a' + 'A' : *in;
			to_upper = false;
		} else {
			*out = *in;
		}
		++out;
		++in;
	}
	*out = '\0';
}


struct CSharpPluginImpl : public CSharpPlugin {
	CSharpPluginImpl(Engine& engine);
	~CSharpPluginImpl();
	const char* getName() const override { return "csharp_script"; }
	void createScenes(Universe& universe) override;
	void* getAssembly() const override;
	void* getDomain() const override;
	void unloadAssembly() override;
	void loadAssembly() override;
	const HashMap<u32, String>& getNames() const override { return m_names; }
	void setStaticField(const char* name_space, const char* class_name, const char* field_name, void* value);
	void registerProperties();
	u32 getVersion() const override { return 0; }
	void serialize(OutputMemoryStream& serializer) const override {}
	bool deserialize(u32 version, InputMemoryStream& serializer) override { return true; }

	Engine& m_engine;
	IAllocator& m_allocator;
	MonoDomain* m_domain = nullptr;
	MonoAssembly* m_assembly = nullptr;
	MonoDomain* m_assembly_domain = nullptr;
	HashMap<u32, String> m_names;
	DelegateList<void()> m_on_assembly_unload;
	DelegateList<void()> m_on_assembly_load;
};

/*
MonoString* csharp_Resource_getPath(Resource* resource) {
	if (resource == nullptr) return mono_string_new(mono_domain_get(), "");
	return mono_string_new(mono_domain_get(), resource->getPath().c_str());
}


Resource* csharp_Resource_load(Engine& engine, MonoString* path, MonoString* type) {
	MonoStringHolder type_str = type;
	ResourceType res_type((const char*)type_str);
	MonoStringHolder path_str = path;
	ResourceManagerBase* manager = engine.getResourceManager().get(res_type);
	return manager ? manager->load(Path((const char*)path_str)) : nullptr;
}


bool csharp_Entity_hasComponent(Universe* universe, Entity entity, MonoString* type) {
	MonoStringHolder type_str = type;
	ComponentType cmp_type = Reflection::getComponentType((const char*)type_str);
	return universe->hasComponent(entity, cmp_type);
}


void csharp_Entity_setParent(Universe* universe, Entity parent, Entity child) {
	universe->setParent(parent, child);
}


Entity csharp_Entity_getParent(Universe* universe, Entity entity) {
	return universe->getParent(entity);
}


Universe* csharp_getUniverse(IScene* scene) {
	return &scene->getUniverse();
}


MonoObject* csharp_getEntity(Universe* universe, int entity_idx) {
	auto* cs_scene = (CSharpScriptScene*)universe->getScene(CSHARP_SCRIPT_TYPE);
	u32 gc_handle = cs_scene->getEntityGCHandle({entity_idx});
	return mono_gchandle_get_target(gc_handle);
}


IScene* csharp_getScene(Universe* universe, MonoString* type_str) {
	MonoStringHolder type = type_str;
	ComponentType cmp_type = Reflection::getComponentType((const char*)type);
	return universe->getScene(cmp_type);
}


Entity csharp_instantiatePrefab(Universe* universe, PrefabResource* prefab, const Vec3& pos, const Quat& rot, float scale) {
	return universe->instantiatePrefab(*prefab, pos, rot, scale);
}


IScene* csharp_getSceneByName(Universe* universe, MonoString* type_str) {
	MonoStringHolder name = type_str;
	return universe->getScene(crc32((const char*)name));
}


int csharp_Component_getEntityIDFromGUID(IDeserializer* serializer, u64 guid) {
	return serializer->getEntity({guid}).index;
}


u64 csharp_Component_getEntityGUIDFromID(ISerializer* serializer, int id) {
	return serializer->getGUID({id}).value;
}


void csharp_Component_create(Universe* universe, int entity, MonoString* type_str) {
	MonoStringHolder type = type_str;
	ComponentType cmp_type = Reflection::getComponentType((const char*)type);
	IScene* scene = universe->getScene(cmp_type);
	if (!scene) return;
	if (universe->hasComponent({entity}, cmp_type)) {
		logError("C# Script") << "Component " << (const char*)type << " already exists in entity " << entity;
		return;
	}

	universe->createComponent(cmp_type, {entity});
}


void csharp_Entity_destroy(Universe* universe, int entity) {
	universe->destroyEntity({entity});
}


void csharp_Entity_setPosition(Universe* universe, int entity, const Vec3& pos) {
	universe->setPosition({entity}, pos);
}


Vec3 csharp_Entity_getPosition(Universe* universe, int entity) {
	return universe->getPosition({entity});
}


void csharp_Entity_setRotation(Universe* universe, int entity, const Quat& pos) {
	universe->setRotation({entity}, pos);
}


void csharp_Entity_setLocalRotation(Universe* universe, int entity, const Quat& pos) {
	universe->setLocalRotation({entity}, pos);
}


void csharp_Entity_setLocalPosition(Universe* universe, int entity, const Vec3& pos) {
	universe->setLocalPosition({entity}, pos);
}


Vec3 csharp_Entity_getLocalPosition(Universe* universe, int entity) {
	return universe->getLocalTransform({entity}).pos;
}


Quat csharp_Entity_getLocalRotation(Universe* universe, int entity) {
	return universe->getLocalTransform({entity}).rot;
}


Quat csharp_Entity_getRotation(Universe* universe, int entity) {
	return universe->getRotation({entity});
}


void csharp_Entity_setName(Universe* universe, int entity, MonoString* name) {
	MonoStringHolder str = name;
	universe->setEntityName({entity}, (const char*)str);
}


MonoString* csharp_Entity_getName(Universe* universe, int entity) {
	return mono_string_new(mono_domain_get(), universe->getEntityName({entity}));
}
*/

template <typename T> struct ToCSharpType { typedef T Type; };
template <> struct ToCSharpType<const char*> { typedef MonoString* Type; };
template <> struct ToCSharpType<Path> { typedef MonoString* Type; };
template <typename T> T toCSharpValue(T val) {
	return val;
}
MonoString* toCSharpValue(const char* val) {
	return mono_string_new(mono_domain_get(), val);
}
MonoString* toCSharpValue(const Path& val) {
	return mono_string_new(mono_domain_get(), val.c_str());
}
template <typename T> T fromCSharpValue(T val) {
	return val;
}
const char* fromCSharpValue(MonoString* val) {
	return mono_string_to_utf8(val);
}


template <typename T> struct CSharpTypeConvertor {
	using Type = T;

	static Type convert(Type val) { return val; }
	static Type convertRet(Type val) { return val; }
};


template <> struct CSharpTypeConvertor<void> { using Type = void; };


template <> struct CSharpTypeConvertor<const char*> {
	using Type = MonoString*;

	static const char* convert(MonoString* val) { return mono_string_to_utf8(val); }
	static MonoString* convertRet(const char* val) { return mono_string_new(mono_domain_get(), val); }
};


template <> struct CSharpTypeConvertor<const Path&> {
	using Type = MonoString*;

	static Path convert(MonoString* val) { return Path(mono_string_to_utf8(val)); }
	static MonoString* convertRet(const Path& val) { return mono_string_new(mono_domain_get(), val.c_str()); }
};


template <> struct CSharpTypeConvertor<const ImVec2&> {
	struct Vec2POD {
		float x;
		float y;
	};
	using Type = Vec2POD;

	static ImVec2 convert(Type& val) { return *(const ImVec2*)&val; }
};


template <> struct CSharpTypeConvertor<ImVec2> {
	struct Vec2POD {
		float x;
		float y;
	};
	using Type = Vec2POD;

	static Type convert(ImVec2& val) { return *(Vec2POD*)&val; }
	static Type convert(const ImVec2& val) { return *(Vec2POD*)&val; }
};


template <typename F> struct CSharpFunctionProxy;
template <typename F> struct CSharpMethodProxy;


template <typename R, typename... Args> struct CSharpFunctionProxy<R(Args...)> {
	using F = R(Args...);

	template <F fnc> static typename CSharpTypeConvertor<R>::Type call(typename CSharpTypeConvertor<Args>::Type... args) {
		return CSharpTypeConvertor<R>::convert(fnc(CSharpTypeConvertor<Args>::convert(args)...));
	}
};

template <typename... Args> struct CSharpFunctionProxy<void(Args...)> {
	using F = void(Args...);

	template <F fnc> static void call(typename CSharpTypeConvertor<Args>::Type... args) { fnc(CSharpTypeConvertor<Args>::convert(args)...); }
};


template <bool B, class T = void> struct enable_if {};

template <class T> struct enable_if<true, T> { typedef T type; };


template <typename T1, typename T2> struct is_same { static constexpr bool value = false; };

template <typename T> struct is_same<T, T> { static constexpr bool value = true; };


template <typename R, typename T, typename... Args> struct CSharpMethodProxy<R (T::*)(Args...) const> {
	using F = R (T::*)(Args...) const;
	using ConvertedR = typename CSharpTypeConvertor<R>::Type;

	template <F fnc, typename Ret = R> static typename enable_if<is_same<Ret, void>::value, Ret>::type call(T* inst, typename CSharpTypeConvertor<Args>::Type... args) {
		(inst->*fnc)(CSharpTypeConvertor<Args>::convert(args)...);
	}

	template <F fnc, typename Ret = R> static typename enable_if<!is_same<Ret, void>::value, ConvertedR>::type call(T* inst, typename CSharpTypeConvertor<Args>::Type... args) {
		return CSharpTypeConvertor<R>::convertRet((inst->*fnc)(CSharpTypeConvertor<Args>::convert(args)...));
	}
};


template <typename R, typename T, typename... Args> struct CSharpMethodProxy<R (T::*)(Args...)> {
	using F = R (T::*)(Args...);
	using ConvertedR = typename CSharpTypeConvertor<R>::Type;

	template <F fnc, typename Ret = R> static typename enable_if<is_same<Ret, void>::value, void>::type call(T* inst, typename CSharpTypeConvertor<Args>::Type... args) {
		(inst->*fnc)(CSharpTypeConvertor<Args>::convert(args)...);
	}

	template <F fnc, typename Ret = R> static typename enable_if<!is_same<Ret, void>::value, ConvertedR>::type call(T* inst, typename CSharpTypeConvertor<Args>::Type... args) {
		return CSharpTypeConvertor<R>::convertRet((inst->*fnc)(CSharpTypeConvertor<Args>::convert(args)...));
	}
};


template <typename Getter, Getter getter> auto csharp_getProperty(typename ClassOf<Getter>::Type* scene, int cmp) -> typename ToCSharpType<typename ResultOf<Getter>::Type>::Type {
	decltype(auto) val = (scene->*getter)({cmp});
	return toCSharpValue(val);
}


template <typename C, int (C::*Function)(EntityRef)> int csharp_getSubobjectCount(C* scene, int entity) {
	return (scene->*Function)({entity});
}


template <typename C, void (C::*Function)(EntityRef, int)> void csharp_addSubobject(C* scene, int entity) {
	(scene->*Function)({entity}, -1);
}


template <typename C, void (C::*Function)(EntityRef, int)> void csharp_removeSubobject(C* scene, int entity, int index) {
	(scene->*Function)({entity}, index);
}


template <typename R, typename C, R (C::*Function)(EntityRef, int)> typename ToCSharpType<R>::Type csharp_getSubproperty(C* scene, int entity, int index) {
	R val = (scene->*Function)({entity}, index);
	return toCSharpValue(val);
}

/*
template <typename Setter, Setter setter> void csharp_setProperty(typename ClassOf<Setter>::Type* scene, int cmp, typename ToCSharpType<typename ArgNType<1, Setter>::Type>::Type value) {
	(scene->*setter)({cmp}, fromCSharpValue(value));
}


template <typename T, typename C, void (C::*Function)(EntityRef, int, T)> void csharp_setSubproperty(C* scene, int entity, typename ToCSharpType<T>::Type value, int index) {
	(scene->*Function)({entity}, index, fromCSharpValue(value));
}


template <typename T, typename C, void (C::*Function)(EntityRef, const T&)> void csharp_setProperty(C* scene, int entity, typename ToCSharpType<T>::Type value) {
	(scene->*Function)({entity}, T(fromCSharpValue(value)));
}


Resource* csharp_loadResource(Engine* engine, MonoString* path, MonoString* type) {
	MonoStringHolder type_str = type;
	ResourceType res_type((const char*)type_str);
	ResourceManagerBase* manager = engine->getResourceManager().get(res_type);
	if (!manager) return nullptr;

	MonoStringHolder path_str = path;
	return manager->load(Path((const char*)path_str));
}
*/

void csharp_logError(MonoString* message) {
	MonoStringHolder tmp = message;
	logError("C#") << (const char*)tmp;
}


struct CSharpScriptSceneImpl : public CSharpScriptScene {
	struct Script {
		Script(IAllocator& allocator)
			: properties(allocator) {}

		enum Flags : u32 { HAS_UPDATE = 1 << 0, HAS_ON_INPUT = 1 << 2 };

		u32 script_name_hash;
		u32 gc_handle = INVALID_GC_HANDLE;
		FlagSet<Flags, u32> flags;
		String properties;
	};


	struct ScriptComponent {
		ScriptComponent(IAllocator& allocator)
			: scripts(allocator) {}

		Array<Script> scripts;
		EntityRef entity;
	};


	CSharpScriptSceneImpl(CSharpPluginImpl& plugin, Universe& universe)
		: m_system(plugin)
		, m_universe(universe)
		, m_scripts(plugin.m_allocator)
		, m_entities_gc_handles(plugin.m_allocator)
		, m_updates(plugin.m_allocator)
		, m_on_inputs(plugin.m_allocator)
		, m_is_game_running(false) {
		universe.registerComponentType(CSHARP_SCRIPT_TYPE, this, &CSharpScriptSceneImpl::createScriptComponent, &CSharpScriptSceneImpl::destroyScriptComponent);

#include "api.inl"

		createImGuiAPI();
		createEngineAPI();

		m_system.m_on_assembly_load.bind<&CSharpScriptSceneImpl::onAssemblyLoad>(this);
		m_system.m_on_assembly_unload.bind<&CSharpScriptSceneImpl::onAssemblyUnload>(this);
		onAssemblyLoad();
	}


	~CSharpScriptSceneImpl() {
		m_system.m_on_assembly_load.unbind<&CSharpScriptSceneImpl::onAssemblyLoad>(this);
		m_system.m_on_assembly_unload.unbind<&CSharpScriptSceneImpl::onAssemblyUnload>(this);
	}


	void onAssemblyLoad() {
		for (ScriptComponent* cmp : m_scripts) {
			createCSharpEntity(cmp->entity);
			Array<Script>& scripts = cmp->scripts;
			for (Script& script : scripts) {
				setScriptNameHash(*cmp, script, script.script_name_hash);
			}
			applyProperties(*cmp);
		}
	}


	void onAssemblyUnload() {
		m_updates.clear();
		m_on_inputs.clear();
		for (u32 handle : m_entities_gc_handles) {
			mono_gchandle_free(handle);
		}
		m_entities_gc_handles.clear();
		for (ScriptComponent* cmp : m_scripts) {
			Array<Script>& scripts = cmp->scripts;
			for (Script& script : scripts) {
				if (script.gc_handle != INVALID_GC_HANDLE) mono_gchandle_free(script.gc_handle);
				script.gc_handle = INVALID_GC_HANDLE;
			}
		}
	}


	static void imgui_Text(const char* text) { ImGui::Text("%s", text); }


	static void imgui_LabelText(const char* label, const char* text) { ImGui::LabelText(label, "%s", text); }


	void createImGuiAPI() {
		mono_add_internal_call("ImGui::Begin", &CSharpFunctionProxy<bool(const char*, bool*, ImGuiWindowFlags)>::call<ImGui::Begin>);
		mono_add_internal_call("ImGui::CollapsingHeader", &CSharpFunctionProxy<bool(const char*, bool*, ImGuiTreeNodeFlags)>::call<ImGui::CollapsingHeader>);
		mono_add_internal_call("ImGui::LabelText", &CSharpFunctionProxy<void(const char*, const char*)>::call<imgui_LabelText>);
		mono_add_internal_call("ImGui::PushID", &CSharpFunctionProxy<void(const char*)>::call<ImGui::PushID>);
		mono_add_internal_call("ImGui::Selectable", &CSharpFunctionProxy<bool(const char*, bool, ImGuiSelectableFlags, const ImVec2&)>::call<ImGui::Selectable>);
		mono_add_internal_call("ImGui::Text", &CSharpFunctionProxy<void(const char*)>::call<imgui_Text>);
		mono_add_internal_call("ImGui::InputText", &CSharpFunctionProxy<decltype(ImGui::InputText)>::call<ImGui::InputText>);

#define REGISTER_FUNCTION(F) mono_add_internal_call("ImGui::" #F, &CSharpFunctionProxy<decltype(ImGui::F)>::call<ImGui::F>);
		REGISTER_FUNCTION(AlignTextToFramePadding);
		REGISTER_FUNCTION(BeginChildFrame);
		REGISTER_FUNCTION(BeginPopup);
		REGISTER_FUNCTION(Button);
		REGISTER_FUNCTION(Checkbox);
		REGISTER_FUNCTION(Columns);
		REGISTER_FUNCTION(DragFloat);
		REGISTER_FUNCTION(Dummy);
		REGISTER_FUNCTION(End);
		REGISTER_FUNCTION(EndChildFrame);
		REGISTER_FUNCTION(EndPopup);
		REGISTER_FUNCTION(GetColumnWidth);
		REGISTER_FUNCTION(GetWindowWidth);
		REGISTER_FUNCTION(GetWindowHeight);
		REGISTER_FUNCTION(GetWindowSize);
		REGISTER_FUNCTION(Indent);
		REGISTER_FUNCTION(IsItemHovered);
		REGISTER_FUNCTION(IsMouseClicked);
		REGISTER_FUNCTION(IsMouseDown);
		REGISTER_FUNCTION(InputInt);
		REGISTER_FUNCTION(NewLine);
		REGISTER_FUNCTION(NextColumn);
		REGISTER_FUNCTION(OpenPopup);
		REGISTER_FUNCTION(PopItemWidth);
		REGISTER_FUNCTION(PopID);
		REGISTER_FUNCTION(PushItemWidth);
		REGISTER_FUNCTION(Rect);
		REGISTER_FUNCTION(SameLine);
		REGISTER_FUNCTION(Separator);
		REGISTER_FUNCTION(SetCursorScreenPos);
		REGISTER_FUNCTION(SetNextWindowPos);
		REGISTER_FUNCTION(SetNextWindowSize);
		REGISTER_FUNCTION(ShowDemoWindow);
		REGISTER_FUNCTION(SliderFloat);
		REGISTER_FUNCTION(Unindent);
#undef REGISTER_FUNCTION
	}


	void createEngineAPI() {
		mono_add_internal_call("Lumix.Engine::logError", csharp_logError);
		// TODO
		/*
		mono_add_internal_call("Lumix.Engine::loadResource", csharp_loadResource);
		mono_add_internal_call("Lumix.Component::getEntityIDFromGUID", csharp_Component_getEntityIDFromGUID);
		mono_add_internal_call("Lumix.Component::getEntityGUIDFromID", csharp_Component_getEntityGUIDFromID);
		mono_add_internal_call("Lumix.Component::create", csharp_Component_create);
		mono_add_internal_call("Lumix.Component::getScene", csharp_getScene);
		mono_add_internal_call("Lumix.Universe::instantiatePrefab", csharp_instantiatePrefab);
		mono_add_internal_call("Lumix.Universe::getSceneByName", csharp_getSceneByName);
		mono_add_internal_call("Lumix.Universe::getEntity", csharp_getEntity);
		mono_add_internal_call("Lumix.IScene::getUniverse", csharp_getUniverse);
		mono_add_internal_call("Lumix.Entity::hasComponent", csharp_Entity_hasComponent);
		mono_add_internal_call("Lumix.Entity::setParent", csharp_Entity_setParent);
		mono_add_internal_call("Lumix.Entity::getParent", csharp_Entity_getParent);
		mono_add_internal_call("Lumix.Entity::destroy", csharp_Entity_destroy);
		mono_add_internal_call("Lumix.Entity::setPosition", csharp_Entity_setPosition);
		mono_add_internal_call("Lumix.Entity::getPosition", csharp_Entity_getPosition);
		mono_add_internal_call("Lumix.Entity::setRotation", csharp_Entity_setRotation);
		mono_add_internal_call("Lumix.Entity::setLocalPosition", csharp_Entity_setLocalPosition);
		mono_add_internal_call("Lumix.Entity::getLocalPosition", csharp_Entity_getLocalPosition);
		mono_add_internal_call("Lumix.Entity::setLocalRotation", csharp_Entity_setLocalRotation);
		mono_add_internal_call("Lumix.Entity::getLocalRotation", csharp_Entity_getLocalRotation);
		mono_add_internal_call("Lumix.Entity::getRotation", csharp_Entity_getRotation);
		mono_add_internal_call("Lumix.Entity::setName", csharp_Entity_setName);
		mono_add_internal_call("Lumix.Entity::getName", csharp_Entity_getName);
		mono_add_internal_call("Lumix.Resource::getPath", csharp_Resource_getPath);
		mono_add_internal_call("Lumix.Resource::load", csharp_Resource_load);*/
	}


	void onContact(const PhysicsScene::ContactData& data) {
		ASSERT(false); // TODO
					   /*MonoObject* e1_obj = mono_gchandle_get_target(getEntityGCHandle(data.e1));
					   MonoObject* e2_obj = mono_gchandle_get_target(getEntityGCHandle(data.e2));
					   auto call = [this, &data](const ScriptComponent* cmp, MonoObject* entity) {
						   for (const Script& scr : cmp->scripts) {
							   tryCallMethod(true, scr.gc_handle, nullptr, "OnContact", entity, data.position);
						   }
					   };
			   
					   int idx = m_scripts.find(data.e1);
					   if (idx >= 0) call(m_scripts.at(idx), e2_obj);
					   idx = m_scripts.find(data.e2);
					   if (idx >= 0) call(m_scripts.at(idx), e1_obj);*/
	}


	void startGame() override {
		PhysicsScene* phy_scene = (PhysicsScene*)m_universe.getScene(crc32("physics"));
		if (phy_scene) {
			phy_scene->onContact().bind<&CSharpScriptSceneImpl::onContact>(this);
			;
		}

		for (ScriptComponent* cmp : m_scripts) {
			Array<Script>& scripts = cmp->scripts;
			for (Script& script : scripts) {
				tryCallMethod(script.gc_handle, "Start", nullptr, 0, false);
			}
		}
		m_is_game_running = true;
	}


	void stopGame() override {
		m_is_game_running = false;
		PhysicsScene* phy_scene = (PhysicsScene*)m_universe.getScene(crc32("physics"));
		if (phy_scene) {
			phy_scene->onContact().unbind<&CSharpScriptSceneImpl::onContact>(this);
			;
		}
	}


	void getClassName(u32 name_hash, char (&out_name)[256]) const {
		/*int idx = m_system.m_names.find(name_hash);
		if (idx < 0) {
			out_name[0] = 0;
			return;
		}

		copyString(out_name, m_system.m_names.at(idx).c_str());*/
		ASSERT(false); // TODO
	}


	enum class ScriptClass : u32 { UNKNOWN, ENTITY, RESOURCE };

	/*
	void serializeCSharpScript(ISerializer& serializer, Entity entity) {
		ScriptComponent* script = m_scripts[entity];
		serializer.write("count", script->scripts.size());
		for (Script& inst : script->scripts) {
			serializer.write("script_name_hash", inst.script_name_hash);
			if (inst.gc_handle == INVALID_GC_HANDLE) {
				serializer.write("properties", "");
				continue;
			}

			MonoObject* obj = mono_gchandle_get_target(inst.gc_handle);
			MonoObject* res;
			if (tryCallMethod(true, obj, &res, "Serialize", &serializer)) {
				MonoObject* exc;
				MonoStringHolder str = mono_object_to_string(res, &exc);
				if (exc) {
					handleException(exc);
				} else {
					serializer.write("properties", (const char*)str);
				}
			} else {
				serializer.write("properties", "");
			}
		}
	}
	*/

	void applyProperties(ScriptComponent& script) {
		for (Script& inst : script.scripts) {
			if (inst.gc_handle == INVALID_GC_HANDLE) continue;
			if (inst.properties.length() == 0) continue;

			MonoObject* obj = mono_gchandle_get_target(inst.gc_handle);
			MonoString* str = mono_string_new(mono_domain_get(), inst.properties.getData());
			tryCallMethod(true, obj, nullptr, "Deserialize", str);
		}
	}

	/*
	void deserializeCSharpScript(IDeserializer& serializer, Entity entity, int scene_version) {
		auto& allocator = m_system.m_allocator;
		ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(allocator);
		script->entity = entity;
		m_scripts.insert(entity, script);
		createCSharpEntity(script->entity);

		int count;
		serializer.read(&count);
		script->scripts.reserve(count);
		string tmp(allocator);
		for (int i = 0; i < count; ++i) {
			Script& inst = script->scripts.emplace(allocator);
			u32 hash;
			serializer.read(&hash);
			setScriptNameHash(entity, i, hash);

			serializer.read(&tmp);
			if (m_system.m_assembly) {
				MonoObject* res;
				MonoClass* mono_class = mono_class_from_name(mono_assembly_get_image(m_system.m_assembly), "Lumix", "Component");

				MonoString* str_arg = mono_string_new(mono_domain_get(), tmp.c_str());
				if (tryCallStaticMethod(true, mono_class, &res, "ConvertGUIDToID", str_arg, &serializer)) {
					MonoObject* exc;
					MonoStringHolder str = mono_object_to_string(res, &exc);
					if (exc) {
						handleException(exc);
					} else {
						inst.properties = (const char*)str;
					}
				}
			}
		}

		if (m_system.m_assembly) applyProperties(*script);

		m_universe.onComponentCreated(entity, CSHARP_SCRIPT_TYPE, this);
	}


	void serializeScript(Entity entity, int scr_index, OutputBlob& blob) override {
		Script& scr = m_scripts[entity]->scripts[scr_index];
		blob.write(scr.script_name_hash);
	}


	void deserializeScript(Entity entity, int scr_index, InputBlob& blob) override {
		u32 name_hash = blob.read<u32>();
		setScriptNameHash(entity, scr_index, name_hash);
	}
	*/

	void insertScript(EntityRef entity, int idx) override { m_scripts[entity]->scripts.emplaceAt(idx, m_system.m_allocator); }


	int addScript(EntityRef entity) override {
		ScriptComponent* script_cmp = m_scripts[entity];
		script_cmp->scripts.emplace(m_system.m_allocator);
		return script_cmp->scripts.size() - 1;
	}


	void removeScript(EntityRef entity, int scr_index) override {
		setScriptNameHash(entity, scr_index, 0);
		m_scripts[entity]->scripts.erase(scr_index);
	}


	u32 getEntityGCHandle(EntityRef entity) override {
		auto iter = m_entities_gc_handles.find(entity);
		if (iter.isValid()) return iter.value();
		u32 handle = createCSharpEntity(entity);
		return handle;
	}


	u32 getGCHandle(EntityRef entity, int scr_index) const override {
		Script& scr = m_scripts[entity]->scripts[scr_index];
		return scr.gc_handle;
	}


	const char* getScriptName(EntityRef entity, int scr_index) override {
		ASSERT(false); // TODO
		/*Script& scr = m_scripts[entity]->scripts[scr_index];
		int idx = m_system.m_names.find(scr.script_name_hash);
		if (idx < 0) return "";
		return m_system.m_names.at(idx).c_str();*/
		return {};
	}


	int getScriptCount(EntityRef entity) const override { return m_scripts[entity]->scripts.size(); }


	u32 getScriptNameHash(EntityRef entity, int scr_index) override { return m_scripts[entity]->scripts[scr_index].script_name_hash; }


	void setScriptNameHash(ScriptComponent& cmp, Script& script, u32 name_hash) {
		if (script.gc_handle != INVALID_GC_HANDLE) {
			ASSERT(m_system.m_assembly);
			mono_gchandle_free(script.gc_handle);
			if (script.flags.isSet(Script::HAS_UPDATE)) {
				script.flags.unset(Script::HAS_UPDATE);
				m_updates.eraseItems([&script](u32 iter) { return iter == script.gc_handle; });
			}
			if (script.flags.isSet(Script::HAS_ON_INPUT)) {
				script.flags.unset(Script::HAS_ON_INPUT);
				m_on_inputs.eraseItems([&script](u32 iter) { return iter == script.gc_handle; });
			}
			script.script_name_hash = 0;
			script.gc_handle = INVALID_GC_HANDLE;
		}

		if (name_hash != 0) {
			if (m_system.m_assembly) {
				char class_name[256];
				getClassName(name_hash, class_name);
				script.gc_handle = createObjectGC("", class_name);
				if (script.gc_handle != INVALID_GC_HANDLE) setCSharpComponent(script, cmp);
			}

			script.script_name_hash = name_hash;
		}
	}


	void setCSharpComponent(Script& script, ScriptComponent& cmp) {
		MonoObject* obj = mono_gchandle_get_target(script.gc_handle);
		ASSERT(obj);
		MonoClass* mono_class = mono_object_get_class(obj);

		MonoProperty* prop = mono_class_get_property_from_name(mono_class, "entity");
		ASSERT(prop);

		u32 handle = m_entities_gc_handles[cmp.entity];
		MonoObject* entity_obj = mono_gchandle_get_target(handle);
		ASSERT(entity_obj);

		MonoMethod* method = mono_property_get_set_method(prop);
		MonoObject* exc;
		void* mono_args[] = {entity_obj};
		MonoObject* res = mono_runtime_invoke(method, obj, mono_args, &exc);

		if (mono_class_get_method_from_name(mono_class, "Update", 1)) {
			m_updates.push(script.gc_handle);
			script.flags.set(Script::HAS_UPDATE);
		}
		if (mono_class_get_method_from_name(mono_class, "OnInput", 1)) {
			m_on_inputs.push(script.gc_handle);
			script.flags.set(Script::HAS_ON_INPUT);
		}
	}


	void setScriptNameHash(EntityRef entity, int scr_index, u32 name_hash) override {
		ScriptComponent* script_cmp = m_scripts[entity];
		if (script_cmp->scripts.size() <= scr_index) return;

		setScriptNameHash(*script_cmp, script_cmp->scripts[scr_index], name_hash);
	}


	u32 createCSharpEntity(EntityRef entity) {
		if (!m_system.m_assembly) return INVALID_GC_HANDLE;

		u32 handle = createObjectGC("Lumix", "Entity", &m_universe);
		m_entities_gc_handles.insert(entity, handle);

		MonoObject* obj = mono_gchandle_get_target(handle);
		ASSERT(obj);
		MonoClass* mono_class = mono_object_get_class(obj);

		MonoClassField* field = mono_class_get_field_from_name(mono_class, "entity_Id_");
		ASSERT(field);

		mono_field_set_value(obj, field, &entity.index);

		MonoClassField* universe_field = mono_class_get_field_from_name(mono_class, "instance_");
		ASSERT(universe_field);

		void* y = &m_universe;
		mono_field_set_value(obj, universe_field, &y);

		Universe* x;
		mono_field_get_value(obj, universe_field, &x);

		return handle;
	}


	void createScriptComponent(EntityRef entity) {
		auto& allocator = m_system.m_allocator;

		ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(allocator);
		script->entity = entity;
		m_scripts.insert(entity, script);
		createCSharpEntity(script->entity);
		m_universe.onComponentCreated(entity, CSHARP_SCRIPT_TYPE, this);
	}


	void destroyScriptComponent(EntityRef entity) {
		auto* script = m_scripts[entity];
		auto handle_iter = m_entities_gc_handles.find(script->entity);
		if (handle_iter.isValid()) {
			mono_gchandle_free(handle_iter.value());
			m_entities_gc_handles.erase(handle_iter);
		}
		for (Script& scr : script->scripts) {
			setScriptNameHash(*script, scr, 0);
		}
		LUMIX_DELETE(m_system.m_allocator, script);
		m_scripts.erase(entity);
		m_universe.onComponentDestroyed(entity, CSHARP_SCRIPT_TYPE, this);
	}


	void serialize(OutputMemoryStream& serializer) override {
		/*serializer.write(m_scripts.size());
		for (ScriptComponent **iter = m_scripts.begin(), **end = m_scripts.end(); iter != end; ++iter) {
			ScriptComponent* script_cmp = *iter;
			serializer.write(script_cmp->entity);
			serializer.write(script_cmp->scripts.size());
			for (int i = 0, n = script_cmp->scripts.size(); i < n; ++i) {
				Script& scr = script_cmp->scripts[i];
				serializer.write(scr.script_name_hash);

				MonoObject* obj = mono_gchandle_get_target(scr.gc_handle);
				MonoObject* cs_serialized;
				if (obj && tryCallMethod(true, obj, &cs_serialized, "Serialize", (ISerializer*)nullptr)) {
					MonoObject* exc;
					MonoStringHolder str = mono_object_to_string(cs_serialized, &exc);
					if (exc) {
						handleException(exc);
						serializer.writeString("");
					} else {
						serializer.writeString((const char*)str);
					}
				} else {
					serializer.writeString("");
				}
			}
		}*/
		ASSERT(false); // TODO
	}


	void deserialize(struct InputMemoryStream& serialize, const struct EntityMap& entity_map) override {
		/*int len = serializer.read<int>();
		m_scripts.reserve(len);
		IAllocator& allocator = m_system.m_allocator;
		Array<u8> buf(allocator);
		for (int i = 0; i < len; ++i) {
			ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(allocator);

			serializer.read(script->entity);
			m_scripts.insert(script->entity, script);
			createCSharpEntity(script->entity);
			int scr_count;
			serializer.read(scr_count);
			for (int j = 0; j < scr_count; ++j) {
				Script& scr = script->scripts.emplace(m_system.m_allocator);
				scr.gc_handle = INVALID_GC_HANDLE;
				scr.script_name_hash = serializer.read<u32>();
				setScriptNameHash(*script, scr, scr.script_name_hash);

				u32 size = serializer.read<u32>();
				buf.resize(size);
				MonoObject* obj = mono_gchandle_get_target(scr.gc_handle);
				if (obj) {
					if (size > 0) {
						serializer.read(&buf[0], size);
						MonoString* str = mono_string_new(mono_domain_get(), (const char*)&buf[0]);
						tryCallMethod(true, obj, nullptr, "Deserialize", str);
					} else {
						MonoString* str = mono_string_new(mono_domain_get(), "");
						tryCallMethod(true, obj, nullptr, "Deserialize", str);
					}
				}
			}
			Entity entity = {script->entity.index};
			m_universe.onComponentCreated(script->entity, CSHARP_SCRIPT_TYPE, this);
		}*/
		ASSERT(false); // TODO
	}

	IPlugin& getPlugin() const override { return m_system; }


	MonoObject* createKeyboardEvent(const InputSystem::Event& event) {
		if (event.type == InputSystem::Event::BUTTON) {
			u32 event_gc_handle = createObjectGC("Lumix", "KeyboardInputEvent");
			MonoObject* obj = mono_gchandle_get_target(event_gc_handle);

			MonoClass* mono_class = mono_object_get_class(obj);
			MonoClassField* field = mono_class_get_field_from_name(mono_class, "key_id");
			mono_field_set_value(obj, field, (void*)&event.data.button.key_id);

			field = mono_class_get_field_from_name(mono_class, "key_id");
			mono_field_set_value(obj, field, (void*)&event.data.button.key_id);

			field = mono_class_get_field_from_name(mono_class, "is_down");
			bool is_down = event.data.button.down;
			mono_field_set_value(obj, field, (void*)&is_down);

			return obj;
		}

		return nullptr;
	}


	MonoObject* createMouseEvent(const InputSystem::Event& event) {
		ASSERT(false); // TODO
		/*switch (event.type) {
			case InputSystem::Event::AXIS: {
				u32 event_gc_handle = createObjectGC("Lumix", "MouseAxisInputEvent");
				MonoObject* obj = mono_gchandle_get_target(event_gc_handle);

				MonoClass* mono_class = mono_object_get_class(obj);

				MonoClassField* field = mono_class_get_field_from_name(mono_class, "x");
				mono_field_set_value(obj, field, (void*)&event.data.axis.x);

				field = mono_class_get_field_from_name(mono_class, "y");
				mono_field_set_value(obj, field, (void*)&event.data.axis.y);

				field = mono_class_get_field_from_name(mono_class, "x_abs");
				mono_field_set_value(obj, field, (void*)&event.data.axis.x_abs);

				field = mono_class_get_field_from_name(mono_class, "y_abs");
				mono_field_set_value(obj, field, (void*)&event.data.axis.y_abs);

				return obj;
			}
			case InputSystem::Event::BUTTON: {
				u32 event_gc_handle = createObjectGC("Lumix", "MouseButtonInputEvent");
				MonoObject* obj = mono_gchandle_get_target(event_gc_handle);

				MonoClass* mono_class = mono_object_get_class(obj);

				MonoClassField* field = mono_class_get_field_from_name(mono_class, "key_id");
				mono_field_set_value(obj, field, (void*)&event.data.button.key_id);

				field = mono_class_get_field_from_name(mono_class, "x_abs");
				mono_field_set_value(obj, field, (void*)&event.data.button.x_abs);

				field = mono_class_get_field_from_name(mono_class, "y_abs");
				mono_field_set_value(obj, field, (void*)&event.data.button.x_abs);

				field = mono_class_get_field_from_name(mono_class, "is_down");
				bool is_down = event.data.button.state == InputSystem::ButtonEvent::DOWN;
				mono_field_set_value(obj, field, (void*)&is_down);

				return obj;
			}
			default: ASSERT(false); break;
		}*/
		return nullptr;
	}


	void processInput() {
		InputSystem& input = m_system.m_engine.getInputSystem();
		const InputSystem::Event* events = input.getEvents();
		for (u32 gc_handle : m_on_inputs) {
			MonoObject* cs_event = nullptr;
			for (int i = 0, n = input.getEventsCount(); i < n; ++i) {
				const InputSystem::Event& event = events[i];
				switch (event.device->type) {
					case InputSystem::Device::KEYBOARD: {
						cs_event = createKeyboardEvent(event);
						break;
					}
					case InputSystem::Device::MOUSE: {
						cs_event = createMouseEvent(event);
						break;
					}
				}
				tryCallMethod(true, gc_handle, nullptr, "OnInput", cs_event);
			}
		}
	}


	void update(float time_delta, bool paused) override {
		if (paused) return;
		if (!m_is_game_running) return;

		processInput();

		for (u32 gc_handle : m_updates) {
			tryCallMethod(true, gc_handle, nullptr, "Update", time_delta);
		}
	}


	void lateUpdate(float time_delta, bool paused) override {}


	Universe& getUniverse() override { return m_universe; }


	void clear() override {
		for (u32 handle : m_entities_gc_handles) {
			mono_gchandle_free(handle);
		}
		m_entities_gc_handles.clear();
		for (ScriptComponent* script_cmp : m_scripts) {
			for (Script& script : script_cmp->scripts) {
				setScriptNameHash(*script_cmp, script, 0);
			}
			LUMIX_DELETE(m_system.m_allocator, script_cmp);
		}
		m_scripts.clear();
	}


	template <typename T> void* toCSharpArg(T* arg) { return (void*)arg; }

	template <> void* toCSharpArg<MonoString*>(MonoString** arg) { return *arg; }

	template <> void* toCSharpArg<MonoObject*>(MonoObject** arg) { return *arg; }


	template <typename... T> bool tryCallMethod(bool try_parents, u32 gc_handle, MonoObject** result, const char* method_name, T... args) {
		MonoObject* obj = mono_gchandle_get_target(gc_handle);
		if (!obj) return false;
		return tryCallMethod(try_parents, obj, result, method_name, args...);
	}

	template <typename... T> bool tryCallMethod(bool try_parents, MonoObject* obj, MonoObject** result, const char* method_name, T... args) {
		ASSERT(obj);
		MonoClass* mono_class = mono_object_get_class(obj);
		ASSERT(mono_class);
		MonoMethod* method = mono_class_get_method_from_name(mono_class, method_name, sizeof...(args));
		if (!method && try_parents) {
			while (!method) {
				mono_class = mono_class_get_parent(mono_class);
				if (!mono_class) return false;
				method = mono_class_get_method_from_name(mono_class, method_name, sizeof...(args));
			}
		}
		if (!method) return false;

		MonoObject* exc = nullptr;
		void* mono_args[] = {toCSharpArg(&args)...};
		MonoObject* res = mono_runtime_invoke(method, obj, mono_args, &exc);
		handleException(exc);
		if (result && !exc) *result = res;

		return exc == nullptr;
	}

	template <typename... T> bool tryCallStaticMethod(bool try_parents, MonoClass* mono_class, MonoObject** result, const char* method_name, T... args) {
		ASSERT(mono_class);
		MonoMethod* method = mono_class_get_method_from_name(mono_class, method_name, sizeof...(args));
		if (!method && try_parents) {
			while (!method) {
				mono_class = mono_class_get_parent(mono_class);
				if (!mono_class) return false;
				method = mono_class_get_method_from_name(mono_class, method_name, sizeof...(args));
			}
		}
		if (!method) return false;

		MonoObject* exc = nullptr;
		void* mono_args[] = {toCSharpArg(&args)...};
		MonoObject* res = mono_runtime_invoke(method, nullptr, mono_args, &exc);
		handleException(exc);
		if (result && !exc) *result = res;

		return exc == nullptr;
	}


	bool tryCallMethod(bool try_parents, MonoObject* obj, MonoObject** result, const char* method_name) {
		ASSERT(obj);
		MonoClass* mono_class = mono_object_get_class(obj);
		ASSERT(mono_class);
		MonoMethod* method = mono_class_get_method_from_name(mono_class, method_name, 0);
		if (!method && try_parents) {
			while (!method) {
				mono_class = mono_class_get_parent(mono_class);
				if (!mono_class) return false;
				method = mono_class_get_method_from_name(mono_class, method_name, 0);
			}
		}
		if (!method) return false;

		MonoObject* exc = nullptr;
		MonoObject* res = mono_runtime_invoke(method, obj, nullptr, &exc);
		handleException(exc);
		if (result && !exc) *result = res;

		return exc == nullptr;
	}


	bool tryCallMethod(u32 gc_handle, const char* method_name, void** args, int args_count, bool try_parents) override {
		if (gc_handle == INVALID_GC_HANDLE) return false;
		MonoObject* obj = mono_gchandle_get_target(gc_handle);
		ASSERT(obj);
		MonoClass* mono_class = mono_object_get_class(obj);

		ASSERT(mono_class);
		MonoMethod* method = mono_class_get_method_from_name(mono_class, method_name, args_count);
		if (!method && try_parents) {
			while (!method) {
				mono_class = mono_class_get_parent(mono_class);
				if (!mono_class) return false;
				method = mono_class_get_method_from_name(mono_class, method_name, args_count);
			}
		}
		if (!method) return false;

		MonoObject* exc = nullptr;
		mono_runtime_invoke(method, obj, args, &exc);

		handleException(exc);
		return exc == nullptr;
	}


	u32 createObjectGC(const char* name_space, const char* class_name) {
		MonoObject* obj = createObject(name_space, class_name);
		if (!obj) return INVALID_GC_HANDLE;
		return mono_gchandle_new(obj, false);
	}


	MonoObject* createObject(const char* name_space, const char* class_name) {
		MonoClass* mono_class = mono_class_from_name(mono_assembly_get_image(m_system.m_assembly), name_space, class_name);
		if (!mono_class) return nullptr;

		MonoObject* obj = mono_object_new(mono_domain_get(), mono_class);
		if (!obj) return nullptr;

		mono_runtime_object_init(obj);
		return obj;
	}


	u32 createObjectGC(const char* name_space, const char* class_name, void* arg) {
		MonoClass* mono_class = mono_class_from_name(mono_assembly_get_image(m_system.m_assembly), name_space, class_name);
		if (!mono_class) return INVALID_GC_HANDLE;

		MonoObject* obj = mono_object_new(mono_domain_get(), mono_class);
		if (!obj) return INVALID_GC_HANDLE;

		u32 gc_handle = mono_gchandle_new(obj, false);

		void* args[] = {&arg};
		tryCallMethod(gc_handle, ".ctor", args, 1, false);
		return gc_handle;
	}


	HashMap<EntityRef, ScriptComponent*> m_scripts;
	HashMap<EntityRef, u32> m_entities_gc_handles;
	Array<u32> m_updates;
	Array<u32> m_on_inputs;
	CSharpPluginImpl& m_system;
	Universe& m_universe;
	bool m_is_game_running;
};


static void initDebug() {
	mono_debug_init(MONO_DEBUG_FORMAT_MONO);
	const char* options[] = {"--soft-breakpoints", "--debugger-agent=transport=dt_socket,address=127.0.0.1:55555,embedding=1,server=y,suspend=n"};
	mono_jit_parse_options(2, (char**)options);
}


CSharpPluginImpl::CSharpPluginImpl(Engine& engine)
	: m_engine(engine)
	, m_allocator(engine.getAllocator())
	, m_names(m_allocator)
	, m_on_assembly_load(m_allocator)
	, m_on_assembly_unload(m_allocator) {
	registerProperties();

	// mono_trace_set_level_string("debug");
	auto printer = [](const char* msg, mono_bool is_stdout) { logError("Mono") << msg; };

	auto logger = [](const char* log_domain, const char* log_level, const char* message, mono_bool fatal, void* user_data) { logError("Mono") << message; };

	mono_trace_set_print_handler(printer);
	mono_trace_set_printerr_handler(printer);
	mono_trace_set_log_handler(logger, nullptr);

	mono_set_dirs("C:\\Program Files\\Mono\\lib", "C:\\Program Files\\Mono\\etc");
	mono_config_parse(nullptr);
	
	mono_set_dirs(nullptr, nullptr);
	char exe_path[MAX_PATH_LENGTH];
	OS::getExecutablePath(Span(exe_path));
	char exe_dir[MAX_PATH_LENGTH];
	Path::getDir(Span(exe_dir), exe_path);
	StaticString<MAX_PATH_LENGTH * 3> assemblies_paths(exe_dir, ";.");
	mono_set_assemblies_path(assemblies_paths);
	initDebug();
	m_domain = mono_jit_init("lumix");
	mono_thread_set_main(mono_thread_current());
	loadAssembly();
	mono_install_unhandled_exception_hook([](MonoObject* exc, void* user_data) { handleException(exc); }, nullptr);
}


void CSharpPluginImpl::registerProperties() {
	using namespace Reflection;
	static auto csharp_scene = scene("csharp", component("csharp_script"));
	registerScene(csharp_scene);
}


void CSharpPluginImpl::setStaticField(const char* name_space, const char* class_name, const char* field_name, void* value) {
	MonoClass* mono_class = mono_class_from_name(mono_assembly_get_image(m_assembly), name_space, class_name);
	ASSERT(mono_class);
	if (!mono_class) return;

	MonoVTable* vtable = mono_class_vtable(mono_domain_get(), mono_class);
	ASSERT(vtable);
	if (!vtable) return;

	MonoClassField* field = mono_class_get_field_from_name(mono_class, field_name);
	ASSERT(field);
	if (!field) return;

	mono_field_static_set_value(vtable, field, &value);
}


CSharpPluginImpl::~CSharpPluginImpl() {
	unloadAssembly();
	mono_jit_cleanup(m_domain);
}


void* CSharpPluginImpl::getAssembly() const {
	return m_assembly;
}


void* CSharpPluginImpl::getDomain() const {
	return m_domain;
}


void CSharpPluginImpl::unloadAssembly() {
	if (!m_assembly) return;

	m_on_assembly_unload.invoke();

	m_names.clear();
	if (mono_domain_get() != m_domain) mono_domain_set(m_domain, true);
	MonoObject* exc = NULL;
	mono_gc_collect(mono_gc_max_generation());
	mono_domain_finalize(m_assembly_domain, 2000);
	mono_gc_collect(mono_gc_max_generation());
	mono_domain_try_unload(m_assembly_domain, &exc);
	if (exc) {
		handleException(exc);
		return;
	}
	m_assembly = nullptr;
	m_assembly_domain = nullptr;
}


static bool isNativeComponent(MonoClass* cl) {
	MonoCustomAttrInfo* attrs = mono_custom_attrs_from_class(cl);
	if (!attrs) return false;
	MonoObject* obj = mono_custom_attrs_get_attr(attrs, cl);
	if (!obj) return false;
	MonoClass* attr_class = obj ? mono_object_get_class(obj) : nullptr;
	if (!attr_class) return false;
	const char* attr_name = mono_class_get_name(attr_class);
	return equalStrings(attr_name, "NativeComponent");
}


void CSharpPluginImpl::loadAssembly() {
	ASSERT(!m_assembly);

	const char* path = "main.dll";

	IAllocator& allocator = m_engine.getAllocator();
	m_assembly_domain = mono_domain_create_appdomain("lumix_runtime", nullptr);
	mono_domain_set_config(m_assembly_domain, ".", "");
	mono_domain_set(m_assembly_domain, true);
	m_assembly = mono_domain_assembly_open(m_assembly_domain, path);
	if (!m_assembly) return;
	mono_assembly_set_main(m_assembly);

	MonoImage* img = mono_assembly_get_image(m_assembly);

	m_names.clear();
	int num_types = mono_image_get_table_rows(img, MONO_TABLE_TYPEDEF);
	for (int i = 2; i <= num_types; ++i) {
		MonoClass* cl = mono_class_get(img, i | MONO_TOKEN_TYPE_DEF);
		const char* n = mono_class_get_name(cl);
		MonoClass* parent = mono_class_get_parent(cl);

		if (!isNativeComponent(cl) && inherits(cl, "Component")) {
			m_names.insert(crc32(n), String(n, allocator));
		}
	}
	setStaticField("Lumix", "Engine", "instance_", &m_engine);
	m_on_assembly_load.invoke();
}

void CSharpPluginImpl::createScenes(Universe& universe) {
	auto scene = UniquePtr<CSharpScriptSceneImpl>::create(m_engine.getAllocator(), *this, universe);
	universe.addScene(scene.move());
}


LUMIX_PLUGIN_ENTRY(csharp) {
	return LUMIX_NEW(engine.getAllocator(), CSharpPluginImpl)(engine);
}


} // namespace Lumix
