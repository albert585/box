# v833_lv9_demos
全志v833主控基于Tina Linux运行LVGL实例

使用LVGL9.4

# 使用方法

## 1. 拉取项目：
```bash
git clone --recursive https://github.com/albert585/v833_lv9_demos
```

## 2. 编译项目：
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .
make -C build -j$(nproc)
```

# TODO

1. 完善 Lua 脚本拓展
2. 对当前的 container 进行修改以实现页面管理器，目前的 container 也存在不少问题，需要修改，比如 button 居中，界面的自动隐藏(删除)，而且 robot 按钮需要删除，视觉小说按钮需要有效化
3. 完善音频播放器，修改退出按钮使之可用，并且根据项目进行适配当前设备
4. 完成对 ffplayer 的真正的适配工作(这个放在另一个项目中)
