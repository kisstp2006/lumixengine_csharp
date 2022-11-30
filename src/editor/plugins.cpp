#define LUMIX_NO_CUSTOM_CRT
#include "../csharp.h"
#include "editor/asset_browser.h"
#include "editor/file_system_watcher.h"
#include "editor/property_grid.h"
#include "editor/studio_app.h"
#include "editor/utils.h"
#include "editor/world_editor.h"
#include "engine/engine.h"
#include "engine/log.h"
#include "engine/os.h"
#include "engine/reflection.h"
#include "engine/resource.h"
#include "engine/resource_manager.h"
#include "imgui/imgui.h"
#include "subprocess.h"
#include <cstdlib>
#include <mono/metadata/mono-debug.h>


namespace Lumix {


static const ComponentType CSHARP_SCRIPT_TYPE = reflection::getComponentType("csharp_script");

struct CSPropertyVisitor : reflection::IPropertyVisitor {
	template <typename T> void write(const reflection::Property<T>& prop, const char* cs_type, const char* cpp_type) {
		StaticString<128> csharp_name;
		getCSharpName(prop.name, csharp_name);

		/**api_file << "{\n"
					 "	auto getter = &csharp_getProperty<decltype(&"
				  << prop.getter_code << "), &" << prop.getter_code
				  << ">;\n"
					 "	mono_add_internal_call(\"Lumix."
				  << class_name << "::get" << csharp_name
				  << "\", getter);\n"
					 "	auto setter = &csharp_setProperty<decltype(&"
				  << prop.setter_code << "), &" << prop.setter_code
				  << ">;\n"
					 "	mono_add_internal_call(\"Lumix."
				  << class_name << "::set" << csharp_name
				  << "\", setter);\n"
					 "}\n\n";*/

		*file << "		[MethodImplAttribute(MethodImplOptions.InternalCall)]\n"
				 "		extern static "
			  << cs_type << " get" << csharp_name
			  << "(IntPtr scene, int cmp);\n"
				 "\n"
				 "		[MethodImplAttribute(MethodImplOptions.InternalCall)]\n"
				 "		extern static void set"
			  << csharp_name << "(IntPtr scene, int cmp, " << cs_type
			  << " value);\n"
				 "\n\n";

		bool is_bool = equalStrings(cs_type, "bool");
		*file << "		public " << cs_type << " " << (is_bool ? "Is" : "") << csharp_name
			  << "\n"
				 "		{\n"
				 "			get { return get"
			  << csharp_name
			  << "(scene_, entity_.entity_Id_); }\n"
				 "			set { set"
			  << csharp_name
			  << "(scene_, entity_.entity_Id_, value); }\n"
				 "		}\n"
				 "\n";
	}

	void visit(const reflection::Property<float>& prop) override { write(prop, "float", "float"); }
	void visit(const reflection::Property<u32>& prop) override { write(prop, "uint", "u32"); }
	void visit(const reflection::Property<i32>& prop) override { write(prop, "int", "int"); }
	void visit(const reflection::Property<EntityPtr>& prop) override {}
	void visit(const reflection::Property<IVec3>& prop) override { write(prop, "IVec3", "IVec3"); }
	void visit(const reflection::Property<Vec2>& prop) override { write(prop, "Vec2", "Vec2"); }
	void visit(const reflection::Property<Vec3>& prop) override { write(prop, "Vec3", "Vec3"); }
	void visit(const reflection::Property<Vec4>& prop) override { write(prop, "Vec4", "Vec4"); }
	void visit(const reflection::Property<Path>& prop) override { write(prop, "string", "Path"); }
	void visit(const reflection::Property<bool>& prop) override { write(prop, "bool", "bool"); }
	void visit(const reflection::Property<const char*>& prop) override { write(prop, "string", "const char*"); }
	void visit(const reflection::ArrayProperty& prop) override {}
	void visit(const reflection::BlobProperty& prop) override {}

	const reflection::ComponentBase* cmp;
	const char* class_name;
	os::OutputFile* api_file;
	os::OutputFile* file;
};

struct StudioCSharpPlugin : public StudioApp::GUIPlugin {
	StudioCSharpPlugin(StudioApp& app)
		: m_app(app)
		, m_compile_log(app.getWorldEditor().getAllocator()) {
		m_filter[0] = '\0';
		m_new_script_name[0] = '\0';

		IAllocator& allocator = app.getWorldEditor().getAllocator();
		m_watcher = FileSystemWatcher::create("cs", allocator);
		m_watcher->getCallback().bind<&StudioCSharpPlugin::onFileChanged>(this);

		findMono();

		makeUpToDate();
	}

	bool packData(const char* dest_dir) {
		/*char exe_path[LUMIX_MAX_PATH];
		os::getExecutablePath(Span(exe_path));
		char exe_dir[LUMIX_MAX_PATH];

		const char* mono_dlls[] = {"mono-2.0-sgen.dll", "System.dll", "mscorlib.dll", "System.Configuration.dll"};
		for (const char* dll : mono_dlls) {
			Path::getDir(Span(exe_dir), exe_path);
			StaticString<LUMIX_MAX_PATH> tmp(exe_dir, dll);
			if (!os::fileExists(tmp)) return false;
			StaticString<LUMIX_MAX_PATH> dest(dest_dir, dll);
			if (!os::copyFile(tmp, dest)) {
				logError("C#") << "Failed to copy " << tmp << " to " << dest;
				return false;
			}
		}

		StaticString<LUMIX_MAX_PATH> dest(dest_dir, "main.dll");
		if (!os::copyFile("main.dll", dest)) {
			logError("C#") << "Failed to copy main.dll to " << dest;
			return false;
		}

		return true;*/
		ASSERT(false); // TODO
		return false;
	}


