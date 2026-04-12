# v833_lv9_demos 项目说明

## 项目信息

- **项目名称**: v833_lv9_demos
- **目标平台**: 全志 V833 主控芯片
- **操作系统**: Tina Linux
- **Git 仓库**: https://github.com/albert585/v833_lv9_demos
- **LVGL 版本**: 9.4.0（作为 Git 子模块）
- **项目版本**: 1.0.0

## 项目概述

这是一个基于全志 V833 主控芯片的 LVGL 图形界面演示项目，运行在 Tina Linux 系统上。项目已从 LVGL 8.4.0 升级到 9.4.0 版本，构建图形界面，并集成了音频播放、视频播放、文件管理、设置、视觉小说引擎、游戏等功能模块。

**重要变更**:
- LVGL 版本从 8.4.0 升级到 9.4.0
- 移除了独立的 lv_drivers 子模块，驱动配置已集成到 lv_conf.h 中
- 视觉小说引擎已适配 LVGL 9.x API
- 部分代码可能需要进一步适配 LVGL 9.x 的 API 变更
- ✅ 内嵌 Lua 解释器，为插件系统铺路
  - 完整的 Lua 5.4 解释器集成
  - 支持标准 Lua 库（base、string、table、math、io、os、utf8）
  - 可通过 C API 扩展自定义功能
  - 提供简单的 Lua 脚本执行接口
- ✅ 音频系统优化
  - 使用 ALSA PCM 直接输出模式
  - ALSA Mixer 已启用（用于硬件音量控制）
  - 音频重采样（44100Hz、16位、立体声）
  - 新增 audio_ctrl 模块用于硬件音量控制
- ✅ FFmpeg 版本选择
  - 支持 FFmpeg 4.x 和 6.x 版本切换
  - 通过 CMake 参数 FFMPEG_VERSION 配置
- ⚠️ 视频播放器配置调整
  - 使用 ALSA PCM 直接输出
  - ALSA Mixer 已启用（用于硬件音量控制）
  - 新增 ff_player 独立播放器模块
- ✅ ALSA PCM 初始化问题已修复
  - 优化参数配置（44100Hz、立体声）
  - 智能设备选择机制
  - 设备回退机制
- ✅ 新增游戏功能
  - 集成 2048 小游戏（通过 lv_lib_100ask）
- ✅ 新增自定义组件
  - lv_text_clock 文字时钟组件

### 主要技术栈
- **GUI 框架**: LVGL 9.4.0（作为 Git 子模块）
- **构建系统**: CMake 3.10+
- **编程语言**: C (C99 标准), C++ (C17 标准)
- **脚本引擎**: Lua 5.4（内嵌）
- **目标平台**: ARM Linux (arm-unknown-linux-musleabihf)
- **显示驱动**: Linux Framebuffer (FBDEV)
- **输入设备**: EVDEV (触摸屏)
- **音频处理**: FFmpeg 4.x/6.x, ALSA
- **加密库**: OpenSSL
- **游戏引擎**: lv_lib_100ask (2048 游戏)

### 核心功能模块
- **多媒体播放器**: 
  - 音频播放：支持音频文件播放、进度控制、播放速度控制，基于 FFmpeg 和 ALSA PCM
  - 视频播放：支持视频文件播放，基础视频解码和显示功能
    - 音频支持已启用（但音频和视频独立播放）
    - ALSA PCM 直接输出
    - 音频重采样（44100Hz、16位、立体声）
    - 新增 ff_player 独立播放器模块
- **文件管理器**: 浏览和管理文件系统，支持文件选择事件
- **设置界面**: 系统配置和参数调整
- **视觉小说引擎**: 基于 JSON 配置的视觉小说系统，支持背景图、角色图、文本框等多媒体元素，支持网络图片资源
- **游戏系统**: 
  - 2048 小游戏（基于 lv_lib_100ask）
  - 支持触摸滑动操作
  - 实时分数显示
- **自定义组件**:
  - lv_text_clock 文字时钟（实时显示时间）
- **按键处理**: Home 键和 Power 键的特殊功能（双击 Home 切换前台/后台，Power 键控制睡眠/唤醒）
- **电源管理**: 支持浅睡眠和深睡眠模式，自动切换机制，CPU 频率控制
- **机器人模式**: 切换到机器人运行模式，关闭当前应用并启动机器人程序
- **Lua 插件系统**: 内嵌 Lua 解释器，支持动态脚本执行和功能扩展

## 架构设计

### 系统集成
项目采用模块化架构，各功能模块通过事件系统进行交互：
1. **主循环**（main.c）：负责 LVGL 初始化、设备初始化和主事件循环
2. **容器系统**（container.c/h）：提供主界面容器，管理各功能模块的显示/隐藏
3. **事件系统**（events.c/h）：统一处理所有用户交互事件，协调各模块切换
4. **功能模块**：各功能模块独立实现，通过事件回调与主系统集成
5. **Lua 解释器**（lua/）：内嵌 Lua 虚拟机，支持脚本执行和插件扩展
6. **自定义组件**（views/）：自定义 LVGL 组件库

### 显示和输入系统
- **显示初始化**（lv_linux_disp_init）：
  - 使用 `lv_linux_fbdev_create()` 创建 framebuffer 显示设备
  - 默认设备：`/dev/fb0`（可通过环境变量 `LV_LINUX_FBDEV_DEVICE` 配置）
  - 显示旋转：90 度（`LV_DISPLAY_ROTATION_90`）
  - DPI 设置：130（lv_conf.h 中配置）

- **触摸初始化**（lv_linux_touch_init）：
  - 使用 `lv_evdev_create()` 创建 EVDEV 输入设备
  - 默认设备：`/dev/input/event0`
  - 触摸屏校准：`lv_evdev_set_calibration(touch, 20, 860, 220, -120)`（已更新）

### 视觉小说引擎集成
视觉小说引擎作为独立模块集成到主系统中：
- 通过 `event_open_visual_novel` 事件启动，隐藏主容器并初始化引擎
- 通过 `event_close_visual_novel` 事件关闭，释放资源并显示主容器
- 使用 JSON 配置文件（`src/lib/virsual_novel/data/story.json`）定义故事内容
- 支持网络图片资源，实现动态内容加载
- 基于 LVGL 9.4.0 构建，支持最新的 LVGL API

