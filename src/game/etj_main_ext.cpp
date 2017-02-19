#include "etj_local.h"
#include "etj_game.h"
#include "etj_session.h"
#include "etj_commands.h"
#include "etj_save.h"
#include "etj_levels.h"
#include "etj_database.h"
#include "etj_custom_map_votes.h"
#include "etj_utilities.h"
#include <boost/algorithm/string.hpp>
#include "etj_motd.h"
#include "etj_timerun.h"
#include "etj_map_statistics.h"
#include "etj_tokens.h"
#include "etj_shared.h"
#include "etj_user_repository.h"

Game game;

void OnClientConnect(int clientNum, qboolean firstTime, qboolean isBot)
{
	// Do not do g_entities + clientNum here, entity is not initialized yet
	G_DPrintf("OnClientConnect called by %d\n", clientNum);

	if (firstTime)
	{
		game.session->Init(clientNum);

		G_DPrintf("Requesting guid from %d\n", clientNum);

		trap_SendServerCommand(clientNum,
			ETJump::Constants::Authentication::GUID_REQUEST.c_str());
	}
	else
	{
		game.session->ReadSessionData(clientNum);
		game.timerun->clientConnect(clientNum, game.session->GetId(clientNum));
	}

	if (game.session->IsIpBanned(clientNum))
	{
		G_LogPrintf("Kicked banned client: %d\n", clientNum);
		trap_DropClient(clientNum, "You are banned.", 0);
	}
}

void OnClientBegin(gentity_t *ent)
{
	G_DPrintf("OnClientBegin called by %d\n", ClientNum(ent));
	if (!ent->client->sess.motdPrinted)
	{
		game.motd->PrintMotd(ent);
		ent->client->sess.motdPrinted = qtrue;
	}
}

void OnClientDisconnect(gentity_t *ent)
{
	G_DPrintf("OnClientDisconnect called by %d\n", ClientNum(ent));

	game.session->OnClientDisconnect(ClientNum(ent));
}

void WriteSessionData()
{
	for (int i = 0; i < level.numConnectedClients; i++)
	{
		int clientNum = level.sortedClients[i];
		game.session->WriteSessionData(clientNum);
	}
}

/*
Changes map to a random map
*/
void ChangeMap()
{
	std::string map = game.mapStatistics->randomMap();
	CPAll((boost::format("Changing map to %s.") % map).str());
	trap_SendConsoleCommand(EXEC_APPEND, va("map %s\n", map.c_str()));
}

void RunFrame(int levelTime)
{
	game.mapStatistics->runFrame(levelTime);
	ETJump::userRepository->checkForCompletedTasks();
}

void OnGameInit()
{
	game.levels         = std::make_shared<Levels>();
	game.database       = std::make_shared<Database>();
	game.session        = std::make_shared<Session>(game.database.get());
	game.commands       = std::make_shared<Commands>();
	game.saves          = std::make_shared<SaveSystem>(game.session.get());
	game.mapStatistics  = std::make_shared<MapStatistics>();
	game.customMapVotes = std::make_shared<CustomMapVotes>(game.mapStatistics.get());
	game.motd           = std::make_shared<Motd>();
	game.timerun        = std::make_shared<Timerun>();
	game.tokens         = std::make_shared<Tokens>();

	if (strlen(g_levelConfig.string))
	{
		if (!game.levels->ReadFromConfig())
		{
			G_LogPrintf("Error while reading admin config: %s\n", game.levels->ErrorMessage().c_str());
		}
		else
		{
			G_Printf("Successfully loaded levels from config: %s\n", g_levelConfig.string);
		}
	}

	if (strlen(g_userConfig.string) > 0)
	{
		if (!game.database->InitDatabase(g_userConfig.string))
		{
			G_LogPrintf("DATABASE ERROR: %s\n", game.database->GetMessage().c_str());
		}
		else
		{
			G_LogPrintf("Users loaded successfully from database: %s\n", g_userConfig.string);
		}
	}

	game.mapStatistics->initialize(std::string(g_mapDatabase.string), level.rawmapname);
	game.customMapVotes->Load();
	game.motd->Initialize();
	game.timerun->init(GetPath(g_timerunsDatabase.string), level.rawmapname);

	if (g_tokensMode.integer)
	{
		// Utilities::WriteFile handles the correct path (etjump/...)
		auto path = std::string(g_tokensPath.string) + "/" + std::string(level.rawmapname) + ".json";
		game.tokens->loadTokens(path);
	}
}

