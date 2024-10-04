#pragma once

#include <regex>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

#define COLOR_RED "\033[0m\033[1;31m"
#define COLOR_GREEN "\033[0m\033[1;32m"
#define COLOR_YELLOW "\033[1m\033[1;33m"
#define COLOR_BLUE "\033[0m\033[1;34m"
#define COLOR_MAGENTA "\033[0m\033[1;35m"
#define COLOR_CYAN "\033[0m\033[1;36m"
#define COLOR_END "\033[0m"

namespace c2m
{
	struct DeclarationDetails 
	{
		bool CFunction;
		bool Variable;
		bool ConstructorFunction; // useless
		bool DestructorFunction;

		std::string Name;
		std::vector<std::string> ParenthesesPairs;
	};

	struct Export
	{
		uintptr_t Ordinal;
		uintptr_t Rva;
		std::string MangledDeclaration;
		std::string ClearDeclaration;
		DeclarationDetails DeclarationDetails;
	};



	class State
	{
	private:
		std::filesystem::path m_filePath;
		std::filesystem::path m_fileName;
		std::filesystem::path m_cachePath;

		std::vector<Export> m_exports;
	private:
		// declaration processing
		std::string SimplifyDeclaration(const std::string& declaration);
	private:
		void GenerateCacheFile();
		void SaveToCacheFile();
		void LoadExportsFromPEFile();
		void LoadExportsFromCacheFile();
	private:
		void ParseDeclarationDetails(const std::string& declaration, DeclarationDetails& details);
		void PrintSearchTargetDetails(DeclarationDetails& details);
		void PrintExport(Export& exp, uintptr_t baseAddress);
	public:
		void LoadFile(const std::filesystem::path& path);

		void PrintMangledNameByClearDeclaration(const std::string& declaration) noexcept;
		void PrintMangledNameByAddress(uintptr_t baseAddress, uintptr_t address) noexcept;
		void PrintMangledNameByRVA(uintptr_t rva) noexcept;
	};
}