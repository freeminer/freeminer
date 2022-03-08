/*
Minetest
Copyright (C) 2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <server.h>
#include "test.h"

#include "util/string.h"
#include "util/serialize.h"

class FakeServer : public Server
{
public:
	// clang-format off
	FakeServer() : Server("fakeworld", SubgameSpec("fakespec", "fakespec"), true,
					Address(), true, nullptr)
	{
	}
	// clang-format on

private:
	void SendChatMessage(session_t peer_id, const ChatMessage &message)
	{
		// NOOP
	}
};

class TestServerShutdownState : public TestBase
{
public:
	TestServerShutdownState() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestServerShutdownState"; }

	void runTests(IGameDef *gamedef);

	void testInit();
	void testReset();
	void testTrigger();
	void testTick();
};

static TestServerShutdownState g_test_instance;

void TestServerShutdownState::runTests(IGameDef *gamedef)
{
	TEST(testInit);
	TEST(testReset);
	TEST(testTrigger);
	TEST(testTick);
}

void TestServerShutdownState::testInit()
{
<<<<<<< HEAD:src/unittest/test_player.cpp
	RemotePlayer rplayer("testplayer_save", gamedef->idef());
	PlayerSAO sao(NULL, 1, false);
	sao.initialize(&rplayer, std::set<std::string>());
	rplayer.setPlayerSAO(&sao);
	sao.setBreath(10);
	sao.setHPRaw(8);
	sao.setYaw(0.1f);
	sao.setPitch(0.6f);
	sao.setBasePosition(v3f(450.2f, -15.7f, 68.1f));
#if WTF
	rplayer.save(".", gamedef);
	UASSERT(fs::PathExists("testplayer_save"));
#endif
=======
	Server::ShutdownState ss;
	UASSERT(!ss.is_requested);
	UASSERT(!ss.should_reconnect);
	UASSERT(ss.message.empty());
	UASSERT(ss.m_timer == 0.0f);
>>>>>>> 5.5.0:src/unittest/test_server_shutdown_state.cpp
}

void TestServerShutdownState::testReset()
{
<<<<<<< HEAD:src/unittest/test_player.cpp
	RemotePlayer rplayer("testplayer_load", gamedef->idef());
	PlayerSAO sao(NULL, 1, false);
	sao.initialize(&rplayer, std::set<std::string>());
	rplayer.setPlayerSAO(&sao);
	sao.setBreath(10);
	sao.setHPRaw(8);
	sao.setYaw(0.1f);
	sao.setPitch(0.6f);
	sao.setBasePosition(v3f(450.2f, -15.7f, 68.1f));
#if WTF
	rplayer.save(".", gamedef);
	UASSERT(fs::PathExists("testplayer_load"));
#endif

	RemotePlayer rplayer_load("testplayer_load", gamedef->idef());
	PlayerSAO sao_load(NULL, 2, false);
	std::ifstream is("testplayer_load", std::ios_base::binary);
	UASSERT(is.good());
	rplayer_load.deSerialize(is, "testplayer_load", &sao_load);
	is.close();

	UASSERT(rplayer_load.getName() == "testplayer_load");
	UASSERT(sao_load.getBreath() == 10);
	UASSERT(sao_load.getHP() == 8);
	UASSERT(sao_load.getYaw() == 0.1f);
	UASSERT(sao_load.getPitch() == 0.6f);
	UASSERT(sao_load.getBasePosition() == v3f(450.2f, -15.7f, 68.1f));
=======
	Server::ShutdownState ss;
	ss.reset();
	UASSERT(!ss.is_requested);
	UASSERT(!ss.should_reconnect);
	UASSERT(ss.message.empty());
	UASSERT(ss.m_timer == 0.0f);
}

void TestServerShutdownState::testTrigger()
{
	Server::ShutdownState ss;
	ss.trigger(3.0f, "testtrigger", true);
	UASSERT(!ss.is_requested);
	UASSERT(ss.should_reconnect);
	UASSERT(ss.message == "testtrigger");
	UASSERT(ss.m_timer == 3.0f);
}

void TestServerShutdownState::testTick()
{
	std::unique_ptr<FakeServer> fakeServer(new FakeServer());
	Server::ShutdownState ss;
	ss.trigger(28.0f, "testtrigger", true);
	ss.tick(0.0f, fakeServer.get());

	// Tick with no time should not change anything
	UASSERT(!ss.is_requested);
	UASSERT(ss.should_reconnect);
	UASSERT(ss.message == "testtrigger");
	UASSERT(ss.m_timer == 28.0f);

	// Tick 2 seconds
	ss.tick(2.0f, fakeServer.get());
	UASSERT(!ss.is_requested);
	UASSERT(ss.should_reconnect);
	UASSERT(ss.message == "testtrigger");
	UASSERT(ss.m_timer == 26.0f);

	// Tick remaining seconds + additional expire
	ss.tick(26.1f, fakeServer.get());
	UASSERT(ss.is_requested);
	UASSERT(ss.should_reconnect);
	UASSERT(ss.message == "testtrigger");
	UASSERT(ss.m_timer == 0.0f);
>>>>>>> 5.5.0:src/unittest/test_server_shutdown_state.cpp
}
