
/*
main.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifdef _MSC_VER
#ifndef SERVER // Dedicated server isn't linked with Irrlicht
	#pragma comment(lib, "Irrlicht.lib")
	// This would get rid of the console window
	//#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif
	//#pragma comment(lib, "zlibwapi.lib")
	#pragma comment(lib, "Shell32.lib")
#endif

#include "irrlicht.h" // createDevice

#include "mainmenumanager.h"
#include "irrlichttypes_extrabloated.h"
#include "debug.h"
#include "unittest/test.h"
#include "server.h"
#include "filesys.h"
#include "version.h"
#include "guiMainMenu.h"
#include "game.h"
#include "defaultsettings.h"
#include "gettext.h"
#include "profiler.h"
#include "log_types.h"
#include "quicktune.h"
#include "httpfetch.h"
#include "guiEngine.h"
#include "map.h"
#include "fontengine.h"
#include "gameparams.h"
#include "database.h"
#ifndef SERVER
#include "client/clientlauncher.h"
#endif

#if USE_ENET
// todo: move to connection
#include "enet/enet.h"
#endif

#ifdef HAVE_TOUCHSCREENGUI
#include "touchscreengui.h"
#endif

#define DEBUGFILE "debug.txt"
#define DEFAULT_SERVER_PORT 30000

typedef std::map<std::string, ValueSpec> OptionList;

/**********************************************************************
 * Private functions
 **********************************************************************/

static bool get_cmdline_opts(int argc, char *argv[], Settings *cmd_args);
static void set_allowed_options(OptionList *allowed_options);

static void print_help(const OptionList &allowed_options);
static void print_allowed_options(const OptionList &allowed_options);
static void print_version();
static void print_worldspecs(const std::vector<WorldSpec> &worldspecs,
							 std::ostream &os);
static void print_modified_quicktune_values();

static void list_game_ids();
static void list_worlds();
static void setup_log_params(const Settings &cmd_args);
static bool create_userdata_path();
static bool init_common(int *log_level, const Settings &cmd_args, int argc, char *argv[]);
static void startup_message();
static bool read_config_file(const Settings &cmd_args);
static void init_debug_streams(int *log_level, const Settings &cmd_args);

static bool game_configure(GameParams *game_params, const Settings &cmd_args);
static void game_configure_port(GameParams *game_params, const Settings &cmd_args);

static bool game_configure_world(GameParams *game_params, const Settings &cmd_args);
static bool get_world_from_cmdline(GameParams *game_params, const Settings &cmd_args);
static bool get_world_from_config(GameParams *game_params, const Settings &cmd_args);
static bool auto_select_world(GameParams *game_params);
static std::string get_clean_world_path(const std::string &path);

static bool game_configure_subgame(GameParams *game_params, const Settings &cmd_args);
static bool get_game_from_cmdline(GameParams *game_params, const Settings &cmd_args);
static bool determine_subgame(GameParams *game_params);

static bool run_dedicated_server(const GameParams &game_params, const Settings &cmd_args);
static bool migrate_database(const GameParams &game_params, const Settings &cmd_args);

/**********************************************************************/

/*
	gettime.h implementation
*/

#ifdef SERVER

u32 getTimeMs()
{
	/* Use imprecise system calls directly (from porting.h) */
	return porting::getTime(PRECISION_MILLI);
}

u32 getTime(TimePrecision prec)
{
	return porting::getTime(prec);
}

#endif

class StderrLogOutput: public ILogOutput
{
public:
	/* line: Full line with timestamp, level and thread */
	void printLog(const std::string &line)
	{
		std::cerr << line << std::endl;
	}
} main_stderr_log_out;

class DstreamNoStderrLogOutput: public ILogOutput
{
public:
	/* line: Full line with timestamp, level and thread */
	void printLog(const std::string &line)
	{
		dstream_no_stderr << line << std::endl;
	}
} main_dstream_no_stderr_log_out;

