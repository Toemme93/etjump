// g_cmds_ext.c: Extended command set handling
// -------------------------------------------
//
#include "g_local.h"
#include "../../etjump/ui/menudef.h"

int iWeap = WS_MAX;

static void Cmd_SpecInvite_f(gentity_t *ent, unsigned int dwCommand, qboolean invite /* or uninvite */);
static void Cmd_SpecLock_f(gentity_t *ent, unsigned int dwCommand, qboolean lock);
static void Cmd_SpecList_f(gentity_t *ent, unsigned int dwCommand, qboolean notUsed);

//
// Update info:
//	1. Add line to aCommandInfo w/appropriate info
//	2. Add implementation for specific command (see an existing command for an example)
//
typedef struct
{
	const char *pszCommandName;
	qboolean fAnytime;
	qboolean fValue;
	void (*pCommand)(gentity_t *ent, unsigned int dwCommand, qboolean fValue);
	const char *pszHelpInfo;
} cmd_reference_t;

// VC optimizes for dup strings :)
static const cmd_reference_t aCommandInfo[] =
{
	{ "+stats",         qtrue,  qtrue,  NULL,                  ":^7 HUD overlay showing current weapon stats info"                                          },
	{ "+topshots",      qtrue,  qtrue,  NULL,                  ":^7 HUD overlay showing current top accuracies of all players"                              },
	{ "?",              qtrue,  qtrue,  G_commands_cmd,        ":^7 Gives a list of OSP-specific commands"                                                  },
	{ "autorecord",     qtrue,  qtrue,  NULL,                  ":^7 Creates a demo with a consistent naming scheme"                                         },
	{ "autoscreenshot", qtrue,  qtrue,  NULL,                  ":^7 Creates a screenshot with a consistent naming scheme"                                   },
	{ "bottomshots",    qtrue,  qfalse, G_weaponRankings_cmd,  ":^7 Shows WORST player for each weapon. Add ^3<weapon_ID>^7 to show all stats for a weapon" },
	{ "callvote",       qtrue,  qfalse, Cmd_CallVote_f, " <params>:^7 Calls a vote"                          },
	{ "commands",       qtrue,  qtrue,  G_commands_cmd,        ":^7 Gives a list of OSP-specific commands"                                                  },
	{ "currenttime",    qtrue,  qtrue,  NULL,                  ":^7 Displays current local time"                                                            },
	{ "follow",         qtrue,  qtrue,  Cmd_Follow_f,          " <player_ID|allies|axis>:^7 Spectates a particular player or team"                          },
	{ "notready",       qtrue,  qfalse, G_ready_cmd,           ":^7 Sets your status to ^5not ready^7 to start a match"                                     },
	{ "players",        qtrue,  qtrue,  G_players_cmd,         ":^7 Lists all active players and their IDs/information"                                     },
	{ "ready",          qtrue,  qtrue,  G_ready_cmd,           ":^7 Sets your status to ^5ready^7 to start a match"                                         },
	{ "readyteam",      qfalse, qtrue,  G_teamready_cmd,       ":^7 Sets an entire team's status to ^5ready^7 to start a match"                             },
	{ "say_teamnl",     qtrue,  qtrue,  G_say_teamnl_cmd,      "<msg>:^7 Sends a team chat without location info"                                           },
	{ "scores",         qtrue,  qtrue,  G_scores_cmd,          ":^7 Displays current match stat info"                                                       },
	{ "specinvite",     qtrue,  qtrue,  Cmd_SpecInvite_f,      ":^7 Invites a player to spectate"                                                           },
	{ "specuninvite",   qtrue,  qfalse, Cmd_SpecInvite_f,      ":^7 Uninvites a player to spectate"                                                         },
	{ "speclock",       qtrue,  qtrue,  Cmd_SpecLock_f,        ":^7 Locks a player from spectators"                                                         },
	{ "specunlock",     qtrue,  qfalse, Cmd_SpecLock_f,        ":^7Unlocks a player from spectators"                                                        },
	{ "speclist",       qtrue,  qtrue,  Cmd_SpecList_f,        ":^7Lists specinvited players"                                                               },
	{ "statsall",       qtrue,  qfalse, G_statsall_cmd,        ":^7 Shows weapon accuracy stats for all players"                                            },
	{ "statsdump",      qtrue,  qtrue,  NULL,                  ":^7 Shows player stats + match info saved locally to a file"                                },
	{ "team",           qtrue,  qtrue,  (void (*)(gentity_t *, unsigned int, qboolean))Cmd_Team_f, " <b|r|s|none>:^7 Joins a team (b = allies, r = axis, s = spectator)"},
	{ "topshots",       qtrue,  qtrue,  G_weaponRankings_cmd,  ":^7 Shows BEST player for each weapon. Add ^3<weapon_ID>^7 to show all stats for a weapon"  },
	{ "unready",        qtrue,  qfalse, G_ready_cmd,           ":^7 Sets your status to ^5not ready^7 to start a match"                                     },
	{ "weaponstats",    qtrue,  qfalse, G_weaponStats_cmd,     " [player_ID]:^7 Shows weapon accuracy stats for a player"                                   },
	{ 0,                qfalse, qtrue,  NULL,                  0                                                                                            }
};