### Lua 解释器集成
Lua 解释器作为内嵌模块集成到项目中：
- 完整的 Lua 5.4 虚拟机实现
- 支持标准库：base、string、table、math、io、os、utf8
- 静态链接为 liblua.a，不依赖外部 Lua 库
- 目标：为插件系统和动态功能扩展提供基础
- 位置：`src/lib/lua/`
- 提供 `run_lua_script()` 函数用于执行 Lua 脚本

### LVGL 9.x 升级说明
项目已从 LVGL 8.4.0 升级到 9.4.0，主要变更包括：
- **驱动配置集成**: lv_drv_conf.h 已移除，所有驱动配置现在集成到 lv_conf.h 中
- **API 变更**: LVGL 9.x 对 API 进行了大量重构，部分函数和结构体名称已改变
- **渲染引擎**: 默认使用软件渲染（LV_USE_DRAW_SW），性能优化
- **配置格式**: lv_conf.h 的配置格式与 8.x 版本有较大差异
- **兼容性**: 视觉小说引擎已适配 LVGL 9.x API，但其他模块可能需要进一步适配
- **子模块变更**: lv_drivers 不再作为独立子模块存在
- **视频播放配置**: 
  - 使用 ALSA PCM 直接输出
  - ALSA Mixer 已启用（用于硬件音量控制）

## 项目结构

```
v833_lv9_demos/
├── src/                      # 主程序源代码
│   ├── main.c               # 主程序入口
│   ├── main.h               # 主程序头文件
│   ├── test_alsa.c          # ALSA PCM 测试程序
│   └── lib/                 # 核心功能库
│       ├── audio.c/h        # 音频播放功能（ALSA PCM）
│       ├── audio_ctrl.c/h   # ALSA 音频硬件控制（音量、启用/禁用）
│       ├── button.c/h       # 按钮组件
│       ├── container.c/h    # 容器组件
│       ├── events.c/h       # 事件处理
│       ├── file_manager.c/h # 文件管理器
│       ├── player.c/h       # 播放器组件
│       ├── settings.c/h     # 设置界面
│       ├── ff_player.c/h    # FFmpeg 独立播放器（音视频）
│       ├── lua.c            # Lua 脚本执行器
│       ├── lua/             # Lua 解释器（内嵌）
│       │   ├── lapi.c/h     # Lua API
│       │   ├── lauxlib.c/h  # Lua 辅助库
│       │   ├── lbaselib.c   # 基础库
│       │   ├── lcode.h      # 代码生成
│       │   ├── lctype.h     # 类型处理
│       │   ├── ldebug.h     # 调试
│       │   ├── ldo.h        # 虚拟机操作
│       │   ├── lfunc.c/h    # 函数
│       │   ├── lgc.h        # 垃圾回收
│       │   ├── linit.c      # 初始化
│       │   ├── liolib.c     # I/O 库
│       │   ├── ljumptab.h   # 跳转表
│       │   ├── llex.h       # 词法分析
│       │   ├── llimits.h    # 限制
│       │   ├── lmathlib.c   # 数学库
│       │   ├── lmem.h       # 内存管理
│       │   ├── lobject.h    # 对象
│       │   ├── lopcodes.h   # 操作码
│       │   ├── lopnames.h   # 操作码名称
│       │   ├── loslib.c     # 操作系统库
│       │   ├── lparser.h    # 解析器
│       │   ├── lprefix.h    # 前缀
│       │   ├── lstate.c/h   # 状态
│       │   ├── lstring.c/h  # 字符串
│       │   ├── lstrlib.c    # 字符串库
│       │   ├── ltable.c/h   # 表
│       │   ├── ltm.h        # 元方法
│       │   ├── lua.h/hpp    # Lua 核心 API
│       │   ├── luaconf.h    # Lua 配置
│       │   ├── lualib.h     # Lua 标准库
│       │   ├── lundump.h    # 反汇编
│       │   ├── lutf8lib.c   # UTF-8 库
│       │   ├── lvm.c/h      # 虚拟机
│       │   └── lzio.h       # 输入输出
│       ├── views/           # 自定义 LVGL 组件
│       │   ├── lv_text_clock.c/h  # 文字时钟组件
│       │   └── ...         # 其他自定义组件
│       ├── lv_lib_100ask/   # 100ask 组件库
│       │   ├── src/         # 组件源码
│       │   │   └── lv_100ask_2048/  # 2048 游戏
│       │   ├── docs/        # 组件文档
│       │   └── examples/    # 示例代码
│       └── virsual_novel/   # 视觉小说引擎
│           ├── visual_novel_engine.c/h  # 核心引擎
│           ├── resource_manager.c/h    # 资源管理器
│           ├── data_parser.c/h         # JSON 数据解析器
│           ├── cJSON.c/h               # JSON 解析库
│           ├── simple_json.c/h         # 简单 JSON 实现
│           ├── README.md               # 引擎说明文档
│           └── data/                   # 故事数据目录
│               └── story.json          # 故事配置文件（"樱花季节的回忆"）
├── lvgl/                    # LVGL 源码（Git 子模块）
│   ├── src/                 # LVGL 核心源码
│   ├── examples/            # 示例代码
│   ├── demos/               # 演示程序
│   └── docs/                # 文档
├── config/                  # 不同设备的配置文件
│   ├── t01pro/             # T01Pro 设备配置
│   │   ├── lv_conf.h
│   │   └── lv_drv_conf.h
│   └── t3/                 # T3 设备配置
│       ├── lv_conf.h
│       └── lv_drv_conf.h
├── scripts/                 # 脚本文件
│   ├── switch_foreground   # 切换到前台脚本
│   └── switch_robot        # 切换到机器人模式脚本
├── include/                 # 头文件目录
│   └── sunxi_display2.h    # 显示驱动头文件
├── build/                   # 构建输出目录（.gitignore）
├── pack/                    # 打包目录（.gitignore）
├── CMakeLists.txt           # CMake 构建配置
├── user_cross_compile_setup.cmake  # 交叉编译工具链配置
├── lv_conf.h               # LVGL 配置文件
├── lv_conf.h.v8            # LVGL 8.x 配置备份
├── lv_conf.h.backup        # LVGL 配置备份
├── Makefile                # Makefile 构建配置
├── .gitignore              # Git 忽略配置
├── .gitmodules             # Git 子模块配置
├── cmake_install.cmake     # CMake 安装配置
├── LICENSE                 # 许可证文件
├── README.md               # 项目说明文档
├── COPYRIGHT               # 第三方库版权声明
├── AGENTS.md               # iFlow CLI 项目文档
└── IFLOW.md                # iFlow CLI 项目文档（.gitignore）
```

