// SPDX-License-Identifier: LGPL-2.1-or-later
#include "test.h"
#include "fm_gravity.h"
#include "mapblock.h"
#include "mapgen/fm_earth_gravity.h"
#include "network/fm_gravity_sync.h"
#include "network/networkpacket.h"
#include "emerge.h"
#include "mock_server.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "settings.h"
#include <algorithm>

class TestFmGravity : public TestBase
{
public:
	TestFmGravity() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmGravity"; }
	void runTests(IGameDef *) override
	{
		TEST(testSources);
		TEST(testMassSources);
		TEST(testValidation);
		TEST(testBlockCoordinates);
		TEST(testBackground);
		TEST(testEarthProjectionDefaults);
		TEST(testTorusGravityDefaults);
		TEST(testEarthEnvironmentDefaults);
		TEST(testGravityInitPacket);
	}

	void testMassSources()
	{
		FmGravityField field;
		UASSERT(field.setSources("107.5,7.5,7.5,mass=1e9,1"));
		const v3f gravity = field.sample(v3bpos_t(0, 0, 0));
		UASSERT(std::abs(gravity.X / BS - 0.0066736326f) < 0.00000001f);
		UASSERT(gravity.Y == 0 && gravity.Z == 0);
		UASSERT(field.setSources(
				"23.5,7.5,7.5,mass=1e9,16; -8.5,7.5,7.5,0.26071484375,16"));
		UASSERT(field.sample(v3bpos_t(0, 0, 0)).getLength() < 0.00001f);
		UASSERT(field.setSources("7.5,7.5,7.5,mass=1e9,1"));
		UASSERT(field.sample(v3bpos_t(0, 0, 0)).getLength() == 0);
	}

	void testSources()
	{
		FmGravityField field;
		UASSERT(field.setSources("23.5,7.5,7.5,10,16"));
		v3f one = field.sample(v3bpos_t(0, 0, 0));
		UASSERT(std::abs(one.X / BS - 5.0f) < 0.001f);
		UASSERT(one.Y == 0 && one.Z == 0);
		const v3f combined = field.sampleWithBackground(v3bpos_t(0, 0, 0), 9.81f * BS);
		UASSERT(std::abs(combined.X / BS - 5.0f) < 0.001f);
		UASSERT(std::abs(combined.Y / BS + 9.81f) < 0.001f);
		UASSERT(field.setSources("23.5,7.5,7.5,10,16; -8.5,7.5,7.5,10,16"));
		UASSERT(field.sample(v3bpos_t(0, 0, 0)).getLength() < 0.001f);
		UASSERT(field.setSources("7.5,7.5,7.5,10,16"));
		UASSERT(field.sample(v3bpos_t(0, 0, 0)).getLength() == 0);
	}

	void testValidation()
	{
		FmGravityField field;
		UASSERT(field.setSources("0,0,0,9.81,32"));
		UASSERT(!field.setSources("0,0,0,9.81,0"));
		UASSERT(!field.empty());
		UASSERT(!field.setSources("0,0,0,9.81,32;bad"));
		UASSERT(!field.setSources("0,0,0,-1,32"));
		UASSERT(!field.setSources("0,0,0,mass=-1e300,0.000001"));
		UASSERT(!field.setSources("0,0,0,mass=1e300,0.000001"));
		UASSERT(!field.setSources("0,0,0,mass=1e9x,32"));
		UASSERT(!field.setSources("0,0,0,mass=1e9,0"));
		UASSERT(field.setSources(""));
		UASSERT(field.empty());
		UASSERT(field.sampleWithBackground(v3bpos_t(0, 0, 0), 0).getLength() == 0);
	}

	void testBlockCoordinates()
	{
		UASSERT(getNodeBlockPos(v3pos_t(-1, -16, -17)) == v3bpos_t(-1, -1, -2));
	}

	void testBackground()
	{
		float value = -1;
		UASSERT(FmGravityField::parseBackground("1.62", value));
		UASSERT(std::abs(value - 1.62f) < 0.0001f);
		UASSERT(FmGravityField::parseBackground("0", value));
		UASSERT(value == 0);
		UASSERT(FmGravityField::parseBackground("1", value));
		UASSERT(value == 1);
		UASSERT(!FmGravityField::parseBackground("true", value));
		UASSERT(!FmGravityField::parseBackground("false", value));
		UASSERT(!FmGravityField::parseBackground("-1", value));
		UASSERT(!FmGravityField::parseBackground("nan", value));
		UASSERT(!FmGravityField::parseBackground("1.2junk", value));
		UASSERT(!FmGravityField::parseBackground("10001", value));
	}

