/*
Swirl - an implementation of BEEP for Lua
Copyright (c) 2008 Sam Roberts
*/

/** global definitions **/

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"

#include <CBEEP.h>

#include "vcom.h"

static void core_check_diagnostic(lua_State* L, int idx, struct diagnostic* diagnostic, int def)
{
  memset(diagnostic, 0, sizeof(*diagnostic));

  if(def)
    diagnostic->code = luaL_optinteger(L, idx, def);
  else
    diagnostic->code = luaL_checkinteger(L, idx);
  diagnostic->message = (char*) luaL_optstring(L, idx+1, NULL);
  diagnostic->lang = (char*) luaL_optstring(L, idx+2, NULL);
}

#define CHAN0_REGID "swirl.chan0"

static void core_chan0_push(lua_State* L, struct chan0_msg* f, int ncore)
{
  struct chan0_msg** ud = lua_newuserdata(L, sizeof(*ud));

  *ud = NULL;

  luaL_getmetatable(L, CHAN0_REGID);
  lua_setmetatable(L, -2);

  // setfenv(<chan0 ud>, { session = <core ud> })
  lua_newtable(L);
  lua_pushvalue(L, ncore);
  lua_setfield(L, -2, "_core");
  lua_setfenv(L, -2);

  *ud = f;
}

static struct chan0_msg* core_chan0_get(lua_State* L, int narg)
{
  struct chan0_msg** fp = luaL_checkudata(L, 1, CHAN0_REGID);
  luaL_argcheck(L, *fp, narg, "chan0 has been destroyed");
  return *fp;
}

static void core_chan0_release(lua_State* L, int narg)
{
  struct chan0_msg** fp = luaL_checkudata(L, 1, CHAN0_REGID);
  *fp = NULL;
}

static int core_chan0_gc_(lua_State* L, const char* reason)
{
  struct chan0_msg** fp = luaL_checkudata(L, 1, CHAN0_REGID);
  struct chan0_msg* f = *fp;

  //printf("%s %s ud=%p chan0=%p session==%p\n", reason, CHAN0_REGID, fp, f, f ? f->session : 0);

  if(f) {
    blu_chan0_destroy(f);
    *fp = NULL;
    // clear the fenv
    lua_newtable(L);
    lua_setfenv(L, 1);
  }

  return 0;
}

static int core_chan0_gc(lua_State* L)
{
  return core_chan0_gc_(L, "gc");
}

static int core_chan0_destroy(lua_State* L)
{
  return core_chan0_gc_(L, "destroy");
}

static int core_chan0_tostring(lua_State* L)
{
  struct chan0_msg** ud = luaL_checkudata(L, 1, CHAN0_REGID);

  if(!*ud) {
    lua_pushfstring(L, "%s: %p (null)", CHAN0_REGID, ud);
  } else {
    struct chan0_msg* f = *ud;
    lua_pushfstring(L,
	"%s: ic=%c sc=%c ch=%d msg=%d features=%s localize=%s name=%s",
	CHAN0_REGID,
	f->op_ic, f->op_sc,
	f->channel_number, f->message_number,
	f->features, f->localize, f->server_name
	);

    if(f->error) {
      lua_pushfstring(L, " err=(%d,%s,%s)", f->error->code, f->error->lang, f->error->message);
      lua_concat(L, 2);
    }

    {
      struct profile* p = f->profile;
      int cnt = 2; // existing, plus [%d]
      lua_pushfstring(L, " [%d]", f->profileC);
      for( ; p ; p = p->next, cnt++) {
	lua_pushfstring(L, " %s=%s", p->uri, p->piggyback);
      }
      lua_concat(L, cnt);
    }

  }

  return 1;
}

static int core_chan0_channelno(lua_State* L)
{
  struct chan0_msg* f = core_chan0_get(L, 1);
  lua_pushinteger(L, f->channel_number);
  return 1;
}