## 构建和运行

### 环境要求
- CMake 3.10 或更高版本
- ARM 交叉编译工具链: `/usr/arm-unknown-linux-musleabihf/`
- 依赖库: evdev, libdrm, gbm, libinput, freetype2, SDL2 (可选), FFmpeg 4.x/6.x, ALSA, OpenSSL, zlib

### 构建步骤

1. **克隆项目（包含子模块）**
   ```bash
   git clone --recursive https://github.com/albert585/v833_lv9_demos
   cd v833_lv9_demos
   ```

2. **配置 CMake 构建环境**
   ```bash
   # 使用默认 FFmpeg 版本（6.x）
   cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .
   
   # 使用 FFmpeg 4.x
   cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -DFFMPEG_VERSION=4 -B build -S .
   ```

3. **编译项目**
   ```bash
   make -C build -j$(nproc)
   ```

4. **运行程序（在目标设备上）**
   ```bash
   ./build/bin/lvglsim
   ```

### FFmpeg 版本选择

项目支持 FFmpeg 4.x 和 6.x 两个版本，通过 CMake 参数 `FFMPEG_VERSION` 配置：

| 版本 | 路径 | 特性 |
|------|------|------|
| 4.x | `/srv/ffmpeg-ssd` | 不包含 avdevice，强制使用 ALSA PCM 模式 |
| 6.x | `/srv/ffmpeg` | 包含完整功能，支持 avdevice |

**使用方法**：
```bash
# 使用 FFmpeg 6.x（默认）
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .

# 使用 FFmpeg 4.x
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -DFFMPEG_VERSION=4 -B build -S .
```

### 构建目标
- `lvglsim`: 主可执行文件，位于 `build/bin/` 目录
- `lua`: Lua 静态库，位于 `build/lib/` 目录
- `lvgl_linux`: 静态库（包含自定义 LVGL 扩展），位于 `build/lib/` 目录
- `lvgl`: LVGL 9.4.0 核心库
- `lv_lib_100ask`: 100ask 组件库（包含 2048 游戏）
- `run`: 构建并运行（仅用于本地测试）
- `clean-all`: 清理所有构建产物
- `all`: 构建所有目标（默认）

### 清理构建
```bash
make -C build clean
# 或
make -C build clean-all
```

## 视觉小说引擎

### 概述
视觉小说引擎是基于 LVGL 9.4.0 构建的独立模块，通过 JSON 配置文件管理图片路径和文字内容，实现灵活的视觉小说制作与展示。引擎支持本地文件路径和网络 URL 图片资源，无需重新编译即可更新故事内容。

### 主要功能
- **JSON 配置驱动**: 通过 JSON 文件定义每页的背景图、角色图、文字内容、文本框样式等
- **动态资源加载**: 支持本地文件路径和网络 URL 图片资源
- **页面管理**: 支持多页视觉小说内容切换，每页可包含多个角色
- **状态管理**: 引擎状态包括 IDLE、INIT、RUNNING、PAUSED、FINISHED
- **交互控制**: 支持点击或按键切换页面，实现故事推进
- **资源管理**: 引用计数机制管理资源生命周期，自动释放未使用资源

### JSON 配置格式
```json
{
  "title": "樱花季节的回忆",
  "author": "视觉小说开发者",
  "pages": [
    {
      "id": "page1",
      "background": "背景图片路径或URL",
      "characters": [
        {
          "id": "char1",
          "image": "角色图片路径或URL",
          "x": 200,
          "y": 250,
          "scale": 0.8,
          "visible": true
        }
      ],
      "text": "页面文字内容",
      "textbox": {
        "visible": true,
        "x": 50,
        "y": 400,
        "width": 700,
        "height": 150,
        "bg_color": "#000000",
        "text_color": "#FFFFFF",
        "font_size": 16
      },
      "next_page": "page2"
    }
  ]
}
```

**配置说明**：
- `title`: 故事标题
- `author`: 作者名称
- `pages`: 页面数组，每个页面包含：
  - `id`: 页面唯一标识符
  - `background`: 背景图片路径（支持本地路径或网络 URL）
  - `characters`: 角色数组，每个角色包含：
    - `id`: 角色唯一标识符
    - `image`: 角色图片路径（支持本地路径或网络 URL）
    - `x`, `y`: 角色位置坐标
    - `scale`: 角色缩放比例
    - `visible`: 角色可见性
  - `text`: 页面文字内容
  - `textbox`: 文本框配置
  - `next_page`: 下一页 ID（null 表示最后一页）

### API 接口
- `vn_engine_init(json_path)`: 初始化引擎，加载 JSON 配置
- `vn_engine_start()`: 启动视觉小说引擎
- `vn_engine_pause()`: 暂停引擎
- `vn_engine_resume()`: 恢复引擎
- `vn_engine_stop()`: 停止引擎
- `vn_engine_load_page(page_id)`: 加载指定页面
- `vn_engine_load_next_page()`: 加载下一页
- `vn_engine_get_state()`: 获取当前引擎状态
- `vn_engine_free_current_resources()`: 释放当前页面资源
- `vn_engine_deinit()`: 反初始化引擎，释放资源

### 使用示例
```c
// 打开视觉小说
event_open_visual_novel(e);

// 关闭视觉小说
event_close_visual_novel(e);
```

### 扩展功能（待实现）
- 多语言支持
- 音效支持
- 选择分支功能
- 动画效果
- 存档/读档功能
- 字体加载功能优化（当前使用 LVGL 默认字体）

## Lua 插件系统

### 概述
项目已内嵌完整的 Lua 5.4 解释器，为插件系统和动态功能扩展提供基础。Lua 虚拟机作为静态库（liblua.a）集成到项目中，不依赖外部 Lua 库。

### 主要功能
- **完整的 Lua 5.4 支持**：包含所有 Lua 5.4 标准库和特性
- **标准库支持**：
  - `base`: 基础库（print、tonumber、type 等）
  - `string`: 字符串操作库
  - `table`: 表操作库
  - `math`: 数学运算库
  - `io`: 文件 I/O 库
  - `os`: 操作系统库
  - `utf8`: UTF-8 字符串处理库
