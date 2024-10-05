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
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>

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
	program.add_argument("--src")
		.required()
		.nargs(1)
		.help("the source PE file");

	program.add_argument("-d", "--declaration")
		.default_value("")
		.nargs(1)
		.help("the clear declaration of C++ function/variable");

	program.add_argument("--file")
		.default_value("")
		.nargs(1)
		.help("use file to process multi-lined data");

	program.add_argument("--script")
		.default_value("")
		.nargs(1)
		.help("python script to process the input data and use custom output (used with --file)");

	program.add_argument("--va")
		.default_value(0x0)
		.nargs(1)
		.help("the function virtual address, used with --base option");

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

	if (program.is_used("--file") && program.is_used("--declaration"))
		throw std::exception{ "--file & --declaration can't be used together." };

	if (program.is_used("--file") && program.is_used("--va"))
		throw std::exception{ "--file & --va can't be used together." };

	if (program.is_used("--rva") && (program.is_used("--declaration") || program.is_used("--va")))
		throw std::exception{ "--rva & --declaration can't be used together." };

	if (program.is_used("--base"))
	{
		if (program.is_used("--va"))
			return VIRTUAL_ADDRESS;
		if (program.is_used("--file"))
			return FILE_VIRTUAL_ADDRESS;
		throw std::exception{ "--va: 1 argument(s) expected. 0 provided." };
	}

	if (program.is_used("--rva"))
	{
		if (program.is_used("--file"))
			return FILE_RVA;
		return RVA;
	}

	if (program.is_used("--declaration"))
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

bool skipLine = false;

PYBIND11_EMBEDDED_MODULE(c2m, m) {
	pybind11::class_<c2m::DeclarationDetails>(m, "declaration_details")
		.def(pybind11::init<bool, bool, bool, bool, std::string>())
		.def_readonly("c_function", &c2m::DeclarationDetails::CFunction)
		.def_readonly("variable", &c2m::DeclarationDetails::Variable)
		.def_readonly("constructor_function", &c2m::DeclarationDetails::ConstructorFunction)
		.def_readonly("destructor_function", &c2m::DeclarationDetails::DestructorFunction)
		.def_readonly("name", &c2m::DeclarationDetails::Name);


	pybind11::class_<c2m::Export>(m, "export")
		.def(pybind11::init<uintptr_t, uintptr_t, std::string, std::string, c2m::DeclarationDetails>())
		.def_readonly("ordinal", &c2m::Export::Ordinal)
		.def_readonly("rva", &c2m::Export::Rva)
		.def_readonly("mangled_declaration", &c2m::Export::MangledDeclaration)
		.def_readonly("clear_declaration", &c2m::Export::ClearDeclaration)
		.def_readonly("declaration_details", &c2m::Export::DeclarationDetails);
}

int main(int argc, char* argv[])
{
	std::filesystem::create_directory("cache");

	c2m::State state{};
	argparse::ArgumentParser program{ "clear2mangled" };
	_C2MMODE mode{ UNKNOWN };

	pybind11::scoped_interpreter guard{};
	pybind11::module_ c2mmodule{};
	pybind11::module_ pymodule{};
	pybind11::object inputFunction{};
	pybind11::object outputFunction{};

	try { 
		InitializeCommandLine(program, argc, argv);
		mode = GetC2mMode(program);

		bool useScriptOutput = false;

		if (program.is_used("--script"))
		{
			//if (!std::filesystem::exists(program.get<std::string>("--script")))
			//	throw std::exception{ "script file does not exist" };

			// load script
			pybind11::module_ sys = pybind11::module_::import("sys");
			sys.attr("path").attr("append")("./");
			c2mmodule = pybind11::module_::import("c2m");


			//std::println(std::cout, "{}", program.get<std::string>("--script"))
			pymodule = pybind11::module_::import((program.get<std::string>("--script")).c_str());

			pymodule.def("c2m_skipline", []() { skipLine = true; });

			if (!pybind11::hasattr(pymodule, "c2m_input"))
				throw std::exception{ "function c2m_input not found in script" };
			
			inputFunction = pymodule.attr("c2m_input");

			if (!pybind11::isinstance<pybind11::function>(inputFunction))
				throw std::exception{ "c2m_input is not a function" };

			if (pybind11::hasattr(pymodule, "c2m_output"))
			{
				outputFunction = pymodule.attr("c2m_output");
				if (!pybind11::isinstance<pybind11::function>(outputFunction))
					throw std::exception{ "c2m_output is not a function" };

				useScriptOutput = true;
			}
		}
	
		state.LoadFile(program.get<std::string>("--src"));


		std::vector<std::string> lines{};
		auto processLines = [&](std::function<void(const std::string&)> operation)
			{
				ReadFileLines(program.get<std::string>("--file"), lines);
				for (auto& line : lines)
				{
					skipLine = false;
					std::string result{ line };
					if (program.is_used("--script"))
						result = inputFunction(line).cast<std::string>();
					
					if (skipLine)
						continue;

					operation(result);
				}
			};
		std::function<void(c2m::Export*)> outputer;
		if (useScriptOutput)
			outputer = [&](c2m::Export* exp) 
			{ 
				pybind11::object dd = c2mmodule.attr("declaration_details")(exp->DeclarationDetails.CFunction,
																			exp->DeclarationDetails.Variable,
																			exp->DeclarationDetails.ConstructorFunction,
																			exp->DeclarationDetails.DestructorFunction,
																			exp->DeclarationDetails.Name);
				pybind11::object exportobj = c2mmodule.attr("export")(exp->Ordinal,
					exp->Rva,
					exp->MangledDeclaration,
					exp->ClearDeclaration,
					exp->DeclarationDetails);

				try { outputFunction(exportobj); }
				catch (const std::exception& err) { std::println(std::cerr, "{}", err.what()); return -1; }
				return 0;
			};

		switch (mode)
		{
		case DECLARATION:
			state.PrintMangledNameByClearDeclaration(program.get<std::string>("--declaration"));
			break;
		case VIRTUAL_ADDRESS:
			state.PrintMangledNameByAddress(program.get<uintptr_t>("--base"), std::stoll(program.get<std::string>("--va"), nullptr, 16));
			break;
		case RVA:
			state.PrintMangledNameByRVA(program.get<uintptr_t>("--rva"));
			break;
		case FILE_DECLARATION:
			processLines([&](const std::string& str)
				{						
					state.PrintMangledNameByClearDeclaration(str, outputer);
				});
			break;
		case FILE_VIRTUAL_ADDRESS:
			processLines([&](const std::string& str)
				{
					uintptr_t address = std::stoll(str, nullptr, 16);
					state.PrintMangledNameByAddress(program.get<uintptr_t>("--base"), address, outputer);
				});
			break;
		case FILE_RVA:
			processLines([&](const std::string& str)
				{
					uintptr_t rva = std::stoll(str, nullptr, 16);
					state.PrintMangledNameByRVA(rva, outputer);
				});

			ReadFileLines(program.get<std::string>("--file"), lines);
			for (auto& line : lines)
			{
					uintptr_t rva = std::stoll(line, nullptr, 16);
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