static OptionList allowed_options;

int main(int argc, char *argv[])
{
	int retval = 0;

#if USE_ENET
	if (enet_initialize() != 0) {
		std::cerr << "enet failed to initialize\n";
		return EXIT_FAILURE;
	}
	atexit(enet_deinitialize);
#endif

	debug_set_exception_handler();

	log_add_output_maxlev(&main_stderr_log_out, LMT_ACTION);
	log_add_output_all_levs(&main_dstream_no_stderr_log_out);

	log_register_thread("main");

	Settings cmd_args;
	bool cmd_args_ok = get_cmdline_opts(argc, argv, &cmd_args);
	if (!cmd_args_ok
			|| cmd_args.getFlag("help")
			|| cmd_args.exists("nonopt1")) {
		print_help(allowed_options);
		return cmd_args_ok ? 0 : 1;
	}

	if (cmd_args.getFlag("version")) {
		print_version();
		return 0;
	}

	setup_log_params(cmd_args);

	porting::signal_handler_init();
	porting::initializePaths();

	if (!create_userdata_path()) {
		errorstream << "Cannot create user data directory "<< porting::path_user << std::endl;
		//return 1;
	}

	// Initialize debug stacks
	debug_stacks_init();
	DSTACK(__FUNCTION_NAME);

	// Debug handler
	BEGIN_DEBUG_EXCEPTION_HANDLER

	// List gameids if requested
	if (cmd_args.exists("gameid") && cmd_args.get("gameid") == "list") {
		list_game_ids();
		return 0;
	}

	// List worlds if requested
	if (cmd_args.exists("world") && cmd_args.get("world") == "list") {
		list_worlds();
		return 0;
	}

	GameParams game_params;
	if (!init_common(&game_params.log_level, cmd_args, argc, argv))
		return 1;

#if !defined(__ANDROID__) && !defined(_MSC_VER)
	// Run unit tests
	if (cmd_args.getFlag("run-unittests")) {
		run_tests();
		return 0;
	}
#endif

#ifdef SERVER
	game_params.is_dedicated_server = true;
#else
	game_params.is_dedicated_server = cmd_args.getFlag("server");
#endif

	if (!game_configure(&game_params, cmd_args))
		return 1;

	sanity_check(game_params.world_path != "");

	infostream << "Using commanded world path ["
	           << game_params.world_path << "]" << std::endl;

	//Run dedicated server if asked to or no other option
	g_settings->set("server_dedicated",
			game_params.is_dedicated_server ? "true" : "false");

	if (game_params.is_dedicated_server)
		return run_dedicated_server(game_params, cmd_args) ? 0 : 1;

#ifndef SERVER
	ClientLauncher launcher;
	retval = launcher.run(game_params, cmd_args) ? 0 : 1;
#else
	retval = 0;
#endif

	// Update configuration file
	if (g_settings_path != "")
		g_settings->updateConfigFile(g_settings_path.c_str());

	print_modified_quicktune_values();

	// Stop httpfetch thread (if started)
	httpfetch_cleanup();

	END_DEBUG_EXCEPTION_HANDLER(errorstream)

	return retval;
}


/*****************************************************************************
 * Startup / Init
 *****************************************************************************/


static bool get_cmdline_opts(int argc, char *argv[], Settings *cmd_args)
{
	set_allowed_options(&allowed_options);

	return cmd_args->parseCommandLine(argc, argv, allowed_options);
}

