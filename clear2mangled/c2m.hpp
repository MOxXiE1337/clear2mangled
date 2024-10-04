#pragma once

#include <regex>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace c2m
{
	struct DeclarationDetails 
	{
		bool CFunction;
		bool Variable;
		bool ConstructorFunction;
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
	public:
		void LoadFile(const std::filesystem::path& path);

		const Export* GetMangledNameByClearDeclaration(const std::string& declaration) noexcept;
		const Export* GetMangledNameByAddress(uintptr_t baseAddress, uintptr_t address) noexcept;
		const Export* GetMangledNameByRVA(uintptr_t rva) noexcept;
	};
}