	void findMono() {
		if (!os::fileExists("C:\\Program Files\\Mono\\bin\\mcs.bat")) {
			logError("C:\\Program Files\\Mono\\bin\\mcs.bat does not exist, can not compile C# scripts");
		}
	}

	void update(float) {
		if (m_deferred_compile) compile();
		if (!m_compilation_running) return;

		if (subprocess_finished(&m_compile_process)) {
			int ret_code;
			subprocess_join(&m_compile_process, &ret_code);
			if (ret_code == 0) {
				CSharpScriptScene* scene = getScene();
				CSharpPlugin& plugin = (CSharpPlugin&)scene->getPlugin();
				plugin.loadAssembly();
			} else {
				char tmp[1024];
				FILE* p_stderr = subprocess_stderr(&m_compile_process);
				if (fgets(tmp, sizeof(tmp), p_stderr)) {
					m_compile_log.cat(tmp);
				}
				logError(m_compile_log);

				FILE* p_stdout = subprocess_stdout(&m_compile_process);
				if (fgets(tmp, sizeof(tmp), p_stdout)) {
					m_compile_log.cat(tmp);
				}
				logError(m_compile_log);
			}
			subprocess_destroy(&m_compile_process);
			m_compilation_running = false;
		} else {
			char tmp[1024];
			FILE* p_stderr = subprocess_stderr(&m_compile_process);
			if (fgets(tmp, sizeof(tmp), p_stderr)) {
				m_compile_log.cat(tmp);
			}
		}
	}


	void makeUpToDate() {
		IAllocator& allocator = m_app.getWorldEditor().getAllocator();
		if (!os::fileExists("main.dll")) {
			compile();
			return;
		}

		u64 dll_modified = os::getLastModified("main.dll");
		os::FileIterator* iter = os::createFileIterator("cs", allocator);
		os::FileInfo info;
		while (os::getNextFile(iter, &info)) {
			if (info.is_directory) continue;

			StaticString<LUMIX_MAX_PATH> tmp("cs\\", info.filename);
			u64 script_modified = os::getLastModified(tmp);
			if (script_modified > dll_modified) {
				compile();
				os::destroyFileIterator(iter);
				return;
			}
		}
		os::destroyFileIterator(iter);
	}


	void onFileChanged(const char* path) {
		if (Path::hasExtension(path, "cs")) m_deferred_compile = true;
	}


	void createNewScript(const char* name) {
		os::OutputFile file;
		char class_name[128];

		const char* cin = name;
		char* cout = class_name;
		bool to_upper = true;
		while (*cin && cout - class_name < lengthOf(class_name) - 1) {
			char c = *cin;
			if (c >= 'a' && c <= 'z') {
				*cout = to_upper ? *cin - 'a' + 'A' : *cin;
				to_upper = false;
			} else if (c >= 'A' && c <= 'Z') {
				*cout = *cin;
				to_upper = false;
			} else if (c >= '0' && c <= '9') {
				*cout = *cin;
				to_upper = true;
			} else {
				to_upper = true;
				--cout;
			}
			++cout;
			++cin;
		}
		*cout = '\0';

		StaticString<LUMIX_MAX_PATH> path("cs/", class_name, ".cs");
		if (os::fileExists(path)) {
			logError(path, " already exists");
			return;
		}
		if (!file.open(path)) {
			logError("Failed to create file ", path);
			return;
		}

		file << "public class " << class_name << " : Lumix.Component\n{\n}\n";
		file.close();
	}


	void listDirInCSProj(os::OutputFile& file, const char* dirname) {
		IAllocator& allocator = m_app.getWorldEditor().getAllocator();
		StaticString<LUMIX_MAX_PATH> path("cs/", dirname);
		os::FileIterator* iter = os::createFileIterator(path, allocator);
		os::FileInfo info;
		while (os::getNextFile(iter, &info)) {
			if (info.filename[0] == '.' || info.is_directory) continue;
			file << "\t\t<Compile Include=\"" << dirname << info.filename << "\" />\n";
		}
		os::destroyFileIterator(iter);
	}


	void openVSProject() {
		const char* base_path = m_app.getWorldEditor().getEngine().getFileSystem().getBasePath();
		StaticString<LUMIX_MAX_PATH> full_path(base_path, "cs/main.csproj");
		os::shellExecuteOpen(full_path);
	}


