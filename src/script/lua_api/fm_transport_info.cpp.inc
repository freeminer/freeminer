// Included in ModApiUtil::l_get_version; keep this table in sync with ConnectionMulti.
lua_pushboolean(L, USE_MULTI);
lua_setfield(L, table, "transport_multi");
lua_newtable(L);
#if MINETEST_TRANSPORT
lua_pushboolean(L, true);
lua_setfield(L, -2, "mt");
#endif
#if USE_ENET
lua_pushboolean(L, true);
lua_setfield(L, -2, "enet");
#endif
#if USE_SCTP
lua_pushboolean(L, true);
lua_setfield(L, -2, "sctp");
#endif
#if USE_WEBSOCKET
lua_pushboolean(L, true);
lua_setfield(L, -2, "ws");
#endif
#if USE_WEBSOCKET_SCTP
lua_pushboolean(L, true);
lua_setfield(L, -2, "ws_sctp");
#endif
lua_setfield(L, table, "transports");
