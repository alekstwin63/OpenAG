#ifndef STEAM_MATCHMAKING_H
#define STEAM_MATCHMAKING_H
#pragma once

#include <stdint.h>

struct gameserveritem_t
{
	int m_nAppID;
	int m_nMaxPlayers;
	int m_nPlayers;
	int m_nBotPlayers;
	bool m_bPassword;
	bool m_bSecure;
	uint32_t m_ulLastTimeServerQueried;
	uint32_t m_ulFlags;
	char m_szServerName[64];
	char m_szGameDescription[64];
	char m_szMapName[32];
	char m_szGameDirectory[32];
	char m_szVersion[16];
	char m_szProduct[16];
	int m_nRegion;
	int m_nPing;
	bool m_bHadSuccessfulResponse;
	bool m_bDoNotRefresh;
	char m_szGameTags[128];
	uint64_t m_steamID;
	uint32_t m_unServerIP;
	uint16_t m_usQueryPort;
	uint16_t m_usConnectionPort;
};

class ISteamMatchmakingServerListResponse
{
public:
	virtual void ServerResponded(int hRequest, int iServer) = 0;
	virtual void ServerFailedToRespond(int hRequest, int iServer) = 0;
	virtual void RefreshComplete(int hRequest, int response) = 0;
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
	virtual gameserveritem_t *GetRequestDetails(void *hRequest, int iServer) = 0;
	virtual void CancelQuery(void *hRequest) = 0;
	virtual void RefreshQuery(void *hRequest) = 0;
	virtual bool IsRefreshing(void *hRequest) = 0;
	virtual int GetServerCount(void *hRequest) = 0;
	virtual gameserveritem_t *GetServerDetails(void *hRequest, int iServer) = 0;
	virtual void CancelServerQuery(void *hServerQuery) = 0;
	virtual int PingServer(uint32_t unIP, uint16_t usPort, uint64_t steamID, ISteamMatchmakingPingResponse *pRequestServersResponse) = 0;
};

typedef ISteamMatchmakingServers* (*SteamMatchmakingServers_t)(void);

#endif
