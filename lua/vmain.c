/*
A binding of the Vortex BEEP toolkit into lua 5.1

See:

http://www.aspl.es/fact/files/af-arch/vortex/html/index.html
http://www.beepcore.org/
http://www.lua.org/

-- LICENSE --

Copyright (c) 2006 Sam Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "lauxlib.h"
#include "lua.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define G_LOG_DOMAIN "vortex/lua"

#include <vortex.h>

#define V_FRAME_REGID      "VortexFrame"      //"0C5353ED-6DD0-11DB-852B-000393AD088C"
#define V_CHANNEL_REGID    "VortexChannel"    //"363C4518-6DFB-11DB-9133-000393AD088C"
#define V_CONNECTION_REGID "VortexConnection"

static void v_channel_push(lua_State* L, VortexChannel* channel);

#define V_MAIN  "vortex"

/*
Used to protect the lua state from callbacks. Only one active thread is allowed
to access the state at any one time, the main lua chunk must explicitly make it
available with vortex:listener_wait().
*/
static
pthread_mutex_t v_lua_enter = PTHREAD_MUTEX_INITIALIZER;

#define LOCK()\
  v_debug("TRY %s", __FUNCTION__);\
  {int e=pthread_mutex_lock(&v_lua_enter); assert(!e);}\
  v_debug("LOCK %s", __FUNCTION__);

/* \
  {int top_ = lua_gettop(L);
  */

#define UNLOCK()\
  {int e=pthread_mutex_unlock(&v_lua_enter); assert(!e);}\
  v_debug("UNLOCK %s", __FUNCTION__);

/*
  vortex{      the vortex module table
    _profiles{}   the table of registered profiles
                        key is a URI string
                        value is the table registered
    _objects{}    the table of vortex objects
                        key is lightuserdata of the vortex object pointer
                        value is the lua userdata object

                        table has weak values, it will not block garbage collection
  }

  lua_getfield(L, 1, "_profiles");
  lua_getfield(L, 1, "_objects");
*/

/** objects **/

/* Get vortex. */
static void v_get_table(lua_State* L)
{
  lua_getfield(L, LUA_GLOBALSINDEX, "vortex");
  g_assert(lua_istable(L, -1));
}

/* Gets the profile and pushes it, or pushes nil if not found. */
static void v_prof_get(lua_State* L, const char* profile)
{
  int top = lua_gettop(L);

  v_get_table(L);

  lua_getfield(L, -1, "_profiles");
  lua_getfield(L, -1, profile);

  lua_replace(L, top + 1);
  lua_settop(L, top + 1);
}

/* Get fname for profile. Return true if found. Leave nil or the function at top of stack */
static gboolean v_prof_get_fn(lua_State* L, const char* profile, const char* fname)
{
  v_prof_get(L, profile);

  if(lua_isnil(L, -1))
    return 0;

  lua_getfield(L, -1, fname);

  lua_remove(L, -2);

  return !lua_isnil(L, -1);
}


/* Get vortex._objects. */
static void v_obj_get_table(lua_State* L)
{
  v_get_table(L);
  lua_getfield(L, -1, "_objects");
  lua_remove(L, -2);
}

/* Gets the object and pushes it, or pushes nil if not found. */
static void v_obj_get (lua_State *L, void* addr)
{
  int top = lua_gettop(L);

  v_obj_get_table(L);

  lua_pushlightuserdata(L, addr);
  lua_gettable(L, -2);

  lua_replace(L, top + 1);
  lua_settop(L, top + 1);
}

/* Set the object, overwriting if it existed! Object is on top of stack, and
 * will be left there.
 */
static void v_obj_set(lua_State* L, void* addr)
{
  int top = lua_gettop(L);

  v_obj_get_table(L);

  lua_pushlightuserdata(L, addr);
  lua_pushvalue(L, top);
  lua_settable(L, -3);

  lua_settop(L, top);
}

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
        /* TODO - print USERDATA type, by checking metatable */
      default:
        break;
    }
  }
  printf("\n");
}

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