- **静态链接**：Lua 作为静态库链接，减少运行时依赖
- **C API 扩展**：可通过 C API 注册自定义函数和模块
- **脚本执行接口**：提供 `run_lua_script()` 函数用于执行 Lua 脚本文件

### API 接口

#### Lua 脚本执行
```c
/**
 * 执行 Lua 脚本文件
 * @param filename Lua 脚本文件路径
 */
void run_lua_script(const char *filename);
```

### 使用示例
```c
// 执行 Lua 脚本
run_lua_script("/path/to/script.lua");
```

### 版权信息
- **版权所有**: Copyright (C) 1994-2025 Lua.org, PUC-Rio
- **作者**: R. Ierusalimschy, L. H. de Figueiredo, W. Celes
- **许可证**: MIT License
- **详细信息**: 参见项目根目录下的 COPYRIGHT 文件

### 开发约定
- **Lua 版本**: 5.4
- **构建方式**: 静态库（liblua.a）
- **头文件位置**: `src/lib/lua/`
- **源文件位置**: `src/lib/lua/*.c`
- **CMake 目标**: `lua`

### 扩展指南（待实现）
- 如何在 C 代码中嵌入 Lua 脚本
- 如何注册自定义 C 函数供 Lua 调用
- 如何从 Lua 调用 LVGL API
- 插件加载和管理机制
- Lua 脚本热更新支持

## 游戏系统

### 2048 游戏

#### 概述
项目集成了基于 LVGL 的 2048 小游戏，通过 lv_lib_100ask 组件库提供。

#### 主要功能
- **经典 2048 玩法**：通过滑动合并数字方块
- **触摸支持**：支持触摸屏滑动操作
- **实时分数**：显示当前得分和最高方块
- **游戏状态**：检测游戏结束状态
- **重新开始**：支持随时开始新游戏

#### API 接口
```c
// 创建 2048 游戏
lv_obj_t * lv_100ask_2048_create(lv_obj_t * parent);

// 开始新游戏
void lv_100ask_2048_set_new_game(lv_obj_t * obj);

// 获取当前分数
uint16_t lv_100ask_2048_get_score(lv_obj_t * obj);

// 获取最高方块
uint16_t lv_100ask_2048_get_best_tile(lv_obj_t * obj);

// 获取游戏状态（是否结束）
bool lv_100ask_2048_get_status(lv_obj_t * obj);
```

#### 使用示例
```c
// 打开 2048 游戏
event_open_2048(e);

// 关闭 2048 游戏
event_close_2048(e);
```

#### 配置选项
在 `lv_lib_100ask/lv_lib_100ask_conf.h` 中配置：
- `LV_USE_100ASK_2048`: 启用/禁用 2048 游戏（默认：1）
- `LV_100ASK_2048_MATRIX_SIZE`: 矩阵大小（默认：4）
- `LV_100ASK_2048_SIMPLE_TEST`: 启用简单测试（默认：0）

## 自定义组件

### lv_text_clock 文字时钟

#### 概述
实时显示时间的 LVGL 文字标签组件，使用定时器自动更新。

#### 主要功能
- **实时时间显示**：显示当前系统时间
- **自动更新**：使用 LVGL 定时器每秒更新
- **灵活配置**：支持自定义样式和格式

#### API 接口
```c
// 创建文字时钟
lv_obj_t * lv_text_clock_create(lv_obj_t * parent);
```

#### 使用示例
```c
// 创建文字时钟
lv_obj_t * clock = lv_text_clock_create(parent);

// 设置样式（可选）
lv_obj_set_style_text_font(clock, &lv_font_montserrat_24, 0);
lv_obj_set_style_text_color(clock, lv_color_hex(0xFFFFFF), 0);
```

## 音频控制模块

### 概述
audio_ctrl 模块提供 ALSA 音频硬件控制功能，用于管理音频设备的启用/禁用和音量控制。

### API 接口
```c
// 启用音频设备
void audio_enable(void);

// 禁用音频设备
void audio_disable(void);

// 初始化音频设备
int audio_init(void);

// 设置音量（0-100）
int audio_volume_set(int percent);

// 获取当前音量（0-100）
int audio_volume_get(void);
```

### 使用示例
```c
// 初始化音频
if (audio_init() == 0) {
    // 设置音量为 50%
    audio_volume_set(50);
    
    // 获取当前音量
    int volume = audio_volume_get();
    printf("Current volume: %d%%\n", volume);
}
```

## FFmpeg 独立播放器

### 概述
ff_player 是基于 FFmpeg 的独立音视频播放器模块，支持音频和视频文件播放，使用 ALSA PCM 输出音频。

### 主要功能
- **音频播放**：支持多种音频格式（MP3, AAC, FLAC 等）
- **视频播放**：支持多种视频格式（MP4, AVI, MKV 等）
- **播放控制**：播放、暂停、停止、跳转
- **进度查询**：获取当前播放位置和总时长
- **多线程**：使用独立线程播放，不阻塞主线程
- **ALSA 输出**：使用 ALSA PCM 直接输出音频

### API 接口
```c
// 创建播放器
ff_player_t * player_create();

// 打开文件
int player_open(ff_player_t * player, const char * filename);

// 初始化音频
int player_init_audio(ff_player_t * player);

// 初始化视频
int player_init_video(ff_player_t * player, lv_obj_t * lv_obj);

// 播放控制
int player_play(ff_player_t * player);
int player_pause(ff_player_t * player);
int player_resume(ff_player_t * player);
int player_stop(ff_player_t * player);

// 跳转（按百分比）
int player_seek_pct(ff_player_t * player, double percent);

// 跳转（按毫秒）
int player_seek_ms(ff_player_t * player, int64_t target_ms);

// 获取位置（毫秒）
int64_t player_get_position_ms(ff_player_t * player);

// 获取时长（毫秒）
int64_t player_get_duration_ms(ff_player_t * player);

// 获取位置（百分比）
double player_get_position_pct(ff_player_t * player);

// 获取状态
player_state_t player_get_state(ff_player_t * player);

// 设置播放完成回调
void player_set_finish_callback(ff_player_t * player, void (*func_ptr)(ff_player_t));

// 销毁播放器
void player_destroy(ff_player_t * player);
```