static void set_allowed_options(OptionList *allowed_options)
{
	allowed_options->clear();

	allowed_options->insert(std::make_pair("help", ValueSpec(VALUETYPE_FLAG,
			_("Show allowed options"))));
	allowed_options->insert(std::make_pair("version", ValueSpec(VALUETYPE_FLAG,
			_("Show version information"))));
	allowed_options->insert(std::make_pair("config", ValueSpec(VALUETYPE_STRING,
			_("Load configuration from specified file"))));
	allowed_options->insert(std::make_pair("port", ValueSpec(VALUETYPE_STRING,
			_("Set network port (UDP)"))));
	allowed_options->insert(std::make_pair("run-unittests", ValueSpec(VALUETYPE_FLAG,
			_("Run the unit tests and exit"))));
	allowed_options->insert(std::make_pair("map-dir", ValueSpec(VALUETYPE_STRING,
			_("Same as --world (deprecated)"))));
	allowed_options->insert(std::make_pair("world", ValueSpec(VALUETYPE_STRING,
			_("Set world path (implies local game) ('list' lists all)"))));
	allowed_options->insert(std::make_pair("worldname", ValueSpec(VALUETYPE_STRING,
			_("Set world by name (implies local game)"))));
	allowed_options->insert(std::make_pair("quiet", ValueSpec(VALUETYPE_FLAG,
			_("Print to console errors only"))));
	allowed_options->insert(std::make_pair("info", ValueSpec(VALUETYPE_FLAG,
			_("Print more information to console"))));
	allowed_options->insert(std::make_pair("verbose",  ValueSpec(VALUETYPE_FLAG,
			_("Print even more information to console"))));
	allowed_options->insert(std::make_pair("trace", ValueSpec(VALUETYPE_FLAG,
			_("Print enormous amounts of information to log and console"))));
	allowed_options->insert(std::make_pair("logfile", ValueSpec(VALUETYPE_STRING,
			_("Set logfile path ('' = no logging)"))));
	allowed_options->insert(std::make_pair("gameid", ValueSpec(VALUETYPE_STRING,
			_("Set gameid (\"--gameid list\" prints available ones)"))));
	allowed_options->insert(std::make_pair("migrate", ValueSpec(VALUETYPE_STRING,
			_("Migrate from current map backend to another (Only works when using freeminerserver or with --server)"))));
	allowed_options->insert(std::make_pair("autoexit", ValueSpec(VALUETYPE_STRING,
			_("Exit after X seconds"))));
#ifndef SERVER
	allowed_options->insert(std::make_pair("videomodes", ValueSpec(VALUETYPE_FLAG,
			_("Show available video modes"))));
	allowed_options->insert(std::make_pair("speedtests", ValueSpec(VALUETYPE_FLAG,
			_("Run speed tests"))));
	allowed_options->insert(std::make_pair("address", ValueSpec(VALUETYPE_STRING,
			_("Address to connect to. ('' = local game)"))));
	allowed_options->insert(std::make_pair("random-input", ValueSpec(VALUETYPE_FLAG,
			_("Enable random user input, for testing"))));
	allowed_options->insert(std::make_pair("server", ValueSpec(VALUETYPE_FLAG,
			_("Run dedicated server"))));
	allowed_options->insert(std::make_pair("name", ValueSpec(VALUETYPE_STRING,
			_("Set player name"))));
	allowed_options->insert(std::make_pair("password", ValueSpec(VALUETYPE_STRING,
			_("Set password"))));
	allowed_options->insert(std::make_pair("go", ValueSpec(VALUETYPE_FLAG,
			_("Disable main menu"))));
#endif

}

static void print_help(const OptionList &allowed_options)
{
	dstream << _("Allowed options:") << std::endl;
	print_allowed_options(allowed_options);
}

static void print_allowed_options(const OptionList &allowed_options)
{
	for (OptionList::const_iterator i = allowed_options.begin();
			i != allowed_options.end(); ++i) {
		std::ostringstream os1(std::ios::binary);
		os1 << "  --" << i->first;
		if (i->second.type != VALUETYPE_FLAG)
			os1 << _(" <value>");

		dstream << padStringRight(os1.str(), 24);

		if (i->second.help != NULL)
			dstream << i->second.help;

		dstream << std::endl;
	}
}