// OSP-specific Commands
qboolean G_commandCheck(gentity_t *ent, char *cmd, qboolean fDoAnytime)
{
	unsigned int          i, cCommands = sizeof(aCommandInfo) / sizeof(aCommandInfo[0]);
	const cmd_reference_t *pCR;

	for (i = 0; i < cCommands; i++)
	{
		pCR = &aCommandInfo[i];
		if (NULL != pCR->pCommand && pCR->fAnytime == fDoAnytime && 0 == Q_stricmp(cmd, pCR->pszCommandName))
		{
			if (!G_commandHelp(ent, cmd, i))
			{
				pCR->pCommand(ent, i, pCR->fValue);
			}
			return(qtrue);
		}
	}

	return(qfalse);
}

// Prints specific command help info.
qboolean G_commandHelp(gentity_t *ent, const char *pszCommand, unsigned int dwCommand)
{
	char arg[MAX_TOKEN_CHARS];

	if (!ent)
	{
		return(qfalse);
	}
	trap_Argv(1, arg, sizeof(arg));
	if (!Q_stricmp(arg, "?"))
	{
		CP(va("print \"\n^3%s%s\n\n\"", pszCommand, aCommandInfo[dwCommand].pszHelpInfo));
		return(qtrue);
	}

	return(qfalse);
}


// Debounces cmd request as necessary.
qboolean G_cmdDebounce(gentity_t *ent, const char *pszCommandName)
{
	if (ent->client->pers.cmd_debounce > level.time)
	{
		CP(va("print \"Wait another %.1fs to issue ^3%s\n\"", 1.0 * (float)(ent->client->pers.cmd_debounce - level.time) / 1000.0,
		      pszCommandName));
		return(qfalse);
	}

	ent->client->pers.cmd_debounce = level.time + CMD_DEBOUNCE;
	return(qtrue);
}


void G_noTeamControls(gentity_t *ent)
{
	CP("cpm \"Team commands not enabled on this server.\n\"");
}





////////////////////////////////////////////////////////////////////////////
/////
/////			Match Commands
/////
/////