### 使用示例
```c
// 创建播放器
ff_player_t * player = player_create();

// 打开文件
if (player_open(player, "path/to/video.mp4") == 0) {
    // 初始化视频
    player_init_video(player, video_obj);
    
    // 播放
    player_play(player);
    
    // 暂停
    player_pause(player);
    
    // 跳转到 50%
    player_seek_pct(player, 0.5);
    
    // 获取进度
    double progress = player_get_position_pct(player);
    printf("Progress: %.1f%%\n", progress * 100);
}

// 销毁播放器
player_destroy(player);
```

## 开发约定

### 代码风格
- 使用 C99 标准编写 C 代码
- 使用 C17 标准编写 C++ 代码
- 编译选项包含 `-Wall -Wextra -Wpedantic -g -O2` 以启用严格警告和优化
- 函数和变量命名采用小写加下划线的方式（snake_case）

### 目录组织
- `src/main.c`: 主程序入口，负责 LVGL 初始化、显示和输入设备设置、主循环
- `src/lib/`: 所有功能模块的源代码和头文件
- 每个功能模块成对出现 `.c` 和 `.h` 文件

### 模块化设计
- **audio**: 封装 FFmpeg 音频解码和 ALSA PCM 播放功能，支持播放速度控制
  - API: `audio_player_init`, `audio_player_open`, `audio_player_play`, `audio_player_pause`, `audio_player_stop`, `audio_player_set_volume`, `audio_player_set_position`, `audio_player_set_speed`, `audio_player_deinit`
  - 额外 API: `audio_player_get_position`, `audio_player_get_duration`
  - 使用 ALSA PCM 直接输出（snd_pcm_writei）
  - 使用 ALSA Mixer 控制硬件音量（0-100 范围）
- **audio_ctrl**: ALSA 音频硬件控制模块
  - API: `audio_enable`, `audio_disable`, `audio_init`, `audio_volume_set`, `audio_volume_get`
  - 用于管理音频设备的启用/禁用和音量控制
- **ff_player**: 基于 FFmpeg 的独立音视频播放器
  - API: `player_create`, `player_open`, `player_play`, `player_pause`, `player_resume`, `player_stop`, `player_seek_pct`, `player_seek_ms`, `player_get_position_ms`, `player_get_duration_ms`, `player_get_position_pct`, `player_get_state`, `player_destroy`
  - 支持音频和视频播放
  - 使用 ALSA PCM 输出音频
  - 使用多线程播放
- **player**: 基于 audio 模块构建的高级播放器 UI 组件，包含进度条、音量控制等
  - API: `player_create`, `player_set_file`, `player_toggle_play_pause`, `player_stop`, `player_get_state`, `player_get_position_pct`, `player_destroy`
  - 额外 API: `player_preinit_alsa`, `player_destroy_callback`
- **file_manager**: 文件浏览和管理功能，支持文件选择事件
- **settings**: 系统设置界面
- **button/container/events**: UI 组件和事件处理系统
- **views**: 自定义 LVGL 组件库
  - **lv_text_clock**: 文字时钟组件
  - API: `lv_text_clock_create`
- **virsual_novel**: 独立的视觉小说引擎模块，包含：
  - **visual_novel_engine**: 核心引擎，管理页面切换和渲染
  - **resource_manager**: 资源加载和管理，支持引用计数
  - **data_parser**: JSON 配置文件解析
  - **cJSON**: JSON 解析库
  - **simple_json**: 简单 JSON 实现
- **lua**: Lua 5.4 解释器，包含：
  - 核心 API: `lua.h`, `luaconf.h`
  - 辅助库: `lauxlib.h`
  - 标准库: `lualib.h`
  - 标准库实现: `lbaselib.c`, `lstrlib.c`, `ltable.c`, `lmathlib.c`, `liolib.c`, `loslib.c`, `lutf8lib.c`
  - 虚拟机: `lvm.c`, `lstate.c`, `ldo.h`, `ldebug.h`
  - 垃圾回收: `lgc.h`
  - 编译器: `lparser.h`, `lcode.h`, `llex.h`
  - **lua.c**: Lua 脚本执行器
    - API: `run_lua_script`
- **lv_lib_100ask**: 100ask 组件库
  - **lv_100ask_2048**: 2048 小游戏
  - API: `lv_100ask_2048_create`, `lv_100ask_2048_set_new_game`, `lv_100ask_2048_get_score`, `lv_100ask_2048_get_best_tile`, `lv_100ask_2048_get_status`
- **lv_ffmpeg** (LVGL 扩展): 视频播放器组件
  - 基础 API: `lv_ffmpeg_player_create`, `lv_ffmpeg_player_set_src`, `lv_ffmpeg_player_set_cmd`, `lv_ffmpeg_player_set_auto_restart`
  - 音频解码: 使用 FFmpeg 解码音频流，支持音频重采样到 44100Hz、16位、立体声
  - 音频输出: ALSA PCM 直接输出（snd_pcm_writei）

### 硬件相关代码
- 显示设备: `/dev/fb0` (framebuffer), `/dev/disp` (display 控制器)
- 输入设备: `/dev/input/event0` (触摸屏), `/dev/input/event1` (电源键), `/dev/input/event2` (Home 键)
- 触摸屏校准参数: x=20, y=860, x_max=220, y_max=-120（已更新）
- 显示旋转: 90 度（`LV_DISPLAY_ROTATION_90`）
- DPI 设置: 130

### 事件系统
系统事件处理模块（events.c/h）提供以下功能：
- **event_open_manager**: 打开文件管理器
- **event_close_manager**: 关闭文件管理器和设置界面
- **event_open_settings**: 打开设置界面
- **btn_robot_click**: 切换到机器人运行模式
- **file_select_event**: 文件选择事件处理
- **slider_event_cb**: 滑块事件回调
- **event_open_visual_novel**: 打开视觉小说引擎
- **event_close_visual_novel**: 关闭视觉小说引擎
- **event_open_2048**: 打开 2048 游戏
- **event_close_2048**: 关闭 2048 游戏
- **event_play_video**: 播放视频
- **event_close_player**: 关闭播放器
- **event_audio_test**: 音频测试
- **player_destroy_callback**: 播放器销毁回调

