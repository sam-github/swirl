
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
        /* TODO - print USERDATA type, by checking metatable */
      default:
        break;
    }
  }
  printf("\n");
}


