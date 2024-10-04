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
	std::filesystem::create_directory("cache");

	c2m::State state{};
	argparse::ArgumentParser program{ "clear2mangled" };
	_C2MMODE mode{ UNKNOWN };
	
	try { 
		InitializeCommandLine(program, argc, argv);
		mode = GetC2mMode(program);
		state.LoadFile(program.get<std::string>("file"));

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