### 电源管理
- **浅睡眠**: 关闭 LCD 和触摸屏，60 秒后自动进入深睡眠
- **深睡眠**: 写入 `/sys/power/state` 进入内存睡眠模式，按电源键可唤醒
- **唤醒**: 通过 Power 键唤醒，重新打开 LCD 和触摸屏
- **后台模式**: 双击 Home 键进入后台，5 分钟后自动切回前台
- **CPU 频率控制**: 使用 CPU governor 控制功耗（powersave 模式）

### 机器人模式
- **switchRobot()**: 切换到机器人运行模式
  - 切换到后台模式
  - 关闭显示和输入设备
  - 终止 WiFi 连接（wpa_supplicant）
  - 启动机器人程序（robot_run_1）
- **switchForeground()**: 从后台切回前台
  - 切换到原工作目录
  - 执行返回脚本（/mnt/app/switch_foreground）
- 主程序启动时会自动终止现有的 robotd 和 robot_run_1 进程

### 脚本说明
- **scripts/switch_foreground**: 切换到前台脚本
  - 停止 robotd 和 robot_run_1 进程
  - 停止 lvglsim 进程
  - 启动 lvglsim 应用
- **scripts/switch_robot**: 切换到机器人模式脚本
  - 根据 /mnt/app/robot_select.txt 选择启动机器人程序
  - 支持 robot_run_1 和 robot_run 两种模式

### 依赖库路径
- 头文件路径:
  - `/srv/alsa/include`: ALSA 音频库头文件
  - `/srv/ffmpeg/include`: FFmpeg 6.x 音视频编解码库头文件
  - `/srv/ffmpeg-ssd/include`: FFmpeg 4.x 音视频编解码库头文件
  - `/srv/zlib-armv7/include`: zlib 压缩库头文件
- 库文件路径:
  - `/srv/evdev/lib`: Linux 输入设备事件库
  - `/srv/openssl/lib`: OpenSSL 加密库
  - `/srv/zlib/lib`: zlib 压缩库
  - `/srv/ffmpeg/lib`: FFmpeg 6.x 库（avcodec, avutil, avformat, swscale, swresample, avdevice）
  - `/srv/ffmpeg-ssd/lib`: FFmpeg 4.x 库（avcodec, avutil, avformat, swscale, swresample）
  - `/srv/alsa/lib`: ALSA 音频库

### 链接库
编译时会链接以下库：
- evdev: 输入设备事件处理
- ssl, crypto: OpenSSL 加密功能
- avcodec, avutil, avformat, swscale, swresample, avdevice: FFmpeg 音视频编解码和设备支持（6.x）
- avcodec, avutil, avformat, swscale, swresample: FFmpeg 音视频编解码支持（4.x，不包含 avdevice）
- m: 数学库
- z: zlib 压缩库
- pthread: 多线程支持
- asound: ALSA 音频库
- g2d: 2D 图形加速（如果启用 CONFIG_LV_USE_DRAW_G2D）

## 配置文件

### LVGL 配置 (lv_conf.h)
- 颜色深度: 32-bit (XRGB8888)
- 内存池大小: 5MB
- 标准库包装: 使用 LVGL 内置实现（LV_STDLIB_BUILTIN）
- 默认刷新率: 33ms
- 默认 DPI: 130
- 渲染引擎: 软件渲染（LV_USE_DRAW_SW）
- 文本编码: UTF-8
- 支持的 LVGL 组件: 大部分基础组件已启用
- FFmpeg 支持: 已启用（LV_USE_FFMPEG = 1）
- FFmpeg 音频支持: 已启用
  - 支持音频解码和播放
  - 使用 ALSA PCM 直接输出（snd_pcm_writei）
  - 使用 ALSA Mixer 控制硬件音量（0-100 范围）
- 文件系统: POSIX 文件系统支持已启用（LV_USE_FS_POSIX = 1）
- 注意: LVGL 9.x 不再使用 lv_drv_conf.h，驱动配置已集成到主配置文件中

### CMake 配置 (CMakeLists.txt)
- **FFMPEG_VERSION**: FFmpeg 版本选择（"4" 或 "6"，默认："6"）
- **CMAKE_BUILD_TYPE**: 构建类型（Debug 或 Release，默认：Release）
  - Debug: 无优化，保留调试符号
  - Release: 启用 O2 优化
- **CROSS_COMPILE_INCLUDE_DIRS**: 交叉编译头文件路径
- **FFMPEG_INCLUDE_DIR**: FFmpeg 头文件路径（根据版本自动选择）
- **FFMPEG_LIB_DIR**: FFmpeg 库文件路径（根据版本自动选择）
- **FFMPEG_LIBS**: FFmpeg 链接库（根据版本自动选择）

### 可选后端支持
CMakeLists.txt 支持多种后端（通过 CONFIG_LV_USE_* 宏控制）：
- **EVDEV**: Linux 输入设备事件支持（已启用）
- **Linux FBDEV**: Linux Framebuffer 显示支持（已启用）
- **Linux DRM**: Direct Rendering Manager 显示支持
- **GBM**: Generic Buffer Manager 支持
- **libinput**: 高级输入设备支持
- **Freetype**: 字体渲染支持
- **SDL2**: 跨平台图形和输入支持
- **Wayland**: Wayland 显示协议支持
- **X11**: X Window System 支持
- **OpenGLES**: OpenGL ES 加速支持
- **G2D**: NXP G2D 硬件加速支持

### 设备配置
- `config/t01pro/`: T01Pro 设备的 LVGL 和驱动配置
- `config/t3/`: T3 设备的 LVGL 和驱动配置

## 测试和调试

### ALSA PCM 测试
- **测试程序**: `src/test_alsa.c`（编译到 `build/bin/test_alsa`）
  - 测试多个 ALSA 设备（plughw:0,0, default, hw:0,0）
  - 验证不同的参数组合
  - 详细的错误输出

### 视觉小说引擎测试
- **README.md**: 视觉小说引擎说明文档（位于 `src/lib/virsual_novel/README.md`）
- **数据文件**: `src/lib/virsual_novel/data/story.json` 包含示例故事（"樱花季节的回忆"）
  - 支持 3 页内容，包含背景图、角色图和文本框
  - 使用网络图片资源（字节跳动 CDN）
  - 角色包含位置、缩放和可见性配置