static int core_chan0_messageno(lua_State* L)
{
  struct chan0_msg* f = core_chan0_get(L, 1);
  lua_pushinteger(L, f->message_number);
  return 1;
}

/*-
- ic, op = chan0:op()

operation is composed of:
  ic ("i" is indication, "c" is confirmation, "e" is error)
  op ("s" is start, "c" is close, "g" is greeting)
*/
static int core_chan0_op(lua_State* L)
{
  struct chan0_msg* f = core_chan0_get(L, 1);
  lua_pushlstring(L, &f->op_ic, 1);
  lua_pushlstring(L, &f->op_sc, 1);
  return 2;
}

static int core_chan0_profiles(lua_State* L)
{
  struct chan0_msg* f = core_chan0_get(L, 1);
  lua_newtable(L);
  struct profile* p;
  for(p = f->profile; p; p = p->next) {
    lua_newtable(L);
    v_setfieldstring(L, -1, "uri", p->uri);
    if(p->piggyback_length) {
      v_setfieldlstring(L, -1, "content", p->piggyback, p->piggyback_length);
    }
    v_tableinsert(L, -2);
  }

  return 1;
}

static int core_chan0_error(lua_State* L)
{
  struct chan0_msg* f = core_chan0_get(L, 1);
  if(!f->error)
    return 0;

  lua_pushinteger(L, f->error->code);
  lua_pushstring(L, f->error->message);
  lua_pushstring(L, f->error->lang);

  return 3;
}

static int core_chan0_features(lua_State* L)
{
  struct chan0_msg* f = core_chan0_get(L, 1);
  v_pushstringornil(L, f->features);
  return 1;
}

static int core_chan0_localize(lua_State* L)
{
  struct chan0_msg* f = core_chan0_get(L, 1);
  v_pushstringornil(L, f->localize);
  return 1;
}

static int core_chan0_servername(lua_State* L)
{
  struct chan0_msg* f = core_chan0_get(L, 1);
  lua_pushstring(L, f->server_name);
  return 1;
}

static int core_chan0_core(lua_State* L)
{
  core_chan0_get(L, 1); // don't need value, but want type checking
  lua_getfenv(L, 1);
  lua_getfield(L, -1, "_core");
  return 1;
}

/*-
- chan0:accept(uri[, content])
- chan0:accept()

In response to a start request (the on_start callback), start the channel with the specified
profile.

In response to a close request (the on_close callback), close the channel.
*/
static int core_chan0_accept(lua_State* L)
{
  struct chan0_msg* f = core_chan0_get(L, 1);
  size_t sz = 0;
  struct profile p = { 0 };

  switch(f->op_sc) {
    case 's':
      p.uri = (char*) luaL_checkstring(L, 2);
      p.piggyback = (char*) luaL_optlstring(L, 3, NULL, &sz);
      p.piggyback_length = sz;
      break;
    case 'c':
      break;
    default:
      return 0;
  }

  blu_chan0_reply(f, p.uri ? &p : NULL, NULL); // chan was freed by reply

  core_chan0_release(L, 1);

  return 0;
}

/*-
- chan0:reject(code, message, lang)

In response to a start request (the on_start callback), refuse to start the channel.

In response to a close request (the on_close callback), refuse to close.
*/
static int core_chan0_reject(lua_State* L)
{
  struct chan0_msg* f = core_chan0_get(L, 1);
  struct diagnostic diagnostic;

  core_check_diagnostic(L, 2, &diagnostic, 0);

  blu_chan0_reply(f, NULL, &diagnostic);

  core_chan0_release(L, 1); // chan was freed by reply

  return 0;
}

static const struct luaL_reg core_chan0_methods[] = {
  { "__gc",               core_chan0_gc },
  { "__tostring",         core_chan0_tostring },
  { "destroy",            core_chan0_destroy },
  { "channelno",          core_chan0_channelno }, // <start>
  { "messageno",          core_chan0_messageno }, // <start>
  { "op",                 core_chan0_op },
  { "profiles",           core_chan0_profiles },  // <start> (may have content)
  { "error",              core_chan0_error },
  { "features",           core_chan0_features },
  { "localize",           core_chan0_localize },
  { "servername",         core_chan0_servername },// <start>
  { "core",               core_chan0_core },
  { "accept",             core_chan0_accept },
  { "reject",             core_chan0_reject },
  { NULL, NULL }
};

