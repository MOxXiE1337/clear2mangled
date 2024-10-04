import libpe;

#include "c2m.hpp"

#include <json/json.h>

bool HaveSubString(const std::string& parentString, const std::string& subString)
{
	return parentString.find(subString) != std::string::npos;
}

bool SearchAllSubStrings(const std::string& string, const std::regex& pattern, std::vector<std::string>& results)
{
	results.clear();

	std::smatch match{};
	std::string::const_iterator begin(string.cbegin());

	while (std::regex_search(begin, string.cend(), match, pattern))
	{
		results.push_back(match.str());
		begin = match.suffix().first;
	}
	return !results.empty();
}

std::string FindLastMatch(const std::string& input, const std::regex& pattern) {
	std::smatch match;
	size_t lastPos = 0;
	std::string lastMatch;

	while (std::regex_search(input.begin() + lastPos, input.end(), match, pattern)) {
		lastMatch = match[0];
		lastPos += match.position(0) + match.length(0);
	}

	return lastMatch; 
}

void ReplaceSubstring(std::string& str, const std::string& toReplace, const std::string& replacement) {
	size_t pos = 0;
	while ((pos = str.find(toReplace, pos)) != std::string::npos) {
		str.replace(pos, toReplace.length(), replacement);
		pos += replacement.length(); // 移动到下一个位置
	}
}

std::string RemoveAngleBrackets(const std::string& string)
{
	std::string result{ string };
	int level = 0;
	size_t bracket;
	std::vector<std::string> brackets{};

	for (size_t i = 0; i < string.size(); i++)
	{
		if (string[i] == '<')
		{
			if (level == 0)
			{
				bracket = i;
			}
			level++;
		}
		if (string[i] == '>')
		{
			level--;
			if (level == 0)
			{
				brackets.push_back(string.substr(bracket, i - bracket + 1));
			}
		}
	}


	for (auto& i : brackets)
	{
		ReplaceSubstring(result, i, "");
	}
	return result;
}

namespace c2m
{
	std::string State::SimplifyDeclaration(const std::string& declaration)
	{
		std::string simplified{ declaration };

		simplified = std::regex_replace(simplified, std::regex{ R"((public|private|protected): )" }, "");
		simplified = std::regex_replace(simplified, std::regex{ R"((static|virtual) )" }, "");
		simplified = std::regex_replace(simplified, std::regex{ R"((__\w+call|__cdecl))" }, ""); // to prevent like __cdecl* __cdecl
		simplified = std::regex_replace(simplified, std::regex{ R"(  )" }, " ");

		simplified = std::regex_replace(simplified, std::regex{ R"((class|struct) )" }, "");
		simplified = std::regex_replace(simplified, std::regex{ R"( >)" }, ">");
		simplified = std::regex_replace(simplified, std::regex{ R"( \&)" }, "&");
		simplified = std::regex_replace(simplified, std::regex{ R"( \*)" }, "*");
		simplified = std::regex_replace(simplified, std::regex{ R"(\)const )" }, ") const");

		simplified = std::regex_replace(simplified, std::regex{ R"(`vftable')" }, "vftable");
		simplified = std::regex_replace(simplified, std::regex{ R"(`vbtable')" }, "vbtable");
		simplified = std::regex_replace(simplified, std::regex{ R"(`default constructor closure')" }, "default_constructor_closure");
		simplified = std::regex_replace(simplified, std::regex{ R"(`vbase destructor')" }, "vbase_destructor");
		simplified = std::regex_replace(simplified, std::regex{ R"(`|')" }, "");
		simplified = std::regex_replace(simplified, std::regex{ R"( __ptr64)" }, "");

		simplified = std::regex_replace(simplified, std::regex{ R"(\{.+\})" }, "");

		if (simplified[0] == ' ')
			simplified = simplified.substr(1);

		return simplified;
	}

	void State::GenerateCacheFile()
	{
		try {
			
			LoadExportsFromPEFile();
			
			SaveToCacheFile();
		}
		catch (const std::exception& err)
		{
			throw err;
		}
	}

	void State::SaveToCacheFile()
	{
		std::ofstream file;
		file.open(m_cachePath);

		if (!file.is_open())
			throw std::exception{ "failed to open cache file." };

		Json::Value root;

		for (auto& i : m_exports)
		{
			Json::Value exp;
			exp["ordinal"] = i.Ordinal;
			exp["rva"] = i.Rva;
			exp["mangled_declaration"] = i.MangledDeclaration;
			exp["clear_declaration"] = i.ClearDeclaration;

			exp["declaration_details"] = Json::Value{};
			exp["declaration_details"]["c_function"] = i.DeclarationDetails.CFunction;
			exp["declaration_details"]["variable"] = i.DeclarationDetails.Variable;
			exp["declaration_details"]["constructor_function"] = i.DeclarationDetails.ConstructorFunction;
			exp["declaration_details"]["destructor_function"] = i.DeclarationDetails.DestructorFunction;

			exp["declaration_details"]["name"] = i.DeclarationDetails.Name;
			exp["declaration_details"]["parentheses_pairs"] = Json::Value{};
			for (auto& j : i.DeclarationDetails.ParenthesesPairs)
				exp["declaration_details"]["parentheses_pairs"].append(j);

			root.append(exp);
		}

		file << root.toStyledString() << std::endl;
		file.close();
	}

