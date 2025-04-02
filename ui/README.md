# **Wavy UI**

> [!IMPORTANT]
> 
> This is an isolated codebase of Wavy!!!
> 
> The UI is still in its infancy and will have very **BREAKING** changes.
> 

This will contain the UI architecture of Wavy as a whole.

> [!NOTE]
> 
> All global resources are to be placed in `resources/resources.qrc`
> 

## Dependencies  

To build and run **Wavy-UI**, ensure you have the following dependencies installed:  

### **Required Dependencies**  

| Dependency  | Description |
|------------|-------------|
| **Qt6** (_Core, Widgets_) | Required for GUI development. |
| **CMake** | Build system for configuring and compiling the project. |
| **GCC / Clang** | C++ compiler supporting C++20. |
| **pkg-config** | Helps find Qt and other libraries during compilation. |

### **Optional Dependencies**  

| Dependency  | Description |
|------------|-------------|
| **FFmpeg** (optional) | Required if integrating audio processing in the future. |

### **Installation**  

#### **Linux (Debian-based)**  
```sh
sudo apt update && sudo apt install qt6-base-dev cmake g++ pkg-config
```

#### **Arch Linux**  
```sh
sudo pacman -S qt6-base cmake gcc pkgconf
```

#### **MacOS (Homebrew)**  
```sh
brew install qt cmake pkg-config
```

## Building

```bash 
cmake -S . -B build 
cmake --build build/
```

To run the binary:

```bash 
./build/Wavy-UI
```