#define FRAME_REGID "swirl.frame"

static void core_frame_push(lua_State* L, struct frame* f, int ncore)
{
  struct frame** ud = lua_newuserdata(L, sizeof(*ud));

  *ud = NULL;

  luaL_getmetatable(L, FRAME_REGID);
  lua_setmetatable(L, -2);

  // t = { session = <core userdata> }
  // setfenv(frame, t)
  lua_newtable(L);
  lua_pushvalue(L, ncore);
  lua_setfield(L, -2, "_core");
  lua_setfenv(L, -2);

  *ud = f;
}

static struct frame* core_frame_get(lua_State* L, int narg)
{
  struct frame** fp = luaL_checkudata(L, 1, FRAME_REGID);
  luaL_argcheck(L, *fp, narg, "frame has been destroyed");
  return *fp;
}

static int core_frame_gc_(lua_State* L, const char* reason)
{
  struct frame** fp = luaL_checkudata(L, 1, FRAME_REGID);
  struct frame* f = *fp;

  //printf("%s %s ud=%p frame=%p session==%p\n", reason, FRAME_REGID, fp, f, f ? f->session : 0);

  if(f) {
    blu_frame_destroy(f);
    *fp = NULL;
    // clear the fenv
    lua_newtable(L);
    lua_setfenv(L, 1);
  }

  return 0;
}

static int core_frame_gc(lua_State* L)
{
  return core_frame_gc_(L, "gc");
}

static int core_frame_destroy(lua_State* L)
{
  return core_frame_gc_(L, "destroy");
}

static int core_frame_tostring(lua_State* L)
{
  struct frame** ud = luaL_checkudata(L, 1, FRAME_REGID);

  if(!*ud) {
    lua_pushfstring(L, "%s: %p (null)", FRAME_REGID, ud);
  } else {
    struct frame* f = *ud;
    lua_pushfstring(L, "%s: %p type %c chan %d msg %d ans %d more %c size %d",
	FRAME_REGID, ud,
	f->msg_type,
	f->channel_number, f->message_number, f->answer_number,
	f->more, f->size);
  }

  return 1;
}

static int core_frame_type(lua_State* L)
{
  struct frame* f = core_frame_get(L, 1);
  const char* msg_type = NULL;
  switch(f->msg_type) {
    case 'M': msg_type = "msg"; break;
    case 'R': msg_type = "rpy"; break;
    case 'A': msg_type = "ans"; break;
    case 'N': msg_type = "nul"; break;
    case 'E': msg_type = "err"; break;
  }
  assert(msg_type);
  lua_pushstring(L, msg_type);
  return 1;
}

static int core_frame_channelno(lua_State* L)
{
  struct frame* f = core_frame_get(L, 1);
  lua_pushinteger(L, f->channel_number);
  return 1;
}

static int core_frame_messageno(lua_State* L)
{
  struct frame* f = core_frame_get(L, 1);
  lua_pushinteger(L, f->message_number);
  return 1;
}

static int core_frame_answerno(lua_State* L)
{
  struct frame* f = core_frame_get(L, 1);
  lua_pushinteger(L, f->answer_number);
  return 1;
}

static int core_frame_more(lua_State* L)
{
  struct frame* f = core_frame_get(L, 1);
  lua_pushboolean(L, f->more == '*' ? 1 : 0);
  return 1;
}

static int core_frame_payload(lua_State* L)
{
  struct frame* f = core_frame_get(L, 1);
  lua_pushlstring(L, f->payload, f->size);
  return 1;
}

