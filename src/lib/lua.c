#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
#include "lvgl/lvgl.h"
#include <string.h>

/* ============================================================
 * LVGL Lua 绑定示例
 * ============================================================
 * 
 * 设计思路：
 * 1. 对象指针作为 lightuserdata 在 Lua 和 C 之间传递
 * 2. 使用表模块组织 API（lv.btn(), lv.label() 等）
 * 3. 颜色使用十六进制字符串 "#RRGGBB"
 */

/* 颜色解析：将 "#RRGGBB" 字符串转换为 lv_color_t */
static lv_color_t parse_color(const char *hex)
{
    if (hex[0] == '#') hex++;
    
    uint32_t val = 0;
    for (int i = 0; i < 6 && hex[i]; i++) {
        val <<= 4;
        if (hex[i] >= '0' && hex[i] <= '9') val |= hex[i] - '0';
        else if (hex[i] >= 'a' && hex[i] <= 'f') val |= hex[i] - 'a' + 10;
        else if (hex[i] >= 'A' && hex[i] <= 'F') val |= hex[i] - 'A' + 10;
    }
    
    return lv_color_hex(val);
}

/* ============================================================
 * 基础对象操作
 * ============================================================ */

/* lv.obj(parent) - 创建对象 */
static int lv_lua_obj_create(lua_State *L)
{
    lv_obj_t *parent = lua_isnoneornil(L, 1) ? 
                       lv_screen_active() : 
                       (lv_obj_t *)lua_touserdata(L, 1);
    
    lv_obj_t *obj = lv_obj_create(parent);
    lua_pushlightuserdata(L, obj);
    return 1;
}

/* lv.delete(obj) - 删除对象 */
static int lv_lua_obj_delete(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    if (obj) lv_obj_delete(obj);
    return 0;
}

/* lv.screen() - 获取当前屏幕 */
static int lv_lua_screen_active(lua_State *L)
{
    lua_pushlightuserdata(L, lv_screen_active());
    return 1;
}

/* ============================================================
 * 位置和尺寸
 * ============================================================ */

/* lv.setPos(obj, x, y) */
static int lv_lua_set_pos(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    int32_t x = (int32_t)luaL_checkinteger(L, 2);
    int32_t y = (int32_t)luaL_checkinteger(L, 3);
    lv_obj_set_pos(obj, x, y);
    return 0;
}

/* lv.setSize(obj, w, h) */
static int lv_lua_set_size(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    int32_t w = (int32_t)luaL_checkinteger(L, 2);
    int32_t h = (int32_t)luaL_checkinteger(L, 3);
    lv_obj_set_size(obj, w, h);
    return 0;
}

/* lv.center(obj) */
static int lv_lua_center(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    lv_obj_center(obj);
    return 0;
}

/* ============================================================
 * 按钮和标签
 * ============================================================ */

/* lv.btn(parent) - 创建按钮 */
static int lv_lua_btn_create(lua_State *L)
{
    lv_obj_t *parent = lua_isnoneornil(L, 1) ? 
                       lv_screen_active() : 
                       (lv_obj_t *)lua_touserdata(L, 1);
    
    lv_obj_t *btn = lv_btn_create(parent);
    lua_pushlightuserdata(L, btn);
    return 1;
}

/* lv.label(parent) - 创建标签 */
static int lv_lua_label_create(lua_State *L)
{
    lv_obj_t *parent = lua_isnoneornil(L, 1) ? 
                       lv_screen_active() : 
                       (lv_obj_t *)lua_touserdata(L, 1);
    
    lv_obj_t *label = lv_label_create(parent);
    lua_pushlightuserdata(L, label);
    return 1;
}

/* lv.setText(obj, text) - 设置文本 */
static int lv_lua_set_text(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    const char *text = luaL_checkstring(L, 2);
    lv_label_set_text(obj, text);
    return 0;
}

/* ============================================================
 * 样式
 * ============================================================ */

/* lv.setBgColor(obj, "#RRGGBB") */
static int lv_lua_set_bg_color(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    const char *hex = luaL_checkstring(L, 2);
    lv_obj_set_style_bg_color(obj, parse_color(hex), 0);
    return 0;
}

/* lv.setTextColor(obj, "#RRGGBB") */
static int lv_lua_set_text_color(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    const char *hex = luaL_checkstring(L, 2);
    lv_obj_set_style_text_color(obj, parse_color(hex), 0);
    return 0;
}

/* lv.setRadius(obj, radius) */
static int lv_lua_set_radius(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    int32_t r = (int32_t)luaL_checkinteger(L, 2);
    lv_obj_set_style_radius(obj, r, 0);
    return 0;
}

/* lv.setPad(obj, left, top, right, bottom) */
static int lv_lua_set_pad(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    int32_t left = (int32_t)luaL_checkinteger(L, 2);
    int32_t top = (int32_t)luaL_checkinteger(L, 3);
    int32_t right = (int32_t)luaL_checkinteger(L, 4);
    int32_t bottom = (int32_t)luaL_checkinteger(L, 5);
    
    lv_obj_set_style_pad_left(obj, left, 0);
    lv_obj_set_style_pad_top(obj, top, 0);
    lv_obj_set_style_pad_right(obj, right, 0);
    lv_obj_set_style_pad_bottom(obj, bottom, 0);
    return 0;
}

/* ============================================================
 * 图片
 * ============================================================ */