### 2048 游戏测试
- **游戏组件**: `lv_lib_100ask/src/lv_100ask_2048/`
- **示例代码**: `lv_lib_100ask/examples/lv_100ask_2048/lv_100ask_example_2048.c`
- **文档**: `lv_lib_100ask/src/lv_100ask_2048/README_zh.md`

### 调试输出
- 主程序包含关键操作的 printf 输出：
  - `[key]home_up/home_down`: Home 键事件
  - `[lcd]opened/closed`: LCD 显示开关
  - `[tp]opened/closed`: 触摸屏开关
  - `display OK!`: 显示初始化成功
  - `init OK`: 系统初始化完成

### 构建调试
- 编译选项包含 `-Wall -Wextra -Wpedantic -g -O2` 以启用严格警告和调试信息
- 可通过修改 CMakeLists.txt 中的 `set(CMAKE_BUILD_TYPE Debug)` 启用调试模式
- Debug 模式使用 `-g -O0` 编译选项，保留完整调试信息

## 开发工作流

### 修改代码后重新编译
```bash
# 清理并重新编译
make -C build clean
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .
make -C build -j$(nproc)

# 使用 FFmpeg 4.x 重新编译
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -DFFMPEG_VERSION=4 -B build -S .
make -C build -j$(nproc)

# 使用 Debug 模式重新编译
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -DCMAKE_BUILD_TYPE=Debug -B build -S .
make -C build -j$(nproc)
```

### 部署到目标设备
```bash
# 将可执行文件传输到目标设备
scp build/bin/lvglsim user@device:/path/to/destination/
scp build/bin/test_alsa user@device:/path/to/destination/

# 在目标设备上运行
ssh user@device
./path/to/destination/test_alsa
./path/to/destination/lvglsim
```

### Git 工作流
```bash
# 拉取最新代码（包含子模块）
git pull --recurse-submodules

# 更新子模块
git submodule update --remote

# 添加新文件
git add src/lib/new_module.c
git add src/lib/new_module.h

# 提交更改
git commit -m "Add new module feature"

# 推送到远程仓库
git push
```

## 注意事项

1. **交叉编译**: 项目使用 musl libc 的 ARM 工具链，确保工具链路径正确
2. **Git 子模块**: LVGL 作为子模块包含，首次克隆时使用 `--recursive` 参数
3. **硬件依赖**: 程序依赖特定的硬件设备节点，只能在目标 V833 设备上运行
4. **显示旋转**: 默认显示旋转 90 度
5. **触摸校准**: 触摸屏坐标需要校准以匹配显示旋转后的坐标（已更新校准参数）
6. **网络资源**: 视觉小说引擎支持网络图片资源，需要设备有网络连接
7. **进程管理**: 主程序启动时会终止现有的 robotd 和 robot_run_1 进程
8. **电源管理**: 深睡眠模式需要 RTC 唤醒支持，写入 `/sys/class/rtc/rtc0/wakealarm`
9. **内存限制**: LVGL 内存池大小为 5MB，注意内存使用
10. **编译警告**: 项目使用严格编译选项（-Wall -Wextra -Wpedantic -g -O2），确保代码质量
11. **LVGL 版本**: 当前使用 LVGL 9.4.0，注意 API 兼容性（与 8.x 版本有重大变更）
12. **颜色深度**: 使用 32-bit 颜色深度（XRGB8888）
13. **驱动配置**: LVGL 9.x 不再使用独立的 lv_drv_conf.h，驱动配置已集成到 lv_conf.h 中
14. **音频系统**: 使用 ALSA PCM 和 ALSA Mixer 进行音频播放和硬件音量控制
15. **音频设备**: 确保目标设备正确配置 ALSA 设备（`aplay -l` 检查）
16. **FFmpeg 版本**: 支持 FFmpeg 4.x 和 6.x，通过 CMake 参数选择
17. **配置切换**: 修改 lv_conf.h 中的配置宏后需要重新编译整个项目
18. **.gitignore**: build/、pack/ 和 IFLOW.md 目录已被忽略，不会被提交到 Git 仓库
19. **Lua 集成**: Lua 5.4 解释器已内嵌，为插件系统提供基础
20. **版权声明**: 第三方库的版权信息在 COPYRIGHT 文件中，使用这些库时需遵守相应许可证

## 已知问题

- 视觉小说引擎的扩展功能（多语言、音效、选择分支、动画效果、存档/读档）尚未实现
- 文件选择事件处理逻辑（file_select_event）中的 TODO 尚未完成
- 字体加载功能尚未完全实现，当前使用 LVGL 默认字体
- LVGL 9.x API 与 8.x 有重大变更，部分代码可能需要进一步适配
- Lua 插件系统尚未完全实现，当前仅内嵌了 Lua 解释器
- FFmpeg 4.x 版本不支持 avdevice，功能受限

## 未来计划

### 短期目标
- 完成文件选择事件处理逻辑
- 实现视觉小说引擎的存档/读档功能
- 添加音效支持
- 优化内存使用
- 适配 LVGL 9.x API 变更
- 实现 Lua 插件系统的基础框架
- 完善 ff_player 播放器功能

### 中期目标
- 实现视觉小说引擎的选择分支功能
- 添加动画效果支持
- 实现多语言支持
- 优化图片加载性能
- 利用 LVGL 9.x 新特性优化渲染性能
- 开发 Lua 脚本示例和文档
- 添加更多自定义组件

### 长期目标
- 实现完整的 Lua 插件系统
  - 插件加载和卸载机制
  - Lua 脚本热更新
  - C API 扩展指南
  - 插件市场（可选）
- 完善视频播放器功能
  - 重新启用 ALSA Mixer（解决冲突问题）
  - 添加更多视频格式支持
  - 实现视频加速播放
- 实现网络流媒体播放
- 添加更多 UI 组件和主题
- 支持更多硬件平台
- 升级到 LVGL 9.x 最新版本
- 添加更多游戏功能

## 贡献指南

欢迎贡献代码和改进建议！请遵循以下指南：

1. **代码风格**: 遵循项目现有的代码风格（C99 标准，snake_case 命名）
2. **提交信息**: 使用清晰简洁的提交信息，说明更改的目的
3. **测试**: 在提交前确保代码在目标设备上正常工作
4. **文档**: 更新相关文档和注释
5. **问题报告**: 使用 GitHub Issues 报告 bug 和提出功能请求
6. **版权声明**: 使用第三方库时，请在 COPYRIGHT 文件中添加相应的版权信息

