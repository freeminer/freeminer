LOCAL_PATH := $(call my-dir)/..
IRRLICHT_LIBRARY_PATH := ../irrlicht/

LOCAL_ADDRESS_SANITIZER:=true

# copy Irrlicht data to assets
$(shell mkdir -p $(LOCAL_PATH)/assets)
$(shell mkdir -p $(LOCAL_PATH)/assets/media)
$(shell mkdir -p $(LOCAL_PATH)/assets/media/Shaders)
$(shell mkdir -p $(LOCAL_PATH)/src)
$(shell cp $(IRRLICHT_LIBRARY_PATH)/media/Shaders/*.* $(LOCAL_PATH)/assets/media/Shaders/)
$(shell cp $(IRRLICHT_LIBRARY_PATH)/media/irrlichtlogo3.png $(LOCAL_PATH)/assets/media/)
$(shell cp $(IRRLICHT_LIBRARY_PATH)/media/sydney.md2 $(LOCAL_PATH)/assets/media/)
$(shell cp $(IRRLICHT_LIBRARY_PATH)/media/sydney.bmp $(LOCAL_PATH)/assets/media/)

include $(CLEAR_VARS)
LOCAL_MODULE := Irrlicht
LOCAL_SRC_FILES := $(IRRLICHT_LIBRARY_PATH)/lib/Android/libIrrlicht.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := Freeminer

LOCAL_CPP_FEATURES += exceptions

LOCAL_CFLAGS := -D_IRR_ANDROID_PLATFORM_ -DANDROID -pipe -fno-exceptions -fno-rtti -fstrict-aliasing -g -O0

#ifndef NDEBUG
#LOCAL_CFLAGS += -g -D_DEBUG
#else
#LOCAL_CFLAGS += -fexpensive-optimizations -O3
#endif

# LOCAL_CFLAGS += -fsanitize=address
# LOCAL_LDFLAGS += -fsanitize=address

ifeq ($(TARGET_ARCH_ABI),x86)
LOCAL_CFLAGS += -fno-stack-protector
endif

LOCAL_C_INCLUDES := ${IRRLICHT_LIBRARY_PATH}/include src src/sqlite src/script src/lua/src src/json

LOCAL_SRC_FILES := src/ban.cpp src/base64.cpp src/biome.cpp src/camera.cpp src/cavegen.cpp src/chat.cpp src/client.cpp src/clientmap.cpp src/clientobject.cpp src/clouds.cpp src/collision.cpp src/connection.cpp src/content_abm.cpp src/content_cao.cpp src/content_cso.cpp src/content_mapblock.cpp src/content_mapnode.cpp src/content_nodemeta.cpp src/content_sao.cpp src/convert_json.cpp src/craftdef.cpp src/database-dummy.cpp src/database-leveldb.cpp src/database-sqlite3.cpp src/database.cpp src/debug.cpp src/defaultsettings.cpp src/dungeongen.cpp src/emerge.cpp src/environment.cpp src/filecache.cpp src/filesys.cpp src/game.cpp src/genericobject.cpp src/gettext.cpp src/guiChatConsole.cpp src/guiDeathScreen.cpp src/guiEngine.cpp src/guiFileSelectMenu.cpp src/guiFormSpecMenu.cpp src/guiKeyChangeMenu.cpp src/guiMessageMenu.cpp src/guiPasswordChange.cpp src/guiPauseMenu.cpp src/guiTextInputMenu.cpp src/guiVolumeChange.cpp src/hud.cpp src/intlGUIEditBox.cpp src/inventory.cpp src/inventorymanager.cpp src/itemdef.cpp src/keycode.cpp src/light.cpp src/localplayer.cpp src/log.cpp src/main.cpp src/map.cpp src/mapblock.cpp src/mapblock_mesh.cpp src/mapgen.cpp src/mapgen_indev.cpp src/mapgen_math.cpp src/mapgen_singlenode.cpp src/mapgen_v6.cpp src/mapgen_v7.cpp src/mapnode.cpp src/mapsector.cpp src/mesh.cpp src/mods.cpp src/nameidmapping.cpp src/nodedef.cpp src/nodemetadata.cpp src/nodetimer.cpp src/noise.cpp src/object_properties.cpp src/particles.cpp src/pathfinder.cpp src/player.cpp src/porting.cpp src/quicktune.cpp src/rollback.cpp src/rollback_interface.cpp src/serialization.cpp src/server.cpp src/serverlist.cpp src/serverobject.cpp src/sha1.cpp src/shader.cpp src/sky.cpp src/socket.cpp src/sound.cpp src/staticobject.cpp src/subgame.cpp src/test.cpp src/tile.cpp src/tool.cpp src/treegen.cpp src/version.cpp src/voxel.cpp src/voxelalgorithms.cpp src/util/directiontables.cpp src/util/numeric.cpp src/util/pointedthing.cpp src/util/serialize.cpp src/util/string.cpp src/util/timetaker.cpp

# lua api
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) src/script/common/c_content.cpp src/script/common/c_converter.cpp src/script/common/c_internal.cpp src/script/common/c_types.cpp src/script/cpp_api/s_base.cpp src/script/cpp_api/s_entity.cpp src/script/cpp_api/s_env.cpp src/script/cpp_api/s_inventory.cpp src/script/cpp_api/s_item.cpp src/script/cpp_api/s_mainmenu.cpp src/script/cpp_api/s_node.cpp src/script/cpp_api/s_nodemeta.cpp src/script/cpp_api/s_player.cpp src/script/cpp_api/s_server.cpp src/script/lua_api/l_async_events.cpp src/script/lua_api/l_base.cpp src/script/lua_api/l_craft.cpp src/script/lua_api/l_env.cpp src/script/lua_api/l_inventory.cpp src/script/lua_api/l_item.cpp src/script/lua_api/l_mainmenu.cpp src/script/lua_api/l_mapgen.cpp src/script/lua_api/l_nodemeta.cpp src/script/lua_api/l_nodetimer.cpp src/script/lua_api/l_noise.cpp src/script/lua_api/l_object.cpp src/script/lua_api/l_particles.cpp src/script/lua_api/l_rollback.cpp src/script/lua_api/l_server.cpp src/script/lua_api/l_settings.cpp src/script/lua_api/l_util.cpp src/script/lua_api/l_vmanip.cpp src/script/scripting_game.cpp src/script/scripting_mainmenu.cpp src/script/lua_api/marshall.c

# lua
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) src/lua/src/lapi.c src/lua/src/lauxlib.c src/lua/src/lbaselib.c src/lua/src/lcode.c src/lua/src/ldblib.c src/lua/src/ldebug.c src/lua/src/ldo.c src/lua/src/ldump.c src/lua/src/lfunc.c src/lua/src/lgc.c src/lua/src/linit.c src/lua/src/liolib.c src/lua/src/llex.c src/lua/src/lmathlib.c src/lua/src/lmem.c src/lua/src/loadlib.c src/lua/src/lobject.c src/lua/src/lopcodes.c src/lua/src/loslib.c src/lua/src/lparser.c src/lua/src/lstate.c src/lua/src/lstring.c src/lua/src/lstrlib.c src/lua/src/ltable.c src/lua/src/ltablib.c src/lua/src/ltm.c src/lua/src/lundump.c src/lua/src/lvm.c src/lua/src/lzio.c src/lua/src/print.c

# sqlite
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) src/sqlite/sqlite3.c

# jthread
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) src/jthread/pthread/jevent.cpp src/jthread/pthread/jmutex.cpp src/jthread/pthread/jsemaphore.cpp src/jthread/pthread/jthread.cpp

# json
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) src/json/jsoncpp.cpp

LOCAL_LDLIBS := -lEGL -llog -lGLESv1_CM -lGLESv2 -lz -landroid

LOCAL_STATIC_LIBRARIES := Irrlicht android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
