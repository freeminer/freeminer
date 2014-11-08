/*
defaultsettings.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "settings.h"
#include "porting.h"
#include "filesys.h"
#include "config.h"
#include "connection.h" // ENET_IPV6
#include "constants.h"
#include "porting.h"

const bool debug =
#ifdef NDEBUG
	false
#else
	true
#endif
;

void set_default_settings(Settings *settings)
{
	//
	// Client and server
	//

	settings->setDefault("name", "");
	
	//
	// Main freeminer engine settings
	//
	
	// Language
	settings->setDefault("language", "");
	
	// Screen size
	settings->setDefault("screenW", "800");
	settings->setDefault("screenH", "600");
	settings->setDefault("fullscreen", "false");
	settings->setDefault("fullscreen_bpp", "24");
	settings->setDefault("workaround_window_size","5");
	
	// Mouse
	settings->setDefault("invert_mouse", "false");
	settings->setDefault("mouse_sensitivity", "0.2");
	
	// Sound
	settings->setDefault("enable_sound", "true");
	settings->setDefault("sound_volume", "0.8");
	
	// Paths
	settings->setDefault("texture_path", "");
	settings->setDefault("shader_path", "");
	settings->setDefault("video_driver", "opengl");
	settings->setDefault("screenshot_path", ".");
	settings->setDefault("serverlist_file", "favoriteservers.json");
	
	// Main menu
	settings->setDefault("menu_clouds", "true");
	settings->setDefault("main_menu_tab", "multiplayer");
	settings->setDefault("public_serverlist", "1");
	settings->setDefault("main_menu_path", "");
	settings->setDefault("main_menu_mod_mgr", "1");
	settings->setDefault("main_menu_game_mgr", "0");
	settings->setDefault("modstore_download_url", "https://forum.minetest.net/media/");
	settings->setDefault("modstore_listmods_url", "https://forum.minetest.net/mmdb/mods/");
	settings->setDefault("modstore_details_url", "https://forum.minetest.net/mmdb/mod/*/");
	settings->setDefault("password_save", "1");
	
	// Game Speed
	settings->setDefault("wanted_fps", "30");
	settings->setDefault("fps_max", "60");
	settings->setDefault("pause_fps_max", "5");
	settings->setDefault("high_precision_fpu", "true");
	
	// Debugging stuff
#ifdef NDEBUG
	settings->setDefault("show_debug", "false");
	settings->setDefault("deprecated_lua_api_handling", "legacy");
#else
	settings->setDefault("show_debug", "true");
	settings->setDefault("deprecated_lua_api_handling", "log");
#endif
	settings->setDefault("enable_mapgen_debug_info", "false");
	settings->setDefault("profiler_print_interval", debug ? "10" : "0");
	settings->setDefault("debug_log_level", "2");
	settings->setDefault("time_taker_enabled", debug ? "5" : "0");
	settings->setDefault("preload_item_visuals", "false");
	
	//
	// Keymaps
	//
	
	settings->setDefault("keymap_forward", "KEY_KEY_W");
	settings->setDefault("keymap_backward", "KEY_KEY_S");
	settings->setDefault("keymap_left", "KEY_KEY_A");
	settings->setDefault("keymap_right", "KEY_KEY_D");
	settings->setDefault("keymap_jump", "KEY_SPACE");
	settings->setDefault("keymap_sneak", "KEY_LSHIFT");
	settings->setDefault("keymap_drop", "KEY_KEY_Q");
	settings->setDefault("keymap_zoom", "KEY_KEY_Z");
	settings->setDefault("keymap_inventory", "KEY_KEY_I");
	settings->setDefault("keymap_special1", "KEY_KEY_E");
	settings->setDefault("keymap_chat", "KEY_KEY_T");
	settings->setDefault("keymap_cmd", "/");
#if IRRLICHT_VERSION_10000  >= 10703
	settings->setDefault("keymap_console", "KEY_OEM_3");
#else
	settings->setDefault("keymap_console", "KEY_F10");