static void print_version()
{
	dstream << PROJECT_NAME_C " " << g_version_hash << std::endl;
#ifndef SERVER
	dstream << "Using Irrlicht " << IRRLICHT_SDK_VERSION << std::endl;
#endif
	dstream << "Build info: " << g_build_info << std::endl;
}

static void list_game_ids()
{
	std::set<std::string> gameids = getAvailableGameIds();
	for (std::set<std::string>::const_iterator i = gameids.begin();
			i != gameids.end(); i++)
		dstream << (*i) <<std::endl;
}

static void list_worlds()
{
	dstream << _("Available worlds:") << std::endl;
	std::vector<WorldSpec> worldspecs = getAvailableWorlds();
	print_worldspecs(worldspecs, dstream);
}

static void print_worldspecs(const std::vector<WorldSpec> &worldspecs,
							 std::ostream &os)
{
	for (size_t i = 0; i < worldspecs.size(); i++) {
		std::string name = worldspecs[i].name;
		std::string path = worldspecs[i].path;
		if (name.find(" ") != std::string::npos)
			name = std::string("'") + name + "'";
		path = std::string("'") + path + "'";
		name = padStringRight(name, 14);
		os << "  " << name << " " << path << std::endl;
	}
}

static void print_modified_quicktune_values()
{
	bool header_printed = false;
	std::vector<std::string> names = getQuicktuneNames();

	for (u32 i = 0; i < names.size(); i++) {
		QuicktuneValue val = getQuicktuneValue(names[i]);
		if (!val.modified)
			continue;
		if (!header_printed) {
			dstream << "Modified quicktune values:" << std::endl;
			header_printed = true;
		}
		dstream << names[i] << " = " << val.getString() << std::endl;
	}
}

static void setup_log_params(const Settings &cmd_args)
{
	// Quiet mode, print errors only
	if (cmd_args.getFlag("quiet")) {
		log_remove_output(&main_stderr_log_out);
		log_add_output_maxlev(&main_stderr_log_out, LMT_ERROR);
	}

	// If trace is enabled, enable logging of certain things
	if (cmd_args.getFlag("trace")) {
		dstream << _("Enabling trace level debug output") << std::endl;
		log_trace_level_enabled = true;
		dout_con_ptr = &verbosestream; // this is somewhat old crap
		socket_enable_debug_output = true; // socket doesn't use log.h
	}

	// In certain cases, output info level on stderr
	if (cmd_args.getFlag("info") || cmd_args.getFlag("verbose") ||
			cmd_args.getFlag("trace") || cmd_args.getFlag("speedtests"))
		log_add_output(&main_stderr_log_out, LMT_INFO);

	// In certain cases, output verbose level on stderr
	if (cmd_args.getFlag("verbose") || cmd_args.getFlag("trace"))
		log_add_output(&main_stderr_log_out, LMT_VERBOSE);
}

static bool create_userdata_path()
{
	bool success;

#ifdef __ANDROID__
	porting::initAndroid();

	porting::setExternalStorageDir(porting::jnienv);
	if (!fs::PathExists(porting::path_user)) {
		success = fs::CreateDir(porting::path_user);
	} else {
		success = true;
	}
	porting::copyAssets();
#else
	// Create user data directory
	success = fs::CreateDir(porting::path_user);
#endif

	infostream << "path_share = " << porting::path_share << std::endl;
	infostream << "path_user  = " << porting::path_user << std::endl;

	return success;
}