	void testEarthProjectionDefaults()
	{
		Json::Value params(Json::objectValue);
		UASSERT(!fm_earth::defaultGravitySource(params));
		params["projection"] = "flat";
		UASSERT(!fm_earth::defaultGravitySource(params));
		for (const std::string type :
				{"sphere", "spherical", "cube", "inverted_sphere", "inside_sphere"}) {
			params["projection"] = Json::Value(Json::objectValue);
			params["projection"]["type"] = type;
			params["projection"]["origin"]["x"] = 23.5;
			params["projection"]["origin"]["y"] = 7.5;
			params["projection"]["origin"]["z"] = 7.5;
			const auto source = fm_earth::defaultGravitySource(params);
			UASSERT(source);
			FmGravityField server, client;
			UASSERT(server.setSources(*source));
			UASSERT(client.setSources(server.configuration()));
			const v3f gravity = server.sample(v3bpos_t(0, 0, 0)) / BS;
			const bool inverted = type == "inverted_sphere" || type == "inside_sphere";
			UASSERT((inverted ? -gravity.X : gravity.X) > 9.8f);
			UASSERT(std::abs(gravity.X) < 9.9f);
			UASSERT(gravity.Y == 0 && gravity.Z == 0);
			UASSERT(client.sample(v3bpos_t(0, 0, 0)) == server.sample(v3bpos_t(0, 0, 0)));
		}
		params["projection"] = "sphere";
		UASSERT(fm_earth::defaultGravitySource(params));
	}

	void testTorusGravityDefaults()
	{
		Json::Value params(Json::objectValue);
		params["projection"]["type"] = "torus";
		params["projection"]["origin"]["x"] = 7.5;
		params["projection"]["origin"]["y"] = 7.5;
		params["projection"]["origin"]["z"] = 7.5;
		const auto sources = fm_earth::defaultGravitySource(params);
		UASSERT(sources);
		UASSERT(std::count(sources->begin(), sources->end(), ';') == 7);
		FmGravityField server, client;
		UASSERT(server.setSources(*sources));
		UASSERT(client.setSources(server.configuration()));
		const v3f outside = server.sample(v3bpos_t(1875, 0, 0)) / BS;
		const v3f inside = server.sample(v3bpos_t(625, 0, 0)) / BS;
		const v3f top = server.sample(v3bpos_t(1250, 625, 0)) / BS;
		UASSERT(std::abs(outside.X + 9.81f) < 0.001f);
		UASSERT(inside.X > 4.4f && inside.X < 4.6f);
		UASSERT(top.X < -3.0f && top.Y < -8.8f);
		UASSERT(client.sample(v3bpos_t(1875, 0, 0)) ==
				server.sample(v3bpos_t(1875, 0, 0)));

		params["projection"]["radius"] = 5000;
		params["projection"]["major_radius"] = 10000;
		const auto scaled = fm_earth::defaultGravitySource(params);
		UASSERT(scaled && server.setSources(*scaled));
		const v3f scaled_outside = server.sample(v3bpos_t(937, 0, 0)) / BS;
		UASSERT(std::abs(scaled_outside.X + 9.81f) < 0.03f);
		params["projection"]["major_radius"] = 5000;
		UASSERT(!fm_earth::defaultGravitySource(params));
		params["projection"] = "torus";
		UASSERT(fm_earth::defaultGravitySource(params));
	}