static int core_frame_core(lua_State* L)
{
  core_frame_get(L, 1); // don't need value, but want type checking
  lua_getfenv(L, 1);
  lua_getfield(L, -1, "_core");
  return 1;
}

static const struct luaL_reg core_frame_methods[] = {
  { "__gc",               core_frame_gc },
  { "__tostring",         core_frame_tostring },
  { "destroy",            core_frame_destroy },
  { "type",               core_frame_type },
  { "channelno",          core_frame_channelno },
  { "messageno",          core_frame_messageno },
  { "answerno",           core_frame_answerno },
  { "more",               core_frame_more },
  { "payload",            core_frame_payload },
  { "core",               core_frame_core },
  { NULL, NULL }
};

#define CORE_REGID "swirl.core"

struct Core
{
  struct session* s;

  // Can probably just be a struct session**, now, with it's user pointer being
  // the lua_State
  lua_State* L;
};

typedef struct Core* Core;

static lua_State* core_get_cb(struct session* s, const char* name)
{
  Core c = blu_session_info_get(s);
  lua_State* L = c->L;
  int top = lua_gettop(L);

  lua_getglobal(L, "swirl");
  lua_getfield(L, -1, "_cores");
  lua_pushlightuserdata(L, s);
  lua_gettable(L, -2);

  lua_insert(L, top + 1);
  lua_settop(L, top + 1);

  if(lua_isnil(L, -1)) {
    // Called during session_create(), before we've been given the session pointer.
    lua_settop(L, top);
    return NULL;
  }

  lua_getfield(L, -1, name);

  // Leave stack with [<cb fn>, <userdata>]
  lua_insert(L, top+1);

  assert(lua_gettop(L) == top + 2);

  return L;
}

static void core_push_status(lua_State* L, struct session* s)
{
  int status =  bll_status(s);
  const char* text = bll_status_text(s);
  int chno = bll_status_channel(s);

  lua_pushinteger(L, status);
  v_pushstringornil(L, text);
  if(chno >= 0)
    lua_pushinteger(L, chno);
  else
    lua_pushnil(L);
}

#define LOWER_CB "_notify_lower_cb"

static void notify_lower_cb(struct session* s, int op)
{
  static const char* opstr[] = { NULL, "status", "inready", "outready" };
  lua_State* L = core_get_cb(s, LOWER_CB);
  if(!L) return;
  lua_pushstring(L, opstr[op]);
  core_push_status(L, s);
  v_pcall(L, "swirl notify_lower", 5, 0);
}

#define UPPER_CB "_notify_upper_cb"

static void notify_upper_cb( struct session * s, long chno, int op)
{
  static const char* opstr[] = { NULL,
    "frame", "message", "qempty", "answered", "quiescent", "windowfull" };
  lua_State* L = core_get_cb(s, UPPER_CB);
  if(!L) return;
  lua_pushinteger(L, chno);
  lua_pushstring(L, opstr[op]);
  v_pcall(L, "swirl notify_lower", 3, 0);
}

