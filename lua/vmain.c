/*
vortex - a binding of the vortex BEEP toolkit into lua 5.1

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

#define V_MAIN  "vortex"

/*
Used to protect the lua state from callbacks. Only one active thread is allowed
to access the state at any one time, the main lua chunk must explicitly make it
available with vortex:listener_wait().
*/
static
pthread_mutex_t v_lua_enter = PTHREAD_MUTEX_INITIALIZER;

#define LOCK() v_debug("TRY %s", __FUNCTION__); {int e=pthread_mutex_lock(&v_lua_enter); assert(!e);} v_stack_invariant(L); v_debug("LOCK %s", __FUNCTION__);
#define UNLOCK() v_stack_invariant(L); {int e=pthread_mutex_unlock(&v_lua_enter); assert(!e);} v_debug("UNLOCK %s", __FUNCTION__);

/*
When the lua stack is unlocked the vortex threads can make callbacks. To make
this process a bit simpler, the stack is pre-setup with some useful entries
before unlocking, and all callbacks should preserve this invariant.

  [1] vortex{}      the vortex module table
  [2] _profiles{}   the table of registered profiles
                        key is a URI string
                        value is the table registered
  [3] _objects{}    the table of vortex objects
                        key is lightuserdata of the vortex object pointer
                        value is the lua userdata object

                        table has weak values, it will not block garbage collection
*/

#define V_IDX_OBJECTS 3

/* Return 0 on failure, pushes and returns index of profile on success. */
int v_profile_get(lua_State* L, const char* profile)
{
  lua_getfield(L, 2, profile);

  if(lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return 0;
  }
  return lua_gettop(L);
}

/* Check lua stack invariant. */
void v_stack_invariant(lua_State* L)
{
  lua_settop(L, 3);
  g_assert(lua_type(L, 1) == LUA_TTABLE);
  g_assert(lua_type(L, 2) == LUA_TTABLE);
  g_assert(lua_type(L, 3) == LUA_TTABLE);
}



#define BSTR(x) ((x) ? 't' : 'f')

static void v_debug(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  // goes where? g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, fmt, ap);
  vfprintf(stdout, fmt, ap);
  fprintf(stdout, "\n");
  va_end(ap);
}

