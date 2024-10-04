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
	FILE_DECLARATION,
	VIRTUAL_ADDRESS,
	FILE_VIRTUAL_ADDRESS,
	RVA,
	FILE_RVA
};

void InitializeCommandLine(argparse::ArgumentParser& program, int argc, char* argv[])
{
	program.add_argument("src")
		.required()
		.nargs(1)
		.help("the source PE file");

	program.add_argument("declaration/va")
		.default_value("")
		.nargs(1)
		.help("the clear declaration of C++ function/variable or the virtual address of the function/variable (used with --base option)");

	program.add_argument("--file")
		.default_value("")
		.nargs(1)
		.help("use file to process multi-lined data");

	program.add_argument("--script")
		.default_value("")
		.nargs(1)
		.help("python script to process the input data (used with --file)");

	program.add_argument("--base")
		.default_value(-1)
		.scan<'x', uintptr_t>()
		.nargs(1)
		.help("the base address of the module");

	program.add_argument("--rva")
		.default_value(-1)
		.scan<'x', uintptr_t>()
		.nargs(0, 1)
		.help("the rva of the function/variable");

	try { program.parse_args(argc, argv); }
	catch (const std::exception& err) { throw err; }
}

_C2MMODE GetC2mMode(argparse::ArgumentParser& program)
{
	if(!program.is_used("--file") && program.is_used("--script"))
		throw std::exception{ "--file & --script must be used together." };

	if (program.is_used("--base") && program.is_used("--rva"))
		throw std::exception{ "--base & --rva can't be used together." };

	if (program.is_used("--file") && program.is_used("declaration/va"))
		throw std::exception{ "--file & declaration/va can't be used together." };

	if (program.is_used("--rva") && program.is_used("declaration/va"))
		throw std::exception{ "--rva & declaration/va can't be used together." };

	if (program.is_used("--base"))
	{
		if (program.is_used("declaration/va"))
			return VIRTUAL_ADDRESS;
		if (program.is_used("--file"))
			return FILE_VIRTUAL_ADDRESS;
		throw std::exception{ "declaration/va: 1 argument(s) expected. 0 provided." };
	}

	if (program.is_used("--rva"))
	{
		if (program.is_used("--file"))
			return FILE_RVA;
		return RVA;
	}

	if (program.is_used("declaration/va"))
	{
		return DECLARATION;
	}
	else
	{
		if (program.is_used("--file"))
			return FILE_DECLARATION;
	}


	return UNKNOWN;
}

void ReadFileLines(const std::filesystem::path& path, std::vector<std::string>& lines)
{
	std::ifstream file{ path };
	if (!file.is_open())
		throw std::exception{ (std::string("failed to open file \"") + path.string() + "\"").c_str()};
	std::string line{};
	
	while (std::getline(file, line)) 
	{
		lines.push_back(line);
	}

	file.close();
}

int main(int argc, char* argv[])
{
	std::filesystem::create_directory("cache");

	c2m::State state{};
	argparse::ArgumentParser program{ "clear2mangled" };
	_C2MMODE mode{ UNKNOWN };
	
	try { 
		InitializeCommandLine(program, argc, argv);
		mode = GetC2mMode(program);

		if (program.is_used("--script"))
		{
			if (!std::filesystem::exists(program.get<std::string>("--script")))
				throw std::exception{ "script file does not exist" };
		}
	
		state.LoadFile(program.get<std::string>("src"));


		std::vector<std::string> lines{};
		std::string input{};

		switch (mode)
		{
		case DECLARATION:
			state.PrintMangledNameByClearDeclaration(program.get<std::string>("declaration/va"));
			break;
		case VIRTUAL_ADDRESS:
			state.PrintMangledNameByAddress(program.get<uintptr_t>("--base"), std::stoll(program.get<std::string>("declaration/va"), nullptr, 16));
			break;
		case RVA:
			state.PrintMangledNameByRVA(program.get<uintptr_t>("--rva"));
			break;
		case FILE_DECLARATION:
			ReadFileLines(program.get<std::string>("--file"), lines);
			for (auto& line : lines)
			{
				if (program.is_used("--script"))
					input = RunCmd("python \"" + program.get<std::string>("--script") + "\" \"" + line + "\"");
				else
					input = line;

				state.PrintMangledNameByClearDeclaration(input);
			}
			break;
		case FILE_VIRTUAL_ADDRESS:
			ReadFileLines(program.get<std::string>("--file"), lines);
			for (auto& line : lines)
			{
				if (program.is_used("--script"))
					input = RunCmd("python \"" + program.get<std::string>("--script") + "\" \"" + line + "\"");
				else
					input = line;

				uintptr_t address = std::stoll(input, nullptr, 16);
				state.PrintMangledNameByAddress(program.get<uintptr_t>("--base"), address);
			}
			break;
		case FILE_RVA:
			ReadFileLines(program.get<std::string>("--file"), lines);
			for (auto& line : lines)
			{
				if (program.is_used("--script"))
					input = RunCmd("python \"" + program.get<std::string>("--script") + "\" \"" + line + "\"");
				else
					input = line;
				uintptr_t rva = std::stoll(input, nullptr, 16);
				state.PrintMangledNameByRVA(rva);
			}
			break;
		default:
			std::println(std::cerr, "unknown c2m mode.");
			return -1;
		}
	}
	catch(const std::exception& err) {
		std::println(std::cerr, "{}", err.what());
		return -1;
	}

	return 0;
}