static bool init_common(int *log_level, const Settings &cmd_args, int argc, char *argv[])
{
	startup_message();
	set_default_settings(g_settings);

	// Initialize sockets
	sockets_init();
	atexit(sockets_cleanup);

	if (!read_config_file(cmd_args))
		return false;

	init_debug_streams(log_level, cmd_args);

	g_time_taker_enabled = g_settings->getU16("time_taker_enabled") ? g_settings->getU16("time_taker_enabled") : ((g_settings->getFloat("profiler_print_interval") || *log_level >= LMT_INFO) ? 100 : 0);

	int autoexit_ = 0;
	cmd_args.getS32NoEx("autoexit", autoexit_);
	g_profiler_enabled = g_settings->getFloat("profiler_print_interval") || autoexit_;

	// Initialize random seed
	srand(time(0));
	mysrand(time(0));

	// Initialize HTTP fetcher
	httpfetch_init(g_settings->getS32("curl_parallel_limit"));

#ifdef _MSC_VER
	init_gettext((porting::path_share + DIR_DELIM + "locale").c_str(),
		g_settings->get("language"), argc, argv);
	//Remove windows console window if settings request
	if (!g_settings->getBool("console_enabled"))
		FreeConsole();

#else
	init_gettext((porting::path_share + DIR_DELIM + "locale").c_str(),
		g_settings->get("language"));
#endif

	return true;
}

static void startup_message()
{
	infostream << PROJECT_NAME << " " << _("with")
	           << " SER_FMT_VER_HIGHEST_READ="
               << (int)SER_FMT_VER_HIGHEST_READ << ", "
               << g_build_info << std::endl;
}

static bool read_config_file(const Settings &cmd_args)
{
	// Path of configuration file in use

	if (cmd_args.exists("config")) {
		bool r = g_settings->readConfigFile(cmd_args.get("config").c_str());
		if (!r) {
			errorstream << "Could not read configuration from \""
			            << cmd_args.get("config") << "\"" << std::endl;
			return false;
		}
		g_settings_path = cmd_args.get("config");
	} else {
		std::vector<std::string> filenames;
		filenames.push_back(porting::path_user + DIR_DELIM + "freeminer");
		// Legacy configuration file location
		filenames.push_back(porting::path_user +
				DIR_DELIM + ".." + DIR_DELIM + "freeminer");

#if RUN_IN_PLACE
		// Try also from a lower level (to aid having the same configuration
		// for many RUN_IN_PLACE installs)
		filenames.push_back(porting::path_user +
				DIR_DELIM + ".." + DIR_DELIM + ".." + DIR_DELIM + "freeminer");
#endif

		for (size_t i = 0; i < filenames.size(); i++) {

			if (g_settings->readJsonFile(filenames[i] + ".json")) {
				g_settings_path = filenames[i] + ".json";
				break;
			}

			bool r = g_settings->readConfigFile((filenames[i] + ".conf").c_str());
			if (r) {
				g_settings_path = filenames[i] + ".conf";
				break;
			}
		}

		// If no path found, use the first one (menu creates the file)
		if (g_settings_path == "")
			g_settings_path = filenames[0] + ".conf";
	}

	return true;
}

static void init_debug_streams(int *log_level, const Settings &cmd_args)
{
#if RUN_IN_PLACE
	std::string logfile = DEBUGFILE;
#else
	std::string logfile = porting::path_user + DIR_DELIM + DEBUGFILE;
#endif
	if (cmd_args.exists("logfile"))
		logfile = cmd_args.get("logfile");

	log_remove_output(&main_dstream_no_stderr_log_out);
	*log_level = g_settings->getS32("debug_log_level");

	if (*log_level == 0) //no logging
		logfile = "";
	if (*log_level < 0) {
		dstream << "WARNING: Supplied debug_log_level < 0; Using 0" << std::endl;
		*log_level = 0;
	} else if (*log_level > LMT_NUM_VALUES) {
		dstream << "WARNING: Supplied debug_log_level > " << LMT_NUM_VALUES
		        << "; Using " << LMT_NUM_VALUES << std::endl;
		*log_level = LMT_NUM_VALUES;
	}

	log_add_output_maxlev(&main_dstream_no_stderr_log_out,
			(LogMessageLevel)(*log_level - 1));

	debugstreams_init(false, logfile == "" ? NULL : logfile.c_str());

	infostream << "logfile = " << logfile << std::endl;

	atexit(debugstreams_deinit);
}

