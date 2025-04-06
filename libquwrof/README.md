# libquwrof (Wavy Project)

libquwrof is the Qt interface of the **libwavy**.

It aims to integrate all the aspects of **libwavy** in a clean UI and is the UI architecture for Wavy-UI.

Some goals for this library include:

1. Easy UI abstraction for Wavy - UI 
2. Complete integration with most if not ALL aspects of **libwavy**
3. Make it decently UI configurable (ideally do NOT want to go down this hellhole)
4. Work as a practical and usable interface for most users (so should look pretty good and work damn well)
5. Have NO major ambitions for this library!

Why the 5th point?

The goal of the Wavy project is to provide the abstraction and painless experience of using **libwavy**. That is
in our opinion, the selling point of this repository. The UI is for people who do not want to whack their brains over how our API works and just want results.

Hence, the UI should just **WORK**. No unnecessary cool looking features in this library. Just a barebones Qt + libwavy impl with a neat schema

## Why the name?

Quwrof is actually how "Chrollo" is written in the HunterXHunter language!

As for why this library is named after him, he is always dripped out and has style.

This lib aspires to be like him stylistically.

**How is it pronounced?**: libchrollo

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

#### **Fedora**  
```sh
sudo dnf install qt6-qtbase-devel cmake gcc-c++ pkgconf-pkg-config
```

#### **macOS (Homebrew)**  
```sh
brew install qt cmake pkg-config
```