static
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
static int v_pcall(lua_State* L, const char* what, int nargs, int nresults)
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
      g_warning("error calling `%s' - %s", what, lua_tostring(L, -1));
      v_debug("error calling `%s' - %s", what, lua_tostring(L, -1));
      lua_pop(L, 1);
      break;
    default:
      g_error("error calling `%s' - %s", what, err == LUA_ERRMEM ? "out of memory" : "error handler errored");
      v_debug("error calling `%s' - %s", what, err == LUA_ERRMEM ? "out of memory" : "error handler errored");
      break;
  }

  return err;
}

/** miscellaneous light weight vortex objects **/

static
const char* v_frametype_str(VortexFrameType frametype)
{
  switch(frametype)
  {
    case VORTEX_FRAME_TYPE_MSG: return "msg";
    case VORTEX_FRAME_TYPE_RPY: return "rpy";
    case VORTEX_FRAME_TYPE_ANS: return "ans";
    case VORTEX_FRAME_TYPE_ERR: return "err";
    case VORTEX_FRAME_TYPE_NUL: return "nul";
    case VORTEX_FRAME_TYPE_SEQ: return "seq";
    case VORTEX_FRAME_TYPE_UNKNOWN: break;
  }
  return "unknown";
}

static
void v_frametype_push(lua_State*L, VortexFrameType frametype)
{
  lua_pushstring(L, v_frametype_str(frametype));
}

static
const char* v_encoding_str(VortexEncoding encoding)
{
  switch(encoding)
  {
    case EncodingNone:   return "none";
    case EncodingBase64: return "base64";
    case EncodingUnknown: break;
  }
  return "unknown";
}

static
void v_encoding_push(lua_State*L, VortexEncoding encoding)
{
  lua_pushstring(L, v_encoding_str(encoding));
}

/*-
** connection - object wrapper for VortexConnection
*/

// APIs:
//   connection:remote_profiles() vortex_connection_get_remote_profiles

static
int vl_connection_gc(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);

  v_debug("DESTROY: %s - %p", V_CONNECTION_REGID, *ud);

  if(*ud)
    vortex_connection_unref(*ud, "lua/gc");

  *ud = NULL;

  return 0;
}

/*-
-- str = tostring(conn)

For debugging purposes, return a string identifying the connection.
*/
static
int vl_connection_tostring(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  lua_pushfstring(L,
      "VortexConnection<#%d>",
      vortex_connection_get_id(*ud)
      );

  return 1;
}


static
void v_connection_push(lua_State* L, VortexConnection* connection)
{
  v_obj_get(L, connection);

  if(lua_isnil(L, -1)) {
    /* doesn't exist, create new */
    void** ud = NULL;
    lua_pop(L, 1);
    ud = lua_newuserdata(L, sizeof(connection));
    *ud = NULL;

    luaL_getmetatable(L, V_CONNECTION_REGID);
    lua_setmetatable(L, -2);

    v_obj_set(L, connection);

    *ud = connection;
    vortex_connection_ref(connection, "lua/gc");

    v_debug("CREATE: %s - %p", V_CONNECTION_REGID, *ud);
  }

  /* Either way, user-data is now at top of stack. */
}

/*
// TODO - use these for delivering callbacks later, not needed for obj lifetime tracking
static
void v_on_connection_closed(VortexConnection* connection, gpointer user_data)
{
  lua_State* L = user_data;

  LOCK();

  v_debug("->connection close %p %s %s",
      connection,
      vortex_connection_get_host(connection),
      vortex_connection_get_port(connection)
      );

  UNLOCK();
}

static
gboolean v_on_connection_accepted(VortexConnection * connection, gpointer user_data)
{
  lua_State* L = user_data;
  gboolean ok = TRUE;

  LOCK();

  v_debug("->connection accept %p %s %s",
      connection,
      vortex_connection_get_host(connection),
      vortex_connection_get_port(connection)
      );

  vortex_connection_set_on_close_full(connection, v_on_connection_closed, user_data);

  UNLOCK();

  return ok;
}
*/

