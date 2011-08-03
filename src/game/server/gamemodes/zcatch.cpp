/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* Made by erd and Teetime */

#include <base/math.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include "zcatch.hpp"

CGameController_zCatch::CGameController_zCatch(class CGameContext *pGameServer) 
: IGameController(pGameServer)
{
	m_pGameType = "zCatch";
	m_Mode = g_Config.m_SvMode;
}

void CGameController_zCatch::Tick()
{
	DoWincheck();
	IGameController::Tick();
	CheckForGameOver();
	
	if(m_Mode != g_Config.m_SvMode) // automatic reload if sv_mode was changed
	{
		//Reload map
		Server()->ReloadMap();
		m_Mode = g_Config.m_SvMode;
	}
}

bool CGameController_zCatch::IsZCatch()
{
	return true;
}

void CGameController_zCatch::OnPlayerInfoChange(class CPlayer *pP)
{		
	/*
	if(pP->m_SpawnkillProtected)
	{
		pP->m_TeeInfos.m_UseCustomColor = 1;
		pP->m_TeeInfos.m_ColorBody = 0xff00;
		pP->m_TeeInfos.m_ColorFeet = 0xff00;
	}
	else */
	if(g_Config.m_SvColorIndicator)
	{
		int num = 161 - pP->m_CatchedPlayers * 10;
		pP->m_TeeInfos.m_UseCustomColor = 1;
		pP->m_TeeInfos.m_ColorBody = num * 0x010000 + 0xff00;
		pP->m_TeeInfos.m_ColorFeet = num * 0x010000 + 0xff00;
	}
}

int CGameController_zCatch::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	CPlayer *pPVictim = pVictim->GetPlayer();
		
	if(pKiller != pPVictim)
	{
		pKiller->m_Kills++;
		pKiller->m_Score += pPVictim->m_CatchedPlayers + 1;
		pKiller->m_CatchedPlayers++;
		
		/* Check if the killer is already killed and in spectator (victim may died through wallshot) */
		if(pKiller->GetTeam() != TEAM_SPECTATORS)
		{
			pPVictim->m_CatchedBy = pKiller->GetCID();
			pPVictim->SetTeamDirect(TEAM_SPECTATORS);
		
			if(pPVictim->m_PlayerWantToFollowCatcher)
				pPVictim->m_SpectatorID = pKiller->GetCID(); // Let the victim follow his catcher
				
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Caught by \"%s\". You will join the game automatically when \"%s\" dies.", Server()->ClientName(pKiller->GetCID()), Server()->ClientName(pKiller->GetCID()));	
			GameServer()->SendChatTarget(pPVictim->GetCID(), aBuf);
		}
	}
	else if(WeaponID == WEAPON_SELF)
	{
		//punishment when selfkill
		pPVictim->m_Score -= 14;
	}
	
	pPVictim->m_Deaths++;
	pPVictim->m_Score--;
	pPVictim->m_CatchedPlayers = 0;

	for(int i=0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			if(GameServer()->m_apPlayers[i]->m_CatchedBy == pPVictim->GetCID())
			{
				GameServer()->m_apPlayers[i]->m_CatchedBy = ZCATCH_NOT_CATCHED;
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));
			}
			OnPlayerInfoChange(GameServer()->m_apPlayers[i]);
		}
	}
	
	return 0;
}

void CGameController_zCatch::StartRound()
{
	ResetGame();

	m_RoundStartTick = Server()->Tick();
	m_SuddenDeath = 0;
	m_GameOverTick = -1;
	GameServer()->m_World.m_Paused = false;
	m_aTeamscore[TEAM_RED] = 0;
	m_aTeamscore[TEAM_BLUE] = 0;
	m_UnbalancedTick = -1;
	m_ForceBalanced = false;
	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{			
			GameServer()->m_apPlayers[i]->m_CatchedBy = ZCATCH_NOT_CATCHED;
			GameServer()->m_apPlayers[i]->m_Kills = 0;
			GameServer()->m_apPlayers[i]->m_Deaths = 0;
			GameServer()->m_apPlayers[i]->m_TicksSpec = 0;
			GameServer()->m_apPlayers[i]->m_TicksIngame = 0;
			GameServer()->m_apPlayers[i]->m_CatchedPlayers = 0;
		}
	}
	char aBufMsg[256];
	str_format(aBufMsg, sizeof(aBufMsg), "start round type='%s' teamplay='%d'", m_pGameType, m_GameFlags&GAMEFLAG_TEAMS);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBufMsg);
}

