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

#define V_FRAME_REGID   "0C5353ED-6DD0-11DB-852B-000393AD088C"
#define V_CHANNEL_REGID "363C4518-6DFB-11DB-9133-000393AD088C"

#define V_MAIN  "vortex"

static
pthread_mutex_t v_lua_enter = PTHREAD_MUTEX_INITIALIZER;

#define LOCK() {int e=pthread_mutex_lock(&v_lua_enter); assert(!e);}
#define UNLOCK() {int e=pthread_mutex_unlock(&v_lua_enter); assert(!e);}

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

#if 0
static int checkfn(lua_State* L, int idx)
{
  luaL_argcheck(L, lua_gettype(L, idx) == LUA_TFUNCTION, idx, "expected function");
}


gboolean on_accepted (VortexConnection * connection, gpointer data)
{
	g_print ("New connection accepted from: %s:%s\n", 
		 vortex_connection_get_host (connection),
		 vortex_connection_get_port (connection));

	/* return TRUE to accept the connection to be created */
	return TRUE;
}
#endif

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
      lua_pop(L, 1);
      break;
    default:
      g_error("error calling `%s' - %s", what,
          err == LUA_ERRMEM ? "out of memory" : "error handler errored");
      break;
  }

  return err;
}

static void v_channel_push(lua_State* L, VortexChannel* channel)
{
  VortexChannel** ud = lua_newuserdata(L, sizeof(channel));
  *ud = NULL;

  luaL_getmetatable(L, V_CHANNEL_REGID);
  lua_setmetatable(L, -2);

  *ud = channel;
}

static void v_channel_clear(lua_State* L, int idx)
{
  VortexChannel** ud = luaL_checkudata(L, idx, V_CHANNEL_REGID);
  *ud = NULL;
}

/*-
-- channel:send_rpy(msgno, ...)

Send reply to msgno on channel.

Reply payload is concatenation of ...

Errors on failure.

Example:
  channel:send_rpy(frame:msgno(), "reply to msg#", frame:msgno())
*/
static int v_channel_send_rpy(lua_State* L)
{
  v_debug_stack(L);

  {
  VortexChannel** ud = luaL_checkudata(L, 1, V_CHANNEL_REGID);
  int msgno = luaL_checkinteger(L, 2);
  const char* payload = NULL;
  size_t payload_sz = 0;
  gboolean ok = TRUE;

  luaL_argcheck(L, *ud, 1, "channel has been collected");
  luaL_argcheck(L, lua_gettop(L) > 2, 3, "payload not provided");

  lua_concat(L, lua_gettop(L) - 2);

  payload = lua_tolstring(L, -1, &payload_sz);

  v_debug("channel(#%d):send_rpy(%d, %s)",
      vortex_channel_get_number(*ud), msgno, payload, BSTR(ok));

  ok = vortex_channel_send_rpy(*ud,(gchar*)payload, payload_sz, msgno);

  v_debug("channel(#%d):send_rpy() => %c",
      vortex_channel_get_number(*ud), BSTR(ok));

  if(!ok)
    luaL_error(L, "vortex_channel_send_rpy");

  return 0;
  }
}

static const struct luaL_reg v_channel_methods[] = {
//  { "__gc",               obj_gc },
  { "send_rpy",           v_channel_send_rpy },
  { NULL, NULL }
};

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
-- msgno = frame:msgno()

Get msg sequence number of a frame.
*/
static int v_frame_msgno(lua_State* L)
{
  VortexFrame** ud = luaL_checkudata(L, 1, V_FRAME_REGID);
  luaL_argcheck(L, *ud, 1, "frame has been collected");

  lua_pushinteger(L, (int) vortex_frame_get_msgno(*ud));

  {
    int msgno = lua_tointeger(L, -1);
    v_debug("frame:msgno() => %d/%d", vortex_frame_get_msgno(*ud), msgno);
    v_debug_stack(L);
  }

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
  { "msgno",              v_frame_msgno },
  { "payload",            v_frame_payload },
  { NULL, NULL }
};

/*-
-- profile = vortex:profiles_register(profilestr, {start=fn, close=fn, frame=fn})

Profile is a user-data, currently you can't do anything with it. It is always
registered, and can't be unregistered.

*/
struct v_profile_cb {
  lua_State* L;
  int ref;
  char* profile;
  // FIXME - because of this, profile should have metadata.__gc
};