/*-
-- ok = conn:close()

Close connection.

Ref: vortex_connection_close()
*/
int vl_connection_close(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  int b;

  UNLOCK()

  // vortex_connection_close does an unref on success... but
  // we are trying to keep connection object around until lua
  // object is GDed, so play with ref count.
  vortex_connection_ref(*ud, "lua/conn:close()");

  b = vortex_connection_close(*ud);

  if(!b) {
    /* our ref was not released, to it ouselves */
    vortex_connection_unref(*ud, "lua/conn:close()");
  }

  LOCK();

  lua_pushboolean(L, b);

  return 1;
}

/*-
-- host = conn:host()

Host name of connection.

Ref: vortex_connection_get_host()
*/
int vl_connection_host(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  lua_pushstring(L, vortex_connection_get_host(*ud));
  return 1;
}

/*-
-- port = conn:port()

Port number of connection.

Ref: vortex_connection_get_port()
*/
int vl_connection_port(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  lua_pushstring(L, vortex_connection_get_port(*ud));
  return 1;
}

/*-
-- connid = conn:id()

Get connection's ID, a a vortex-assigned unique identifier
for each connection.

Ref: vortex_connection_get_id()
*/
int vl_connection_id(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  lua_pushinteger(L, vortex_connection_get_id(*ud));
  return 1;
}

/*-
-- str = conn:features()

Get connection's "features".

Ref: vortex_connection_get_features()
*/
int vl_connection_features(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  lua_pushstring(L, vortex_connection_get_features(*ud));
  return 1;
}

/*-
-- chan = conn:chan(channo)

Get channel from a connection by the BEEP channel number.

Ref: vortex_connection_get_channel()
*/
int vl_connection_chan(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  int channo = luaL_checkinteger(L, 2);
  v_channel_push(L, vortex_connection_get_channel(*ud, channo));
  return 1;
}

static const struct luaL_reg v_connection_methods[] = {
  { "__gc",           vl_connection_gc },
  { "__tostring",     vl_connection_tostring },
  { "close",          vl_connection_close },
  { "host",           vl_connection_host }, // This is remote host/port, not local.
  { "port",           vl_connection_port }, // Name should reflect this, and need other APIs for local.
  { "id",             vl_connection_id },
  { "features",       vl_connection_features },
  { "chan",           vl_connection_chan },
//{ "role",           vl_connection_role },
  { NULL, NULL }
};


/*-
** channel - object wrapper for VortexChannel

A channel can be closed at the BEEP or library level, causing the
underlying VortexChannel to be freed. All the channel methods
will error() if the underlying channel has been freed, except:
- tostring(chan)
- chan:exists()

*/

struct ChannelCollect
{
  VortexChannel* channel;
  lua_State* L;
};

static void v_channel_collect(gpointer v)
{
  struct ChannelCollect* collect = v;
  lua_State* L = collect->L;

  LOCK();

  v_obj_get(L, collect->channel);

  v_debug("DESTROY: VortexChannel %p - found? %c", collect->channel, BSTR(!lua_isnil(L, -1)));

  if(!lua_isnil(L, -1)) {
    VortexChannel** ud = luaL_checkudata(L, -1, V_CHANNEL_REGID);
    *ud = NULL;
  }

  lua_pop(L, 1);

  free(v);

  UNLOCK();
}

static void v_channel_push(lua_State* L, VortexChannel* channel)
{
  if(!channel) {
    lua_pushnil(L);
    return;
  }

  v_obj_get(L, channel);

  if(lua_isnil(L, -1)) {
    /* doesn't exist, create new */
    void** ud = NULL;
    lua_pop(L, 1);
    ud = lua_newuserdata(L, sizeof(channel));
    *ud = NULL;

    luaL_getmetatable(L, V_CHANNEL_REGID);
    lua_setmetatable(L, -2);

    v_obj_set(L, channel);

    *ud = channel;

    if(!vortex_channel_get_data(channel, "channel")) {
      struct ChannelCollect* collect = malloc(sizeof(*collect));
      collect->L = L;
      collect->channel = channel;
      vortex_channel_set_data_full(channel, "channel", collect, NULL, v_channel_collect);
    }

    v_debug("CREATE: %s - %p", V_CHANNEL_REGID, *ud);
  }

  /* Either way, user-data is now at top of stack. */
}