void OnGameShutdown()
{
	WriteSessionData();
//    game.database->ExecuteQueuedOperations();
	game.database->CloseDatabase();
	game.mapStatistics->saveChanges();
	game.tokens->reset();

	game.levels = nullptr;
	game.database = nullptr;
	game.session = nullptr;
	game.commands = nullptr;
	game.saves = nullptr;
	game.customMapVotes = nullptr;
	game.motd = nullptr;
	game.timerun = nullptr;
	game.mapStatistics = nullptr;
	game.tokens = nullptr;
}

qboolean OnConnectedClientCommand(gentity_t *ent)
{
	G_DPrintf("OnClientCommand called for %d (%s): %s\n", ClientNum(ent), ConcatArgs(0), ent->client->pers.netname);

	auto argv    = GetArgs();
	auto command = (*argv)[0];
	boost::to_lower(command);

	if (ent->client->pers.connected != CON_CONNECTED)
	{
		return qfalse;
	}

	if (game.commands->ClientCommand(ent, command))
	{
		return qtrue;
	}

	if (game.commands->AdminCommand(ent))
	{
		return qtrue;
	}

	return qfalse;
}

// Returning qtrue means no other commands will be checked
qboolean OnClientCommand(gentity_t *ent)
{
	G_DPrintf("OnClientCommand called for %d (%s): %s\n", ClientNum(ent), ConcatArgs(0), ent->client->pers.netname);

	auto argv    = GetArgs();
	auto command = (*argv)[0];
	boost::to_lower(command);

	if (command == "update_user")
	{
		char buf[MAX_TOKEN_CHARS];
		trap_Argv(1, buf, sizeof(buf));
		int id = atoi(buf);
		trap_Argv(2, buf, sizeof(buf));
		std::string value = buf;
		ETJump::IUserRepository::UserUpdateModel uum;
		uum.commands = value;
		uum.name = value;
		uum.greeting = value;
		uum.title = value;
		ETJump::userRepository->updateAsync(id, uum, [](const std::shared_ptr<ETJump::IUserRepository::TaskResult<ETJump::User>> result)
		{
			AP(va("print \"Found user: %d %s %s\n\"",
				result->result.id(),
				result->result.name().c_str(),
				result->result.commands().c_str()));
		});
	}

	if (command == ETJump::Constants::Authentication::AUTHENTICATE)
	{
		game.session->GuidReceived(ent);
		game.timerun->clientConnect(ClientNum(ent), game.session->GetId(ent));

		if (trap_Argc() == 3)
		{
			char guid[MAX_TOKEN_CHARS];
			char hwid[MAX_TOKEN_CHARS];
			trap_Argv(1, guid, sizeof(guid));
			trap_Argv(2, hwid, sizeof(hwid));
			char *ip = NULL;
			char userinfo[MAX_INFO_STRING] = "\0";
			trap_GetUserinfo(ClientNum(ent), userinfo, sizeof(userinfo));
			ip = Info_ValueForKey(userinfo, "ip");
			ETJump::userRepository->createOrUpdateAsync(ent->client->pers.netname, guid, hwid, ip, [](const std::shared_ptr<ETJump::IUserRepository::TaskResult<ETJump::User>> result)
			{
				if (result->error.length() > 0)
				{
					AP(va("print \"Error: %s\n\"", result->error.c_str()));
					return;
				}

				AP(va("print \"Found user: %d %s %s\n\"", 
					result->result.id(), 
					result->result.name().c_str(),
					result->result.commands().c_str()));
			});

		}

		return qtrue;
	}

	return qfalse;
}

/*
=======================
Server console commands
=======================
*/
qboolean OnConsoleCommand()
{
	G_DPrintf("OnConsoleCommand called: %s.\n", ConcatArgs(0));

	auto argv    = GetArgs();
	auto command = (*argv)[0];
	boost::to_lower(command);

	if (command == "generatemotd")
	{
		game.motd->GenerateMotdFile();
		return qtrue;
	}

	if (command == "generatecustomvotes")
	{
		game.customMapVotes->GenerateVotesFile();
		return qtrue;
	}

	if (command == "logstate")
	{
		LogServerState();
		return qtrue;
	}

	if (game.commands->AdminCommand(NULL))
	{
		return qtrue;
	}

	return qfalse;
}

const char *GetRandomMap()
{
	return game.mapStatistics->randomMap();
}