	void State::LoadExportsFromPEFile()
	{
		libpe::Clibpe pe;
		if (pe.OpenFile(m_filePath.c_str()) != libpe::PEOK)
			throw std::exception{ "failed to parse the target PE file." };

		const auto exports = pe.GetExport();

		if (!exports) 
			throw std::exception{ "target file does not have exports." };

		std::println(std::cout, "c2m is generating cache file, this may take some time...");

		static char buffer[1024];
		for (auto& i : exports.value().vecFuncs)
		{
			std::string cmd = "undname.exe " + i.strFuncName;

			FILE* fp;
			if ((fp = _popen(cmd.c_str(), "r")) == NULL)
				throw std::exception{ "failed to generate process pipe." };;

			memset(buffer, 0, 1024);
			while (fgets(buffer, 1022, fp) != NULL)
			{
				std::string_view result{ buffer };
				size_t pos = result.find("is :- \"");

				if (pos == std::string::npos)
					continue;

				std::string clearDeclaration = SimplifyDeclaration(std::string{ result.substr(pos + 7, result.find_last_of('\"') - pos - 7) });
				DeclarationDetails details{};
				ParseDeclarationDetails(clearDeclaration, details);

				m_exports.push_back(
					{
						i.dwOrdinal,
						i.dwFuncRVA,
						i.strFuncName,
						clearDeclaration,
						details
					}
				);
			}

			if (_pclose(fp) == -1)
				throw std::exception{ "failed to close process pipe." };
		}
	}

	void State::LoadExportsFromCacheFile()
	{
		std::ifstream file;
		file.open(m_cachePath);

		if (!file.is_open())
			throw std::exception{ "failed to open cache file." };

		Json::Reader reader;
		Json::Value root;

		if (!reader.parse(file, root, false))
			throw std::exception{ "failed to parse json file." };

		for (auto& i : root)
		{
			DeclarationDetails details{};
			details.CFunction = i["declaration_details"]["c_function"].asBool();
			details.Variable = i["declaration_details"]["variable"].asBool();
			details.ConstructorFunction = i["declaration_details"]["constructor_function"].asBool();
			details.DestructorFunction = i["declaration_details"]["destructor_function"].asBool();
			details.Name = i["declaration_details"]["name"].asString();

			for (auto& j : i["declaration_details"]["parentheses_pairs"])
			{
				details.ParenthesesPairs.push_back(j.asString());
			}

			m_exports.push_back(
				{
					i["ordinal"].asUInt64(),
					i["rva"].asUInt(),
					i["mangled_declaration"].asString(),
					i["clear_declaration"].asString(),
					details
				}
			);
		}
	}

	void State::ParseDeclarationDetails(const std::string& declaration, DeclarationDetails& details)
	{
		// parse parentheses pairs
		if (!SearchAllSubStrings(declaration, std::regex{ R"(\(([^()]*(\([^()]*\)[^()]*)*)\))" }, details.ParenthesesPairs))
		{
			// no space? 
			if (!HaveSubString(declaration, " ") && 
				!HaveSubString(declaration, "<") && 
				!HaveSubString(declaration, ">") &&
				!HaveSubString(declaration, ":"))
			{
				details.CFunction = true;
			}					
			else
			{	
				details.Variable = true; 
				// the declaration still may be a variable (function pointer)
				// like std::basic_string<char,std::char_traits<char>,std::allocator<char>> (* ExportedGlobalVariable)(std::basic_iostream<char,std::char_traits<char>> const&)
			}
		}

		// extract export name
		std::regex namePattern{ R"((::| )~?([\w<> +=+-/\*]+))" };
		if (details.ParenthesesPairs.empty())
		{
			if (details.CFunction)
				details.Name = declaration;
			else
			{
				details.Name = RemoveAngleBrackets(declaration);
				details.Name = FindLastMatch(details.Name, namePattern); // variable	
			}
				
		}
		else
		{
			if (declaration[declaration.find(details.ParenthesesPairs[0]) - 1] == ' ') // function pointer?
			{
				details.Variable = true;
				// get from the first pair
				size_t pos = details.ParenthesesPairs[0].find("* ");
				details.Name = details.ParenthesesPairs[0].substr(pos + 2, details.ParenthesesPairs[0].size() - pos - 1);
			}
			else
			{
				size_t endPos = declaration.find(details.ParenthesesPairs[0]);
				details.Name = RemoveAngleBrackets(declaration.substr(0, endPos));
				details.Name = FindLastMatch(details.Name, namePattern);
			}
		}

		details.Name = std::regex_replace(details.Name, std::regex{ R"(::)" }, "");
		if (details.Name[0] == ' ')
			details.Name = details.Name.substr(1);

		// constructor function?
		if (details.Name == "default_constructor_closure")
			details.ConstructorFunction = true;

		if (HaveSubString(details.Name, "~") || details.Name == "vbase_destructor")
			details.DestructorFunction = true;

	}