// ************** COMMANDS / ?
//
// Lists server commands.
void G_commands_cmd(gentity_t *ent, unsigned int dwCommand, qboolean fValue)
{
	int i, rows, num_cmds = sizeof(aCommandInfo) / sizeof(aCommandInfo[0]) - 1;

	rows = num_cmds / HELP_COLUMNS;
	if (num_cmds % HELP_COLUMNS)
	{
		rows++;
	}
	if (rows < 0)
	{
		return;
	}

	CP("cpm \"^5\nAvailable OSP Game-Commands:\n----------------------------\n\"");
	for (i = 0; i < rows; i++)
	{
		if (i + rows * 3 + 1 <= num_cmds)
		{
			CP(va("print \"^3%-17s%-17s%-17s%-17s\n\"", aCommandInfo[i].pszCommandName,
			      aCommandInfo[i + rows].pszCommandName,
			      aCommandInfo[i + rows * 2].pszCommandName,
			      aCommandInfo[i + rows * 3].pszCommandName));
		}
		else if (i + rows * 2 + 1 <= num_cmds)
		{
			CP(va("print \"^3%-17s%-17s%-17s\n\"", aCommandInfo[i].pszCommandName,
			      aCommandInfo[i + rows].pszCommandName,
			      aCommandInfo[i + rows * 2].pszCommandName));
		}
		else if (i + rows + 1 <= num_cmds)
		{
			CP(va("print \"^3%-17s%-17s\n\"", aCommandInfo[i].pszCommandName, aCommandInfo[i + rows].pszCommandName));
		}
		else
		{
			CP(va("print \"^3%-17s\n\"", aCommandInfo[i].pszCommandName));
		}
	}

	CP("cpm \"\nType: ^3\\command_name ?^7 for more information\n\"");
}



// ************** PLAYERS
//
// Show client info
void G_players_cmd(gentity_t *ent, unsigned int dwCommand, qboolean fValue)
{
	int       i, idnum, max_rate, cnt = 0, tteam;
	int       user_rate, user_snaps;
	gclient_t *cl;
	gentity_t *cl_ent;
	char      n2[MAX_NETNAME], ready[16], ref[16], rate[256];
	const char      *s = nullptr; 
	char userinfo[MAX_INFO_STRING];
	const char *coach = nullptr, *tc = nullptr;


	if (g_gamestate.integer == GS_PLAYING)
	{
		if (ent)
		{
			CP("print \"\n^3 ID^1 : ^3Player                    Nudge  Rate  MaxPkts  Snaps\n\"");
			CP("print \"^1-----------------------------------------------------------^7\n\"");
		}
		else
		{
			G_Printf(" ID : Player                    Nudge  Rate  MaxPkts  Snaps\n");
			G_Printf("-----------------------------------------------------------\n");
		}
	}
	else
	{
		if (ent)
		{
			CP("print \"\n^3Status^1   : ^3ID^1 : ^3Player                    Nudge  Rate  MaxPkts  Snaps\n\"");
			CP("print \"^1---------------------------------------------------------------------^7\n\"");
		}
		else
		{
			G_Printf("Status   : ID : Player                    Nudge  Rate  MaxPkts  Snaps\n");
			G_Printf("---------------------------------------------------------------------\n");
		}
	}

	max_rate = trap_Cvar_VariableIntegerValue("sv_maxrate");

	for (i = 0; i < level.numConnectedClients; i++)
	{
		idnum  = level.sortedClients[i];//level.sortedNames[i];
		cl     = &level.clients[idnum];
		cl_ent = g_entities + idnum;

		SanitizeString(cl->pers.netname, n2, qtrue);
		n2[26]   = 0;
		ref[0]   = 0;
		ready[0] = 0;

		// Rate info
		if (cl_ent->r.svFlags & SVF_BOT)
		{
			strcpy(rate, va("%s%s%s%s", "[BOT]", " -----", "       --", "     --"));
		}
		else if (cl->pers.connected == CON_CONNECTING)
		{
			strcpy(rate, va("%s", "^3>>> CONNECTING <<<"));
		}
		else
		{
			trap_GetUserinfo(idnum, userinfo, sizeof(userinfo));
			s          = Info_ValueForKey(userinfo, "rate");
			user_rate  = (max_rate > 0 && atoi(s) > max_rate) ? max_rate : atoi(s);
			s          = Info_ValueForKey(userinfo, "snaps");
			user_snaps = atoi(s);

			strcpy(rate, va("%5d%6d%9d%7d", cl->pers.clientTimeNudge, user_rate, cl->pers.clientMaxPackets, user_snaps));
		}

		if (g_gamestate.integer != GS_PLAYING)
		{
			if (cl->sess.sessionTeam == TEAM_SPECTATOR || cl->pers.connected == CON_CONNECTING)
			{
				strcpy(ready, ((ent) ? "^5--------^1 :" : "-------- :"));
			}
			else if (cl->pers.ready || (g_entities[idnum].r.svFlags & SVF_BOT))
			{
				strcpy(ready, ((ent) ? "^3(READY)^1  :" : "(READY)  :"));
			}
			else
			{
				strcpy(ready, ((ent) ? "NOTREADY^1 :" : "NOTREADY :"));
			}
		}

		if (cl->sess.coach_team)
		{
			tteam = cl->sess.coach_team;
			coach = (ent) ? "^3C" : "C";
		}
		else
		{
			tteam = cl->sess.sessionTeam;
			coach = " ";
		}

		tc = (ent) ? "^7 " : " ";
		if (g_gametype.integer >= GT_WOLF)
		{
			if (tteam == TEAM_AXIS)
			{
				tc = (ent) ? "^1X^7" : "X";
			}
			if (tteam == TEAM_ALLIES)
			{
				tc = (ent) ? "^4L^7" : "L";
			}
		}

		if (ent)
		{
			CP(va("print \"%s%s%2d%s^1:%s %-26s^7%s  ^3%s\n\"", ready, tc, idnum, coach, ((ref[0]) ? "^3" : "^7"), n2, rate, ref));
		}
		else
		{
			G_Printf("%s%s%2d%s: %-26s%s  %s\n", ready, tc, idnum, coach, n2, rate, ref);
		}

		cnt++;
	}

	if (ent)
	{
		CP(va("print \"\n^3%2d^7 total players\n\n\"", cnt));
	}
	else
	{
		G_Printf("\n%2d total players\n\n", cnt);
	}

	// Team speclock info
	if (g_gametype.integer >= GT_WOLF)
	{
		for (i = TEAM_AXIS; i <= TEAM_ALLIES; i++)
		{
			if (teamInfo[i].spec_lock)
			{
				if (ent)
				{
					CP(va("print \"** %s team is speclocked.\n\"", aTeams[i]));
				}
				else
				{
					G_Printf("** %s team is speclocked.\n", aTeams[i]);
				}
			}
		}
	}
}