const char *G_MatchOneMap(const char *arg)
{
	auto                     currentMaps = game.mapStatistics->getCurrentMaps();
	std::vector<std::string> matchingMaps;
	std::string              mapName = arg ? arg : "";
	boost::algorithm::to_lower(mapName);

	for (auto & map : *(currentMaps))
	{
		if (map.find(mapName) != std::string::npos)
		{
			matchingMaps.push_back(map);
		}
	}

	static char matchingMap[MAX_STRING_TOKENS] = "\0";
	if (matchingMaps.size() == 1)
	{
		Q_strncpyz(matchingMap, matchingMaps[0].c_str(), sizeof(matchingMap));
		return matchingMap;
	}

	if (matchingMaps.size() > 1)
	{
		auto match = std::find(matchingMaps.begin(), matchingMaps.end(), mapName);
		if (match != matchingMaps.end())
		{
			Q_strncpyz(matchingMap, match->c_str(), sizeof(matchingMap));
			return matchingMap;
		}
	}

	return NULL;
}

const char *GetRandomMapByType(const char *customType)
{
	static char buf[MAX_TOKEN_CHARS] = "\0";

	if (!customType)
	{
		G_Error("customType is NULL.");
	}

	Q_strncpyz(buf, game.customMapVotes->RandomMap(customType).c_str(), sizeof(buf));
	return buf;
}

// returns null if map type doesnt exists.
const char *CustomMapTypeExists(const char *mapType)
{
	static char buf[MAX_TOKEN_CHARS] = "\0";
	if (!mapType)
	{
		G_Error("mapType is NULL.");
	}

	CustomMapVotes::TypeInfo info = game.customMapVotes->GetTypeInfo(mapType);

	if (info.typeExists)
	{
		Q_strncpyz(buf, info.callvoteText.c_str(), sizeof(buf));
		return buf;
	}

	return NULL;
}

void ClientNameChanged(gentity_t *ent)
{
	game.session->NewName(ent);
}

std::string GetTeamString(int clientNum)
{
	gentity_t *ent = g_entities + clientNum;

	if (ent->client->sess.sessionTeam == TEAM_ALLIES)
	{
		return "ALLIES";
	}
	if (ent->client->sess.sessionTeam == TEAM_AXIS)
	{
		return "AXIS";
	}
	return "SPECTATOR";
}

void LogServerState()
{
	time_t t;
	time(&t);
	std::string   state = "Server state at " + TimeStampToString(static_cast<int>(t)) + ":\n";
	boost::format fmter("%1% %2% %3%");
	for (int i = 0; i < level.numConnectedClients; i++)
	{
		int clientNum = level.sortedClients[i];

		fmter % clientNum % GetTeamString(clientNum) % (g_entities + clientNum)->client->pers.netname;
		state += fmter.str() + "\n";
	}

	if (level.numConnectedClients == 0)
	{
		state += "No players on the server.\n";
	}

	LogPrint(state);
}

void StartTimer(const char *runName, gentity_t *ent)
{
	game.timerun->startTimer(runName, ClientNum(ent), ent->client->pers.netname, ent->client->ps.commandTime);
}
void StopTimer(const char *runName, gentity_t *ent)
{
	game.timerun->stopTimer(ClientNum(ent), ent->client->ps.commandTime, runName);
}
void TimerunConnectNotify(gentity_t *ent)
{
	game.timerun->connectNotify(ClientNum(ent));
}
void InterruptRun(gentity_t *ent)
{
	if (!ent)
	{
		return;
	}

	game.timerun->interrupt(ClientNum(ent));
}

void G_increaseCallvoteCount(const char *mapName)
{
	game.mapStatistics->increaseCallvoteCount(mapName);
}

void G_increasePassedCount(const char *mapName)
{
	game.mapStatistics->increasePassedCount(mapName);
}

bool allTokensCollected(gentity_t *ent)
{
	auto tokenCounts = game.tokens->getTokenCounts();

	auto easyCount   = 0;
	auto mediumCount = 0;
	auto hardCount   = 0;
	for (auto i = 0; i < MAX_TOKENS_PER_DIFFICULTY; ++i)
	{
		if (ent->client->pers.collectedEasyTokens[i])
		{
			++easyCount;
		}

		if (ent->client->pers.collectedMediumTokens[i])
		{
			++mediumCount;
		}

		if (ent->client->pers.collectedHardTokens[i])
		{
			++hardCount;
		}
	}

	return tokenCounts[0] == easyCount && tokenCounts[1] == mediumCount && tokenCounts[2] == hardCount;
}
