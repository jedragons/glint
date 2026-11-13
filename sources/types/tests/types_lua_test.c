#include <glint/alloc.h>
#include <glint/types_lua.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <mewunit.h>
#include <stdio.h>

TEST(lua_types, {
    static const char gtypes__test_code[] =
        "local z=Vec2.new(); assert(z.x==0 and z.y==0)\n"
        "local a,b=Vec2.new(6,8),Vec2.new(2,4)\n"
        "local c=a+b; assert(c.x==8 and c.y==12)\n"
        "c=a-b; assert(c.x==4 and c.y==4)\n"
        "c=a*b; assert(c.x==12 and c.y==32)\n"
        "c=a/b; assert(c.x==3 and c.y==2)\n"
        "c=a:scale(0.5); assert(c.x==3 and c.y==4 and a.x==6)\n"
        "a.x=7; assert(a.x==7)\n"
        "local r=Rect.new(); assert(r.x==0 and r.y==0 and r.w==0 and r.h==0)\n"
        "r=Rect.new(1,2,3,4); assert(r.x==1 and r.y==2 and r.w==3 and r.h==4)\n"
        "local n=Color.new(); assert(n.r==0 and n.g==0 and n.b==0 and n.a==0)\n"
        "n=Color.new(0x12345678); assert(n.r==18 and n.g==52 and n.b==86 and n.a==120)\n"
        "n=Color.new(1,2,3); assert(n.r==1 and n.g==2 and n.b==3 and n.a==255)\n"
        "n=Color.new(1,2,3,4); assert(n.a==4)\n"
        "assert(Color.red.r==230 and Color.red.g==41 and Color.red.b==55 and Color.red.a==255)\n"
        "assert(Color.blank.a==0 and Color.white.r==255 and Color.black.r==0)\n"
        "local red=Color.red; local original=red.r; red.r=(original+1)%256; assert(Color.red.r==original)\n"
        "assert(not pcall(function() Color.new(256,0,0) end))\n";
    galloc_Allocator alloc = galloc_malloc_allocator();
    lua_State *lua = luaL_newstate();
    mewassert("create Lua state", lua);
    luaL_openlibs(lua);
    mewassert("register types", gtypes_bind_lua(alloc, lua));
    if (luaL_dostring(lua, gtypes__test_code) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(lua, -1));
        mewassert("run types test", false);
    }
    lua_close(lua);
})

mewtest_main();