/*-
-- str = tostring(chan)

For debugging purposes. Returns a string identify the user-data as a
VortexChannel object, with it's channel number and connection identifier.
*/
static
int vl_channel_tostring(lua_State* L)
{
  VortexChannel** ud = luaL_checkudata(L, 1, V_CHANNEL_REGID);
  if(*ud)
    lua_pushfstring(L,
        "VortexChannel<#%d, connid=%d>",
        vortex_channel_get_number(*ud),
        vortex_connection_get_id(vortex_channel_get_connection(*ud))
        );
  else
    lua_pushstring(L, "VortexChannel<collected>");
  return 1;
}


/*-
-- bool = chan:exists()

Returns true if the underlying VortexChannel still exists, false otherwise.
*/
static int vl_channel_exists(lua_State* L)
{
  VortexChannel** ud = luaL_checkudata(L, 1, V_CHANNEL_REGID);

  lua_pushboolean(L, *ud != NULL);

  return 1;
}

/*-
-- channo = chan:no()

Get channel number.
*/
static int vl_channel_number(lua_State* L)
{
  VortexChannel** ud = luaL_checkudata(L, 1, V_CHANNEL_REGID);

  luaL_argcheck(L, *ud, 1, "channel has been collected");

  lua_pushnumber(L, vortex_channel_get_number(*ud));

  return 1;
}

/*-
-- msgno = chan:send_msg(...)

Send msg on channel, return value is msgno for this message.

Payload is concatenation of ...

Errors on failure.

Example:
  msgno = chan:send_msg("it is ", os.date(), " here")
*/
/*
TODO - maybe better to return
  msgno, errstr = chan:send_msg(...)

where msgno is a number on success, nil on failure, and errstr
is nil on success, and a descriptive string on failure.

*/
static int vl_channel_send_msg(lua_State* L)
{
  VortexChannel** ud = luaL_checkudata(L, 1, V_CHANNEL_REGID);
  int msgno = -1;
  const char* payload = NULL;
  size_t payload_sz = 0;
  gboolean ok = TRUE;

  luaL_argcheck(L, *ud, 1, "channel has been collected");

  lua_concat(L, lua_gettop(L) - 1);

  payload = lua_tolstring(L, -1, &payload_sz);

  ok = vortex_channel_send_msg(*ud, payload, payload_sz, &msgno);

  //v_debug("channel(%d):send_msg() => %c, %d", vortex_channel_get_number(*ud), BSTR(ok), msgno);

  if(!ok)
    luaL_error(L, "vortex_channel_send_msg");

  lua_pushinteger(L, msgno);

  return 1;
}

/* common implementation of send_err, send_rpy, send_ans */
static int v_channel_send_(lua_State* L, gboolean (*fn)(VortexChannel*, const gchar*, gint, gint), const char* fn_name)
{
  VortexChannel** ud = luaL_checkudata(L, 1, V_CHANNEL_REGID);
  int msgno = luaL_checkinteger(L, 2);
  const char* payload = NULL;
  size_t payload_sz = 0;
  gboolean ok = TRUE;

  luaL_argcheck(L, *ud, 1, "channel has been collected");

  lua_concat(L, lua_gettop(L) - 2);

  payload = lua_tolstring(L, -1, &payload_sz);

  v_debug("channel(#%d):%s(%d)...", vortex_channel_get_number(*ud), fn_name, msgno);

  ok = fn(*ud, payload, payload_sz, msgno);

  v_debug("channel(#%d):%s(%d) => %c", vortex_channel_get_number(*ud), fn_name, msgno, BSTR(ok));

  if(!ok)
    luaL_error(L, "vortex_channel_%s", fn_name);

  return 0;
}

/*-
-- chan:send_rpy(msgno, ...)

Send reply to msgno on channel.

Payload is concatenation of ...

Errors on failure.

Example:
  chan:send_rpy(frame:msgno(), "reply to msg#", frame:msgno())
*/
static int vl_channel_send_rpy(lua_State* L)
{
  return v_channel_send_(L, vortex_channel_send_rpy, "send_rpy");
}

/*-
-- chan:send_err(msgno, ...)

Send error reply to msgno on channel.

Payload is concatenation of ...

Errors on failure.

Example:
  chan:send_err(frame:msgno())
*/
static int vl_channel_send_err(lua_State* L)
{
  return v_channel_send_(L, vortex_channel_send_err, "send_err");
}