/*
- core = swirl.core(
	1,2  nil, nil,
	        -- TODO - remove later
	3    il=[I|L],
	4    features=STR,
	5    localize=STR,
	6    profile={[STR],...},
	7    error={ecode, emsg, elang},
	     )

core will call self:_upper_notify_cb() and self:_lower_notify_cb()

*/
static int core_create(lua_State* L)
{
  lua_settop(L, 7);
  char il = *luaL_optstring(L, 3, "I");
  // TODO error if il is not I or L
  const char* features = luaL_optlstring(L, 4, NULL, NULL);
  const char* localize = luaL_optlstring(L, 5, NULL, NULL);

  if(!lua_isnil(L, 6))
    luaL_checktype(L, 6, LUA_TTABLE);

  if(!lua_isnil(L, 7))
    luaL_checktype(L, 7, LUA_TTABLE);

  // NOTE - profile or error must be set, it's ok to have zero profiles (makes
  // sense for a client-only peer)

  // Create the profile array dynamically.
  int profilecount = lua_objlen(L, 6);
  const char** profiles = lua_newuserdata(L, sizeof(*profiles) * (profilecount + 1));
  int i;
  for(i = 0; i < profilecount; i++) {
    lua_rawgeti(L, 6, i+1);
    profiles[i] = lua_tostring(L, -1);
    // TODO should check type is string, and longer than 3 characters
    lua_pop(L, 1);
    luaL_argcheck(L, profiles[i], 6, "profiles must be URI strings");
  }
  profiles[i] = NULL;

  struct diagnostic error = { 0 };
  if(lua_objlen(L, 7)) {
    lua_rawgeti(L, 7, 1);
    lua_rawgeti(L, 7, 2);
    lua_rawgeti(L, 7, 3);
    error.code = lua_tonumber(L, -3);
    error.lang = (char*) lua_tostring(L, -2);
    error.message = (char*) lua_tostring(L, -1);
    lua_pop(L, 3);
  }

  Core c = lua_newuserdata(L, sizeof(*c));

  memset(c, 0, sizeof(*c));

  c->L = L;

  // setmetatable(<core ud>, <core meta>)
  luaL_getmetatable(L, CORE_REGID);
  lua_setmetatable(L, -2);

  // setfenv(<core ud>, {})
  lua_newtable(L);
  lua_setfenv(L, -2);

  // ready to create the session, now
  c->s = bll_session_create(
      malloc,
      free,
      notify_lower_cb,
      notify_upper_cb,
      il,
      (char*)features,
      (char*)localize,
      (char**)profiles,
      error.code ? &error : NULL,
      (void*) c
      );

  if(!c->s) {
    return luaL_error(L, "%s", "bll_session_create failed");
  }

  // Now we know the session's pointer value, we can use it to lookup the userdata.
  lua_getglobal(L, "swirl");
  lua_getfield(L, -1, "_cores");
  lua_pushlightuserdata(L, c->s);
  lua_pushvalue(L, -4);
  lua_settable(L, -3);
  lua_pop(L, 2);

  //fprintf(stdout, "mk core %p\n", lua_topointer(L, -1));

  return 1;
}

static int core_gc(lua_State* L)
{
  Core c = luaL_checkudata(L, 1, CORE_REGID);

  //fprintf(stdout, "gc core %p\n", lua_topointer(L, 1));

  if(c->s) {
    bll_session_destroy(c->s);
    c->s = NULL;
  }

  return 0;
}

static int core_tostring(lua_State* L)
{
  luaL_checkudata(L, 1, CORE_REGID);

  lua_pushfstring(L, "%s: %p", CORE_REGID, lua_topointer(L, 1));

  return 1;
}

static int core_pull(lua_State* L)
{
  Core c = luaL_checkudata(L, 1, CORE_REGID);
  struct beep_iovec* biv = bll_out_buffer(c->s);

  if(!biv)
    return 0;

  luaL_Buffer b;
  luaL_buffinit(L, &b);
  int i;
  for(i = 0; i < biv->vec_count; i++) {
    struct iovec* v = &biv->iovec[i];
    luaL_addlstring(&b, v->iov_base, v->iov_len);
  }
  luaL_pushresult(&b);

  size_t sz = 0;
  lua_tolstring(L, -1, &sz);

  if(!sz)
    return 0;

  return 1;
}

static int core_pulled(lua_State* L)
{
  Core c = luaL_checkudata(L, 1, CORE_REGID);
  int sz = luaL_optinteger(L, 2, 0);

  bll_out_count(c->s, sz);

  return 0;
}

static size_t min(size_t a, size_t b)
{
  if(a < b)
    return a;
  else
    return b;
}