static bool game_configure(GameParams *game_params, const Settings &cmd_args)
{
	game_configure_port(game_params, cmd_args);

	if (!game_configure_world(game_params, cmd_args)) {
		errorstream << "No world path specified or found." << std::endl;
		return false;
	}

	game_configure_subgame(game_params, cmd_args);

	return true;
}

static void game_configure_port(GameParams *game_params, const Settings &cmd_args)
{
	if (cmd_args.exists("port"))
		game_params->socket_port = cmd_args.getU16("port");
	else
		game_params->socket_port = g_settings->getU16("port");

	if (game_params->socket_port == 0)
		game_params->socket_port = DEFAULT_SERVER_PORT;
}

static bool game_configure_world(GameParams *game_params, const Settings &cmd_args)
{
	if (get_world_from_cmdline(game_params, cmd_args))
		return true;
	if (get_world_from_config(game_params, cmd_args))
		return true;

	return auto_select_world(game_params);
}

static bool get_world_from_cmdline(GameParams *game_params, const Settings &cmd_args)
{
	std::string commanded_world = "";

	// World name
	std::string commanded_worldname = "";
	if (cmd_args.exists("worldname"))
		commanded_worldname = cmd_args.get("worldname");

	// If a world name was specified, convert it to a path
	if (commanded_worldname != "") {
		// Get information about available worlds
		std::vector<WorldSpec> worldspecs = getAvailableWorlds();
		bool found = false;
		for (u32 i = 0; i < worldspecs.size(); i++) {
			std::string name = worldspecs[i].name;
			if (name == commanded_worldname) {
				dstream << _("Using world specified by --worldname on the "
					"command line") << std::endl;
				commanded_world = worldspecs[i].path;
				found = true;
				break;
			}
		}
		if (!found) {
			dstream << _("World") << " '" << commanded_worldname
			        << _("' not available. Available worlds:") << std::endl;
			print_worldspecs(worldspecs, dstream);
			return false;
		}

		game_params->world_path = get_clean_world_path(commanded_world);
		return commanded_world != "";
	}

	if (cmd_args.exists("world"))
		commanded_world = cmd_args.get("world");
	else if (cmd_args.exists("map-dir"))
		commanded_world = cmd_args.get("map-dir");
	else if (cmd_args.exists("nonopt0")) // First nameless argument
		commanded_world = cmd_args.get("nonopt0");

	game_params->world_path = get_clean_world_path(commanded_world);
	return commanded_world != "";
}

static bool get_world_from_config(GameParams *game_params, const Settings &cmd_args)
{
	// World directory
	std::string commanded_world = "";

	if (g_settings->exists("map-dir"))
		commanded_world = g_settings->get("map-dir");

	game_params->world_path = get_clean_world_path(commanded_world);

	return commanded_world != "";
}

static bool auto_select_world(GameParams *game_params)
{
	// No world was specified; try to select it automatically
	// Get information about available worlds

	verbosestream << _("Determining world path") << std::endl;

	std::vector<WorldSpec> worldspecs = getAvailableWorlds();
	std::string world_path;

	// If there is only a single world, use it
	if (worldspecs.size() == 1) {
		world_path = worldspecs[0].path;
		dstream <<_("Automatically selecting world at") << " ["
		        << world_path << "]" << std::endl;
	// If there are multiple worlds, list them
	} else if (worldspecs.size() > 1 && game_params->is_dedicated_server) {
		dstream << _("Multiple worlds are available.") << std::endl;
		dstream << _("Please select one using --worldname <name>"
				" or --world <path>") << std::endl;
		print_worldspecs(worldspecs, dstream);
		return false;
	// If there are no worlds, automatically create a new one
	} else {
		// This is the ultimate default world path
		world_path = porting::path_user + DIR_DELIM + "worlds" +
				DIR_DELIM + "world";
		infostream << "Creating default world at ["
		           << world_path << "]" << std::endl;
	}

	assert(world_path != "");	// Post-condition
	game_params->world_path = world_path;
	return true;
}

