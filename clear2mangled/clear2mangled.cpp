import libpe;

#include <string>
#include <string_view>
#include <format>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>

#include <argparse/argparse.hpp>
#include <json/json.h>

struct export_function_desc
{
	uintptr_t ordinal;
	uintptr_t rva;
	std::string mangled_name;
	std::string clear_name;
};

std::vector<export_function_desc> exports;

bool generate_names(std::filesystem::path filepath)
{
	libpe::Clibpe pe;
	if (pe.OpenFile(filepath.c_str()) != libpe::PEOK)
		return false;

	const auto peexport = pe.GetExport();

	if (!peexport) // no export?
		return false;

	static char buffer[1024];

	for (auto& exp : peexport.value().vecFuncs)
	{
		// generate clear exports
		std::string cmd =  "undname.exe " + exp.strFuncName;

		FILE* fp;
		if ((fp = _popen(cmd.c_str(), "r")) == NULL)
			return false;

		memset(buffer, 0, 1024);
		while (fgets(buffer, 1022, fp) != NULL)
		{
			std::string_view result{ buffer };
			size_t pos = result.find("is :- \"");
			if (pos == std::string::npos)
				continue;

			std::string clear_name = std::string{ result.substr(pos + 7, result.find_last_of('\"') - pos - 7) };

			// remove protect description
			if (clear_name.find("private: ") != std::string::npos)
				clear_name = clear_name.substr(9);

			if (clear_name.find("protected: ") != std::string::npos)
				clear_name = clear_name.substr(11);

			if(clear_name.find("public: ") != std::string::npos)
				clear_name = clear_name.substr(8);

	
			exports.push_back({
				exp.dwOrdinal,
				exp.dwFuncRVA,
				exp.strFuncName,
				std::string{clear_name}
				});
		}

		if (_pclose(fp) == -1)
			return false;
	}
}

bool generate_cache(std::filesystem::path filename)
{
	// write to json
	std::filesystem::path cachepath = "./cache/" + filename.string() + ".json";

	std::ofstream file;
	file.open(cachepath);

	if (!file.is_open())
		return false;

	Json::Value root;
	Json::FastWriter writer;

	for (auto& i : exports)
	{
		Json::Value exp;
		exp["ordinal"] = i.ordinal;
		exp["rva"] = i.rva;
		exp["mangled_name"] = i.mangled_name;
		exp["clear_name"] = i.clear_name;
		root.append(exp);
	}

	std::string jsonstr = writer.write(root);
	file << jsonstr << std::endl;
	file.close();
}

bool read_cache(std::filesystem::path filename)
{
	std::filesystem::path cachepath = "./cache/" + filename.string() + ".json";

	std::ifstream file;
	file.open(cachepath);

	if (!file.is_open())
		return false;

	Json::Reader reader;
	Json::Value root;

	if (!reader.parse(file, root, false))
		return false;

	for (auto& i : root)
	{
		exports.push_back(
			{
				i["ordinal"].asUInt64(),
				i["rva"].asUInt(),
				i["mangled_name"].asString(),
				i["clear_name"].asString()
			}
			);
	}
}

int levenshtein_distance(const std::string& s1, const std::string& s2) {
	size_t len1 = s1.size();
	size_t len2 = s2.size();

	std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));

	for (size_t i = 0; i <= len1; ++i) {
		dp[i][0] = i;
	}
	for (size_t j = 0; j <= len2; ++j) {
		dp[0][j] = j;
	}

	for (size_t i = 1; i <= len1; ++i) {
		for (size_t j = 1; j <= len2; ++j) {
			if (s1[i - 1] == s2[j - 1]) {
				dp[i][j] = dp[i - 1][j - 1]; 
			}
			else {
				dp[i][j] = std::min({ dp[i - 1][j] + 1,      
									 dp[i][j - 1] + 1,      
									 dp[i - 1][j - 1] + 1 });  
			}
		}
	}

	return dp[len1][len2];
}

