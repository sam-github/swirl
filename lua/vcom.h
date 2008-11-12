/*
Very common utilities for binding C libraries into Lua.
Copyright (c) 2008 Sam Roberts
This file is placed in the public domain.
*/

/** debug support **/

#define BSTR(x) ((x) ? 't' : 'f')

static void v_debug(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  // goes where? g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, fmt, ap);
  fprintf(stdout, "_");
  vfprintf(stdout, fmt, ap);
  fprintf(stdout, "\n");
  va_end(ap);
}

static void v_debug_stack(lua_State* L)
{
  int i;
  for(i = 1; i <= lua_gettop(L); i++) {
    if(i > 1)
      printf(" ");
    printf("[%d] %s:", i, lua_typename(L, lua_type(L, i)));
    switch(lua_type(L, i)) {
      case LUA_TSTRING:
        printf("'%s'", lua_tostring(L, i));
        break;
      case LUA_TNUMBER:
        printf("%g", lua_tonumber(L, i));
        break;
      case LUA_TBOOLEAN:
        printf("%s", lua_toboolean(L, i) ? "true" : "false");
        break;
      case LUA_TUSERDATA:
        /* TODO - print USERDATA type, by checking metatable */
      case LUA_TTABLE:
      case LUA_TTHREAD:
      case LUA_TFUNCTION:
        printf("%p", lua_topointer(L, i));
	break;
      default:
        break;
    }
  }
  printf("\n");
}

/* traceback is passed to v_pcall */
static int v_traceback (lua_State *L) {
  lua_getfield(L, LUA_GLOBALSINDEX, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return 1;
  }
  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 1;
  }
  lua_pushvalue(L, 1);  /* pass error message */
  lua_pushinteger(L, 2);  /* skip this function and traceback */
  lua_call(L, 2, 1);  /* call debug.traceback */
  return 1;
}

/*
pcall wrapper, logging what errors, or aborting on internal errors (error
handler failed, or out of memory)
*/
int v_pcall(lua_State* L, const char* what, int nargs, int nresults)
{
  int err;
  int base = lua_gettop(L) - nargs;
  lua_pushcfunction(L, v_traceback);
  lua_insert(L, base);
  err = lua_pcall(L, nargs, nresults, base);
  lua_remove(L, base);
  switch(err)
  {
    case 0:
      break;
    case LUA_ERRRUN:
      // code errored, not fatal
      v_debug("error calling `%s' - %s", what, lua_tostring(L, -1));
      lua_pop(L, 1);
      break;
    default:
      v_debug("error calling `%s' - %s", what, err == LUA_ERRMEM ? "out of memory" : "error handler errored");
      break;
  }

  return err;
}

#if 0
/* Support for registering (non-profile-based) callbacks. */
struct OnCb {
  lua_State* L;
  int fnref;
};

/* Creates callback from fn (or nil) at top of stack), and pops it. Errors if
 * other than a fn or nil is at top of stack.
 */
static
struct OnCb* v_cb_new(lua_State* L)
{
  if(!lua_isnil(L, -1))
  luaL_checktype(L, -1, LUA_TFUNCTION);

  struct OnCb* cb = g_new0(struct OnCb, 1);
  cb->L = L;
  cb->fnref = luaL_ref(L, LUA_REGISTRYINDEX);
  return cb;
}

/* Calls cb with the nargs at top of stack, and freeing and unrefing the cb. */
static
int v_cb_call_and_release(struct OnCb** pcb, const char* what, int nargs)
{
  struct OnCb* cb = *pcb;
  struct lua_State* L = cb->L;
  int e = 0;

  lua_rawgeti(L, LUA_REGISTRYINDEX, cb->fnref);
  luaL_unref(cb->L, LUA_REGISTRYINDEX, cb->fnref);

  if(lua_isnil(L, -1)) {
    /* pop the nil, and the args */
    lua_pop(L, 1 + nargs);
  } else {
    /* put the fn under the args, then call */
    lua_insert(L, lua_gettop(L) - nargs);
    //v_debug_stack(L);
    e = v_pcall(L, what, nargs, 0);
  }

  g_free(cb);
  *pcb = 0;

  return e;
}
#endif