	void State::PrintSearchTargetDetails(DeclarationDetails& details)
	{
		std::println(std::cout, "Name:                {}", details.Name);
		std::println(std::cout, "CFunction:           {}", details.CFunction ? "YES" : "NO");
		std::println(std::cout, "ConstructorFunction: {}", details.ConstructorFunction ? "YES" : "NO");
		std::println(std::cout, "DestructorFunction:  {}", details.DestructorFunction ? "YES" : "NO");
		std::println(std::cout, "");
	}

	void State::PrintExport(Export& exp, uintptr_t baseAddress = -1)
	{
		printf(COLOR_BLUE"%d" COLOR_MAGENTA "\t%p" COLOR_CYAN "\t%s" COLOR_END "\t%s\n",
			exp.Ordinal,
			(baseAddress == -1) ? exp.Rva : baseAddress + exp.Rva,
			exp.DeclarationDetails.Variable ? "Variable" : (exp.DeclarationDetails.CFunction ? "C Function" : "C++ Function"),
			exp.MangledDeclaration.c_str());
		printf("+-----------------------------------------------" COLOR_YELLOW "%s\n\n" COLOR_END, exp.ClearDeclaration.c_str());
	}

	void State::LoadFile(const std::filesystem::path& path)
	{
		if (!std::filesystem::exists(path))
			throw std::exception{ "file does not exist." };

		m_filePath = path;
		m_fileName = path.filename();
		m_cachePath = "./cache/" + m_fileName.string() + ".json";

		
		try {
			if (!std::filesystem::exists(m_cachePath))
			{
				GenerateCacheFile();
			}
			else
			{
				LoadExportsFromCacheFile();
			}
		}
		catch (const std::exception& err) {
			throw err;
		}
	}
	
	void State::PrintMangledNameByClearDeclaration(const std::string& declaration) noexcept
	{
		std::string simplifiedDeclaration = SimplifyDeclaration(declaration);
		std::println(std::cout, "Debug:               {}", RemoveAngleBrackets(simplifiedDeclaration));

		DeclarationDetails details{};
		ParseDeclarationDetails(simplifiedDeclaration, details);
		PrintSearchTargetDetails(details);

		std::vector<Export*> results{};

		for (auto& exp : m_exports)
		{
			if (details.Name == exp.DeclarationDetails.Name &&
				details.CFunction == exp.DeclarationDetails.CFunction &&
				details.ConstructorFunction == exp.DeclarationDetails.ConstructorFunction &&
				details.DestructorFunction == exp.DeclarationDetails.DestructorFunction)
			{
				results.push_back(&exp);
			}
		}

		if (results.empty())
		{
			std::println(std::cout, COLOR_RED "mangled name of \"{}\" not found" COLOR_END, declaration);
		}
		else
		{
			printf(COLOR_GREEN "Ordinal\tRva             \tType    \tName\n" COLOR_END);
			for (auto& i : results)
				PrintExport(*i);
		}
	}

	void State::PrintMangledNameByAddress(uintptr_t baseAddress, uintptr_t address) noexcept
	{
		uintptr_t rva = address - baseAddress;

		std::vector<Export*> results{};

		for (auto& exp : m_exports) {
			if (rva == exp.Rva)
				results.push_back(&exp);
		}

		if (results.empty())
		{
			std::println(std::cout, COLOR_RED "mangled name of rva \"{:x}\" not found" COLOR_END, rva);
		}
		else
		{
			printf(COLOR_GREEN "Ordinal\tVa             \tType    \tName\n" COLOR_END);
			for (auto& i : results)
			{
				PrintExport(*i, baseAddress);
			}
		}
	}

	void State::PrintMangledNameByRVA(uintptr_t rva) noexcept
	{
		std::vector<Export*> results{};

		for (auto& exp : m_exports) {
			if (rva == exp.Rva)
				results.push_back(&exp);
		}

		if (results.empty())
		{
			std::println(std::cout,COLOR_RED "mangled name of rva \"{:x}\" not found" COLOR_END, rva);
		}
		else
		{
			printf(COLOR_GREEN "Ordinal\tRva             \tType    \tName\n" COLOR_END);
			for (auto& i : results)
				PrintExport(*i);
		}
	}
}