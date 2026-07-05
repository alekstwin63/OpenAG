#ifndef STEAM_MATCHMAKING_H
#define STEAM_MATCHMAKING_H
#pragma once

#include <stdint.h>

#pragma pack(push, 8)

struct servernetadr_t
{
	uint16_t m_usConnectionPort;
	uint16_t m_usQueryPort;
	uint32_t m_unIP;
};

struct gameserveritem_t
{
	servernetadr_t m_NetAdr;
	int m_nPing;
	bool m_bHadSuccessfulResponse;
	bool m_bDoNotRefresh;
	char m_szGameDir[32];
	char m_szMap[32];
	char m_szGameDescription[64];
	uint32_t m_nAppID;
	int m_nPlayers;
	int m_nMaxPlayers;
	int m_nBotPlayers;
	bool m_bPassword;
	bool m_bSecure;
	uint32_t m_ulTimeLastPlayed;
	int m_nServerVersion;
	char m_szServerName[64];
	char m_szGameTags[128];
	uint64_t m_steamID;
};

#pragma pack(pop)

class ISteamMatchmakingServerListResponse
{
public:
	virtual void ServerResponded(void *hRequest, int iServer) = 0;
	virtual void ServerFailedToRespond(void *hRequest, int iServer) = 0;
	virtual void RefreshComplete(void *hRequest, int response) = 0;
};

class ISteamMatchmakingPingResponse
{
public:
	virtual void ServerResponded(gameserveritem_t &server) = 0;
	virtual void ServerFailedToRespond() = 0;
};

class ISteamMatchmakingServers
{
public:
	virtual void *RequestInternetServerList(uint32_t iApp, void **ppFilter, uint32_t nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) = 0;
	virtual void *RequestLANServerList(uint32_t iApp, ISteamMatchmakingServerListResponse *pRequestServersResponse) = 0;
	virtual void *RequestFriendsServerList(uint32_t iApp, void **ppFilter, uint32_t nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) = 0;
	virtual void *RequestFavoritesServerList(uint32_t iApp, void **ppFilter, uint32_t nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) = 0;
	virtual void *RequestHistoryServerList(uint32_t iApp, void **ppFilter, uint32_t nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) = 0;
	virtual void *RequestSpectatorServerList(uint32_t iApp, void **ppFilter, uint32_t nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) = 0;
	
	virtual void ReleaseRequest(void *hServerListRequest) = 0;
	virtual gameserveritem_t *GetServerDetails(void *hRequest, int iServer) = 0;
	virtual void CancelQuery(void *hRequest) = 0;
	virtual void RefreshQuery(void *hRequest) = 0;
	virtual bool IsRefreshing(void *hRequest) = 0;
	virtual int GetServerCount(void *hRequest) = 0;
	virtual void RefreshServer(void *hRequest, int iServer) = 0;
	virtual int PingServer(uint32_t unIP, uint16_t usPort, ISteamMatchmakingPingResponse *pRequestServersResponse) = 0;
	virtual int PlayerDetails(uint32_t unIP, uint16_t usPort, void *pRequestServersResponse) = 0;
	virtual int ServerRules(uint32_t unIP, uint16_t usPort, void *pRequestServersResponse) = 0;
	virtual void CancelServerQuery(int hServerQuery) = 0;
};

typedef ISteamMatchmakingServers* (*SteamMatchmakingServers_t)(void);

#endif