#endif
	settings->setDefault("keymap_rangeselect", "KEY_KEY_R");
	settings->setDefault("keymap_freemove", "KEY_KEY_K");
	settings->setDefault("keymap_fastmove", "KEY_KEY_J");
	settings->setDefault("keymap_noclip", "KEY_KEY_H");
	settings->setDefault("keymap_screenshot", "KEY_F12");
	settings->setDefault("keymap_toggle_hud", "KEY_F1");
	settings->setDefault("keymap_toggle_chat", "KEY_F2");
	settings->setDefault("keymap_toggle_force_fog_off", "KEY_F3");
	//settings->setDefault("keymap_toggle_update_camera", "KEY_F4");
	settings->setDefault("keymap_toggle_block_boundaries", "KEY_F4");
	settings->setDefault("keymap_toggle_debug", "KEY_F5");
	settings->setDefault("keymap_toggle_profiler", "KEY_F6");
	settings->setDefault("keymap_camera_mode", "KEY_F7");
	settings->setDefault("keymap_increase_viewing_range_min", "+");
	settings->setDefault("keymap_decrease_viewing_range_min", "-");
	settings->setDefault("keymap_playerlist", "KEY_TAB");
	// Some (temporary) keys for debugging
	settings->setDefault("keymap_print_debug_stacks", "KEY_KEY_P");
	settings->setDefault("keymap_quicktune_prev", "KEY_HOME");
	settings->setDefault("keymap_quicktune_next", "KEY_END");
	settings->setDefault("keymap_quicktune_dec", "KEY_NEXT");
	settings->setDefault("keymap_quicktune_inc", "KEY_PRIOR");
	
	//
	// Connection
	//
	
	// Connecting to server
	settings->setDefault("address", "");
	settings->setDefault("remote_port", "30000");
	
	// Connecting to curl
	settings->setDefault("curl_timeout", "5000");
	settings->setDefault("curl_parallel_limit", "8");
	settings->setDefault("curl_file_download_timeout", "300000");
	settings->setDefault("curl_verify_cert", "true");

	//
	// Fonts
	//
	settings->setDefault("font_path", porting::getDataPath("fonts" DIR_DELIM "liberationsans.ttf"));
	settings->setDefault("font_shadow", "1");
	settings->setDefault("font_shadow_alpha", "128");
	settings->setDefault("mono_font_path", porting::getDataPath("fonts" DIR_DELIM "liberationmono.ttf"));
	settings->setDefault("fallback_font_path", porting::getDataPath("fonts" DIR_DELIM "DroidSansFallbackFull.ttf"));

	settings->setDefault("fallback_font_shadow", "1");
	settings->setDefault("fallback_font_shadow_alpha", "128");

	//
	// Map generation
	//
	
	settings->setDefault("mg_name", "indev");
	settings->setDefault("water_level", "1");
	settings->setDefault("chunksize", "5");
	settings->setDefault("mg_flags", "trees, caves, v6_biome_blend, v6_jungles, dungeons");

	settings->setDefault("mg_math", ""); // configuration in json struct
	settings->setDefault("mg_params", ""); // configuration in json struct	
	
	
	//
	// Gameplay features
	//

	// Filters
	settings->setDefault("anisotropic_filter", "false");
	settings->setDefault("bilinear_filter", "false");
	settings->setDefault("trilinear_filter", "false");
	
	// Waving
	settings->setDefault("enable_waving_leaves", "true");
	settings->setDefault("enable_waving_plants", "true");
	settings->setDefault("enable_waving_water", "true");
	settings->setDefault("water_wave_height", "1.0");
	settings->setDefault("water_wave_length", "20.0");
	settings->setDefault("water_wave_speed", "5.0");
	
	// Shaders
	settings->setDefault("enable_shaders", "true");
	settings->setDefault("enable_bumpmapping", "true");
	settings->setDefault("enable_parallax_occlusion", "true");
	settings->setDefault("parallax_occlusion_scale", "0.06");
	settings->setDefault("parallax_occlusion_bias", "0.03");
	settings->setDefault("generate_normalmaps", "false");
	settings->setDefault("normalmaps_strength", "0.6");
	settings->setDefault("normalmaps_smooth", "1");
	
	// Lighting
	settings->setDefault("smooth_lighting", "true");
	settings->setDefault("disable_wieldlight", "false");
	settings->setDefault("enable_node_highlighting", "true");
	settings->setDefault("mip_map", "false");
	
	// Clouds, water, glass, leaves, fog
	settings->setDefault("enable_clouds", "true");
	settings->setDefault("enable_3d_clouds", "true");
	settings->setDefault("cloud_height", "300");
	settings->setDefault("new_style_water", "false");
	settings->setDefault("opaque_water", "false");
	settings->setDefault("connected_glass", "false");
	settings->setDefault("new_style_leaves", "true");
	settings->setDefault("enable_fog", "true");
	settings->setDefault("directional_colored_fog", "true");
	
	// Visual Features
	settings->setDefault("enable_particles", "true");
	settings->setDefault("selectionbox_width","2");
	settings->setDefault("3d_mode", "none");
	settings->setDefault("3d_paralax_strength", "0.025");
	settings->setDefault("tooltip_show_delay", "400");
	// A bit more than the server will send around the player, to make fog blend well
	settings->setDefault("viewing_range_nodes_max", itos(MAP_GENERATION_LIMIT));
	settings->setDefault("viewing_range_nodes_min", "35");
	settings->setDefault("shadows", "0");

	settings->setDefault("fsaa", "0");
	settings->setDefault("vsync", "false");
	settings->setDefault("fov", "72");
	settings->setDefault("zoom_fov", "15");
	settings->setDefault("farmesh", "0");
	settings->setDefault("farmesh_step", "2");
	settings->setDefault("farmesh_wanted", "500");
	
	// Liquid
	settings->setDefault("liquid_real", "true");
	settings->setDefault("liquid_update", "0.1");
	settings->setDefault("liquid_send", "1.0");
	settings->setDefault("liquid_relax", "2");
	settings->setDefault("liquid_fast_flood", "1");
	
	// Weather
	settings->setDefault("weather", "true");
	settings->setDefault("weather_heat_season", "30");
	settings->setDefault("weather_heat_daily", "8");
	settings->setDefault("weather_heat_width", "3000");
	settings->setDefault("weather_hot_core", "1000");
	settings->setDefault("weather_heat_height", "-333");
	settings->setDefault("year_days", "30");
	settings->setDefault("weather_humidity_season", "30");
	settings->setDefault("weather_humidity_daily", "-12");
	settings->setDefault("weather_humidity_width", "300");
	settings->setDefault("weather_humidity_days", "2");
	
	// Mini Map
	settings->setDefault("hud_map", "false");
	settings->setDefault("hud_map_back", "0,0,0");
	settings->setDefault("hud_map_width", "128");
	settings->setDefault("hud_map_height", "128");
	settings->setDefault("hud_map_scale", "1.0");
	settings->setDefault("hud_map_alpha", "192");
	settings->setDefault("hud_map_above", "true");
	settings->setDefault("hud_map_scan", "64");
	settings->setDefault("hud_map_surface", "32");
	settings->setDefault("hud_map_tracking", "false");
	settings->setDefault("hud_map_border", "16");
	
	// Color / Readability
	settings->setDefault("console_color", "(0,0,0)");
	settings->setDefault("console_alpha", "200");
	settings->setDefault("selectionbox_color", "(0,0,0)");
	settings->setDefault("crosshair_color", "(255,255,255)");
	settings->setDefault("crosshair_alpha", "255");
	settings->setDefault("gui_scaling", "1.0");
	settings->setDefault("hud_hotbar_max_width","1.0");
	
	// Client Backend
	settings->setDefault("desynchronize_mapblock_texture_animation", "true");
	settings->setDefault("client_unload_unused_data_timeout", "600");
	//settings->setDefault("unload_unused_meshes_timeout", "120");
	settings->setDefault("enable_mesh_cache", "true");
	settings->setDefault("repeat_rightclick_time", "0.25");
	settings->setDefault("random_input", "false");	
	settings->setDefault("respawn_auto", "false");
	settings->setDefault("enable_vbo", "true");
	
	//
	// Server stuff
	//
	
	// Announcing and connection
	settings->setDefault("server_announce", "false");
	settings->setDefault("serverlist_url", "servers.freeminer.org");
	settings->setDefault("server_address", "");
	settings->setDefault("bind_address", "");
	settings->setDefault("port", "30000");
	settings->setDefault("server_name", "");
	settings->setDefault("server_description", "");
	settings->setDefault("server_url", "");
	settings->setDefault("enable_remote_media_server", "true");
	settings->setDefault("remote_media", "");
	
	// Check when player joins
	settings->setDefault("strict_protocol_version_checking", "false");
	settings->setDefault("enable_any_name", "0"); // WARNING! SETTING TO "1" COULD CAUSE SECURITY RISKS WITH MODULES WITH PLAYER DATA IN FILES CONTAINS PLAYER NAME IN FILENAME
	settings->setDefault("disallow_empty_password", "false");
	
	// Gameplay settings for players on the server
	settings->setDefault("default_game", "default");
	settings->setDefault("motd", "");
	settings->setDefault("max_users", "100");
	settings->setDefault("creative_mode", "false");
	settings->setDefault("enable_damage", "true");
	settings->setDefault("fixed_map_seed", "");
	settings->setDefault("give_initial_stuff", "false");
	settings->setDefault("default_password", "");
	settings->setDefault("default_privs", "interact, shout");
	settings->setDefault("player_transfer_distance", "0");
	settings->setDefault("enable_pvp", "true");
	settings->setDefault("disable_anticheat", "false");
	settings->setDefault("enable_rollback_recording", "false");
	settings->setDefault("max_spawn_height", "50");
	settings->setDefault("time_speed", "72");

	// Backend server settings
	settings->setDefault("max_packets_per_iteration","1024");
	settings->setDefault("cache_block_before_spawn", "true");
	settings->setDefault("active_object_send_range_blocks", "3");
	settings->setDefault("active_block_range", "2");
	settings->setDefault("abm_random", "false");
	settings->setDefault("enable_force_load", "true");
	settings->setDefault("max_simultaneous_block_sends_per_client", "50");
	settings->setDefault("max_block_send_distance", "30");
	settings->setDefault("max_block_generate_distance", "7");
	settings->setDefault("max_clearobjects_extra_loaded_blocks", "4096");
	settings->setDefault("time_send_interval", "5");
	settings->setDefault("server_unload_unused_data_timeout", "610");
	settings->setDefault("max_objects_per_block", "100");
	settings->setDefault("full_block_send_enable_min_time_from_building", "2.0");
	settings->setDefault("dedicated_server_step", "0.1");
	settings->setDefault("ignore_world_load_errors", "true");
	settings->setDefault("emergequeue_limit_diskonly", ""); // autodetect from number of cpus
	settings->setDefault("emergequeue_limit_generate", ""); // autodetect from number of cpus
	settings->setDefault("emergequeue_limit_total", ""); // autodetect from number of cpus
	settings->setDefault("num_emerge_threads", "");
	// Storage
	settings->setDefault("server_map_save_interval", "300");
	settings->setDefault("sqlite_synchronous", "1");
	settings->setDefault("save_generated_block", "true");
	// IPv6
	settings->setDefault("enable_ipv6", "true");
	settings->setDefault("ipv6_server", "false");
	
	//
	// Movement, Physics and animation
	//
	
	// Movement
	settings->setDefault("enable_movement_fov", "true");
	settings->setDefault("aux1_descends", "false");
	settings->setDefault("doubletap_jump", "false");
	settings->setDefault("always_fly_fast", "true");
	settings->setDefault("free_move", "false");
	settings->setDefault("noclip", "false");
	settings->setDefault("continuous_forward", "false");
	settings->setDefault("fast_move", "false");
	
	// Physics
	settings->setDefault("movement_acceleration_default", "3");
	settings->setDefault("movement_acceleration_air", "2");
	settings->setDefault("movement_acceleration_fast", "10");
	settings->setDefault("movement_speed_walk", "4");
	settings->setDefault("movement_speed_crouch", "1.35");
	settings->setDefault("movement_speed_fast", "20");
	settings->setDefault("movement_speed_climb", "2");
	settings->setDefault("movement_speed_jump", "6.5");
	settings->setDefault("movement_liquid_fluidity", "1");
	settings->setDefault("movement_liquid_fluidity_smooth", "0.5");
	settings->setDefault("movement_liquid_sink", "10");
	settings->setDefault("movement_gravity", "9.81");
	settings->setDefault("view_bobbing", "true");
	settings->setDefault("view_bobbing_amount", "1.0");
	settings->setDefault("fall_bobbing_amount", "0.0");
	
	// Player model animations (sent to client)
	settings->setDefault("animation_default_start", "0");
	settings->setDefault("animation_default_stop", "79");
	settings->setDefault("animation_walk_start", "168");
	settings->setDefault("animation_walk_stop", "187");
	settings->setDefault("animation_dig_start", "189");
	settings->setDefault("animation_dig_stop", "198");
	settings->setDefault("animation_wd_start", "200");
	settings->setDefault("animation_wd_stop", "219");

	//
	// Tweaks for windows
	//

