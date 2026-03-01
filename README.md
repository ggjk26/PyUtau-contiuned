> [!CAUTION]
> 此项目基于OpenAI Codex生成，woekflow由Google Gemini完善。本人对于C++仅有有限基础，Cmake不保证构建成功。并且该项目处于极早期版本，而且任何代码仅基于OpenAI自带的源代码审核和codex的可用性测试，如果我有时间的话我会构建Release上传，尽量做到每个版本Bug最少化。
# PyUtau Continued

## 构建
该项目使用了Qt6，所以需要有Qt6的支持库为前提才能够构建该项目。
Qt6及其额外工具 msvc 2022
构建需要Visual Studio（包含“针对桌面C++开发”，CMake生成的“解决方案文件”，使用x64构建而不是Win32），在真正生成程序需要用到 Qt6 msvc 2022
```bash
cmake -S . -B build
cmake --build build
#以下在Qt6 MSVC 2022 64-bit环境下使用
C:\Qt\6.10.2\msvc2022_64\bin\windeployqt pyutau_continued.exe
```
Codex 生成的代码在MainWindow.cpp的实现仅为占位空指针，所以并没有任何可用功能。