void CGameController_zCatch::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health and armor
	pChr->IncreaseHealth(10);
	if(g_Config.m_SvMode == 2)
		pChr->IncreaseArmor(10);
	
	// give default weapons
	switch(g_Config.m_SvMode)
		{
			case 0:
				pChr->GiveWeapon(WEAPON_HAMMER, -1);
				pChr->GiveWeapon(WEAPON_GUN, 10);
				break;
			case 1:
				pChr->GiveWeapon(WEAPON_RIFLE, -1);
				break;
			case 2:
				pChr->GiveWeapon(WEAPON_HAMMER, -1);
				pChr->GiveWeapon(WEAPON_GUN, 10);
				pChr->GiveWeapon(WEAPON_GRENADE, -1);
				pChr->GiveWeapon(WEAPON_SHOTGUN, -1);
				pChr->GiveWeapon(WEAPON_RIFLE, -1);
				break;
			case 3:
				pChr->GiveWeapon(WEAPON_HAMMER, -1);
				break;
			case 4:
				pChr->GiveWeapon(WEAPON_GRENADE, -1);
				break;
			case 5:
				pChr->GiveNinja();
				break;
		}
	OnPlayerInfoChange(pChr->GetPlayer());
}

void CGameController_zCatch::CheckForGameOver()
{
	int num = 0, num_spec = 0, num_SpecExplicit = 0;
	
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			num++;
			if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
				num_spec++;
			if(GameServer()->m_apPlayers[i]->m_SpecExplicit == 1)
			num_SpecExplicit++;
		}
	}
	
	if(num == 1)
	{
		//Do nothing
	}
	else if((num - num_spec == 1) && (num != num_spec) && (num - num_SpecExplicit != 1)) 
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				GameServer()->m_apPlayers[i]->m_Score += g_Config.m_SvBonus;
		}
		EndRound();
	}
}

void CGameController_zCatch::EndRound()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{		
			GameServer()->m_apPlayers[i]->m_CatchedBy = ZCATCH_NOT_CATCHED; //Set all players in server as non-catched
			
			if(GameServer()->m_apPlayers[i]->m_SpecExplicit == 0)
			{
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));
				OnPlayerInfoChange(GameServer()->m_apPlayers[i]);
				
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Kills: %d | Deaths: %d", GameServer()->m_apPlayers[i]->m_Kills, GameServer()->m_apPlayers[i]->m_Deaths);				
				GameServer()->SendChatTarget(i, aBuf);
				
				if(GameServer()->m_apPlayers[i]->m_TicksSpec != 0 || GameServer()->m_apPlayers[i]->m_TicksIngame != 0)
				{
					double TimeInSpec = (GameServer()->m_apPlayers[i]->m_TicksSpec*100.0) / (GameServer()->m_apPlayers[i]->m_TicksIngame + GameServer()->m_apPlayers[i]->m_TicksSpec);
					str_format(aBuf, sizeof(aBuf), "Spec: %.2f%% | Ingame: %.2f%%", (double)TimeInSpec, (double)(100.0 - TimeInSpec));
					GameServer()->SendChatTarget(i, aBuf);	
				}
			}
		}
	}
	
	if(m_Warmup) // game can't end when we are running warmup
		return;
		
	GameServer()->m_World.m_Paused = true;
	m_GameOverTick = Server()->Tick();
	m_SuddenDeath = 0;
}