int main(int argc, char* argv[])
{
	_wmkdir(L"cache");

	argparse::ArgumentParser program{ "clear2mangled" };

	program.add_argument("file")
		.required()
		.help("the source PE file");

	program.add_argument("name_va")
		.default_value(std::string{})
		.help("clear cpp name or virtual address (used with --base)");
	
	program.add_argument("--base")
		.help("module base address (scanned with :x)")
		.scan<'x', uintptr_t>()
		.default_value(-1);

	program.add_argument("--rva")
		.help("get magnled name by rva (scanned with :x)")
		.scan<'x', uintptr_t>()
		.default_value(-1);

	program.add_argument("--nocache")
		.help("not use the cached export table")
		.default_value(false)
		.implicit_value(true);

	std::string description = R"(Examples:
./clear2mangled.exe ./msvcp140.dll _Chmod
./clear2mangled.exe ./msvcp140.dll 'std::basic_streambuf<char,std::char_traits<char> >::_Pninc'
./clear2mangled.exe ./msvcp140.dll --base 40000000 400168B0
./clear2mangled.exe ./msvcp140.dll --rva 168B0)";
	program.add_description(description);

	try
	{
		program.parse_args(argc, argv);
	}
	catch (const std::exception& err)
	{
		std::println(std::cerr, "{}", err.what());
		return -1;
	}

	std::filesystem::path filepath = program.get<std::string>("file");
	std::filesystem::path filename = filepath.filename();

	if (!std::filesystem::exists(filepath))
	{
		std::println(std::cerr, "\"{}\" file does not exist", filepath.string());
		return -1;
	}

	// cache generating
	if (program.get<bool>("--nocache"))
	{
		std::println(std::cout, "--nocache is enabled, cache file won't be generated");

		if (!generate_names(filepath))
		{
			std::println(std::cerr, "failed to generate names");
			return -1;
		}
	}
	else
	{
		std::filesystem::path cachepath = "./cache/" + filename.string() + ".json";

		// generate cache
		if (!std::filesystem::exists(cachepath))
		{
			std::println(std::cout, "clear2mangled is generating cache please wait...");
			if (!generate_names(filepath))
			{
				std::println(std::cerr, "failed to generate names");
				return -1;
			}
			if (!generate_cache(filename))
			{
				std::println(std::cerr, "failed to generate cache file");
				return -1;
			}
		}
		else // directly read from cache
		{
			if (!read_cache(filename))
			{
				std::println(std::cerr, "failed to read cache file");
				return -1;
			}
		}
	}

	// fuzzy match
	std::string name{ program.get<std::string>("name_va") };
	if (program.is_used("name_va") && !program.is_used("--base") && !program.is_used("--rva"))
	{
		// search sub strings first
		std::vector<std::string> substrs{};
		std::vector<export_function_desc> results{};

		std::regex pattern{ "::[~\\w]+<\\w+" };
		std::smatch match{};
		std::string::const_iterator search_start(name.cbegin());

		while (std::regex_search(search_start, name.cend(), match, pattern))
		{
			substrs.push_back(match.str());
			search_start = match.suffix().first;
		}

		bool found = false;

		// if is a C function?
		if (substrs.empty())
		{
			for (auto& exp : exports)
			{
				if (exp.clear_name == exp.mangled_name && name == exp.mangled_name)
				{
					std::printf("C STYLE FUNCTION\n");
					std::printf("ORDINAL\tRVA             \tMANGLED NAME\n");
					std::printf("%d\t%p\t%s\n", exp.ordinal, exp.rva, exp.mangled_name.c_str());
					found = true;
				}
			}
			if (!found)
			{
				std::println(std::cout, "mangled name of \"{}\" not found", name);
			}
		}
		else
		{
			for (auto& exp : exports)
			{
				bool skip = false;

				std::vector<std::string> esubstrs{};
				std::smatch ematch{};
				std::string::const_iterator esearch_start(exp.clear_name.cbegin());
				while (std::regex_search(esearch_start, exp.clear_name.cend(), ematch, pattern))
				{
					esubstrs.push_back(ematch.str());
					esearch_start = ematch.suffix().first;
				}

				if (esubstrs.size() != substrs.size())
					continue;
				
				for (int i = 0; i < esubstrs.size(); i++)
				{
					if (esubstrs[i] != substrs[i])
					{
						skip = true;
						break;
					}
				}

				if (skip)
					continue;

				if (levenshtein_distance(exp.clear_name, name) < 100)
				{
					results.push_back(exp);
					found = true;
				}
			}

			if (!found)
				std::println(std::cout, "mangled name of \"{}\" not found", name);
			else
			{
				printf("MATCH\tORDINAL\tRVA             \tMANGLED NAME\n");
				for (auto& exp : results)
				{
					std::printf("%d\t%d\t%p\t%s\n", levenshtein_distance(name, exp.clear_name), exp.ordinal, exp.rva, exp.mangled_name.c_str());
				}
			}
		}
		return 0;
	}
	

	if (program.is_used("--base"))
	{
		uintptr_t base = program.get<uintptr_t>("--base");
		if (!program.is_used("name_va"))
		{
			std::println(std::cerr, "name_va: 1 argument(s) expected. 0 provided.");
			return -1;
		}

		uintptr_t va = std::stoi(program.get<std::string>("name_va"), nullptr, 16);
		uintptr_t rva = va - base;
		bool found = false;
		std::vector<export_function_desc> results{};
		for (auto& exp : exports)
		{
			if (rva == exp.rva)
			{
				results.push_back(exp);
				found = true; 
			}
		}

		if(!found)
			std::println(std::cout, "mangled name of address {} not found", name);
		else
		{
			printf("ORDINAL\tRVA             \tMANGLED NAME\n");
			for (auto& exp : results)
			{
				std::printf("%d\t%p\t%s\n", exp.ordinal, exp.rva, exp.mangled_name.c_str());
			}
		}

		return 0;
	}

	if (program.is_used("--rva"))
	{
		uintptr_t rva = program.get<uintptr_t>("--rva");
		bool found = false;
		std::vector<export_function_desc> results{};
		for (auto& exp : exports)
		{
			if (rva == exp.rva)
			{
				results.push_back(exp);
				found = true;
			}
		}

		if (!found)
			std::println(std::cout, "mangled name of address {} not found", name);
		else
		{
			printf("ORDINAL\tRVA             \tMANGLED NAME\n");
			for (auto& exp : results)
			{
				std::printf("%d\t%p\t%s\n", exp.ordinal, exp.rva, exp.mangled_name.c_str());
			}
		}
	}

	return 0;
}