	void generateCSProj() {
		os::OutputFile file;
		if (!file.open("cs/main.csproj")) {
			logError("Failed to create cs/main.csproj");
			return;
		}

		file << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
				"\t<Project ToolsVersion=\"15.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n"
				"\t<ItemGroup>\n";

		listDirInCSProj(file, StaticString<LUMIX_MAX_PATH>(""));
		listDirInCSProj(file, StaticString<LUMIX_MAX_PATH>("manual\\"));
		listDirInCSProj(file, StaticString<LUMIX_MAX_PATH>("generated\\"));

		file << "\t</ItemGroup>\n"
				"\t<Import Project=\"$(MSBuildToolsPath)\\Microsoft.CSharp.targets\" />\n"
				"</Project>\n";

		file.close();
	}


	void open(const char* filename) {
		WorldEditor& editor = m_app.getWorldEditor();
		FileSystem& fs = editor.getEngine().getFileSystem();
		StaticString<LUMIX_MAX_PATH> vs_code_project_dir("cs/.vscode/");
		if (!os::dirExists(vs_code_project_dir)) {
			StaticString<LUMIX_MAX_PATH> launch_json_path(vs_code_project_dir, "launch.json");

			bool res = os::makePath(vs_code_project_dir);
			ASSERT(res); // TODO
			const char* launch_json_content = "{\n"
											 "	\"version\": \"0.2.0\",\n"
											 "	\"configurations\": [\n"
											 "		{\n"
											 "			\"name\": \"Attach to Lumix\",\n"
											 "			\"type\": \"mono\",\n"
											 "			\"request\": \"attach\",\n"
											 "			\"address\": \"127.0.0.1\",\n"
											 "			\"port\": 55555\n"
											 "		}\n"
											 "	]\n"
											 "}\n";

			bool res2 = fs.saveContentSync(Path(launch_json_path), Span((const u8*)launch_json_content, stringLength(launch_json_content)));
			(void)res2; // TODO
		}

		StaticString<LUMIX_MAX_PATH> file_path(fs.getBasePath(), "cs/generated/");
		if (filename) file_path << filename;
		os::shellExecuteOpen(file_path);
	}


