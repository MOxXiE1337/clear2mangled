# clear2mangled

`clear2mangled` is a tool written in C++ that converts C++ symbol declarations copied from tools like Windbg into mangled symbol names. This tool uses the export table of PE files for conversion, aiming to simplify debugging and analysis.

## Features

- Convert C++ symbol declarations into mangled symbol names
- Use the export table of PE files for conversion

## Installation

1. Ensure you have Visual Studio and the relevant C++ development tools installed.
2. Clone this repository:

   ```bash
   git clone https://github.com/yourusername/clear2mangled.git
   cd clear2mangled
   ```

   Remember to update the submodule:

   ```bash
   git submodule update --init --recursive
   ```

3. Open the solution file (`clear2mangled.sln`) in Visual Studio and compile the project.

### Note

Please place `undname.exe` from the Visual Studio tools directory in the same folder as `clear2mangled.exe` to ensure the program runs correctly.

This program can only use fuzzy searching. For example, running the following command:

```bash
./clear2mangled.exe ./msvcp140.dll "std::basic_ios<char,std::char_traits<char> >::clear"
```

May produce output like this:

```bash
Debug:               std::basic_ios::clear
Name:                clear
CFunction:           NO
ConstructorFunction: NO
DestructorFunction:  NO

Ordinal Rva                     Type            Name
679     000000000000CCA0        C++ Function    ?clear@?$basic_ios@DU?$char_traits@D@std@@@std@@QAEXH_N@Z
+-----------------------------------------------void std::basic_ios<char,std::char_traits<char>>::clear(int,bool)

680     0000000000036F40        C++ Function    ?clear@?$basic_ios@DU?$char_traits@D@std@@@std@@QAEXI@Z
+-----------------------------------------------void std::basic_ios<char,std::char_traits<char>>::clear(unsigned int)

681     000000000000CCA0        C++ Function    ?clear@?$basic_ios@GU?$char_traits@G@std@@@std@@QAEXH_N@Z
+-----------------------------------------------void std::basic_ios<unsigned short,std::char_traits<unsigned short>>::clear(int,bool)

682     0000000000036F40        C++ Function    ?clear@?$basic_ios@GU?$char_traits@G@std@@@std@@QAEXI@Z
+-----------------------------------------------void std::basic_ios<unsigned short,std::char_traits<unsigned short>>::clear(unsigned int)

683     000000000000CCA0        C++ Function    ?clear@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAEXH_N@Z
+-----------------------------------------------void std::basic_ios<wchar_t,std::char_traits<wchar_t>>::clear(int,bool)

684     0000000000036F40        C++ Function    ?clear@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAEXI@Z
+-----------------------------------------------void std::basic_ios<wchar_t,std::char_traits<wchar_t>>::clear(unsigned int)

685     000000000000CCC0        C++ Function    ?clear@ios_base@std@@QAEXH@Z
+-----------------------------------------------void std::ios_base::clear(int)

686     000000000000CCE0        C++ Function    ?clear@ios_base@std@@QAEXH_N@Z
+-----------------------------------------------void std::ios_base::clear(int,bool)

687     000000000000CCC0        C++ Function    ?clear@ios_base@std@@QAEXI@Z
+-----------------------------------------------void std::ios_base::clear(unsigned int)
```

## Usage

```bash
clear2mangled [--help] [--version] [--base VAR] [--rva VAR] file declaration/va
```

- `--help`: Display help information.
- `--version`: Display version information.
- `--base VAR`: Set the DLL's ImageBase.
- `--rva VAR`: Specify the address to retrieve (relative virtual address).

## Examples

```bash
# Get the corresponding mangled symbol name using a symbol name copied from Windbg
clear2mangled ./msvcp140.dll 'std::basic_ostream<char,std::char_traits<char> >::basic_ostream<char,std::char_traits<char> >'

# Set the DLL's ImageBase and get the mangled symbol name at a specific address
clear2mangled --base 40000000 --rva 400317D0 ./msvcp140.dll

# Get the mangled symbol name using the relative virtual address
clear2mangled --rva 317D0 ./msvcp140.dll
```

The first command uses a symbol name copied from Windbg to get the corresponding mangled symbol name.

The second command sets the DLL's ImageBase to `40000000` and retrieves the mangled symbol name at the address `400317D0`.

The third command retrieves the mangled symbol name using the relative virtual address `317D0`.

## Screenshots

Shits generated from windbg
![image](https://github.com/user-attachments/assets/4d31913c-e488-4d76-8f3d-68e5e7fbe2db)

![image](https://github.com/user-attachments/assets/34d2fcc3-6b10-4abb-9124-1eee20fd835e)



## Contributing

Contributions of any form are welcome! If you find issues or have suggestions for improvements, please submit an issue or create a pull request directly.

1. Fork this repository
2. Create your feature branch (`git checkout -b feature-branch`)
3. Commit your changes (`git commit -m 'Add some feature'`)
4. Push to the branch (`git push origin feature-branch`)
5. Create a new Pull Request

## Contact

For questions or suggestions, please contact [938583253@qq.com](mailto:938583253@qq.com).
