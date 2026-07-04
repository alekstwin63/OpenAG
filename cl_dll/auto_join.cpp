#include "hud.h"
#include "cl_util.h"
#include "net_api.h"
#include "hud_servers.h"
#include "auto_join.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define NET_API gEngfuncs.pNetAPI

namespace auto_join
{

enum state_t
{
	STATE_IDLE,
	STATE_QUERYING,
	STATE_CONNECTING,
};

static state_t state = STATE_IDLE;
static double  start_time = 0;
static int     scan_index = 0;
static int     best_index = -1;
static int     best_ping = 9999;
static int     best_players = 0;
static cvar_t  *cl_autojoin_min_players = NULL;
static cvar_t  *cl_autojoin_max_ping = NULL;

static char    server_addrs[64][64];
static int     server_count = 0;
static int     pending_queries = 0;
static int     server_resolved[64];

static int get_int_value( const char *info, const char *key )
{
	const char *val = NET_API->ValueForKey( info, key );
	if ( !val || !val[0] )
		return -1;
	return atoi( val );
}

static void connect_to_best( void )
{
	if ( best_index < 0 || best_index >= server_count )
	{
		gEngfuncs.Con_Printf( "Auto-join: No suitable server found.\n" );
		state = STATE_IDLE;
		return;
	}

	gEngfuncs.Con_Printf( "\n========================================\n" );
	gEngfuncs.Con_Printf( "  AUTO-JOIN: Connecting to %s\n", server_addrs[best_index] );
	gEngfuncs.Con_Printf( "========================================\n\n" );

	char cmd[256];
	sprintf( cmd, "connect %s\n", server_addrs[best_index] );
	EngineClientCmd( cmd );

	state = STATE_CONNECTING;
}

static void load_servers_from_file( void )
{
	char token[1024];
	server_count = 0;

	char *pfile = (char *)gEngfuncs.COM_LoadFile( "autojoin_servers.txt", 5, NULL );
	if ( !pfile )
	{
		gEngfuncs.Con_Printf( "Auto-join: No autojoin_servers.txt found.\n" );
		gEngfuncs.Con_Printf( "Create it with server IPs (one per line).\n" );
		return;
	}

	char *p = pfile;
	while ( p && server_count < 64 )
	{
		p = gEngfuncs.COM_ParseFile( p, token );
		if ( strlen( token ) <= 0 )
			break;

		if ( token[0] == '/' && token[1] == '/' )
			continue;
		if ( token[0] == '#' )
			continue;

		strncpy( server_addrs[server_count], token, 63 );
		server_addrs[server_count][63] = '\0';
		server_count++;
	}

	gEngfuncs.COM_FreeFile( pfile );
	gEngfuncs.Con_Printf( "Auto-join: Loaded %d servers\n", server_count );
}

static void query_all_servers( void )
{
	int min_players = cl_autojoin_min_players ? (int)cl_autojoin_min_players->value : 1;
	int max_ping_val = cl_autojoin_max_ping ? (int)cl_autojoin_max_ping->value : 200;

	best_index = -1;
	best_ping = 9999;
	best_players = 0;
	pending_queries = 0;

	NET_API->InitNetworking();
	NET_API->CancelAllRequests();

	gEngfuncs.Con_Printf( "\n=== AUTO-JOIN: Querying %d servers ===\n\n", server_count );

	for ( int i = 0; i < server_count; i++ )
	{
		netadr_t adr;
		server_resolved[i] = 0;

		if ( !NET_API->StringToAdr( server_addrs[i], &adr ) )
		{
			gEngfuncs.Con_Printf( "  [%d] %s - cannot resolve\n", i + 1, server_addrs[i] );
			continue;
		}

		server_resolved[i] = 1;
		pending_queries++;

		NET_API->SendRequest( 9000 + i, NETAPI_REQUEST_DETAILS, 0, 3.0, &adr, ::ServerResponse );
	}

	gEngfuncs.Con_Printf( "Auto-join: %d queries sent, waiting for responses...\n", pending_queries );
}

void init( void )
{
	cl_autojoin_min_players = CVAR_CREATE( "cl_autojoin_min_players", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	cl_autojoin_max_ping = CVAR_CREATE( "cl_autojoin_max_ping", "200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
}

void start( void )
{
	if ( state == STATE_QUERYING )
	{
		gEngfuncs.Con_Printf( "Auto-join: Already searching... use autojoin_cancel\n" );
		return;
	}

	best_index = -1;
	best_ping = 9999;
	best_players = 0;
	scan_index = 0;
	start_time = gEngfuncs.GetClientTime();
	state = STATE_QUERYING;

	load_servers_from_file();

	if ( server_count <= 0 )
	{
		state = STATE_IDLE;
		return;
	}

	query_all_servers();
}

void cancel( void )
{
	if ( state == STATE_IDLE )
		return;

	gEngfuncs.Con_Printf( "Auto-join: Cancelled.\n" );
	NET_API->CancelAllRequests();
	state = STATE_IDLE;
}

void on_server_response( int context, const char *info, int ping )
{
	if ( state != STATE_QUERYING )
		return;

	int idx = context - 9000;
	if ( idx < 0 || idx >= server_count )
		return;

	pending_queries--;

	int min_players = cl_autojoin_min_players ? (int)cl_autojoin_min_players->value : 1;
	int max_ping_val = cl_autojoin_max_ping ? (int)cl_autojoin_max_ping->value : 200;

	int current = get_int_value( info, "current" );
	int maxp = get_int_value( info, "max" );
	const char *hostname = NET_API->ValueForKey( info, "hostname" );
	const char *map = NET_API->ValueForKey( info, "map" );

	gEngfuncs.Con_Printf( "  [%d/%d] %s [%d/%d] %s (%d ms)%s\n",
		idx + 1, server_count,
		hostname ? hostname : server_addrs[idx],
		current, maxp,
		map ? map : "?",
		ping,
		( current >= min_players && ping <= max_ping_val ) ? " *" : "" );

	if ( current >= min_players && ping <= max_ping_val && ping < best_ping )
	{
		best_ping = ping;
		best_players = current;
		best_index = idx;
	}

	if ( pending_queries <= 0 )
	{
		gEngfuncs.Con_Printf( "\nAuto-join: All responses received.\n" );
		connect_to_best();
	}
}

void think( void )
{
	if ( state == STATE_IDLE || state == STATE_CONNECTING )
		return;

	double elapsed = gEngfuncs.GetClientTime() - start_time;

	if ( elapsed > 15.0 && pending_queries > 0 )
	{
		gEngfuncs.Con_Printf( "Auto-join: Timeout (%d queries still pending).\n", pending_queries );
		NET_API->CancelAllRequests();

		if ( best_index >= 0 )
			connect_to_best();
		else
		{
			gEngfuncs.Con_Printf( "Auto-join: No servers responded.\n" );
			state = STATE_IDLE;
		}
	}
}

}
