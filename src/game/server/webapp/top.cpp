#if defined(CONF_TEERACE)

// TODO: replace crypto++ with another lib?
#include <game/server/webapp.h>
/*#include <engine/external/encrypt/cryptlib.h>
#include <engine/external/encrypt/osrng.h>
#include <engine/external/encrypt/files.h>
#include <engine/external/encrypt/base64.h>
#include <engine/external/encrypt/fltrimpl.h>
#include <engine/external/encrypt/rsa.h>*/
#include <engine/external/json/reader.h>
#include <engine/external/json/writer.h>

#include "top.h"

int CWebTop::GetTop5(void *pUserData)
{
	CParam *pData = (CParam*)pUserData;
	CWebapp *pWebapp = pData->m_pWebapp;
	int Start = pData->m_Start;
	int ClientID = pData->m_ClientID;
	delete pData;
	
	if(!pWebapp->Connect())
		return 0;
	
	char *pReceived = 0;
	char aBuf[512];
	char aURL[128];
	str_format(aURL, sizeof(aURL), "/api/1/runs/best/%d/", pWebapp->MapID());
	str_format(aBuf, sizeof(aBuf), CWebapp::GET, aURL, pWebapp->ServerIP(), pWebapp->ApiKey());
	int Size = pWebapp->SendAndReceive(aBuf, &pReceived);
	pWebapp->Disconnect();
	
	if(Size < 0)
	{
		dbg_msg("webapp", "error: %d (top5)", Size);
		return 0;
	}
	
	Json::Value Top;
	Json::Reader Reader;
	if(!Reader.parse(pReceived, pReceived+Size, Top))
	{
		mem_free(pReceived);
		return 0;
	}
	
	mem_free(pReceived);
	
	COut *pOut = new COut(WEB_USER_TOP);
	pOut->m_ClientID = ClientID;
	for(int i = 0; i < Top.size(); i++)
	{
		Json::Value User = Top[i];
		CUserRank UserRank = CUserRank(User["user"]["username"].asString().c_str(),
								str_tofloat(User["time"].asString().c_str()));
		pOut->m_lUserRanks.add(UserRank);
	}
	
	pWebapp->AddOutput(pOut);
	return 1;
}

#endif