/*
Get field from arg table, errors if argt is not a table, returns
0 if field not found, otherwise pushes field value and returns
index.
*/
static
int v_arg(lua_State* L, int argt, const char* field)
{
  luaL_checktype(L, argt, LUA_TTABLE);

  lua_getfield(L, argt, field);

  if(lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return 0;
  }
  return lua_gettop(L);
}

const char* v_arg_string(lua_State* L, int argt, const char* field, const char* def)
{
  if(!v_arg(L, argt, field))
  {
    if(def) {
      lua_pushstring(L, def);
      return lua_tostring(L, -1);
    } else {
      const char* msg = lua_pushfstring(L, "%s is missing", field);
      luaL_argerror(L, argt, msg);
    }
  }

  if(!lua_tostring(L, -1)) {
    const char* msg = lua_pushfstring(L, "%s is not a string", field);
    luaL_argerror(L, argt, msg);
  }

  return lua_tostring(L, -1);
}

#if 0
static
int v_arg_fn(lua_State* L, int argt, const char* field, int def)
{
  if(!v_arg(L, argt, field))
  {
    if(def) {
      lua_pushnil(L);
      return 0;
    } else {
      const char* msg = lua_pushfstring(L, "%s is missing", field);
      luaL_argerror(L, argt, msg);
    }
  }

  if(!lua_isfunction(L, -1)) {
    const char* msg = lua_pushfstring(L, "%s is not a function", field);
    luaL_argerror(L, argt, msg);
  }

  return lua_gettop(L);
}
#endif

int v_table_hasfield(lua_State* L, int ntable, const char* field)
{
  int has_field = 0;

  lua_getfield(L, ntable, field);

  has_field = !lua_isnil(L, -1);

  lua_pop(L, 1);

  return has_field;
}

/* Create a new metatable, register methods with it, and if there isn't an
 * __index method, set it's __index field to point to itself.
 */
void v_obj_metatable(lua_State* L, const char* regid, const struct luaL_reg methods[])
{
  luaL_newmetatable(L, regid);
  luaL_register(L, NULL, methods);

  if(!v_table_hasfield(L, -1, "__index")) {
    // set metatable.__index = metatable
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
  }

  lua_pop(L, 1);
}

int v_idx(lua_State* L, int idx)
{
  if(idx <= LUA_REGISTRYINDEX || idx >= 0)
    return idx;
  return lua_gettop(L) + 1 + idx;
}

void v_setfieldstring(lua_State* L, int idx, const char* k, const char* s)
{
  idx = v_idx(L, idx);
  lua_pushstring(L, s);
  lua_setfield(L, idx, k);
}

void v_setfieldboolean(lua_State* L, int idx, const char* k, int b)
{
  idx = v_idx(L, idx);
  lua_pushboolean(L, b);
  lua_setfield(L, idx, k);
}

void v_setfieldlstring(lua_State* L, int idx, const char* k, const char* s, size_t sz)
{
  idx = v_idx(L, idx);
  lua_pushlstring(L, s, sz);
  lua_setfield(L, idx, k);
}

void v_tableinsert(lua_State* L, int idx)
{
  lua_rawseti(L, idx, lua_objlen(L, idx) + 1);
}

void v_pushstringornil(lua_State* L, const char* s)
{
  if(s)
    lua_pushstring(L, s);
  else
    lua_pushnil(L);
}

void v_pushweaktable(lua_State* L, const char* mode)
{
  int n = lua_gettop(L);

  lua_newtable(L);
  lua_newtable(L);
  lua_pushstring(L, mode);
  lua_setfield(L, -2, "__mode");
  lua_setmetatable(L, -2);

  assert(lua_gettop(L) == n+1);
}