static int core_push(lua_State* L)
{
  Core c = luaL_checkudata(L, 1, CORE_REGID);
  size_t bufsz = 0;
  const char* buf = luaL_checklstring(L, 2, &bufsz);
  const char* end = buf + bufsz;

  while(buf < end) {
    struct beep_iovec* biv = bll_in_buffer(c->s);
    const char* beg = buf;
    int i;

    if(!biv) {
      lua_pushnil(L);
      lua_pushstring(L, bll_status_text(c->s));
      return 2;
    }

    for(i = 0; i < biv->vec_count && buf < end; i++) {
      struct iovec* v = &biv->iovec[i];
      int tocopy = min(v->iov_len, end - buf);
      memcpy(v->iov_base, buf, tocopy);
      buf += tocopy;
    }

    bll_in_count(c->s, buf-beg);
  }

  lua_pushboolean(L, 1);

  return 1;
}

static int core_frame_read(lua_State* L)
{
  Core c = luaL_checkudata(L, 1, CORE_REGID);
  int chno = luaL_optint(L, 2, -1);
  struct frame* f = blu_frame_read(c->s, chno);
  if(!f)
    return 0;
  core_frame_push(L, f, 1);
  return 1;
}

static int core_chan0_read(lua_State* L)
{
  Core c = luaL_checkudata(L, 1, CORE_REGID);
  struct chan0_msg* ch0 = blu_chan0_in(c->s);
  if(!ch0)
    lua_pushnil(L);
  else
    core_chan0_push(L, ch0, 1);
  return 1;
}

static struct profile* core_build_profiles(lua_State* L, int idx)
{
  int n = lua_objlen(L, idx);

  if(!n)
    return NULL;

  // We allocate with newuserdata so that we can guarantee that it
  // will be garbage collected. We build the linked-list expected
  // by the core API inside this buffer.
  //
  // FIXME - I need to do better error checking on the type and existence
  // of keys!
  struct profile* pbuf = lua_newuserdata(L, n * sizeof(*pbuf));
  int i;
  for(i = 1; i <= n; i++) {
    struct profile* p = pbuf + i - 1;
    lua_rawgeti(L, idx, i);

    if(i > 1)
      (p-1)->next = p;

    p->next = NULL;

    lua_getfield(L, -1, "uri");
    p->uri = (char*) lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "content");
    size_t sz = 0;
    p->piggyback = (char*) lua_tolstring(L, -1, &sz);
    p->piggyback_length = sz;
    lua_pop(L, 1);
/*
Supposed to be handled internal to beepcore, though I think that handling is buggy.
    lua_getfield(L, -1, "encoded");
    p->encoding = lua_toboolean(L, -1);
    lua_pop(L, 1);
*/
  }

  return pbuf;
}

static int core_chan_start(lua_State* L)
{
  Core c = luaL_checkudata(L, 1, CORE_REGID);
  struct chan0_msg chan0 = { 0 };
  chan0.server_name = (char*) luaL_optstring(L, 3, NULL);
  chan0.channel_number = luaL_optinteger(L, 4, -1);

  chan0.profile = core_build_profiles(L, 2);
  luaL_argcheck(L, chan0.profile, 2, "must be an array of profile tables");

  /* allowable fields are:
   *   server_name (may be NULL)
   *   profile (an array of profiles)
   *   channel_number (but should be -1 so it is chosen)
   */
  long chno = blu_channel_start(c->s, &chan0);

  if(bll_status(c->s)) {
    lua_pushnil(L);
    lua_pushstring(L, bll_status_text(c->s));
    return 2;
  }

  /*
  if(chno < 0)
    return luaL_error(L, "failed to start channel #%d, profile %s...",
	chan0.channel_number, chan0.profile->uri);
  */

  assert(chno > 0);

  lua_pushinteger(L, chno);

  return 1;
}

static int core_chan_close(lua_State* L)
{
  Core c = luaL_checkudata(L, 1, CORE_REGID);
  int chno = luaL_checkint(L, 2);

  struct diagnostic diagnostic;

  core_check_diagnostic(L, 3, &diagnostic, 200);

  blu_channel_close(c->s, chno, &diagnostic);

  return 0;
}