/*-
-- chan:send_ans(msgno, ...)

Send one in a series of replies to msgno on channel. Call chan:send_nul()
when all replies in series have been sent.

Payload is concatenation of ...

Errors on failure.

Example:
  while block = io:read(4096) do
    chan:send_ans(msgno, block)
  end
  chan:send_nul(msgno)
*/
static int vl_channel_send_ans(lua_State* L)
{
  return v_channel_send_(L, vortex_channel_send_ans_rpy, "send_ans");
}

/*-
-- chan:send_nul(msgno)

Send notification that a series of ans replies are complete.

Errors on failure.

See: chan:send_ans()
*/
static int vl_channel_send_nul(lua_State* L)
{
  VortexChannel** ud = luaL_checkudata(L, 1, V_CHANNEL_REGID);
  int msgno = luaL_checkinteger(L, 2);
  gboolean ok = TRUE;

  luaL_argcheck(L, *ud, 1, "channel has been collected");

  ok = vortex_channel_finalize_ans_rpy(*ud, msgno);

  // v_debug("channel(#%d):%s(%d) => %c", vortex_channel_get_number(*ud), "send_nul", msgno, BSTR(ok));

  if(!ok)
    luaL_error(L, "vortex_channel_%s", "send_nul");

  return 0;
}

static const struct luaL_reg v_channel_methods[] = {
  { "__tostring",         vl_channel_tostring },
  { "exists",             vl_channel_exists },
  { "no",                 vl_channel_number },
  { "send_msg",           vl_channel_send_msg },
  { "send_rpy",           vl_channel_send_rpy },
  { "send_err",           vl_channel_send_err },
  { "send_ans",           vl_channel_send_ans },
  { "send_nul",           vl_channel_send_nul },
  { NULL, NULL }
};


/*-
** frame - object wrapper for VortexFrame
*/

/*
Frames are of two types:

  - ephemeral, the frames provided by Vortex in callbacks, where
    the frame is valid only for the duration of the callback.

  - persistent, the frames returned by frame:copy()

In both cases, the user data's fenv is set to the channel object, preventing
that object from being garbage collected.  However, when the VortexChannel is
freed, the channel objects ptr will become NULL, so a frame can check the
pointer value of it's fenv to determine whether the underlying channel still
exists.

It's not clear from the frame API what actions fail when the channel no longer
exists, but for now I'll assume everything is "safe" even when the channel no
longer exists.
*/

static int vl_frame_gc(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);

  v_debug("DESTROY: %s - %p", V_FRAME_REGID, *ud);

  if(*ud)
    vortex_frame_free(*ud);

  return 0;
}

static int vl_frame_tostring(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);

  if(*ud)
    lua_pushfstring(L,
        "VortexFrame<#%d, id=%d, channo=%d>",
        vortex_frame_get_msgno(*ud),
        vortex_frame_get_id(*ud),
        vortex_frame_get_channel(*ud)
        );
  else
    lua_pushstring(L, "VortexFrame<collected>");

  return 1;
}

static int v_frame_push(lua_State* L, VortexFrame* frame)
{
  VortexFrame** ud = lua_newuserdata(L, sizeof(*ud));
  *ud = NULL;

  luaL_getmetatable(L, V_FRAME_REGID);
  lua_setmetatable(L, -2);

  v_channel_push(L, vortex_frame_get_channel_ref(frame));
  lua_setfenv(L, -2);

  v_debug("CREATE: %s - %p", V_FRAME_REGID, *ud);

  *ud = frame;

  return 1;
}

/*-
-- fr = frame:copy()

Returns a copy of a frame.

The copy is identical to the src, except it will NOT be automatically collected
by the library, it will be garbage collected "normally".

Frame's passed in callbacks will be collected automatically when the callback
returns. If you need to keep a copy of the frame for longer than the life of
the callback, you must make a copy.
*/
static int vl_frame_copy(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  VortexFrame** copy = NULL;

  luaL_argcheck(L, *ud, 1, "frame has been collected");

  copy = lua_newuserdata(L, sizeof(*copy));
  *copy = NULL;

  lua_getfenv(L, 1);
  lua_setfenv(L, 2);

  *copy = *ud;

  v_debug("COPY: %s - %p", V_FRAME_REGID, *ud);

  return 1;
}