// leave nil or the function at the top of the stack, return true if
// the function was found
static gboolean v_get_profile_fn(struct v_profile_cb* cb, const char* fname)
{
  lua_State* L = cb->L;

  lua_rawgeti(L, LUA_REGISTRYINDEX, cb->ref);
  lua_getfenv(L, -1);
  lua_getfield(L, -1, fname);

  // Leave fn at top of stack
  lua_remove(L, -2);
  lua_remove(L, -2);

  return !lua_isnil(L, -1);
}

// FIXME - maybe should create a connection UD in the registry indexed by a LUD
// of connection, if it isn't there already, and push it?
static
void v_connection_push(lua_State* L, VortexConnection* connection)
{
  lua_pushlightuserdata(L, connection);
}
/*
static
void v_connection_close(lua_State* L, VortexConnection* connection)
{
  remove UD from registry, if there, and mark destroyed
  remove all channel UD from registry, if there, and mark destroyed
}
*/

static
void v_encoding_push(lua_State*L, VortexEncoding encoding)
{
  switch(encoding)
  {
    case EncodingNone:   lua_pushstring(L, "none");    break;
    case EncodingBase64: lua_pushstring(L, "base64");  break;
    default:             lua_pushstring(L, "unknown"); break;
  }
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
  struct v_profile_cb* cb = user_data;
  lua_State* L = cb->L;
  int top = 0;
  gboolean ok = TRUE;
  gchar* content_reply = NULL;

  LOCK();

  top = lua_gettop(L);

  if(v_get_profile_fn(cb, "start")) {
    v_debug("channel->start(%s, %d)", profile, channel_num);
    lua_pushstring(L, profile);
    lua_pushnumber(L, channel_num);
    v_connection_push(L, connection);
    // serverName can be NULL, should we push nil or ""?
    lua_pushstring(L, serverName);
    lua_pushstring(L, profile_content);
    v_encoding_push(L, encoding);

    if(v_pcall(L, "channel->start", 6, 2))
      goto end;

    ok = lua_toboolean(L, -2);
    {
      const char* cr = lua_tostring(L, -1);
      if(cr)
        content_reply = strdup(cr);
    }
    //printf("  return %d, %s\n", ok, content_reply);
  }
  
end:
  v_debug("channel->start() => %c, %s", BSTR(ok), content_reply);

  lua_settop(L, top);

  UNLOCK();

  *profile_content_reply = content_reply;

  return ok;
}

static
gboolean v_on_close_channel_locked (
    lua_State* L,
    gint channel_num,
    VortexConnection* connection,
    struct v_profile_cb* cb)
{
  v_get_profile_fn(cb, "close");

  if(!lua_isnil(L, -1)) {
    lua_pushnumber(L, channel_num);
    lua_pushlightuserdata(L, connection);
    if(v_pcall(L, "channel->close", 2, 1))
      return FALSE;

    return lua_toboolean(L, -1);
  }
  
  return TRUE;
}

static
gboolean v_on_close_channel (
    gint channel_num, 
    VortexConnection* connection, 
    gpointer user_data)
{
  struct v_profile_cb* cb = user_data;
  lua_State* L = cb->L;
  int top = 0;
  gboolean ok = TRUE;

  LOCK();

  top = lua_gettop(L);

  ok = v_on_close_channel_locked(L, channel_num, connection, cb);

  lua_settop(L, top);

  UNLOCK();

  return ok;
}

static
void v_on_frame_received (VortexChannel    * channel,
		     VortexConnection * connection,
		     VortexFrame      * frame,
		     gpointer           user_data)
{
  struct v_profile_cb* cb = user_data;
  lua_State* L = cb->L;
  int top = 0;
  int err;
  int chnum = vortex_channel_get_number(channel);
  int frnum = vortex_frame_get_msgno(frame);

  LOCK();

  top = lua_gettop(L);

  if(v_get_profile_fn(cb, "frame")) {
    // 1: fn
    v_channel_push(L, channel); // 2
    v_frame_push(L, frame);     // 3

    v_debug("channel(%d)->frame(%d)...", chnum, frnum);

    // we need to access args after the pcall, so need to build
    // new pcall stack
    lua_pushvalue(L, top+1); // fn
    lua_pushvalue(L, top+2); // channel
    v_connection_push(L, connection); // connection
    lua_pushvalue(L, top+3); // frame
    err = v_pcall(L, "channel->frame", 3, 0);

    // TODO should the channel be torn down if one of the handlers errors?

    v_debug("channel->frame() => %c", BSTR(!err));
    v_debug("channel(%d)->frame(%d) => %c", chnum, frnum, BSTR(!err));

    // channel and frame are invalid after return from callback, so clear them
    v_channel_clear(L, top+2);
    v_frame_clear(L, top+3);
  }

  lua_settop(L, top);

  UNLOCK();

  return;
}