// ************** READY / NOTREADY
//
// Sets a player's "ready" status.
void G_ready_cmd(gentity_t *ent, unsigned int dwCommand, qboolean state)
{
	const char *status[2] = { " NOT", "" };

	if (g_gamestate.integer == GS_PLAYING || g_gamestate.integer == GS_INTERMISSION)
	{
		CP("cpm \"Match is already in progress!\n\"");
		return;
	}

	if (!state && g_gamestate.integer == GS_WARMUP_COUNTDOWN)
	{
		CP("cpm \"Countdown started.... ^3notready^7 ignored!\n\"");
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		CP("cpm \"You must be in the game to be ^3ready^7!\n\"");
		return;
	}

	// Can't ready until enough players.
	if (level.numPlayingClients < match_minplayers.integer)
	{
		CP("cpm \"Not enough players to start match!\n\"");
		return;
	}

	if (!G_cmdDebounce(ent, aCommandInfo[dwCommand].pszCommandName))
	{
		return;
	}

	// Move them to correct ready state
	if (ent->client->pers.ready == state)
	{
		CP(va("print \"You are already%s ready!\n\"", status[state]));
	}
	else
	{
		ent->client->pers.ready = state;
		if (!level.intermissiontime)
		{
			if (state)
			{
				G_MakeReady(ent);
			}
			else
			{
				G_MakeUnready(ent);
			}

			AP(va("print \"%s^7 is%s ready!\n\"", ent->client->pers.netname, status[state]));
			AP(va("cp \"\n%s\n^3is%s ready!\n\"", ent->client->pers.netname, status[state]));
		}
	}

	G_readyMatchState();
}