/*
If the frame is ephemeral, then the reference from the user data to the
VortexFrame must be manually collected to prevent a double free during gc.
*/
static void v_frame_collect(lua_State* L, int idx)
{
  VortexFrame** ud = luaL_checkudata(L, idx, V_FRAME_REGID);
  *ud = NULL;
}

/*-
-- type = frame:type()

Get type of a frame:
- "msg": this frame should be replied to with chan:send_rpy(), send_ans(), or send_err()
- "rpy": this frame is a complete success reply to an earlier "msg" frame
- "ans": this frame is one in a series of success replies to a "msg" frame
- "nul": this frame is last in a series of success replies to a "msg" frame
- "err": this frame is a failure reply to an earlier "msg" frame
- "seq": an internal frame type, should never be seen?
- "unknown": an error, should never be seen?

Ref: vortex_frame_get_type()
*/
static int vl_frame_type(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  luaL_argcheck(L, *ud, 1, "frame has been collected");
  v_frametype_push(L, vortex_frame_get_type(*ud));
  return 1;
}

/*-
-- msgno = frame:msgno()

Get msg sequence number of a frame.

Ref: vortex_frame_get_msgno()
*/
static int vl_frame_msgno(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  luaL_argcheck(L, *ud, 1, "frame has been collected");
  lua_pushinteger(L, (int) vortex_frame_get_msgno(*ud));
  return 1;
}

/*-
-- payload = frame:payload()

Get payload of a frame, a string.

Ref: vortex_frame_get_payload(), vortex_frame_get_payload_size()
*/
static int vl_frame_payload(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  luaL_argcheck(L, *ud, 1, "frame has been collected");
  lua_pushlstring(L, vortex_frame_get_payload(*ud), vortex_frame_get_payload_size(*ud));
  return 1;
}

/*-
-- chan = frame:chan()

Returns the frame's channel object.

Note that frames can exist for longer than their channel, so the channel object
returned may already have been collected.
*/
static int vl_frame_chan(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  VortexChannel** chan = NULL;
  luaL_argcheck(L, *ud, 1, "frame has been collected");

  lua_getfenv(L, -1);
  chan = luaL_checkudata(L, -1, V_CHANNEL_REGID);

  if(!*chan)
    lua_pushnil(L);

  return 1;
}

/*-
-- channo = frame:channo()

Returns the frame's channel's number.

Note that frames can exist for longer than their channel, but the original
channel number can still be returned.

I think BEEP may reuse channel numbers! If so, the connection may have a
channel of this number, but it won't be this frame's channel!
*/
static int vl_frame_channo(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  luaL_argcheck(L, *ud, 1, "frame has been collected");
  lua_pushinteger(L, vortex_frame_get_channel(*ud));
  return 1;
}

/*-
-- frameid = frame:id()

Returns the frame's ID, a vortex-assigned unique identifier
for each channel.

Ref: vortex_frame_get_id()
*/
static int vl_frame_id(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  luaL_argcheck(L, *ud, 1, "frame has been collected");
  lua_pushinteger(L, vortex_frame_get_id(*ud));
  return 1;
}

static const struct luaL_reg v_frame_methods[] = {
  { "__gc",               vl_frame_gc },
  { "__tostring",         vl_frame_tostring },
  { "copy",               vl_frame_copy },
  { "type",               vl_frame_type },
  { "msgno",              vl_frame_msgno },
  { "payload",            vl_frame_payload },
  { "chan",               vl_frame_chan },
  { "channo",             vl_frame_channo },
  { "id",                 vl_frame_id },
  { NULL, NULL }
};


/*-
** vortex - profile registration and application management
*/