	void testEarthEnvironmentDefaults()
	{
		struct RestoreSetting
		{
			std::string name;
			bool present;
			std::string value;
			explicit RestoreSetting(const std::string &setting) :
					name(setting), present(g_settings->existsLocal(setting))
			{
				if (present)
					value = g_settings->get(name);
			}
			~RestoreSetting()
			{
				if (present)
					g_settings->set(name, value);
				else
					g_settings->remove(name);
			}
		} restore_sources("fm_gravity_sources"), restore_background("gravity_background");
		g_settings->remove("fm_gravity_sources");
		g_settings->set("gravity_background", "1.62");

		const auto configuration = [&](const std::string &projection) {
			MockServer server(getTestTempDirectory());
			MetricsBackend metrics;
			EmergeManager emerge(&server, &metrics);
			auto map = std::make_unique<ServerMap>(
					server.getWorldPath(), &server, &emerge, &metrics);
			map->settings_mgr.setMapSetting("mg_name", "earth", true);
			map->settings_mgr.setMapSetting("mg_earth", projection, true);
			ServerEnvironment env(std::move(map), &server, &metrics);
			UASSERT(std::abs(env.getGravityBackgroundStrength() / BS - 1.62f) < 0.0001f);
			if (env.getGravityConfiguration().empty())
				UASSERT(std::abs(env.getFallingGravity(v3bpos_t(0, 0, 0)).Y / BS +
								 1.62f) < 0.0001f);
			return env.getGravityConfiguration();
		};
		const std::string sphere = R"({"projection":"sphere"})";
		UASSERT(configuration(sphere).find("mass=5.9722e21") != std::string::npos);
		UASSERT(configuration(R"({"projection":"inverted_sphere"})")
						.find("mass=-5.9722e21") != std::string::npos);
		UASSERT(configuration(R"({"projection":"flat"})").empty());
		const std::string torus = R"({"projection":"torus"})";
		const std::string torus_sources = configuration(torus);
		UASSERT(std::count(torus_sources.begin(), torus_sources.end(), ';') == 7);
		g_settings->set("fm_gravity_sources", "1,2,3,4,5");
		UASSERT(configuration(sphere) == "1,2,3,4,5");
		UASSERT(configuration(torus) == "1,2,3,4,5");
		g_settings->set("fm_gravity_sources", "");
		UASSERT(configuration(sphere).empty());
		UASSERT(configuration(torus).empty());
	}

	void testGravityInitPacket()
	{
		const std::string sources = "23.5,7.5,7.5,mass=-1e9,16";
		MSGPACK_PACKET_INIT((int)TOCLIENT_FREEMINER_INIT, 2);
		fm_gravity_sync::packInit(pk, sources, 1.62f * BS);
		NetworkPacket received;
#if MINETEST_PROTO
		NetworkPacket sent(TOCLIENT_FREEMINER_INIT, 0);
		sent.putLongString({buffer.data(), buffer.size()});
		const auto wire = sent.oldForgePacket();
		received.putRawPacket(&wire[0], wire.getSize(), 2);
#else
		received.putRawPacket(
				reinterpret_cast<const u8 *>(buffer.data()), buffer.size(), 2);
#endif
		UASSERT(received.packet_unpack());
		UASSERTEQ(u16, received.getCommand(), TOCLIENT_FREEMINER_INIT);
		FmGravityField field, expected;
		float background = 0;
		bool enabled = false;
		UASSERT(fm_gravity_sync::applyInit(*received.packet, field, background, enabled));
		UASSERT(field.configuration() == sources && enabled);
		UASSERT(std::abs(background / BS - 1.62f) < 0.0001f);
		UASSERT(expected.setSources(sources));
		UASSERT(field.sample(v3bpos_t(0, 0, 0)) == expected.sample(v3bpos_t(0, 0, 0)));

		// Older servers omit the fields; incomplete or malformed data is ignored.
		MsgpackPacket old_packet;
		UASSERT(!fm_gravity_sync::applyInit(old_packet, field, background, enabled));
		UASSERT(field.configuration() == sources && enabled);
		auto incomplete = *received.packet;
		incomplete.erase(TOCLIENT_INIT_GRAVITY_BACKGROUND);
		UASSERT(!fm_gravity_sync::applyInit(incomplete, field, background, enabled));
		auto malformed = *received.packet;
		malformed[TOCLIENT_INIT_GRAVITY_BACKGROUND] =
				malformed[TOCLIENT_INIT_GRAVITY_SOURCES];
		UASSERT(!fm_gravity_sync::applyInit(malformed, field, background, enabled));
		UASSERT(field.configuration() == sources && enabled &&
				std::abs(background / BS - 1.62f) < 0.0001f);

		msgpack::sbuffer zero_buffer;
		msgpack::packer<msgpack::sbuffer> zero_packer(&zero_buffer);
		zero_packer.pack_map(2);
		fm_gravity_sync::packInit(zero_packer, "", 0);
		msgpack::unpacked zero_unpacked;
		msgpack::unpack(zero_unpacked, zero_buffer.data(), zero_buffer.size());
		const auto zero_packet = zero_unpacked.get().as<MsgpackPacket>();
		UASSERT(fm_gravity_sync::applyInit(zero_packet, field, background, enabled));
		UASSERT(field.empty() && background == 0 && !enabled);

#if MINETEST_PROTO
		const u8 truncated[] = {0, TOCLIENT_FREEMINER_INIT, 0, 0, 0, 4, 0x80};
		NetworkPacket broken;
		broken.putRawPacket(truncated, sizeof(truncated), 2);
		UASSERT(!broken.packet_unpack());
#endif
	}
};

static TestFmGravity g_test_instance;