// ************** SAY_TEAMNL
//
// Team chat w/no location info
void G_say_teamnl_cmd(gentity_t *ent, unsigned int dwCommand, qboolean fValue)
{
	if (!ent->client->sess.muted)
	{
		Cmd_Say_f(ent, SAY_TEAMNL, qfalse, qtrue);
	}
}


// ************** SCORES
//
// Shows match stats to the requesting client.
void G_scores_cmd(gentity_t *ent, unsigned int dwCommand, qboolean fValue)
{
	G_printMatchInfo(ent);
}


// ************** SPECINVITE
//
// Sends an invitation to a player to spectate a team.
void G_specinvite_cmd(gentity_t *ent, unsigned int dwCommand, qboolean fLock)
{
	int       tteam, pid;
	gentity_t *player;
	char      arg[MAX_TOKEN_CHARS];

	if (team_nocontrols.integer)
	{
		G_noTeamControls(ent); return;
	}
	if (!G_cmdDebounce(ent, aCommandInfo[dwCommand].pszCommandName))
	{
		return;
	}

	tteam = G_teamID(ent);
	if (tteam == TEAM_AXIS || tteam == TEAM_ALLIES)
	{
		if (!teamInfo[tteam].spec_lock)
		{
			CP("cpm \"Your team isn't locked from spectators!\n\"");
			return;
		}

		// Find the player to invite.
		trap_Argv(1, arg, sizeof(arg));
		if ((pid = ClientNumberFromString(ent, arg)) == -1)
		{
			return;
		}

		player = g_entities + pid;

		// Can't invite self
		if (player->client == ent->client)
		{
			CP("cpm \"You can't specinvite yourself!\n\"");
			return;
		}

		// Can't invite an active player.
		if (player->client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			CP("cpm \"You can't specinvite a non-spectator!\n\"");
			return;
		}

		player->client->sess.spec_invite |= tteam;

		// Notify sender/recipient
		CP(va("print \"%s^7 has been sent a spectator invitation.\n\"", player->client->pers.netname));
		G_printFull(va("*** You've been invited to spectate the %s team!", aTeams[tteam]), player);

	}
	else
	{
		CP("cpm \"Spectators can't specinvite players!\n\"");
	}
}


// ************** WEAPONSTATS
//
// Shows a player's stats to the requesting client.
void G_weaponStats_cmd(gentity_t *ent, unsigned int dwCommand, qboolean fDump)
{
	G_statsPrint(ent, 0);
}


// ************** STATSALL
//
// Shows all players' stats to the requesting client.
void G_statsall_cmd(gentity_t *ent, unsigned int dwCommand, qboolean fDump)
{
	int       i;
	gentity_t *player;

	for (i = 0; i < level.numConnectedClients; i++)
	{
		player = &g_entities[level.sortedClients[i]];
		if (player->client->sess.sessionTeam == TEAM_SPECTATOR)
		{
			continue;
		}
		CP(va("ws %s\n", G_createStats(player)));
	}
}


// ************** TEAMREADY
//
// Sets a player's team "ready" status.
void G_teamready_cmd(gentity_t *ent, unsigned int dwCommand, qboolean state)
{
	int       i, tteam = G_teamID(ent);
	gclient_t *cl;

	if (g_gamestate.integer == GS_PLAYING || g_gamestate.integer == GS_INTERMISSION)
	{
		CP("cpm \"Match is already in progress!\n\"");
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		CP("cpm \"Spectators can't ready a team!\n\"");
		return;
	}

	// Can't ready until enough players.
	if (level.numPlayingClients < match_minplayers.integer)
	{
		CP("cpm \"Not enough players to start match!\n\"");
		return;
	}

	if (!G_cmdDebounce(ent, aCommandInfo[dwCommand].pszCommandName))
	{
		return;
	}

	// Move them to correct ready state
	for (i = 0; i < level.numPlayingClients; i++)
	{
		cl = level.clients + level.sortedClients[i];
		if (cl->sess.sessionTeam == tteam)
		{
			cl->pers.ready = qtrue;

			G_MakeReady(ent);
		}
	}

	G_printFull(va("The %s team is ready!", aTeams[tteam]), NULL);
	G_readyMatchState();
}