static std::string get_clean_world_path(const std::string &path)
{
	const std::string worldmt = "world.mt";
	std::string clean_path;

	if (path.size() > worldmt.size()
			&& path.substr(path.size() - worldmt.size()) == worldmt) {
		dstream << _("Supplied world.mt file - stripping it off.") << std::endl;
		clean_path = path.substr(0, path.size() - worldmt.size());
	} else {
		clean_path = path;
	}
	return path;
}


static bool game_configure_subgame(GameParams *game_params, const Settings &cmd_args)
{
	bool success;

	success = get_game_from_cmdline(game_params, cmd_args);
	if (!success)
		success = determine_subgame(game_params);

	return success;
}

static bool get_game_from_cmdline(GameParams *game_params, const Settings &cmd_args)
{
	SubgameSpec commanded_gamespec;

	if (cmd_args.exists("gameid")) {
		std::string gameid = cmd_args.get("gameid");
		commanded_gamespec = findSubgame(gameid);
		if (!commanded_gamespec.isValid()) {
			errorstream << "Game \"" << gameid << "\" not found" << std::endl;
			return false;
		}
		dstream << _("Using game specified by --gameid on the command line")
		        << std::endl;
		game_params->game_spec = commanded_gamespec;
		return true;
	}

	return false;
}

static bool determine_subgame(GameParams *game_params)
{
	SubgameSpec gamespec;

	assert(game_params->world_path != "");	// Pre-condition

	verbosestream << _("Determining gameid/gamespec") << std::endl;
	// If world doesn't exist
	if (game_params->world_path != ""
			&& !getWorldExists(game_params->world_path)) {
		// Try to take gamespec from command line
		if (game_params->game_spec.isValid()) {
			gamespec = game_params->game_spec;
			infostream << "Using commanded gameid [" << gamespec.id << "]" << std::endl;
		} else { // Otherwise we will be using "minetest"
			gamespec = findSubgame(g_settings->get("default_game"));
			infostream << "Using default gameid [" << gamespec.id << "]" << std::endl;
			if (!gamespec.isValid()) {
				errorstream << "Subgame specified in default_game ["
				            << g_settings->get("default_game")
				            << "] is invalid." << std::endl;
				return false;
			}
		}
	} else { // World exists
		std::string world_gameid = getWorldGameId(game_params->world_path, false);
		// If commanded to use a gameid, do so
		if (game_params->game_spec.isValid()) {
			gamespec = game_params->game_spec;
			if (game_params->game_spec.id != world_gameid) {
				errorstream << "WARNING: Using commanded gameid ["
				            << gamespec.id << "]" << " instead of world gameid ["
				            << world_gameid << "]" << std::endl;
			}
		} else {
			// If world contains an embedded game, use it;
			// Otherwise find world from local system.
			gamespec = findWorldSubgame(game_params->world_path);
			infostream << "Using world gameid [" << gamespec.id << "]" << std::endl;
		}
	}

	if (!gamespec.isValid()) {
		errorstream << "Subgame [" << gamespec.id << "] could not be found."
		            << std::endl;
		return false;
	}

	game_params->game_spec = gamespec;
	return true;
}


/*****************************************************************************
 * Dedicated server
 *****************************************************************************/
