#ifndef AUTO_JOIN_H
#define AUTO_JOIN_H
#pragma once

namespace auto_join
{
	void init( void );
	void think( void );
	void start( void );
	void cancel( void );
	void on_server_response( int context, const char *info, int ping );
}

#endif