// These map to WS_* weapon indexes
const int cQualifyingShots[WS_MAX] =
{
	10,     // 0
	15,     // 1
	15,     // 2
	30,     // 3
	30,     // 4
	30,     // 5
	30,     // 6
	3,      // 7
	100,    // 8
	5,      // 9
	5,      // 10
	5,      // 11
	3,      // 12
	3,      // 13
	5,      // 14
	3,      // 15
	3,      // 16
	5,      // 17
	10,     // 18
	100,    // 19
	30,     // 20
	30      // 21
};

// ************** TOPSHOTS/BOTTOMSHOTS
//
// Gives back overall or specific weapon rankings
int QDECL SortStats(const void *a, const void *b)
{
	gclient_t *ca, *cb;
	float     accA, accB;

	ca = &level.clients[*(int *)a];
	cb = &level.clients[*(int *)b];

	// then connecting clients
	if (ca->pers.connected == CON_CONNECTING)
	{
		return(1);
	}
	if (cb->pers.connected == CON_CONNECTING)
	{
		return(-1);
	}

	if (ca->sess.sessionTeam == TEAM_SPECTATOR)
	{
		return(1);
	}
	if (cb->sess.sessionTeam == TEAM_SPECTATOR)
	{
		return(-1);
	}

	if ((ca->sess.aWeaponStats[iWeap].atts) < cQualifyingShots[iWeap])
	{
		return(1);
	}
	if ((cb->sess.aWeaponStats[iWeap].atts) < cQualifyingShots[iWeap])
	{
		return(-1);
	}

	accA = (float)(ca->sess.aWeaponStats[iWeap].hits * 100.0) / (float)(ca->sess.aWeaponStats[iWeap].atts);
	accB = (float)(cb->sess.aWeaponStats[iWeap].hits * 100.0) / (float)(cb->sess.aWeaponStats[iWeap].atts);

	// then sort by accuracy
	if (accA > accB)
	{
		return(-1);
	}
	return(1);
}

// Shows the most accurate players for each weapon to the requesting client
void G_weaponStatsLeaders_cmd(gentity_t *ent, qboolean doTop, qboolean doWindow)
{
	int             i, iWeap, shots, wBestAcc, cClients, cPlaces;
	int             aClients[MAX_CLIENTS];
	float           acc;
	char            z[MAX_STRING_CHARS];
	const gclient_t *cl;

	z[0] = 0;
	for (iWeap = WS_KNIFE; iWeap < WS_MAX; iWeap++)
	{
		wBestAcc = (doTop) ? 0 : 99999;
		cClients = 0;
		cPlaces  = 0;

		// suckfest - needs two passes, in case there are ties
		for (i = 0; i < level.numConnectedClients; i++)
		{
			cl = &level.clients[level.sortedClients[i]];

			if (cl->sess.sessionTeam == TEAM_SPECTATOR)
			{
				continue;
			}

			shots = cl->sess.aWeaponStats[iWeap].atts;
			if (shots >= cQualifyingShots[iWeap])
			{
				acc                  = (float)((cl->sess.aWeaponStats[iWeap].hits) * 100.0) / (float)shots;
				aClients[cClients++] = level.sortedClients[i];
				if (((doTop) ? acc : (float)wBestAcc) > ((doTop) ? wBestAcc : acc))
				{
					wBestAcc = (int)acc;
					cPlaces++;
				}
			}
		}

		if (!doTop && cPlaces < 2)
		{
			continue;
		}

		for (i = 0; i < cClients; i++)
		{
			cl  = &level.clients[aClients[i]];
			acc = (float)(cl->sess.aWeaponStats[iWeap].hits * 100.0) / (float)(cl->sess.aWeaponStats[iWeap].atts);

			if (((doTop) ? acc : (float)wBestAcc + 0.999) >= ((doTop) ? wBestAcc : acc))
			{
				Q_strcat(z, sizeof(z), va(" %d %d %d %d %d %d", iWeap + 1, aClients[i],
				                          cl->sess.aWeaponStats[iWeap].hits,
				                          cl->sess.aWeaponStats[iWeap].atts,
				                          cl->sess.aWeaponStats[iWeap].kills,
				                          cl->sess.aWeaponStats[iWeap].deaths));
			}
		}
	}
	CP(va("%sbstats%s %s 0", ((doWindow) ? "w" : ""), ((doTop) ? "" : "b"), z));
}