## 视频播放功能

### 概述
项目支持视频文件播放功能，基于 FFmpeg 进行视频解码和渲染。

### 配置选项

#### 启用/禁用音频支持
在 `lv_conf.h` 文件中配置：

### 技术实现

#### 音频处理流程
1. **音频流检测**：在打开视频文件时自动检测是否包含音频流
2. **音频解码**：使用 FFmpeg 解码音频数据
3. **音频重采样**：将音频重采样到 44100Hz、16位、立体声格式
4. **音频输出**：通过 ALSA PCM 直接输出（snd_pcm_writei）

#### 核心功能
- **音频流解码**：支持多种音频格式（MP3, AAC, FLAC 等）
- **音频重采样**：自动转换到标准输出格式（44100Hz、16位、立体声）
- **ALSA PCM 输出**：通过 `snd_pcm_writei` 直接写入 PCM 设备
- **ALSA Mixer 音量控制**：使用 ALSA Mixer 控制硬件音量（0-100 范围）
- **资源管理**：完善的 ALSA 资源清理机制（PCM、Mixer、互斥锁）

### API 接口

#### 视频播放器基础 API
```c
// 创建视频播放器
lv_obj_t *lv_ffmpeg_player_create(lv_obj_t *parent);

// 设置视频文件
lv_result_t lv_ffmpeg_player_set_src(lv_obj_t *obj, const char *path);

// 控制播放
void lv_ffmpeg_player_set_cmd(lv_obj_t *obj, lv_ffmpeg_player_cmd_t cmd);
// cmd 值: LV_FFMPEG_PLAYER_CMD_START, STOP, PAUSE, RESUME

// 设置自动重播
void lv_ffmpeg_player_set_auto_restart(lv_obj_t *obj, bool en);
```

### ALSA PCM 配置

#### 音频参数配置
当前 ALSA PCM 使用以下标准参数（已在 lvgl/src/libs/ffmpeg/lv_ffmpeg.c 中配置）：
- **采样率**: 44100 Hz（标准音频采样率）
- **通道数**: 2（立体声）
- **采样格式**: S16_LE（16位小端有符号整数）
- **周期大小**: 2048 frames（标准周期大小）
- **缓冲区大小**: 8192 frames（4倍周期，平滑播放）

#### ALSA 设备选择
代码实现了智能设备选择机制（在 `ffmpeg_audio_pcm_init` 函数中）：
1. **首选设备**: `plughw:0,0` - 自动格式转换的硬件设备
2. **备选设备**: `default` - 系统默认设备

这种设计确保在不同 ALSA 配置下都能正常工作。

### 使用示例

#### 基础视频播放
```c
// 创建视频播放器
lv_obj_t *player = lv_ffmpeg_player_create(parent);

// 设置视频文件
lv_ffmpeg_player_set_src(player, "path/to/video.mp4");

// 开始播放
lv_ffmpeg_player_set_cmd(player, LV_FFMPEG_PLAYER_CMD_START);
```

#### 控制播放
```c
// 暂停播放
lv_ffmpeg_player_set_cmd(player, LV_FFMPEG_PLAYER_CMD_PAUSE);

// 恢复播放
lv_ffmpeg_player_set_cmd(player, LV_FFMPEG_PLAYER_CMD_RESUME);

// 停止播放
lv_ffmpeg_player_set_cmd(player, LV_FFMPEG_PLAYER_CMD_STOP);
```

### 性能考虑

#### 音频播放
- **内存占用**: 中等（音频缓冲区 + 重采样上下文 + PCM 缓冲区）
- **CPU 使用**: 中等（音频解码 + 重采样 + PCM 写入）
- **优势**: 低延迟，直接控制 PCM 设备，硬件音量控制
- **适用场景**: 需要低延迟音频输出的应用

### 依赖要求

音频播放需要以下库：
- **ALSA 库**：
  - `asound`: ALSA 音频库（必需）
- **FFmpeg 库**：
  - `avcodec`: 音频编解码（必需）
  - `avformat`: 音频格式处理（必需）
  - `avutil`: FFmpeg 工具库（必需）
  - `swresample`: 音频重采样（必需）

这些库已在项目的 CMakeLists.txt 中配置。

### 限制和注意事项

1. **设备依赖**: 音频播放依赖 ALSA 设备，确保目标设备正确配置
2. **格式支持**: 支持的音频格式取决于 FFmpeg 编译时的编解码器支持
3. **内存管理**: 音频播放会增加内存使用，注意监控内存占用
4. **线程安全**: PCM 和 Mixer 写入操作使用互斥锁保护，确保线程安全
5. **音量控制**: 使用 ALSA Mixer 控制硬件音量，会影响所有使用 ALSA 的应用程序

### 故障排查

#### 问题：视频播放正常但没有声音
- 检查 ALSA 设备是否正常工作（`aplay -l`）
- 检查视频文件是否包含音频流
- 检查 ALSA Mixer 是否正确初始化
- 查看日志输出，确认音频流是否被检测到

#### 问题：编译失败
- 确保所有 ALSA 和 FFmpeg 库正确安装
- 检查 CMakeLists.txt 中的库路径配置
- 查看编译错误信息，确认缺少的依赖

#### 问题：PCM 下溢错误
- 这是正常现象，系统会自动恢复
- 如果频繁出现，检查系统性能和缓冲区设置
- 考虑增加缓冲区大小或减少系统负载

## 许可证

请参考项目根目录下的 LICENSE 文件了解许可证信息。

## 第三方库版权声明

本项目使用了以下第三方库，详细信息请参考项目根目录下的 COPYRIGHT 文件：

- **Lua**: Copyright (C) 1994-2025 Lua.org, PUC-Rio (MIT License)
- **lv_lib_100ask**: Copyright (c) 2022 深圳百问网科技有限公司 (MIT License)
- **cJSON**: Copyright (c) 2009-2017 Dave Gamble and cJSON contributors (MIT License)
- **LVGL**: Copyright (c) 2021 LVGL Kft (MIT License)
- **FFmpeg**: Copyright (c) 2000-2024 Fabrice Bellard et al. (LGPL/GPL)

## 联系方式

- 项目主页: https://github.com/albert585/v833_lv9_demos
- 问题反馈: 通过 GitHub Issues