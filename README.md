# Box
一个使用lvgl实现的小GUI

使用LVGL9

# 使用方法

## 1. 拉取项目：
```bash
git clone --recursive https://github.com/albert585/box
```

## 2. 编译项目：
```bash
make -C 3rdparty all
#执行下面的命令前请在user_cross_compile_setup.cmake配置交叉编译工具链
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .
make -C build -j$(nproc)
```

# TODO

1. 尽量实现 Lua 脚本拓展
2. 对当前的 container 进行修改以实现页面管理器，目前的 container 也存在不少问题，需要修改，比如 button 居中，界面的自动隐藏(删除)，而且 robot 按钮需要删除
3. 完善音频播放器，修改退出按钮使之可用，并且根据项目进行适配当前设备
4. 完成对 ffplayer/awplayer 的真正的适配工作
5. 目前因为硬编码第三方库路径导致其他机器无法编译，计划加入可选的依赖编译功能