// Shows best/worst accuracy for all weapons, or sorted
// accuracies for a single weapon.
void G_weaponRankings_cmd(gentity_t *ent, unsigned int dwCommand, qboolean state)
{
	gclient_t *cl;
	int       c = 0, i, shots, wBestAcc;
	char      z[MAX_STRING_CHARS];

	if (trap_Argc() < 2)
	{
		G_weaponStatsLeaders_cmd(ent, state, qfalse);
		return;
	}

	wBestAcc = (state) ? 0 : 99999;

	// Find the weapon
	trap_Argv(1, z, sizeof(z));
	if ((iWeap = atoi(z)) == 0 || iWeap < WS_KNIFE || iWeap >= WS_MAX)
	{
		for (iWeap = WS_SYRINGE; iWeap >= WS_KNIFE; iWeap--)
		{
			if (!Q_stricmp(z, aWeaponInfo[iWeap].pszCode))
			{
				break;
			}
		}
	}

	if (iWeap < WS_KNIFE)
	{
		G_commandHelp(ent, (state) ? "topshots" : "bottomshots", dwCommand);

		Q_strncpyz(z, "^3Available weapon codes:^7\n", sizeof(z));
		for (i = WS_KNIFE; i < WS_MAX; i++)
		{
			Q_strcat(z, sizeof(z), va("  %s - %s\n", aWeaponInfo[i].pszCode, aWeaponInfo[i].pszName));
		}
		CP(va("print \"%s\"", z));
		return;
	}

	memcpy(&level.sortedStats, &level.sortedClients, sizeof(level.sortedStats));
	qsort(level.sortedStats, level.numConnectedClients, sizeof(level.sortedStats[0]), SortStats);

	z[0] = 0;
	for (i = 0; i < level.numConnectedClients; i++)
	{
		cl = &level.clients[level.sortedStats[i]];

		if (cl->sess.sessionTeam == TEAM_SPECTATOR)
		{
			continue;
		}

		shots = cl->sess.aWeaponStats[iWeap].atts;
		if (shots >= cQualifyingShots[iWeap])
		{
			float acc = (float)(cl->sess.aWeaponStats[iWeap].hits * 100.0) / (float)shots;

			c++;
			wBestAcc = (((state) ? acc : wBestAcc) > ((state) ? wBestAcc : acc)) ? (int)acc : wBestAcc;
			Q_strcat(z, sizeof(z), va(" %d %d %d %d %d", level.sortedStats[i],
			                          cl->sess.aWeaponStats[iWeap].hits,
			                          shots,
			                          cl->sess.aWeaponStats[iWeap].kills,
			                          cl->sess.aWeaponStats[iWeap].deaths));
		}
	}

	CP(va("astats%s %d %d %d%s", ((state) ? "" : "b"), c, iWeap, wBestAcc, z));
}

/*
 * Sends an invitation to a client to spectate him.
 */