static
gboolean v_on_start_channel (
    gchar*             profile,
    gint               channel_num,
    VortexConnection*  connection,
    gchar*             serverName,
    gchar*             profile_content,
    gchar**            profile_content_reply,
    VortexEncoding     encoding,
    gpointer           user_data
    )
{
  lua_State* L = user_data;
  gboolean ok = TRUE;
  gchar* content_reply = NULL;

  LOCK();

  if(v_prof_get_fn(L, profile, "start")) {
    //v_debug("channel(%d)->start(profile=%s, content=%s)", channel_num, profile, profile_content);
    lua_pushstring(L, profile);
    lua_pushnumber(L, channel_num);
    v_connection_push(L, connection);
    lua_pushstring(L, serverName); // serverName can be NULL, should we push nil or ""?
    lua_pushstring(L, profile_content);
    v_encoding_push(L, encoding);

    if(v_pcall(L, "channel->start", 6, 2))
      goto end;

    ok = lua_toboolean(L, -2);
    {
      const char* cr = lua_tostring(L, -1);
      if(cr)
        content_reply = g_strdup(cr);
    }
  }

end:
  v_debug("channel(%d)->start(profile %s, content %s) => %c, %s", channel_num, profile, profile_content, BSTR(ok), content_reply);

  UNLOCK();

  *profile_content_reply = content_reply;

  return ok;
}

static
gboolean v_on_close_channel (
    gint               channel_num,
    VortexConnection*  connection,
    gpointer           user_data
    )
{
  lua_State* L = user_data;
  VortexChannel* channel = vortex_connection_get_channel(connection, channel_num);
  const char* profile = vortex_channel_get_profile(channel);
  gboolean ok = TRUE;

  LOCK();

  if(v_prof_get_fn(L, profile, "close"))
  {
    v_channel_push(L, channel);
    v_connection_push(L, connection);
    if(0 == v_pcall(L, "channel->close", 2, 1))
      ok = lua_toboolean(L, -1);
  }

  UNLOCK();

  return ok;
}

static
void v_on_frame_received(
    VortexChannel    * channel,
    VortexConnection * connection,
    VortexFrame      * frame,
    gpointer           user_data
    )
{
  lua_State* L = user_data;
  const char* profile = vortex_channel_get_profile(channel);
  int chnum = vortex_channel_get_number(channel);
  int frnum = vortex_frame_get_msgno(frame);
  int top = 0;

  LOCK();

  top = lua_gettop(L);

  if(v_prof_get_fn(L, profile, "frame")) {
    int err;

    //v_debug("channel(%d)->frame(%d) ...", chnum, frnum);

    v_channel_push(L, channel);
    v_connection_push(L, connection);
    v_frame_push(L, frame);

    /* copy frame below call stack - so it doesn't get collected! */
    lua_pushvalue(L, -1);
    lua_insert(L, top+1);

    //v_debug_stack(L);
    err = v_pcall(L, "channel->frame", 3, 0);

    // TODO should the channel/connection be torn down if one of the handlers errors?

    v_debug("channel(%d)->frame(%d, type %s) => %c", chnum, frnum, v_frametype_str(vortex_frame_get_type(frame)), BSTR(!err));

    // frame is invalid after return from callback, so clear it
    v_frame_collect(L, top+1);
  }

  UNLOCK();

  return;
}

/*-
-- vortex:profiles_register{profile=str, start=fn, frame=fn, close=fn}

The start fn is:
  ok [, content_reply] = function(profile,channo,conn,server,content,encoding)

ok is true to accept the channel create, false to refuse channel creation.

content_reply is a string that will be returned as profile content in the
reply. It is optional, nil means no content_reply.

The frame fn is:
  function(chan, conn, frame)

The close fn is:
  ok = function(chan, conn)
ok is true to accept the channel close, false to refuse channel close.


TODO - the Vortex callbacks use channo for start/close, and VortexChannel for
frame. channel doesn't exist before the start, but why isn't channo passed for
frame? And why is it for close? Can I make all 3 use channo?  Should I make the
last two pass a channel?
*/
static int vl_profiles_register(lua_State* L)
{
  const char* profile = v_arg_string(L, 2, "profile", NULL);
  /* stack: vortex, args, profile */

  lua_getfield(L, 1, "_profiles");
  g_assert(!lua_isnil(L, -1));
  /* stack: vortex, args, profile, vortex._profiles */

  lua_getfield(L, 4, profile);
  luaL_argcheck(L, lua_isnil(L, 5), 2, "profile already registered");
  lua_pop(L, 1);
  g_assert(!vortex_profiles_is_registered(profile));

  lua_pushvalue(L, 2);
  /* stack: vortex, args, profile, vortex._profiles, args */

  lua_setfield(L, 4, profile);
  /* stack: vortex, args, profile, vortex._profiles */

  // Register the profile.

  vortex_profiles_register (
      profile,
      0, L, // start - use extended start
        // vortex has a bug where close uses start's user_data, so pass it
        // above
      v_on_close_channel, L,
      v_on_frame_received, L);

  vortex_profiles_register_extended_start(
      profile,
      v_on_start_channel, L
      );

  return 0;
}