#if defined(_WIN32)
		settings->setDefault("more_threads", "false");
#else
		settings->setDefault("num_emerge_threads", ""); // autodetect from number of cpus
		settings->setDefault("more_threads", "true");
#endif

#if !defined(_WIN32) && !CMAKE_USE_IPV4_DEFAULT && ENET_IPV6
	settings->setDefault("ipv6_server", "true"); // problems on all windows versions (unable to play in local game)
#endif

#if !defined(SERVER) && defined(_MSC_VER)
	settings->setDefault("console_enabled", debug ? "true" : "false");
#endif

	//
	// Tweaks for Android
	//
	
#ifdef __ANDROID__
	settings->setDefault("screenW", "0");
	settings->setDefault("screenH", "0");
	settings->setDefault("enable_shaders", "false");
	settings->setDefault("fullscreen", "true");
	settings->setDefault("enable_particles", "false");
	settings->setDefault("video_driver", "ogles1");
	settings->setDefault("touchtarget", "true");
	settings->setDefault("main_menu_script","/sdcard/freeminer/builtin/mainmenu/init_android.lua");
	settings->setDefault("TMPFolder","/sdcard/freeminer/tmp/");
	settings->setDefault("touchscreen_threshold","20");
	settings->setDefault("smooth_lighting", "false");
	/*
	settings->setDefault("max_simultaneous_block_sends_per_client", "3");
	settings->setDefault("emergequeue_limit_diskonly", "8");
	settings->setDefault("emergequeue_limit_generate", "8");
	settings->setDefault("viewing_range_nodes_max", "50");
	settings->setDefault("viewing_range_nodes_min", "20");
	*/
	settings->setDefault("inventory_image_hack", "false");

	//check for device with small screen
	float x_inches = ((double) porting::getDisplaySize().X /
			(160 * porting::getDisplayDensity()));
	if (x_inches  < 3.5) {
		settings->setDefault("gui_scaling", "0.6");
	}
	else if (x_inches < 4.5) {
		settings->setDefault("gui_scaling", "0.7");
	}
	settings->setDefault("curl_verify_cert","false");

	settings->setDefault("farmesh", "1");
	settings->setDefault("farmesh_step", "1");
#endif
}

void late_init_default_settings(Settings* settings)
{
#ifndef SERVER
	std::stringstream fontsize;
	fontsize << floor(
			DEFAULT_FONT_SIZE *
			porting::getDisplayDensity() *
			settings->getFloat("gui_scaling")
			);

	settings->setDefault("font_size", fontsize.str());
	settings->setDefault("mono_font_size", fontsize.str());
	settings->setDefault("fallback_font_size", fontsize.str());
#endif
}

void override_default_settings(Settings *settings, Settings *from)
{
	std::vector<std::string> names = from->getNames();
	for(size_t i=0; i<names.size(); i++){
		const std::string &name = names[i];
		settings->setDefault(name, from->get(name));
	}
}

