# My Project

# 第一步 建仓/下载llama
# 方案一
# -----------------------------------------------------------------
# 1. 初始化当前目录为 Git 仓库
git init
# 2. 创建一个基础的 commit（submodule 要求主仓库必须有至少一次提交）
# 随便建个文件，或者把你现有的代码加进来
echo "# My Project" > README.md
git add README.md
git commit -m "Initial commit"
# 3. 添加 submodule
git submodule add https://github.com/ggerganov/llama.cpp.git
# 4. 更新并拉取 submodule 的代码
git submodule update --init --recursive

# 方案二
# -----------------------------------------------------------------
# 确保你当前在 ~/HP 目录下
git clone https://github.com/ggerganov/llama.cpp.git


# 第二步 编译
# 1. 补全git身份
# -----------------------------------------------------------------
# 配置全局 Git 身份
git config --global user.email "peng.hao@dinglicom.com"
git config --global user.name "haopeng024"
# 把刚才的 README 和子模块信息提交到主仓库
git add .
git commit -m "Initial commit with llama.cpp submodule"
# 2. 开始编译（针对 Jetson 开启 CUDA 加速）
# -----------------------------------------------------------------
# 进入子模块目录
cd llama.cpp
# 创建构建目录并进入
mkdir build && cd build
# 使用 CMake 配置，关键是要开启 CUDA 支持
# 注意：老版本叫 -DLLAMA_CUBLAS=ON，新版本已经改为 -DGGML_CUDA=ON
cmake .. -DGGML_CUDA=ON
# 开始编译（-j$(nproc) 表示用满 CPU 核心加速编译，Jetson上编译可能需要十几分钟到半小时）
make -j$(nproc)


# 第三步 下载测试模型
# 方法1
# 确保在 llama.cpp 目录下
# 例如下载 Qwen1.5-1.8B 的 q4_k_m 量化模型
python3 -m pip install huggingface_hub
python3 ./models/download-gguf.py Qwen/Qwen1.5-1.8B-Chat-GGUF --q-type q4_k_m
# 方法2
# 安装 python 依赖
pip3 install huggingface_hub
# 下载模型（文件大概 1GB 左右）
python3 ../models/download-gguf.py Qwen/Qwen1.5-1.8B-Chat-GGUF --q-type q4_k_m
# 配置环境变量
# 临时生效
export PATH="$HOME/.local/bin:$PATH"
# 永久生效
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
# 依赖
# ERROR: pip's dependency resolver ... httpcore 1.0.7 requires h11<0.15,>=0.13, but you have h11 0.16.0 ...
# 这是 Jetson 系统里预装的 TensorFlow 等包和新装的 huggingface_hub 之间有版本冲突，属于 Jetson 环境的常见现象。它不影响 huggingface_hub 下载模型的功能，不需要处理。
# 然后验证命令是否可用：
hf --help
# 下载模型
# 回到 build 目录附近，或者建一个 models 文件夹
mkdir -p ~/models
# 下载 Qwen2.5-Coder-3B 的 GGUF 量化模型（适合 Jetson 的尺寸）
hf download Qwen/Qwen2.5-Coder-3B-Instruct-GGUF qwen2.5-coder-3b-instruct-q4_k_m.gguf --local-dir ~/models
# No local file found. Retrying..
# '[Errno 101] Network is unreachable' thrown while requesting HEAD https://huggingface.co/Qwen/Qwen2.5-Coder-3B-Instruct-GGUF/resolve/main/qwen2.5-coder-3b-instruct-q4_k_m.gguf
# Network is unreachable 这个报错说明你的 Jetson 设备直接访问 Hugging Face 官网的网络通路被阻断了，这是国内网络环境下常见的情况。最直接的解决方法就是配置国内镜像站。
# 方案一：配置 hf-mirror.com 镜像（推荐，最省事） hf-mirror.com 是一个公益镜像站，专门为解决国内访问 Hugging Face 的网络问题而搭建。你可以通过设置环境变量，让 hf 命令自动走镜像站下载。
# 1. 临时设置（仅对当前终端窗口有效）
export HF_ENDPOINT=https://hf-mirror.com
# 设置完成后，在同一个终端窗口里，重新执行你之前的下载命令即可：
hf download Qwen/Qwen2.5-Coder-3B-Instruct-GGUF qwen2.5-coder-3b-instruct-q4_k_m.gguf --local-dir ~/models
# 2. 永久设置（推荐） 为了避免每次打开新终端都要重新设置，可以把环境变量写入 ~/.bashrc 文件中：
echo 'export HF_ENDPOINT=https://hf-mirror.com' >> ~/.bashrc
source ~/.bashrc
# 方案二：使用 wget 直接下载（最稳妥） 如果设置环境变量后 hf 命令仍然不稳定，可以直接用 wget 从镜像站下载。你只需要把原始链接里的 huggingface.co 替换为 hf-mirror.com 即可。
# 确保在 ~/models 目录下
cd ~/models
# 使用 wget 从镜像站下载
wget https://hf-mirror.com/Qwen/Qwen2.5-Coder-3B-Instruct-GGUF/resolve/main/qwen2.5-coder-3b-instruct-q4_k_m.gguf
# wget 支持断点续传，如果中断了重新执行相同命令即可。


# 第四步 运行
# -m 后面跟模型路径，-ngl 99 表示所有层都加载到 GPU 上
./bin/llama-cli -m Qwen1.5-1.8B-Chat-Q4_K_M.gguf -p "你好，你是谁？" -n 128 -ngl 99
# 请确保你还在 ~/HP/llama/llama.cpp/build 目录下，然后执行以下命令：
./bin/llama-cli -m ~/models/qwen2.5-coder-3b-instruct-q4_k_m.gguf -p "你好，请用C++写一个Hello World。" -n 256 -ngl 99

# qwen2.5-coder-3b-instruct-q4_k_m.gguf存放路径：/home/jetson/models



# 测试程序
# 第一步：清理错误文件，下载依赖头文件
cd ~/HP/llama/test
rm -f CMakeLists.txt
# 下载两个单头文件（如果这里因为网络失败，请看文末的备用方案）
wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
wget https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
# 第二步：生成极简且正确的 CMakeLists.txt
echo "cmake_minimum_required(VERSION 3.15)" > CMakeLists.txt
echo "project(AIAnalyzer)" >> CMakeLists.txt
echo "set(CMAKE_CXX_STANDARD 17)" >> CMakeLists.txt
echo "add_executable(AIAnalyzer main.cpp)" >> CMakeLists.txt
# 第三步 编译
cd ~/HP/llama/test/build
rm -rf *
cmake ..
make -j$(nproc)
# 第四步 以服务方式启动
# 先进入 build 目录
cd ~/HP/llama/llama.cpp/build
# 确认一下 llama-server 在不在
ls bin/llama-server
# 启动服务（注意要在 build 目录下运行，或者写全路径）
./bin/llama-server -m ~/models/qwen2.5-coder-3b-instruct-q4_k_m.gguf -c 4096 --host 0.0.0.0 --port 8080
# 第五步 执行测试程序
# 新终端中执行
cd ~/HP/llama/test/build
./AIAnalyzer

# 注：如果 ls bin/llama-server 提示找不到文件
cd ~/HP/llama/llama.cpp/build
cmake .. -DGGML_CUDA=ON
make -j$(nproc) llama-server

# 注：简洁的编译
# 命令行直接编译
g++ main.cpp -o AIAnalyzer -std=c++17 -pthread

THrnOOOu3xS9yTbHxqcxPPCFp1yxGu3MQchW