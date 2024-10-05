# clear2mangled

`clear2mangled` is a tool written in C++ that converts C++ symbol declarations copied from tools like Windbg into mangled symbol names. 

This tool uses the export table of PE files for conversion, aiming to simplify debugging and analysis.

## Features

- Convert C++ symbol declarations into mangled symbol names
- Use the export table of PE files for conversion

## Installation

1. Ensure you have Visual Studio and the relevant C++ development tools installed.
2. Ensure you have python installed in your PC
3. Clone this repository:

   ```bash
   git clone https://github.com/yourusername/clear2mangled.git
   cd clear2mangled
   ```

   Remember to update the submodule:

   ```bash
   git submodule update --init --recursive
   ```

4. Open the solution file (`clear2mangled.sln`) in Visual Studio, Select x64
5. Configure the python path (FUCK U PYTHON)

### Note

Please place `python(311).dll` from the python directory and `undname.exe` from the Visual Studio tools directory in the same folder as `clear2mangled.exe` to ensure the program runs correctly.

You need to configure the enviroment variable PYTHONHOME to your current python executable path to ensure the python can be loaded correctly.

This program can only use fuzzy searching. For example, running the following command:

```bash
./clear2mangled.exe --src ./msvcp140.dll -d "std::basic_ios<char,std::char_traits<char> >::clear"
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
./clear2mangled.exe [--help] [--version] --src VAR [--declaration VAR] [--file VAR] [--script VAR] [--va VAR] [--base VAR] [--rva VAR]
```

  `--src              the source PE file [required]`
  
  `-d, --declaration  the clear declaration of C++ function/variable`

  `--file             use file to process multi-lined data`
  
  `--script           python script to process the input data and use custom output (used with --file)`
  
  `--va               the function virtual address, used with --base option`
  
  `--base             the base address of the module`
  
  `--rva              the rva of the function/variable`
  
## Examples

### Process single-lined data
```bash
# Get the corresponding mangled symbol name using a symbol name copied from Windbg
./clear2mangled.exe --src ./msvcp140.dll 'std::basic_ostream<char,std::char_traits<char> >::basic_ostream<char,std::char_traits<char> >'

# Set the DLL's imagebase and get the mangled symbol name at a specific address
./clear2mangled.exe --src ./msvcp140.dll --base 40000000 --va 400317D0

# Get the mangled symbol name using the relative virtual address
./clear2mangled.exe --src ./msvcp140.dll --rva 317D0 
```

### Process multi-lined data
`example_declarations.txt:`
```
std::basic_istream<char,std::char_traits<char> >::tellg
std::basic_istream<char,std::char_traits<char> >::seekg
std::_Lockit::_Lockit
std::basic_ostream<char,std::char_traits<char> >::basic_ostream<char,std::char_traits<char> >
```
```bash
./clear2mangled.exe --src ./msvcp140.dll --file ./example_declarations.txt
```

`example_vas.txt:`
```
0429b690
0429a600
0428a520
0426f270
```
```bash
./clear2mangled.exe --src ./msvcp140.dll --base 04260000 --file ./example_declarations.txt
```

`example_rvas.txt:`
```
3b690
3a600
2a520
0f270
```
```bash
./clear2mangled.exe --src ./msvcp140.dll --rva --file ./example_declarations.txt
```

### Use python script to process complex data
`example.txt`:
```
{ 0x0429b690, GetProcAddress(LoadLibraryA("msvcp140"), "std::basic_istream<char,std::char_traits<char> >::tellg") },
{ 0x0429a600, GetProcAddress(LoadLibraryA("msvcp140"), "std::basic_istream<char,std::char_traits<char> >::seekg") },
{ 0x0428a520, GetProcAddress(LoadLibraryA("msvcp140"), "std::_Lockit::_Lockit") },
{ 0x0426f270, GetProcAddress(LoadLibraryA("msvcp140"), "std::basic_istream<char,std::char_traits<char> >::~basic_istream<char,std::char_traits<char> >") },
```

`example.py`:
```python
 # use regex library to process text data (each line)
import re

# clear2mangle will call this function to process each line (Must defined if --script option is enabled)
def c2m_input(text):
   return re.findall(r'0x\w+', text)[0][2:] # return the function virtual address
   return re.findall(r' ".+"', text)[0][2:-1] # return the function declaration ( please use --base option)

# clear2mangle will call this function for custom output (You can just don't define this function to use the default output)
def c2m_output(export):
   print(f"{hex(export.rva)}, {export.mangled_declaration}") # print the rva and the mangled declaration
```

```bash
./clear2mangled.exe --src ./msvcp140.dll --file ./example.txt --script example
```

`Because of the python module system please put the script in the same directory with clear2mangle executable`

### Python export class definition (in c2m module)
```python
class declaration_details:
   c_function
   constructor_function
   destructor_function
   variable
   name

class export:
   ordianl
   rva
   mangled_declaration
   clear_declaration
   declaration_details
```

## Screenshots
![image](https://github.com/user-attachments/assets/5eb794c8-5ac4-41f3-b48f-56035b7c419b)
![image](https://github.com/user-attachments/assets/66bcab8f-1689-448c-824c-dcc28a094554)
![image](https://github.com/user-attachments/assets/e399d5e3-713c-4eea-81d8-fcb2b70878ab)

## Contributing

Contributions of any form are welcome! If you find issues or have suggestions for improvements, please submit an issue or create a pull request directly.

1. Fork this repository
2. Create your feature branch (`git checkout -b feature-branch`)
3. Commit your changes (`git commit -m 'Add some feature'`)
4. Push to the branch (`git push origin feature-branch`)
5. Create a new Pull Request

## Contact

For questions or suggestions, please contact [938583253@qq.com](mailto:938583253@qq.com).