/*-
-- vortex:profiles_register{profile=str, start=fn, frame=fn, close=fn}

The start fn is:
  ok [, content_reply] = function(profile,channum,conn,server,content,encoding)

ok is true to accept the channel create, false to refuse channel creation.

content_reply is a string that will be returned as profile content in
the reply, or nil.

The frame fn is:
 ...

The close fn is:
 ...

*/
static int v_profiles_register(lua_State* L)
{
  const char* profile = 0;
  struct v_profile_cb* cb = 0;

  // Check we were called with: vortex, profile, fntable
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checktype(L, 2, LUA_TTABLE);
  lua_getfield(L, -1, "profile");

  // stack: vortex, args, profile

  profile = lua_tostring(L, -1);

  luaL_argcheck(L, profile, 2, "profile not set in arg table");

  // Check vortex was initialized, and this profile has not been registered
  lua_getfield(L, 1, "_profiles");
  luaL_argcheck(L, !lua_isnil(L, 4), 1, "vortex not initialized");
  lua_getfield(L, 4, profile);
  luaL_argcheck(L, lua_isnil(L, 5), 2, "profile already registered");
  lua_pop(L, 1);

  // stack: vortex, args, profile, vortex._profiles

  // Create callback data, with fn table as callback "environment"
  cb = lua_newuserdata(L, sizeof(*cb));
  cb->L = L;
  cb->profile = strdup(profile);

  // stack: vortex, args, profile, vortex._profiles, userdata

  lua_pushvalue(L, 2);
  lua_setfenv(L, -2);

  // Register cb as profile
  lua_pushvalue(L, -1);
  lua_setfield(L, 4, profile);

  // Reference cb in registry
  lua_pushvalue(L, -1);
  cb->ref = luaL_ref(L, LUA_REGISTRYINDEX);

  vortex_profiles_register (
      cb->profile, 
      0, cb, // start - use extended start
        // vortex has a bug where close uses start's user_data, so pass it
        // above
      v_on_close_channel, cb,
      v_on_frame_received, cb);

  g_assert(cb);

  vortex_profiles_register_extended_start(
      cb->profile,
      v_on_start_channel, cb
      );

  (void) v_on_start_channel;
  return 0;
}

/*-
-- vortex:init()
Initialize the vortex library.

Calling multiple times has no effect.
*/
static int v_init(lua_State* L)
{
  lua_getfield(L, 1, "_profiles");

  if(!lua_isnil(L, -1)) {
    return 0;
  }

  lua_newtable(L);
  lua_setfield(L, 1, "_profiles");

  vortex_init();

  return 0;
}

/*-
-- vortex:exit()
Deinitialize the vortex library.
*/
int v_exit(lua_State* L)
{
  lua_getfield(L, 1, "_profiles");

  if(!lua_isnil(L, -1)) {
    vortex_exit();
    lua_pushnil(L);
    lua_setfield(L, 1, "_profiles");
  }
  return 0;
}

/*-
-- vortex:listener_new(host, port)

Host should usually be "0.0.0.0", INADDR_ANY.

TODO - accept a table of callbacks: on_ready, on_accept.
*/
static int v_listener_new(lua_State* L)
{
  const char* host = luaL_checkstring(L, 2);
  const char* port = luaL_checkstring(L, 3);

  luaL_argcheck(L, strlen(host), 2, "host is null");
  luaL_argcheck(L, strlen(port), 3, "port is null");

  vortex_listener_new((gchar*)host, (gchar*)port, NULL, NULL);

  return 0;
}

/*-
-- vortex:listener_wait()

Allow read thread to begin processing connection messages.

Will return when vortex:exit() is called.
*/
static int v_listener_wait(lua_State* L)
{
  UNLOCK();

  vortex_listener_wait();

  LOCK();

  return 0;
}

static const struct luaL_reg v_methods[] = {
  { "init",               v_init },
  { "exit",               v_exit },
  { "profiles_register",  v_profiles_register },
  { "listener_new",       v_listener_new },
  { "listener_wait",      v_listener_wait },
  // log_enable{status=bool, color=bool}
  { NULL, NULL }
};

void v_obj_metatable(lua_State* L, const char* regid, const struct luaL_reg methods[])
{
  luaL_newmetatable(L, regid);
  luaL_openlib(L, NULL, methods, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

int luaopen_vortex(lua_State* L) 
{
  v_obj_metatable(L, V_FRAME_REGID, v_frame_methods);
  v_obj_metatable(L, V_CHANNEL_REGID, v_channel_methods);

  luaL_register(L, V_MAIN, v_methods);

  LOCK()

  return 1;
}