static void Cmd_SpecInvite_f(gentity_t *ent, unsigned int dwCommand, qboolean invite /* or uninvite */)
{
	int       clientNum;
	gentity_t *other;
	char      arg[MAX_TOKEN_CHARS];


	if (ClientIsFlooding(ent))
	{
		CP("print \"^1Spam Protection:^7 Specinvite ignored\n\"");
		return;
	}

	// find the client to invite
	trap_Argv(1, arg, sizeof(arg));
	if ((clientNum = ClientNumberFromString(ent, arg)) == -1)
	{
		return;
	}

	other = g_entities + clientNum;



	// can't invite self
	if (other == ent)
	{
		CP(va("print \"You can not spec%sinvite yourself!\n\"", invite ? "" : "un"));
		return;
	}

	if (invite)
	{
		if (COM_BitCheck(ent->client->sess.specInvitedClients, clientNum))
		{
			CP(va("print \"%s^7 is already specinvited.\n\"", other->client->pers.netname));
			return;
		}
		COM_BitSet(ent->client->sess.specInvitedClients, clientNum);

		CP(va("print \"%s^7 has been sent a spectator invitation.\n\"", other->client->pers.netname));
		CPx(other - g_entities, va("cpm \"You have been invited to spectate %s^7.\n\"", ent->client->pers.netname));
	}
	else
	{
		if (!COM_BitCheck(ent->client->sess.specInvitedClients, clientNum))
		{
			CP(va("print \"%s^7 is not specinvited.\n\"", other->client->pers.netname));
			return;
		}

		COM_BitClear(ent->client->sess.specInvitedClients, clientNum);
		if (other->client->sess.spectatorState == SPECTATOR_FOLLOW
		    && other->client->sess.spectatorClient == ent - g_entities
		    && !G_AllowFollow(other, ent))
		{
			StopFollowing(other);
		}

		CP(va("print \"%s^7 was removed from invited spectators.\n\"", other->client->pers.netname));
		CPx(other - g_entities, va("cpm \"You have been uninvited to spectate %s^7.\n\"", ent->client->pers.netname));
	}
}

/*
 * Locks/unlocks a client from spectators.
 */
static void Cmd_SpecLock_f(gentity_t *ent, unsigned int dwCommand, qboolean lock)
{
	int       i;
	gentity_t *other;

	if (ent->client->sess.specLocked == lock)
	{
		CP(va("print \"You are already %slocked from spectators!\n\"", lock ? "" : "un"));
		return;
	}

	ent->client->sess.specLocked = lock;

	// unlocked
	if (!ent->client->sess.specLocked)
	{
		CP("cpm \"You are now unlocked from spectators!\n\"");
		return;
	}

	// locked
	CP("cpm \"You are now locked from spectators!\n\"");
	CP("cpm \"Use ^3specinvite^7 to invite people to spectate.\n\"");

	// update following players
	for (i = 0; i < level.numConnectedClients; i++)
	{
		other = g_entities + level.sortedClients[i];

		if (other->client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			continue;
		}

		if (other->client->sess.spectatorState == SPECTATOR_FOLLOW
		    && other->client->sess.spectatorClient == ent->client->ps.clientNum
		    && !G_AllowFollow(other, ent))
		{
			StopFollowing(other);
		}
	}
}

/*
 * Lists specinvited players.
 */
static void Cmd_SpecList_f(gentity_t *ent, unsigned int dwCommand, qboolean notUsed)
{
	int i;

	// print zza warning
	if (!ent->client->sess.specLocked)
	{
		CP("print \"You are not speclocked.\n\"");
	}

	for (i = 0; i < level.numConnectedClients; i++)
		if (COM_BitCheck(ent->client->sess.specInvitedClients, level.sortedClients[i]))
		{
			CP(va("print \"%s ^7is specinvited.\n\"", level.clients[level.sortedClients[i]].pers.netname));
		}
}