/*-
-- vortex:exit()
Deinitialize the vortex library.

Ref: vortex_exit()
*/
int vl_exit(lua_State* L)
{
  lua_getfield(L, 1, "_profiles");
  lua_getfield(L, 1, "_objects");

  UNLOCK();

  // Allow worker threads to cleanup.

  vortex_exit();
  vortex_thread_pool_exit ();

  LOCK();

  return 0;
}

/*-
-- vortex:listener_new{host=str, port=num}

Creates a new listener on host and port.

Host is optional, it defaults to INADDR_ANY ("0.0.0.0").

TODO - accept a table of arguments:
  on_ready=
  on_accept=

TODO - all callback functions called on_? on_start, on_close, on_frame, ...?

Ref: vortex_listener_new()
*/
static int vl_listener_new(lua_State* L)
{
  const char* host = v_arg_string(L, 2, "host", "0.0.0.0");
  const char* port = v_arg_string(L, 2, "port", 0);

  vortex_listener_new(host, port, NULL, NULL);

  return 0;
}

/*-
-- vortex:listener_wait()

Allow threads to begin processing connection messages.

Will return when vortex:listener_unlock() is called.

Ref: vortex_listener_wait()
*/
static int vl_listener_wait(lua_State* L)
{
  v_debug_stack(L);

  UNLOCK();

  vortex_listener_wait();

  LOCK();

  return 0;
}

/*-
-- vortex:listener_unlock()

Causes vortex:listener_wait() to return, blocking any vortex
processing.

Until listener_wait() is called again no further vortex activity will occur.
*/
static int vl_listener_unlock(lua_State* L)
{
  vortex_listener_unlock();

  return 0;
}


static const struct luaL_reg v_methods[] = {
  // listener_unblock
  { "exit",               vl_exit },
  { "profiles_register",  vl_profiles_register },
  { "listener_new",       vl_listener_new },
  { "listener_wait",      vl_listener_wait },
  { "listener_unlock",    vl_listener_unlock },
  // log_enable{status=bool, color=bool}
  // connection_status
  //    table for connection close handler, and connection accept handler
  { NULL, NULL }
};

void v_obj_metatable(lua_State* L, const char* regid, const struct luaL_reg methods[])
{
  luaL_newmetatable(L, regid);
  luaL_register(L, NULL, methods);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

int luaopen_vortex(lua_State* L)
{
  (void) v_debug_stack;

  v_obj_metatable(L, V_FRAME_REGID, v_frame_methods);
  v_obj_metatable(L, V_CHANNEL_REGID, v_channel_methods);
  v_obj_metatable(L, V_CONNECTION_REGID, v_connection_methods);

  luaL_register(L, V_MAIN, v_methods);

  /* vortex._profiles = {} */
  lua_newtable(L);
  lua_setfield(L, -2, "_profiles");

  /* vortex._objects = {} */
  /* setmetatable(vortex._objects, {__mode = "v"}) */
  lua_newtable(L);
  lua_newtable(L);
  lua_pushstring(L, "v");
  lua_setfield(L, -2, "__mode");
  lua_setmetatable(L, -2);
  lua_setfield(L, -2, "_objects");

  {int e=pthread_mutex_lock(&v_lua_enter); assert(!e);}

  vortex_init();

  //vortex_listener_set_on_connection_accepted(v_on_connection_accepted, L);

  v_debug_stack(L);

  return 1;
}

