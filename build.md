# 编译 XASM Community

1. 确保您安装了C语言编译器（如GCC、Clang、MSVC）。

2. 克隆XASM Community仓库。
    ```bash
    git clone https://github.com/XYSlime/xasm-community.git
    ```

3. 进入仓库目录，并执行编译命令。
    ```bash
    cd xasm-community
    # 下面以gcc为例子编译，换成其他C语言编译器可以自行修改命令
    gcc compile.c -o builds/compile -Ofast # 编译 XASM 编译器
    gcc xasm.c -o builds/xasm -Ofast # 编译 XASM 虚拟机
    # 你可以更改xasm.c和compile.c中的ARCH和VERSION来更改显示的版本号、系统名和架构
    # 例如:
    # const char* OS = "linux";
    # const char* ARCH = "amd64";
    # const char* VERSION = "a0.0.1_debug"; 
    ```

4. 编译完成后，您可以在builds目录下找到编译后的可执行文件。