static void v_debug_stack(lua_State* L)
{
  int i;
  for(i = 1; i <= lua_gettop(L); i++) {
    printf(" [%d] %s:", i, lua_typename(L, lua_type(L, i)));
    switch(lua_type(L, i)) {
      case LUA_TSTRING:
        printf("\"%s\"", lua_tostring(L, i));
        break;
      case LUA_TNUMBER:
        printf("%f", lua_tonumber(L, i));
        break;
      default:
        break;
    }
    printf("\n");
  }
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

/* pcall wrapper, logging what errors, or aborting on internal errors (error
 * handler failed, or out of memory)
 */
static int v_pcall(lua_State* L, const char* what, int nargs, int nresults)
{
  int err;
  // TODO - set up the error handler
  err = lua_pcall(L, nargs, nresults, 0);

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

/** connections **/

// APIs:
//   connection:remote_profiles() vortex_connection_get_remote_profiles

static
int v_connection_gc(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);

  v_debug("DESTROY: %s - %p", V_CONNECTION_REGID, *ud);

  if(*ud)
    vortex_connection_unref(*ud, "lua/gc");

  return 0;
}

static
void v_connection_push(lua_State* L, VortexConnection* connection)
{
  lua_pushlightuserdata(L, connection);
  lua_gettable(L, V_IDX_OBJECTS);

  if(lua_isnil(L, -1)) {
    /* doesn't exist, create new */
    void** ud = NULL;
    lua_pop(L, 1);
    ud = lua_newuserdata(L, sizeof(connection));
    *ud = NULL;

    luaL_getmetatable(L, V_CONNECTION_REGID);
    lua_setmetatable(L, -2);

    *ud = connection;

    lua_pushlightuserdata(L, connection);
    lua_pushvalue(L, -2);
    lua_settable(L, V_IDX_OBJECTS);

    v_debug("CREATE: %s - %p", V_CONNECTION_REGID, *ud);

    vortex_connection_ref(connection, "lua/gc");
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

int v_connection_host(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  lua_pushstring(L, vortex_connection_get_host(*ud));
  return 1;
}

int v_connection_port(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  lua_pushstring(L, vortex_connection_get_port(*ud));
  return 1;
}

int v_connection_id(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  lua_pushinteger(L, vortex_connection_get_id(*ud));
  return 1;
}

int v_connection_features(lua_State* L)
{
  VortexConnection** ud = luaL_checkudata(L, 1, V_CONNECTION_REGID);
  lua_pushstring(L, vortex_connection_get_features(*ud));
  return 1;
}

static const struct luaL_reg v_connection_methods[] = {
  { "__gc",           v_connection_gc },
  { "host",           v_connection_host }, // This is remote host/port, not local.
  { "port",           v_connection_port }, // Name should reflect this, and need other APIs for local.
  { "id",             v_connection_id },
  { "features",       v_connection_features },
//{ "role",           v_connection_id },
  { NULL, NULL }
};



/** channel support **/


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

  lua_pushlightuserdata(L, collect->channel);
  lua_gettable(L, V_IDX_OBJECTS);

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
  lua_pushlightuserdata(L, channel);
  lua_gettable(L, V_IDX_OBJECTS);

  if(lua_isnil(L, -1)) {
    /* doesn't exist, create new */
    void** ud = NULL;
    lua_pop(L, 1);
    ud = lua_newuserdata(L, sizeof(channel));
    *ud = NULL;

    luaL_getmetatable(L, V_CHANNEL_REGID);
    lua_setmetatable(L, -2);

    *ud = channel;

    lua_pushlightuserdata(L, channel);
    lua_pushvalue(L, -2);
    lua_settable(L, V_IDX_OBJECTS);

    v_debug("CREATE: %s - %p", V_CHANNEL_REGID, *ud);

    if(!vortex_channel_get_data(channel, "channel")) {
      struct ChannelCollect* collect = malloc(sizeof(*collect));
      collect->L = L;
      collect->channel = channel;
      vortex_channel_set_data_full(channel, "channel", collect, NULL, v_channel_collect);
    }
  }

  /* Either way, user-data is now at top of stack. */
}

/*-
-- msgno = channel:send_msg(msgno, ...)

Send msg on channel, return value is msgno for this message.

Payload is concatenation of ...

Errors on failure.

Example:
  msgno = channel:send_msg("it is ", os.date(), " here")
*/
static int v_channel_send_msg(lua_State* L)
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
-- channel:send_rpy(msgno, ...)

Send reply to msgno on channel.

Payload is concatenation of ...

Errors on failure.

Example:
  channel:send_rpy(frame:msgno(), "reply to msg#", frame:msgno())
*/
static int v_channel_send_rpy(lua_State* L)
{
  return v_channel_send_(L, vortex_channel_send_rpy, "send_rpy");
}

/*-
-- channel:send_err(msgno, ...)

Send error reply to msgno on channel.

Payload is concatenation of ...

Errors on failure.

Example:
  channel:send_err(frame:msgno())
*/
static int v_channel_send_err(lua_State* L)
{
  return v_channel_send_(L, vortex_channel_send_err, "send_err");
}

/*-
-- channel:send_ans(msgno, ...)

Send one in a series of replies to msgno on channel. Call channel:send_nul()
when all replies in series have been sent.

Payload is concatenation of ...

Errors on failure.

Example:
  while block = io:read(4096) do
    channel:send_ans(msgno, block)
  end
channel:send_nul(msgno)
*/
static int v_channel_send_ans(lua_State* L)
{
  return v_channel_send_(L, vortex_channel_send_ans_rpy, "send_ans");
}

/*-
-- channel:send_nul(msgno)

Send notification that a series of ans replies are complete.

Errors on failure.

Example:
  while block = io:read(4096) do
    channel:send_ans(msgno, block)
  end
channel:send_nul(msgno)
*/
static int v_channel_send_nul(lua_State* L)
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
  { "send_msg",           v_channel_send_msg },
  { "send_rpy",           v_channel_send_rpy },
  { "send_err",           v_channel_send_err },
  { "send_ans",           v_channel_send_ans },
  { "send_nul",           v_channel_send_nul },
  { NULL, NULL }
};


/** frame support **/

static int v_frame_push(lua_State* L, VortexFrame* frame)
{
  VortexFrame** ud = lua_newuserdata(L, sizeof(frame));
  *ud = NULL;

  luaL_getmetatable(L, V_FRAME_REGID);
  lua_setmetatable(L, -2);

  *ud = frame;

  return 1;
}

static void v_frame_clear(lua_State* L, int idx)
{
  VortexFrame** ud = luaL_checkudata(L, idx, V_FRAME_REGID);
  *ud = NULL;
}

/*-
-- type = frame:type()

Get type of a frame:
- "msg": this frame should be replied to with channel:send_rpy(), send_ans(), or send_err()
- "rpy": this frame is a complete success reply to an earlier "msg" frame
- "ans": this frame is one in a series of success replies to a "msg" frame
- "nul": this frame is last in a series of success replies to a "msg" frame
- "err": this frame is a failure reply to an earlier "msg" frame
- "seq": an internal frame type, should never be seen?
- "unknown": an error, should never be seen?

Ref: vortex_frame_get_type()
*/
static int v_frame_type(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  luaL_argcheck(L, *ud, 1, "frame has been collected");
  v_frametype_push(L, vortex_frame_get_type(*ud));
  return 1;
}

/*-
-- msgno = frame:msgno()

Get msg sequence number of a frame.
*/
static int v_frame_msgno(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  luaL_argcheck(L, *ud, 1, "frame has been collected");
  lua_pushinteger(L, (int) vortex_frame_get_msgno(*ud));
  return 1;
}

/*-
-- payload = frame:payload()

Get payload of a frame.
*/
static int v_frame_payload(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  luaL_argcheck(L, *ud, 1, "frame has been collected");
  lua_pushlstring(L, vortex_frame_get_payload(*ud), vortex_frame_get_payload_size(*ud));
  return 1;
}

static const struct luaL_reg v_frame_methods[] = {
//  { "__gc",               obj_gc },
  { "type",               v_frame_type },
  { "msgno",              v_frame_msgno },
  { "payload",            v_frame_payload },
  { NULL, NULL }
};


/*** profile support ***/

// leave nil or the function at the top of the stack, return true if
// the function was found
static gboolean v_get_profile_fn(lua_State* L, const char* profile, const char* fname)
{
  if(!v_profile_get(L, profile))
    g_assert(!profile);

  lua_getfield(L, -1, fname);

  lua_remove(L, -2);

  return !lua_isnil(L, -1);
}

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

  if(v_get_profile_fn(L, profile, "start")) {
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

  if(v_get_profile_fn(L, profile, "close"))
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

  if(v_get_profile_fn(L, profile, "frame")) {
    int err;
    // 1: fn
    v_channel_push(L, channel); // 2 FIXME - no longer need to push channel here
    v_frame_push(L, frame);     // 3

    //v_debug("channel(%d)->frame(%d) ...", chnum, frnum);

    // we need to access args after the pcall, so need to build
    // new pcall stack
    lua_pushvalue(L, top+1); // fn
    lua_pushvalue(L, top+2); // channel
    v_connection_push(L, connection); // connection
    lua_pushvalue(L, top+3); // frame
    //v_debug_stack(L);
    err = v_pcall(L, "channel->frame", 3, 0);

    // TODO should the channel/connection be torn down if one of the handlers errors?

    v_debug("channel(%d)->frame(%d, type %s) => %c", chnum, frnum, v_frametype_str(vortex_frame_get_type(frame)), BSTR(!err));

    // frame is invalid after return from callback, so clear it
    v_frame_clear(L, top+3);
  }

  UNLOCK();

  return;
}

/*-
-- vortex:profiles_register{profile=str, start=fn, frame=fn, close=fn}

The start fn is:
  ok [, content_reply] = function(profile,channum,conn,server,content,encoding)

ok is true to accept the channel create, false to refuse channel creation.

content_reply is a string that will be returned as profile content in
the reply, its optional, nil means no content_reply.

The frame fn is:
 ...

The close fn is:
  ok = function(channel, conn)
    FIXME - make sure channel/connection argument ordering is consistent?
    FIXME - why was channel_num passed, not channel object? Should I pass number?

ok is true to accept the channel close, false to refuse channel close.

*/
static int v_profiles_register(lua_State* L)
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
*/
int v_exit(lua_State* L)
{
  lua_getfield(L, 1, "_profiles");
  lua_getfield(L, 1, "_objects");

  UNLOCK();

  // Allow worker threads to cleanup.

  vortex_exit();

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
*/
static int v_listener_new(lua_State* L)
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
*/
static int v_listener_wait(lua_State* L)
{
  lua_getfield(L, 1, "_profiles");
  lua_getfield(L, 1, "_objects");

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
static int v_listener_unlock(lua_State* L)
{
  vortex_listener_wait();
  return 0;
}


static const struct luaL_reg v_methods[] = {
  // listener_unblock
  { "exit",               v_exit },
  { "profiles_register",  v_profiles_register },
  { "listener_new",       v_listener_new },
  { "listener_wait",      v_listener_wait },
  { "listener_unlock",    v_listener_unlock },
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

  /* vortex._objects = {}, with weak values */
  lua_newtable(L);
  lua_newtable(L);
  lua_pushstring(L, "v");
  lua_setfield(L, -2, "__mode");
  lua_setmetatable(L, -2);
  lua_setfield(L, -2, "_objects");

  {int e=pthread_mutex_lock(&v_lua_enter); assert(!e);}

  vortex_init();

  //vortex_listener_set_on_connection_accepted(v_on_connection_accepted, L);

  //v_debug_stack(L);

  return 1;
}