static bool run_dedicated_server(const GameParams &game_params, const Settings &cmd_args)
{
	DSTACK("Dedicated server branch");

	verbosestream << _("Using world path") << " ["
	              << game_params.world_path << "]" << std::endl;
	verbosestream << _("Using gameid") << " ["
	              << game_params.game_spec.id << "]" << std::endl;

	// Bind address
	std::string bind_str = g_settings->get("bind_address");
	Address bind_addr(0, 0, 0, 0, game_params.socket_port);

	if (g_settings->getBool("ipv6_server")) {
		bind_addr.setAddress(in6addr_any);
	}
	try {
		if (!bind_str.empty())
		bind_addr.Resolve(bind_str.c_str());
	} catch (ResolveError &e) {
		infostream << "Resolving bind address \"" << bind_str
		           << "\" failed: " << e.what()
		           << " -- Listening on all addresses." << std::endl;
	}
	if (bind_addr.isIPv6() && !g_settings->getBool("enable_ipv6")) {
		errorstream << "Unable to listen on "
		            << bind_addr.serializeString()
		            << L" because IPv6 is disabled" << std::endl;
		return false;
	}

	// Database migration
	if (cmd_args.exists("migrate"))
		return migrate_database(game_params, cmd_args);

	// Create server
	Server server(game_params.world_path,
			game_params.game_spec, false, bind_addr.isIPv6());

	server.start(bind_addr);

	int autoexit_ = 0;
	cmd_args.getS32NoEx("autoexit", autoexit_);
	server.m_autoexit = autoexit_;

	// Run server
	bool &kill = *porting::signal_handler_killstatus();
	dedicated_server_loop(server, kill);

	return true;
}

static bool migrate_database(const GameParams &game_params, const Settings &cmd_args)
{
	std::string migrate_to = cmd_args.get("migrate");
	Settings world_mt;
	std::string world_mt_path = game_params.world_path + DIR_DELIM + "world.mt";
	if (!world_mt.readConfigFile(world_mt_path.c_str())) {
		errorstream << "Cannot read world.mt!" << std::endl;
		return false;
	}
	if (!world_mt.exists("backend")) {
		errorstream << "Please specify your current backend in world.mt:"
			<< std::endl
			<< "	backend = {sqlite3|leveldb|redis|dummy}"
			<< std::endl;
		return false;
	}
	std::string backend = world_mt.get("backend");
	if (backend == migrate_to) {
		errorstream << "Cannot migrate: new backend is same"
			<< " as the old one" << std::endl;
		return false;
	}
	Database *old_db = ServerMap::createDatabase(backend, game_params.world_path, world_mt),
		*new_db = ServerMap::createDatabase(migrate_to, game_params.world_path, world_mt);

	u32 count = 0;
	time_t last_update_time = 0;
	bool &kill = *porting::signal_handler_killstatus();

	std::vector<v3s16> blocks;
	old_db->listAllLoadableBlocks(blocks);
	new_db->beginSave();
	for (std::vector<v3s16>::const_iterator it = blocks.begin(); it != blocks.end(); ++it) {
		if (kill) return false;

		/* old slow migrate, but better for future leveldb
		MapBlock *block = old_map.loadBlock(*i);
		if (!block) {
			errorstream << "Failed to load block " << *i << ", skipping it."<<std::endl;
		}
		else {
			old_map.saveBlock(block, new_db);
			old_map.m_blocks.erase(block->getPos());
			delete block;
		}
		*/

		const std::string &data = old_db->loadBlock(*it);
		if (!data.empty()) {
			new_db->saveBlock(*it, data);
		} else {
			errorstream << "Failed to load block " << PP(*it) << ", skipping it." << std::endl;
		}
		if (++count % 0xFF == 0 && time(NULL) - last_update_time >= 1) {
			std::cerr << " Migrated " << count << " blocks, "
				<< (100.0 * count / blocks.size()) << "% completed.\r";
			new_db->endSave();
			new_db->beginSave();
			last_update_time = time(NULL);
		}
	}
	std::cerr << std::endl;
	new_db->endSave();
	delete old_db;
	delete new_db;

	actionstream << "Successfully migrated " << count << " blocks" << std::endl;
	world_mt.set("backend", migrate_to);
	if (!world_mt.updateConfigFile(world_mt_path.c_str()))
		errorstream << "Failed to update world.mt!" << std::endl;
	else
		actionstream << "world.mt updated" << std::endl;

	return true;
}

