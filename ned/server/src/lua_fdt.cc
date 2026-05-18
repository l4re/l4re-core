/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author: Christian Pötzsch <christian.poetzsch@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

/* Read access to a flattened device tree, see ned.dox. */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <libfdt.h>

#include <new>

#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/rm>
#include <l4/re/shared_cap>

#include "lua_cap.h"
#include "lua.h"

namespace Lua { namespace {

static char const *const DT_TYPE = "L4::Dt";

struct Dt
{
  L4Re::Shared_cap<L4Re::Dataspace> ds;
  L4Re::Rm::Unique_region<char *> region;

  void const *fdt() const { return region.get(); }
};

static Dt *
check_dt(lua_State *l, int idx)
{ return static_cast<Dt *>(luaL_checkudata(l, idx, DT_TYPE)); }

static int
node_offset(lua_State *l, Dt const *dt, int idx)
{
  char const *path = luaL_checkstring(l, idx);
  return fdt_path_offset(dt->fdt(), path);
}

static int
__open(lua_State *l)
{
  Lua::Cap *c = check_cap(l, 1);

  Dt *dt = new (lua_newuserdatauv(l, sizeof(Dt), 0)) Dt();
  luaL_setmetatable(l, DT_TYPE);

  dt->ds = c->cap<L4Re::Dataspace>();

  L4Re::Dataspace::Stats stats;
  l4_ret_t err = dt->ds->info(&stats);
  if (err < 0)
    luaL_error(l, "runtime error %s (%d)", l4sys_errtostr(err), err);

  err = L4Re::Env::env()->rm()->attach(&dt->region, stats.size,
                                       L4Re::Rm::F::Search_addr
                                       | L4Re::Rm::F::R,
                                       L4::Ipc::make_cap(dt->ds.get(),
                                                         L4_CAP_FPAGE_RO));
  if (err < 0)
    luaL_error(l, "runtime error %s (%d)", l4sys_errtostr(err), err);

  int fdt_err = fdt_check_full(dt->fdt(), stats.size);
  if (fdt_err < 0)
    luaL_error(l, "invalid device tree: %s", fdt_strerror(fdt_err));

  return 1;
}

static int
__gc(lua_State *l)
{
  check_dt(l, 1)->~Dt();
  return 0;
}

static int
__getprop(lua_State *l)
{
  Dt *dt = check_dt(l, 1);
  int node = node_offset(l, dt, 2);
  char const *name = luaL_checkstring(l, 3);

  int len;
  void const *prop = node >= 0 ? fdt_getprop(dt->fdt(), node, name, &len)
                               : nullptr;
  if (!prop)
    {
      lua_pushnil(l);
      return 1;
    }

  lua_pushlstring(l, static_cast<char const *>(prop), len);
  return 1;
}

static int
__exists(lua_State *l)
{
  Dt *dt = check_dt(l, 1);
  lua_pushboolean(l, node_offset(l, dt, 2) >= 0);
  return 1;
}

static int
__find_compatible(lua_State *l)
{
  Dt *dt = check_dt(l, 1);
  char const *compat = luaL_checkstring(l, 2);

  lua_newtable(l);
  lua_Integer i = 1;
  for (int node = fdt_node_offset_by_compatible(dt->fdt(), -1, compat);
       node >= 0;
       node = fdt_node_offset_by_compatible(dt->fdt(), node, compat))
    {
      char path[256];
      int err = fdt_get_path(dt->fdt(), node, path, sizeof(path));
      if (err < 0)
        luaL_error(l, "node path: %s", fdt_strerror(err));

      lua_pushstring(l, path);
      lua_rawseti(l, -2, i++);
    }

  return 1;
}

class Lib_dt : public Lib
{
public:
  Lib_dt() : Lib(P_env) {}

  void init(lua_State *l) override
  {
    static const luaL_Reg methods[] =
    {
      { "getprop", __getprop },
      { "exists", __exists },
      { "find_compatible", __find_compatible },
      { NULL, NULL }
    };

    luaL_newmetatable(l, DT_TYPE);
    lua_pushcfunction(l, __gc);
    lua_setfield(l, -2, "__gc");
    luaL_newlib(l, methods);
    lua_setfield(l, -2, "__index");
    lua_pop(l, 1);

    lua_require_module(l, package);
    lua_pushcfunction(l, __open);
    lua_setfield(l, -2, "Dt");
    lua_pop(l, 1);
  }
};
static Lib_dt __libdt;

}}
