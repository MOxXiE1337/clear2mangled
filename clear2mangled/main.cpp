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

#include "c2m.hpp"

enum _C2MMODE
{
	UNKNOWN,
	DECLARATION,
	VIRTUAL_ADDRESS,
	RVA
};

void InitializeCommandLine(argparse::ArgumentParser& program, int argc, char* argv[])
{
	program.add_argument("file")
		.required()
		.nargs(1)
		.help("the source PE file");

	program.add_argument("declaration/va")
		.default_value("")
		.nargs(1)
		.help("the clear declaration of C++ function/variable or the virtual address of the function/variable (used with --base option)");

	program.add_argument("--base")
		.default_value(-1)
		.scan<'x', uintptr_t>()
		.nargs(1)
		.help("the base address of the module");

	program.add_argument("--rva")
		.default_value(-1)
		.scan<'x', uintptr_t>()
		.nargs(1)
		.help("the rva of the function/variable");

	try { program.parse_args(argc, argv); }
	catch (const std::exception& err) { throw err; }
}

_C2MMODE GetC2mMode(argparse::ArgumentParser& program)
{
	if (program.is_used("--base") && program.is_used("--rva"))
		throw std::exception{ "--base & --rva can't be used together." };

	if (program.is_used("--base"))
	{
		if(!program.is_used("declaration/va"))
			throw std::exception{ "declaration/va: 1 argument(s) expected. 0 provided." };
		return VIRTUAL_ADDRESS;
	}

	if (program.is_used("--rva"))
		return RVA;

	if (program.is_used("declaration/va"))
		return DECLARATION;
	else
		throw std::exception{ "no option provided." };

	return UNKNOWN;
}

std::string RemoveAngleBrackets(const std::string& string);

int main(int argc, char* argv[])
{
	std::cout << RemoveAngleBrackets("std::basic_ostream<char,std::char_traits<char>>* std::_Ptr_cerr") << std::endl;

	std::filesystem::create_directory("cache");

	c2m::State state{};
	argparse::ArgumentParser program{ "clear2mangled" };
	_C2MMODE mode{ UNKNOWN };
	
	try { 
		InitializeCommandLine(program, argc, argv);
		mode = GetC2mMode(program);
		state.LoadFile(program.get<std::string>("file"));
	}
	catch(const std::exception& err) {
		std::println(std::cerr, "{}", err.what());
		return -1;
	}

	switch (mode)
	{
	case DECLARATION:
		break;
	case VIRTUAL_ADDRESS:
		break;
	case RVA:
		break;
	default:
		std::println(std::cerr, "unknown c2m mode.");
		return -1;
	}

	/*
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
		bool flag0 = false; // idk how to name it :)
		bool static_function = false;
		bool destruct_function = false;

		std::vector<std::string> substrs{};
		std::vector<export_function_desc> results{};

		std::regex pattern{ "::[~\\w]+(<?\\w+)" };
		std::smatch match{};
		std::string::const_iterator search_start(name.cbegin());

		while (std::regex_search(search_start, name.cend(), match, pattern))
		{
			if (match.str().find("<") == std::string::npos)
				flag0 = true;

			if (name.find(match.str()) + match.str().size() == name.size())
				static_function = true;

			if (match.str().find("~") != std::string::npos)
				destruct_function = true;

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
				
				bool flag1 = false;
				bool estatic_function = false;
				bool edestruct_function = false;

				std::vector<std::string> esubstrs{};
				std::smatch ematch{};
				std::string::const_iterator esearch_start(exp.clear_name.cbegin());
				while (std::regex_search(esearch_start, exp.clear_name.cend(), ematch, pattern))
				{
					if (ematch.str().find("<") == std::string::npos)
						flag1 = true;

					if (exp.clear_name.find(ematch.str()) + ematch.str().size() == exp.clear_name.size())
						estatic_function = true;

					if (exp.clear_name.find("~") != std::string::npos)
						destruct_function = true;

					esubstrs.push_back(ematch.str());
					esearch_start = ematch.suffix().first;
				}

				if (flag0 != flag1)
					continue;

				if (!static_function)
				{
					if (estatic_function)
						continue;
				}

				if (destruct_function != edestruct_function)
					continue;
					
				for (auto& i : substrs)
				{
					bool found_in_esubstrs = false;
					for (auto& j : esubstrs)
					{
						if (i == j)
						{
							found_in_esubstrs = true;
							break;
						}
					}
					if (!found_in_esubstrs)
					{
						skip = true;
						break;
					}
				}

				if (skip)
					continue;

				results.push_back(exp);
				found = true;
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
		if (!find_by_rva(rva))
		{
			std::println(std::cout, "mangled name of rva {} not found", rva);
			return -1;
		}

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
	}*/

	return 0;
}