	void onWindowGUI() override {
		if (!ImGui::Begin("C#")) {
			ImGui::End();
			return;
		}

		CSharpScriptScene* scene = getScene();
		CSharpPlugin& plugin = (CSharpPlugin&)scene->getPlugin();
		if (m_compilation_running) {
			ImGui::Text("Compiling...");
		} else {
			if (mono_is_debugger_attached()) {
				ImGui::Text("Debugger attached");
				ImGui::SameLine();
			}

			if (ImGui::Button("Compile")) compile();
			ImGui::SameLine();
			if (ImGui::Button("Bindings")) generateBindings();
			ImGui::SameLine();
			if (ImGui::Button("Generate project")) generateCSProj();
			ImGui::SameLine();
			if (ImGui::Button("Open VS project")) openVSProject();
			ImGui::SameLine();
			if (ImGui::Button("New script")) ImGui::OpenPopup("new_csharp_script");
			if (ImGui::BeginPopup("new_csharp_script")) {
				ImGui::InputText("Name", m_new_script_name, sizeof(m_new_script_name));
				if (ImGui::Button("Create")) {
					createNewScript(m_new_script_name);
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}

		ImGui::InputTextWithHint("##filter", "Filter", m_filter, sizeof(m_filter));

		for (const String& name : plugin.getNames()) {
			if (m_filter[0] != '\0' && stristr(name.c_str(), m_filter) == 0) continue;
			ImGui::PushID((const void*)name.c_str());
			if (ImGui::Button("Edit")) {
				StaticString<LUMIX_MAX_PATH> filename(name.c_str(), ".cs");
				open(filename);
			}
			ImGui::SameLine();
			ImGui::Text("%s", name.c_str());
			ImGui::PopID();
		}

		if (m_compile_log.length() > 0 && ImGui::CollapsingHeader("Log")) {
			ImGui::Text("%s", m_compile_log.c_str());
		}

		ImGui::End();
	}


	const char* getName() const override { return "csharp_script"; }


	CSharpScriptScene* getScene() const {
		WorldEditor& editor = m_app.getWorldEditor();
		return (CSharpScriptScene*)editor.getUniverse()->getScene("csharp_script");
	}


	static void getCSType(const char* cpp_type, StaticString<64>& cs_type) {
		const char* c = cpp_type;
		auto skip = [&c](const char* value) {
			if (startsWith(c, value)) c += stringLength(value);
		};
		skip("struct ");
		skip("Lumix::");
		cs_type = c;
		char* end = cs_type.data + stringLength(cs_type.data) - 1;
		bool is_ref = false;
		while (end >= cs_type.data && (*end == ' ' || *end == '&')) {
			is_ref = is_ref || *end == '&';
			--end;
		}
		++end;
		*end = 0;
		bool is_const = false;
		if (endsWith(cs_type, " const")) {
			is_const = true;
			cs_type.data[stringLength(cs_type.data) - sizeof(" const") + 1] = '\0';
		}
		bool is_ptr = false;
		if (endsWith(cs_type, "*")) {
			is_ptr = true;
			if (startsWith(cs_type, "const char"))
				cs_type = "string";
			else if (startsWith(cs_type, "char const"))
				cs_type = "string";
		} else if (cs_type == "Path") {
			cs_type = "string";
		} else if ((is_ptr || is_ref) && !is_const) {
			StaticString<64> tmp("ref ", cs_type);
			cs_type = tmp;
		}
	}


	static void getInteropType(const char* cpp_type, StaticString<64>& cs_type) {
		const char* c = cpp_type;
		auto skip = [&c](const char* value) {
			if (startsWith(c, value)) c += stringLength(value);
		};
		skip("struct ");
		skip("Lumix::");
		cs_type = c;
		char* end = cs_type.data + stringLength(cs_type.data) - 1;
		while (end >= cs_type.data && (*end == ' ' || *end == '&')) --end;
		++end;
		*end = 0;
		if (endsWith(cs_type, " const")) cs_type.data[stringLength(cs_type.data) - sizeof(" const") + 1] = '\0';
		if (cs_type == "const char*") cs_type = "string";
		if (cs_type == "Path") cs_type = "string";
		if (cs_type == "Entity") cs_type = "int";
	}


	/*static void writeCSArgs(const reflection::FunctionBase& func, os::OutputFile& file, int skip_args, bool cs_internal_call, bool start_with_comma) {
		for (int i = skip_args, c = func.getArgCount(); i < c; ++i) {
			if (i > skip_args || start_with_comma) file << ", ";
			StaticString<64> cs_type;
			getCSType(func.getArgType(i), cs_type);
			if (cs_internal_call && cs_type == "Entity") cs_type = "int";
			file << cs_type << " a" << i - skip_args;
		}
	}


	static void generateEnumsBindings() {
		os::OutputFile cs_file;
		if (!cs_file.open("cs/generated/enums.cs")) {
			logError("C#") << "Failed to create cs/generated/enums.cs";
			return;
		}

		cs_file << "namespace Lumix {\n";

		for (int i = 0, count = reflection::getEnumsCount(); i < count; ++i) {
			const reflection::EnumBase& e = reflection::getEnum(i);
			const char* name_start = e.name;
			if (startsWith(name_start, "enum ")) name_start += stringLength("enum ");
			name_start = reverseFind(name_start, nullptr, ':');
			if (name_start[0] == ':') ++name_start;
			cs_file << "\tenum " << name_start << " {\n";
			for (int j = 0; j < e.values_count; ++j) {
				cs_file << "\t\t" << e.values[j].name;
				if (j < e.values_count - 1) cs_file << ",";
				cs_file << "\n";
			}

			cs_file << "\t}\n";
		}

		cs_file << "}\n";
		cs_file.close();
	}


	static void generateScenesBindings(os::OutputFile& api_file) {
		using namespace Reflection;
		for (int i = 0, c = getScenesCount(); i < c; ++i) {
			const SceneBase& scene = reflection::getScene(i);

			StaticString<128> class_name;
			getCSharpName(scene.name, class_name);
			class_name << "Scene";

			os::OutputFile cs_file;
			StaticString<LUMIX_MAX_PATH> filepath("cs/generated/", class_name, ".cs");
			if (!cs_file.open(filepath)) {
				logError("C#") << "Failed to create " << filepath;
				continue;
			}

			cs_file << "using System;\n"
					   "using System.Runtime.InteropServices;\n"
					   "using System.Runtime.CompilerServices;\n"
					   "\n"
					   "namespace Lumix\n"
					   "{\n"
					   "	public unsafe partial class "
					<< class_name
					<< " : IScene\n"
					   "	{\n"
					   "		public static string Type { get { return \""
					<< scene.name
					<< "\"; } }\n"
					   "\n"
					   "		public "
					<< class_name
					<< "(IntPtr _instance)\n"
					   "			: base(_instance) { }\n"
					   "\n"
					   "		public static implicit operator System.IntPtr("
					<< class_name
					<< " _value)\n"
					   "		{\n"
					   "			return _value.instance_;\n"
					   "		}\n"
					   "\n";

			struct : IFunctionVisitor {
				void visit(const struct FunctionBase& func) override {
					StaticString<128> cs_method_name;
					const char* cpp_method_name = func.decl_code + stringLength(func.decl_code);
					while (cpp_method_name > func.decl_code && *cpp_method_name != ':') --cpp_method_name;
					StaticString<64> cs_return_type;
					getCSType(func.getReturnType(), cs_return_type);
					StaticString<64> interop_return_type;
					getInteropType(func.getReturnType(), interop_return_type);

					if (*cpp_method_name == ':') ++cpp_method_name;
					getCSharpName(cpp_method_name, cs_method_name);
					*api_file << "{\n"
								 "	auto f = &CSharpMethodProxy<decltype(&"
							  << func.decl_code << ")>::call<&" << func.decl_code
							  << ">;\n"
								 "	mono_add_internal_call(\"Lumix."
							  << class_name << "::" << cpp_method_name
							  << "\", f);\n"
								 "}\n"
								 "\n\n";

					*cs_file << "		[MethodImplAttribute(MethodImplOptions.InternalCall)]\n"
								"		extern static "
							 << interop_return_type << " " << cpp_method_name << "(IntPtr instance";

					writeCSArgs(func, *cs_file, 0, true, true);

					*cs_file << ");\n"
								"\n"
								"		public "
							 << cs_return_type << " " << cs_method_name << "(";

					writeCSArgs(func, *cs_file, 0, false, false);

					const char* return_expr = cs_return_type == "void" ? "" : (cs_return_type == "Entity" ? "var ret =" : "return");

					*cs_file << ")\n"
								"		{\n"
								"			"
							 << return_expr << " " << cpp_method_name << "(instance_, ";

					for (int i = 0, c = func.getArgCount(); i < c; ++i) {
						StaticString<64> cs_type;
						getCSType(func.getArgType(i), cs_type);
						if (i > 0) *cs_file << ", ";
						if (startsWith(cs_type, "ref ")) {
							*cs_file << "ref ";
						}
						*cs_file << "a" << i;
						if (equalStrings(cs_type, "Entity")) {
							*cs_file << ".entity_Id_";
						}
					}

					*cs_file << ");\n";

					if (cs_return_type == "Entity") {
						*cs_file << "			return Universe.GetEntity(ret);\n";
					}

					*cs_file << "		}\n"
								"\n";

					*api_file << "{\n"
								 "	auto f = &CSharpMethodProxy<decltype(&"
							  << func.decl_code << ")>::call<&" << func.decl_code
							  << ">;\n"
								 "	mono_add_internal_call(\"Lumix."
							  << class_name << "::" << cs_method_name
							  << "\", f);\n"
								 "}\n";
				}

				const char* class_name;
				os::OutputFile* cs_file;
				os::OutputFile* api_file;
			} visitor;

			visitor.class_name = class_name;
			visitor.cs_file = &cs_file;
			visitor.api_file = &api_file;
			scene.visit(visitor);

			cs_file << "	}\n}\n";

			cs_file.close();
		}
	}*/


	void generateBindings() {
		os::OutputFile api_file;
		const char* api_h_filepath = "../plugins/csharp/src/api.inl";
		if (!api_file.open(api_h_filepath)) {
			logError("Failed to create ", api_h_filepath);
			return;
		}

		const char* base_path = m_app.getWorldEditor().getEngine().getFileSystem().getBasePath();
		StaticString<LUMIX_MAX_PATH> path(base_path, "cs/generated");
		if (!os::makePath(path) && !os::dirExists(path)) {
			logError("Failed to create ", path);
			return;
		}

		// generateEnumsBindings();
		// generateScenesBindings(api_file);

		using namespace reflection;
		Span<const RegisteredComponent> cmps = reflection::getComponents();
		for (const RegisteredComponent& rc : cmps) {
			const ComponentBase* cmp = rc.cmp;
			if (!cmp) continue;
			const char* cmp_name = cmp->name;
			ComponentType cmp_type = cmp->component_type;

			StaticString<128> class_name;
			getCSharpName(cmp_name, class_name);

			os::OutputFile cs_file;
			StaticString<LUMIX_MAX_PATH> filepath("cs/generated/", class_name, ".cs");
			if (!cs_file.open(filepath)) {
				logError("Failed to create ", filepath);
				continue;
			}

			cs_file << "using System;\n"
					   "using System.Runtime.InteropServices;\n"
					   "using System.Runtime.CompilerServices;\n"
					   "\n"
					   "namespace Lumix\n"
					   "{\n";
			cs_file << "	[NativeComponent(Type = \"" << cmp_name << "\")]\n";
			cs_file << "	public class " << class_name << " : Component\n";
			cs_file << "	{\n";

			cs_file << "		public " << class_name
					<< "(Entity _entity)\n"
					   "			: base(_entity,  getScene(_entity.instance_, \""
					<< cmp_name
					<< "\" )) { }\n"
					   "\n\n";

			CSPropertyVisitor visitor;

			visitor.cmp = cmp;
			visitor.class_name = class_name;
			visitor.file = &cs_file;
			visitor.api_file = &api_file;
			cmp->visit(visitor);
			/*
			struct : IFunctionVisitor {
				void visit(const FunctionBase& func) override {
					StaticString<128> cs_method_name;
					const char* cpp_method_name = func.decl_code + stringLength(func.decl_code);
					while (cpp_method_name > func.decl_code && *cpp_method_name != ':') --cpp_method_name;
					if (*cpp_method_name == ':') ++cpp_method_name;
					getCSharpName(cpp_method_name, cs_method_name);
					*api_file << "{\n"
								 "	auto f = &CSharpMethodProxy<decltype(&"
							  << func.decl_code << ")>::call<&" << func.decl_code
							  << ">;\n"
								 "	mono_add_internal_call(\"Lumix."
							  << class_name << "::" << cpp_method_name
							  << "\", f);\n"
								 "}\n"
								 "\n\n";

					StaticString<64> ret_cs;
					getCSType(func.getReturnType(), ret_cs);
					*cs_file << "		[MethodImplAttribute(MethodImplOptions.InternalCall)]\n"
								"		extern static "
							 << ret_cs << " " << cpp_method_name << "(IntPtr instance, int cmp";

					writeCSArgs(func, *cs_file, 1, true, true);

					*cs_file << ");\n"
								"\n"
								"		public "
							 << ret_cs << " " << cs_method_name << "(";

					writeCSArgs(func, *cs_file, 1, false, false);

					*cs_file << ")\n"
								"		{\n"
								"			"
							 << (ret_cs != "void" ? "return " : "") << cpp_method_name << "(scene_, entity_.entity_Id_";

					for (int i = 1, c = func.getArgCount(); i < c; ++i) {
						*cs_file << ", a" << i - 1;
					}

					*cs_file << ");\n"
								"		}\n"
								"\n";
				}
				const char* class_name;
				os::OutputFile* cs_file;
				os::OutputFile* api_file;
			} fnc_visitor;

			fnc_visitor.cs_file = &cs_file;
			fnc_visitor.class_name = class_name;
			fnc_visitor.api_file = &api_file;
			cmp->visit(fnc_visitor);
			*/
			cs_file << "\t} // class\n"
					   "} // namespace\n";

			cs_file.close();
		}
		api_file.close();
	}


	void compile() {
		m_deferred_compile = false;
		if (m_compilation_running) return;

		m_compile_log = "";
		CSharpScriptScene* scene = getScene();
		CSharpPlugin& plugin = (CSharpPlugin&)scene->getPlugin();
		plugin.unloadAssembly();
		IAllocator& allocator = m_app.getWorldEditor().getAllocator();
		const char* args[] = {"c:\\windows\\system32\\cmd.exe", "/c \"\"C:\\Program Files\\Mono\\bin\\mcs.bat\" -out:\"main.dll\" -target:library -debug -unsafe -recurse:\"cs\\*.cs\"", nullptr};
		const int res = subprocess_create(args, 0, &m_compile_process);
		m_compilation_running = res == 0;
	}

	subprocess_s m_compile_process;
	bool m_compilation_running = false;
	StudioApp& m_app;
	UniquePtr<FileSystemWatcher> m_watcher;
	String m_compile_log;
	char m_filter[128];
	char m_new_script_name[128];
	bool m_deferred_compile = false;
};

/*
struct PropertyGridCSharpPlugin final : public PropertyGrid::IPlugin {
	struct SetPropertyCommand final : public IEditorCommand {
		explicit SetPropertyCommand(WorldEditor& _editor)
			: property_name(_editor.getAllocator())
			, new_value(_editor.getAllocator())
			, old_value(_editor.getAllocator())
			, editor(_editor) {}


		SetPropertyCommand(WorldEditor& _editor, Entity entity, int scr_index, const char* property_name, const char* old_value, const char* new_value, IAllocator& allocator)
			: property_name(property_name, allocator)
			, new_value(new_value, allocator)
			, old_value(old_value, allocator)
			, entity(entity)
			, script_index(scr_index)
			, editor(_editor) {}


		bool execute() override {
			set(new_value);
			return true;
		}


		void set(const string& value) {
			CSharpScriptScene* scene = static_cast<CSharpScriptScene*>(editor.getUniverse()->getScene(crc32("csharp_script")));
			u32 gc_handle = scene->getGCHandle(entity, script_index);
			MonoString* prop_mono_str = mono_string_new(mono_domain_get(), property_name.c_str());
			MonoString* value_str = mono_string_new(mono_domain_get(), value.c_str());
			auto that = this;
			void* args[] = {&that, prop_mono_str, value_str};
			scene->tryCallMethod(gc_handle, "OnUndo", args, lengthOf(args), true);
		}


		void undo() override { set(old_value); }


		void serialize(JsonSerializer& serializer) override {
			// TODO
		}


		void deserialize(JsonDeserializer& serializer) override {
			// TODO
		}


		const char* getType() override { return "set_csharp_script_property"; }


		bool merge(IEditorCommand& command) override {
			auto& cmd = static_cast<SetPropertyCommand&>(command);
			if (cmd.entity == entity && cmd.script_index == script_index && cmd.property_name == property_name) {
				cmd.new_value = new_value;
				return true;
			}
			return false;
		}


		WorldEditor& editor;
		String property_name;
		String old_value;
		String new_value;
		int value_type;
		EntityPtr entity;
		int script_index;
	};


	struct AddCSharpScriptCommand final : public IEditorCommand {
		explicit AddCSharpScriptCommand(WorldEditor& _editor)
			: editor(_editor) {}


		bool execute() override {
			auto* scene = static_cast<CSharpScriptScene*>(editor.getUniverse()->getScene(crc32("csharp_script")));
			scr_index = scene->addScript(entity);
			scene->setScriptNameHash(entity, scr_index, name_hash);
			return true;
		}


		void undo() override {
			auto* scene = static_cast<CSharpScriptScene*>(editor.getUniverse()->getScene(crc32("csharp_script")));
			scene->removeScript(entity, scr_index);
		}


		void serialize(JsonSerializer& serializer) override {
			serializer.serialize("entity", entity);
			serializer.serialize("name_hash", name_hash);
		}


		void deserialize(JsonDeserializer& serializer) override {
			serializer.deserialize("entity", entity, INVALID_ENTITY);
			serializer.deserialize("name_hash", name_hash, 0);
		}


		const char* getType() override { return "add_csharp_script"; }


		bool merge(IEditorCommand& command) override { return false; }


		WorldEditor& editor;
		Entity entity;
		u32 name_hash;
		int scr_index;
	};


	struct RemoveScriptCommand final : public IEditorCommand {
		explicit RemoveScriptCommand(WorldEditor& editor)
			: blob(editor.getAllocator())
			, scr_index(-1)
			, entity(INVALID_ENTITY) {
			scene = static_cast<CSharpScriptScene*>(editor.getUniverse()->getScene(crc32("csharp_script")));
		}


		explicit RemoveScriptCommand(IAllocator& allocator)
			: blob(allocator)
			, scene(nullptr)
			, scr_index(-1)
			, entity(INVALID_ENTITY) {}


		bool execute() override {
			scene->serializeScript(entity, scr_index, blob);
			scene->removeScript(entity, scr_index);
			return true;
		}


		void undo() override {
			scene->insertScript(entity, scr_index);
			InputBlob input(blob);
			scene->deserializeScript(entity, scr_index, input);
		}


		void serialize(JsonSerializer& serializer) override {
			serializer.serialize("entity", entity);
			serializer.serialize("scr_index", scr_index);
		}


		void deserialize(JsonDeserializer& serializer) override {
			serializer.deserialize("entity", entity, INVALID_ENTITY);
			serializer.deserialize("scr_index", scr_index, 0);
		}


		const char* getType() override { return "remove_csharp_script"; }


		bool merge(IEditorCommand& command) override { return false; }

		OutputBlob blob;
		CSharpScriptScene* scene;
		Entity entity;
		int scr_index;
	};


	static Resource* csharp_resourceInput(PropertyGridCSharpPlugin* that, MonoString* label, MonoString* type, Resource* resource) {
		MonoStringHolder label_str = label;
		MonoStringHolder type_str = type;
		ResourceType res_type((const char*)type_str);
		AssetBrowser& browser = that->m_app.getAssetBrowser();
		char buf[LUMIX_MAX_PATH];
		copyString(buf, resource ? resource->getPath().c_str() : "");
		if (browser.resourceInput((const char*)label_str, (const char*)label_str, buf, sizeof(buf), res_type)) {
			if (buf[0] == '\0') return nullptr;
			ResourceManagerBase* manager = that->m_app.getWorldEditor().getEngine().getResourceManager().get(res_type);
			if (manager) {
				return manager->load(Path(buf));
			}
		}
		return resource;
	}


	static Entity csharp_entityInput(PropertyGridCSharpPlugin* that, Universe* universe, MonoString* label_mono, Entity entity) {
		StudioApp& app = that->m_app;
		PropertyGrid& prop_grid = app.getPropertyGrid();
		MonoStringHolder label = label_mono;
		prop_grid.entityInput((const char*)label, (const char*)label, entity);
		return entity;
	}


	static void csharp_Component_pushUndoCommand(PropertyGridCSharpPlugin* that,
		Universe* universe,
		Entity entity,
		MonoObject* cmp_obj,
		MonoString* prop,
		MonoString* old_value,
		MonoString* new_value) {
		CSharpScriptScene* scene = (CSharpScriptScene*)universe->getScene(CSHARP_SCRIPT_TYPE);
		MonoStringHolder prop_str = prop;
		MonoStringHolder new_value_str = new_value;
		MonoStringHolder old_value_str = old_value;
		WorldEditor& editor = that->m_app.getWorldEditor();
		IAllocator& allocator = editor.getAllocator();
		int script_count = scene->getScriptCount(entity);
		for (int i = 0; i < script_count; ++i) {
			u32 gc_handle = scene->getGCHandle(entity, i);
			if (mono_gchandle_get_target(gc_handle) == cmp_obj) {
				auto* set_source_cmd =
					LUMIX_NEW(allocator, PropertyGridCSharpPlugin::SetPropertyCommand)(editor, entity, i, (const char*)prop_str, (const char*)old_value_str, (const char*)new_value_str, allocator);
				editor.executeCommand(set_source_cmd);
				break;
			}
		}
	}


	explicit PropertyGridCSharpPlugin(StudioCSharpPlugin& studio_plugin)
		: m_app(studio_plugin.m_app)
		, m_studio_plugin(studio_plugin) {
		mono_add_internal_call("Lumix.Component::pushUndoCommand", &csharp_Component_pushUndoCommand);
		mono_add_internal_call("Lumix.Component::entityInput", &csharp_entityInput);
		mono_add_internal_call("Lumix.Component::resourceInput", &csharp_resourceInput);
	}


	void onGUI(PropertyGrid& grid, ComponentUID cmp) override {
		if (cmp.type != CSHARP_SCRIPT_TYPE) return;

		auto* scene = static_cast<CSharpScriptScene*>(cmp.scene);
		auto& plugin = static_cast<CSharpPlugin&>(scene->getPlugin());
		WorldEditor& editor = m_app.getWorldEditor();
		IAllocator& allocator = editor.getAllocator();

		if (ImGui::Button("Add script")) ImGui::OpenPopup("add_csharp_script_popup");

		if (ImGui::BeginPopup("add_csharp_script_popup")) {
			int count = plugin.getNamesCount();
			for (int i = 0; i < count; ++i) {
				const char* name = plugin.getName(i);
				bool b = false;
				if (ImGui::Selectable(name, &b)) {
					auto* cmd = LUMIX_NEW(allocator, AddCSharpScriptCommand)(editor);
					cmd->entity = cmp.entity;
					cmd->name_hash = crc32(name);
					editor.executeCommand(cmd);
					break;
				}
			}
			ImGui::EndPopup();
		}

		for (int j = 0; j < scene->getScriptCount(cmp.entity); ++j) {
			const char* script_name = scene->getScriptName(cmp.entity, j);
			StaticString<LUMIX_MAX_PATH + 20> header(script_name);
			if (header.empty()) header << j;
			header << "###" << j;
			if (ImGui::CollapsingHeader(header)) {
				u32 gc_handle = scene->getGCHandle(cmp.entity, j);
				if (gc_handle == INVALID_GC_HANDLE) continue;
				ImGui::PushID(j);
				auto* that = this;
				void* args[] = {&that};
				scene->tryCallMethod(gc_handle, "OnInspector", args, 1, true);
				if (ImGui::Button("Edit")) {
					StaticString<LUMIX_MAX_PATH> filename(script_name, ".cs");
					m_studio_plugin.openVSCode(filename);
				}
				ImGui::SameLine();
				if (ImGui::Button("Remove script")) {
					auto* cmd = LUMIX_NEW(allocator, RemoveScriptCommand)(allocator);
					cmd->entity = cmp.entity;
					cmd->scr_index = j;
					cmd->scene = scene;
					editor.executeCommand(cmd);
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
		}
	}

	StudioApp& m_app;
	StudioCSharpPlugin& m_studio_plugin;
};

*/
struct AddCSharpComponentPlugin final : public StudioApp::IAddComponentPlugin {
	AddCSharpComponentPlugin(StudioApp& _app)
		: app(_app) {}


	void onGUI(bool create_entity, bool from_filter, EntityPtr parent, struct WorldEditor& editor) override {
		ImGui::SetNextWindowSize(ImVec2(300, 300));
		if (!ImGui::BeginMenu(getLabel())) return;

		CSharpScriptScene* script_scene = (CSharpScriptScene*)editor.getUniverse()->getScene(CSHARP_SCRIPT_TYPE);
		CSharpPlugin& plugin = (CSharpPlugin&)script_scene->getPlugin();
		for (auto& iter : plugin.getNamesArray()) {
			const char* name = iter.c_str();
			bool b = false;
			if (ImGui::Selectable(name, &b)) {
				if (create_entity) {
					EntityRef entity = editor.addEntity();
					editor.selectEntities(Span(&entity, 1), false);
				}
				if (editor.getSelectedEntities().empty()) return;
				EntityRef entity = editor.getSelectedEntities()[0];

				if (!editor.getUniverse()->hasComponent(entity, CSHARP_SCRIPT_TYPE)) {
					editor.addComponent(Span(&entity, 1), CSHARP_SCRIPT_TYPE);
				}

				const ComponentUID cmp = editor.getUniverse()->getComponent(entity, CSHARP_SCRIPT_TYPE);
				editor.beginCommandGroup("add_cs_script");
				editor.addArrayPropertyItem(cmp, "scripts");

				int scr_count = script_scene->getScriptCount(entity);
				editor.setProperty(cmp.type, "scripts", scr_count - 1, "Script", Span((const EntityRef*)&entity, 1), name);
				editor.endCommandGroup();
				editor.lockGroupCommand();
				if (parent.isValid()) editor.makeParent(parent, entity);
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndMenu();
	}


	const char* getLabel() const override { return "C# Script"; }


	StudioApp& app;
};


namespace {

/*
IEditorCommand* createAddCSharpScriptCommand(WorldEditor& editor) {
return LUMIX_NEW(editor.getAllocator(), PropertyGridCSharpPlugin::AddCSharpScriptCommand)(editor);
}


IEditorCommand* createSetPropertyCommand(WorldEditor& editor) {
return LUMIX_NEW(editor.getAllocator(), PropertyGridCSharpPlugin::SetPropertyCommand)(editor);
}


IEditorCommand* createRemoveScriptCommand(WorldEditor& editor) {
return LUMIX_NEW(editor.getAllocator(), PropertyGridCSharpPlugin::RemoveScriptCommand)(editor);
}
*/

} // namespace


LUMIX_STUDIO_ENTRY(csharp) {
	WorldEditor& editor = app.getWorldEditor();
	IAllocator& allocator = editor.getAllocator();
	StudioCSharpPlugin* plugin = LUMIX_NEW(allocator, StudioCSharpPlugin)(app);
	app.addPlugin(*plugin);

	auto* cmp_plugin = LUMIX_NEW(allocator, AddCSharpComponentPlugin)(app);
	app.registerComponent("", "csharp_script", *cmp_plugin);
	//
	// editor.registerEditorCommandCreator("add_csharp_script", createAddCSharpScriptCommand);
	// editor.registerEditorCommandCreator("remove_csharp_script", createRemoveScriptCommand);
	// editor.registerEditorCommandCreator("set_csharp_script_property", createSetPropertyCommand);

	// auto* pg_plugin = LUMIX_NEW(editor.getAllocator(), PropertyGridCSharpPlugin)(*plugin);
	// app.getPropertyGrid().addPlugin(*pg_plugin);
	// ASSERT(false); // TODO
	return nullptr;
}


} // namespace Lumix