/*-
- core:frame_send(chno, msgtype, msg, msgno, ansno, more)

Low-level frame sending... not to be used directly!
*/
static int core_frame_send(lua_State* L)
{
  static const char* msgtypes[] = { "MSG", "RPY", "ANS", "NUL", "ERR", NULL };
  Core c = luaL_checkudata(L, 1, CORE_REGID);
  int chno = luaL_checkint(L, 2);
  char msgtype = *msgtypes[luaL_checkoption(L, 3, NULL, msgtypes)];
  size_t msgsz = 0;
  const char* msg = luaL_checklstring(L, 4, &msgsz);
  int msgno = luaL_optinteger(L, 5, -1);
  int ansno = luaL_optinteger(L, 6, -1);
  char more = lua_toboolean(L, 7) ? '*' : '.';

  struct frame* f = blu_frame_create(c->s, msgsz);

  f->msg_type = msgtype;
  f->channel_number = chno;
  f->message_number = msgno;
  f->answer_number = ansno;
  f->more = more;
  f->size = msgsz;
  memcpy(f->payload, msg, msgsz);

  blu_frame_send(f);

  return 0;
}

static int core_status(lua_State* L)
{
  Core c = luaL_checkudata(L, 1, CORE_REGID);
  core_push_status(L, c->s);
  return 3;
}

static int core_chan_status(lua_State* L)
{
  static const char* opstr[] = { "closed", "hopen", "hclosed", "busy", "answered", "quiescent" };
  Core c = luaL_checkudata(L, 1, CORE_REGID);
  int chno = luaL_checkint(L, 2);
  int status = blu_channel_status(c->s, chno);

  lua_pushstring(L, opstr[status]);

  return 1;
}

/* Allow a userdata to be indexed, but searching for keys first in it's
 * environment (specific to this userdata) and next in its metatable (generic
 * to this userdata's "type").
 *
 * Note: by default userdata always has the fenv of it's caller at time
 * of creation, which isn't so useful, so you must create a new table and
 * set it as the fenv for new user data if you are using this API.
 */
int v_fenv_index(lua_State* L)
{
  lua_getfenv(L, 1);
  lua_pushvalue(L, 2);
  lua_gettable(L, -2);

  if(!lua_isnil(L, -1))
    return 1; // found in fenv

  if(!lua_getmetatable(L, 1))
    return 0; // there is not metatable

  lua_pushvalue(L, 2);
  lua_gettable(L, -2);

  return 1;
}

/* Allow a userdata to be newindexed, by assigning into it's environment.  */
int v_fenv_newindex(lua_State* L)
{
  lua_getfenv(L, 1);
  lua_pushvalue(L, 2);
  lua_pushvalue(L, 3);
  lua_settable(L, -3);
  return 0;
}

static const struct luaL_reg core_methods[] = {
  { "__index",            v_fenv_index },
  { "__newindex",         v_fenv_newindex },
  { "__gc",               core_gc },
  { "__tostring",         core_tostring },
  { "_pull",              core_pull },
  { "_pulled",            core_pulled },
  { "_push",              core_push },
  { "_frame_read",        core_frame_read },
  { "_chan0_read",        core_chan0_read },
  { "_chan_start",        core_chan_start },
  { "_chan_close",        core_chan_close },
  { "_frame_send",        core_frame_send },
  { "status",             core_status },
  { "chan_status",        core_chan_status },
  { NULL, NULL }
};

static const struct luaL_reg swirl_methods[] = {
  { "_core",              core_create },
  { NULL, NULL }
};

int luaopen_swirl_core(lua_State* L)
{
  (void) v_debug_stack;

  v_obj_metatable(L, CHAN0_REGID, core_chan0_methods);
  v_obj_metatable(L, FRAME_REGID, core_frame_methods);
  v_obj_metatable(L, CORE_REGID, core_methods);

  luaL_register(L, "swirl", swirl_methods);

  // We'll need a weak-valued table for callbacks to find user data by pointer.
  v_pushweaktable(L, "v");
  lua_setfield(L, -2, "_cores");

  return 1;
}