/* lv.img(parent) - 创建图片对象 */
static int lv_lua_img_create(lua_State *L)
{
    lv_obj_t *parent = lua_isnoneornil(L, 1) ? 
                       lv_screen_active() : 
                       (lv_obj_t *)lua_touserdata(L, 1);
    
    lv_obj_t *img = lv_image_create(parent);
    lua_pushlightuserdata(L, img);
    return 1;
}

/* lv.setSrc(img, path) - 设置图片源 */
static int lv_lua_img_set_src(lua_State *L)
{
    lv_obj_t *img = (lv_obj_t *)lua_touserdata(L, 1);
    const char *path = luaL_checkstring(L, 2);
    lv_image_set_src(img, path);
    return 0;
}

/* ============================================================
 * 可见性
 * ============================================================ */

/* lv.show(obj) / lv.hide(obj) */
static int lv_lua_show(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

static int lv_lua_hide(lua_State *L)
{
    lv_obj_t *obj = (lv_obj_t *)lua_touserdata(L, 1);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    return 0;
}

/* ============================================================
 * 工具函数
 * ============================================================ */

/* lv.log(msg) - 打印日志 */
static int lv_lua_log(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    printf("[Lua] %s\n", msg);
    return 0;
}

/* lv.tick() - 获取毫秒时间戳 */
static int lv_lua_tick(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)lv_tick_get());
    return 1;
}

/* ============================================================
 * 注册 LVGL 模块
 * ============================================================ */

static const luaL_Reg lvgl_funcs[] = {
    /* 对象操作 */
    {"obj",      lv_lua_obj_create},
    {"delete",   lv_lua_obj_delete},
    {"screen",   lv_lua_screen_active},
    
    /* 位置尺寸 */
    {"setPos",   lv_lua_set_pos},
    {"setSize",  lv_lua_set_size},
    {"center",   lv_lua_center},
    
    /* 控件 */
    {"btn",      lv_lua_btn_create},
    {"label",    lv_lua_label_create},
    {"setText",  lv_lua_set_text},
    
    /* 样式 */
    {"setBgColor",    lv_lua_set_bg_color},
    {"setTextColor",  lv_lua_set_text_color},
    {"setRadius",     lv_lua_set_radius},
    {"setPad",        lv_lua_set_pad},
    
    /* 图片 */
    {"img",      lv_lua_img_create},
    {"setSrc",   lv_lua_img_set_src},
    
    /* 可见性 */
    {"show",     lv_lua_show},
    {"hide",     lv_lua_hide},
    
    /* 工具 */
    {"log",      lv_lua_log},
    {"tick",     lv_lua_tick},
    
    {NULL, NULL}
};

static void register_lvgl_module(lua_State *L)
{
    luaL_newlib(L, lvgl_funcs);
    lua_setglobal(L, "lv");
}

/* 打印使用帮助 */
static void print_lua_help(void)
{
    printf("\n=== LVGL Lua API ===\n");
    printf("对象:\n");
    printf("  obj = lv.obj([parent])     - 创建容器\n");
    printf("  obj = lv.btn([parent])     - 创建按钮\n");
    printf("  obj = lv.label([parent])   - 创建标签\n");
    printf("  obj = lv.img([parent])     - 创建图片\n");
    printf("  lv.delete(obj)             - 删除对象\n");
    printf("  scr = lv.screen()          - 获取当前屏幕\n");
    printf("\n位置尺寸:\n");
    printf("  lv.setPos(obj, x, y)       - 设置位置\n");
    printf("  lv.setSize(obj, w, h)      - 设置尺寸\n");
    printf("  lv.center(obj)             - 居中\n");
    printf("\n样式:\n");
    printf("  lv.setText(obj, \"text\")   - 设置文本\n");
    printf("  lv.setBgColor(obj, \"#FF0000\") - 背景色\n");
    printf("  lv.setTextColor(obj, \"#FFFFFF\") - 文字色\n");
    printf("  lv.setRadius(obj, 10)      - 圆角\n");
    printf("  lv.setPad(obj, l,t,r,b)    - 内边距\n");
    printf("\n可见性:\n");
    printf("  lv.show(obj) / lv.hide(obj)\n");
    printf("\n工具:\n");
    printf("  lv.log(msg)                - 打印日志\n");
    printf("  ms = lv.tick()             - 获取时间戳\n");
    printf("====================\n\n");
}

/* 带错误处理的 Lua 执行函数 */
void run_lua_script(const char *filename)
{
    /* 1. 创建 Lua 虚拟机 */
    lua_State *L = luaL_newstate();
    if (!L) {
        printf("luaL_newstate failed\n");
        return;
    }

    /* 2. 打开 Lua 标准库（base/io/os/math 等） */
    luaL_openlibs(L);
    
    /* 3. 注册 C 函数 */
    register_c_functions(L);
    
    /* 打印帮助信息 */
    print_lua_help();

    /* 4. 加载并运行 lua 脚本 */
    if (luaL_dofile(L, filename) != LUA_OK) {
        /* 取出栈顶的错误信息 */
        const char *err = lua_tostring(L, -1);
        printf("run lua script fail: %s\n", err);
        lua_pop(L, 1);  /* 弹出错误 */
    }

    /* 5. 关闭虚拟机 */
    lua_close(L);
}

/* 交互式 Lua 命令执行（可选） */
void run_lua_string(const char *code)
{
    lua_State *L = luaL_newstate();
    if (!L) return;
    
    luaL_openlibs(L);
    register_c_functions(L);
    
    if (luaL_dostring(L, code) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        printf("Error: %s\n", err);
        lua_pop(L, 1);
    }
    
    lua_close(L);
}
