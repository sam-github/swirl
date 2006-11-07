#include "lauxlib.h"
#include "lua.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include <vortex.h>

#define V_FRAME_REGID   "0C5353ED-6DD0-11DB-852B-000393AD088C"
#define V_CHANNEL_REGID "363C4518-6DFB-11DB-9133-000393AD088C"

#define V_MAIN  "vortex"

static
pthread_mutex_t v_lua_enter = PTHREAD_MUTEX_INITIALIZER;

#define ENTER() {int e=pthread_mutex_lock(&v_lua_enter); assert(!e);}
#define EXIT() {int e=pthread_mutex_unlock(&v_lua_enter); assert(!e);}

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

static int v_channel_push(lua_State* L, VortexChannel* channel)
{
  VortexChannel** ud = lua_newuserdata(L, sizeof(channel));
  *ud = NULL;

  luaL_getmetatable(L, V_CHANNEL_REGID);
  lua_setmetatable(L, -2);

  *ud = channel;

  return 1;
}

static void v_channel_clear(lua_State* L, int idx)
{
  VortexChannel** ud = luaL_checkudata(L, idx, V_CHANNEL_REGID);
  *ud = NULL;
}

/*-
-- ok = channel:send_rpy(msgno, payload, ...)

Send reply to msgno on channel.

Reply payload is concatenation of all the arguments.

Returns true on success, false on failure.

FIXME - it would be really nice to have a string indicating reason for failure.
*/
static int v_channel_send_rpy(lua_State* L)
{
  VortexChannel** ud = luaL_checkudata(L, 1, V_CHANNEL_REGID);
  int msgno = luaL_checkinteger(L, 2);

  luaL_argcheck(L, *ud, 1, "channel has been collected");
  luaL_argcheck(L, lua_gettop(L) > 1, 2, "payload not provided");

  lua_concat(L, lua_gettop(L) - 2);

  {
    size_t payload_sz;
    const char* payload = lua_tolstring(L, -1, &payload_sz);

    lua_pushboolean(
        L,
        vortex_channel_send_rpy(
          *ud,
          (gchar*)payload, payload_sz,
          msgno
          )
        );
  }

  return 1;
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

  lua_pushnumber(L, vortex_frame_get_msgno(*ud));

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

static gboolean v_call_boolean(
    gint channel_num,
    VortexConnection* connection,
    gpointer user_data,
    const char* fname
    )
{
  struct v_profile_cb* cb = user_data;
  lua_State* L = cb->L;
  int top = 0;
  gboolean ok = TRUE;

  ENTER();

  top = lua_gettop(L);

  v_get_profile_fn(cb, fname);

  if(!lua_isnil(L, -1)) {
    lua_pushnumber(L, channel_num);
    lua_pushlightuserdata(L, connection);
    lua_pcall(L, 2, 1, 0); // TODO - pass an error function

    ok = lua_toboolean(L, -1);
  }
  
  lua_settop(L, top);

  EXIT();

  return ok;
}

static
gboolean v_on_start_channel (gint               channel_num, 
			VortexConnection * connection, 
			gpointer           user_data)
{
  return v_call_boolean(channel_num, connection, user_data, "start");
}

static
gboolean v_on_close_channel (gint               channel_num, 
			VortexConnection * connection, 
			gpointer           user_data)
{
  return v_call_boolean(channel_num, connection, user_data, "close");
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

  ENTER();

  top = lua_gettop(L);

  v_get_profile_fn(cb, "frame"); // 1

  if(!lua_isnil(L, -1)) {
    int e;

    // TODO - pass as full user-data wrapping the C pointers.
    // After the call, null the C pointers, so the user-data
    // objects don't reference invalid memory. The user-data
    // should have a "copy" method for callbacks that want to
    // keep the data around.
    v_channel_push(L, channel); // 2
    v_frame_push(L, frame);     // 3

    // args get cleared, and we need to access them after the pcall, so need to
    // build pcall stack seperately
    lua_pushvalue(L, top+1); // fn
    lua_pushvalue(L, top+2); // channel
    lua_pushlightuserdata(L, connection);
    lua_pushvalue(L, top+3);
    e = lua_pcall(L, 3, 0, 0);
    if(e) {
      g_error("pcall fail [%u] %s\n", e, lua_tostring(L, -1));
      lua_pop(L, 1);
    }

    // channel and frame are invalid after return from callback, so clear them
    v_channel_clear(L, top+2);
    v_frame_clear(L, top+3);
  }
  
  lua_settop(L, top);

  EXIT();

  return;
}

static int v_profiles_register(lua_State* L)
{
  const char* profile = 0;
  struct v_profile_cb* cb = 0;

  // Check we were called with: vortex, profile, fntable
  luaL_checktype(L, 1, LUA_TTABLE);
  profile = luaL_checkstring(L, 2);
  luaL_checktype(L, 3, LUA_TTABLE);

  // Check vortex was initialized, and this profile has not been registered
  lua_getfield(L, 1, "_profiles");
  luaL_argcheck(L, !lua_isnil(L, 4), 1, "vortex not initialized");
  lua_getfield(L, 4, profile);
  luaL_argcheck(L, lua_isnil(L, 5), 2, "profile already registered");
  lua_pop(L, 1);

  // Create callback data, with fn table as callback "environment"
  cb = lua_newuserdata(L, sizeof(*cb));
  cb->L = L;
  cb->profile = strdup(profile);

  lua_pushvalue(L, 3);
  lua_setfenv(L, -2);

  // Register cb as profile
  lua_pushvalue(L, -1);
  lua_setfield(L, 4, profile);

  // Reference cb in registry
  lua_pushvalue(L, -1);
  cb->ref = luaL_ref(L, LUA_REGISTRYINDEX);

  vortex_profiles_register (
      cb->profile, 
      v_on_start_channel, cb, 
      v_on_close_channel, cb,
      v_on_frame_received, cb);

  return 1;
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
  EXIT();

  vortex_listener_wait();

  ENTER();

  return 0;
}

static const struct luaL_reg v_methods[] = {
  { "init",               v_init },
  { "exit",               v_exit },
  { "profiles_register",  v_profiles_register },
  { "listener_new",       v_listener_new },
  { "listener_wait",       v_listener_wait },
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

  luaL_openlib(L, V_MAIN, v_methods, 0);

  ENTER()

  return 1;
}

