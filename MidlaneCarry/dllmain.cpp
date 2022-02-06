#include "../plugin_sdk/plugin_sdk.hpp"

#include "cassiopeia.h"
#include "orianna.h"
#include "syndra.h"
#include "annie.h"
#include "azir.h"
#include "ryze.h"

PLUGIN_NAME( "Midlane Carry" );
SUPPORTED_CHAMPIONS( champion_id::Ryze, champion_id::Cassiopeia, champion_id::Orianna, champion_id::Syndra, champion_id::Annie, champion_id::Azir );

PLUGIN_API bool on_sdk_load( plugin_sdk_core* plugin_sdk_good )
{
	DECLARE_GLOBALS( plugin_sdk_good );

	switch ( myhero->get_champion( ) )
	{
		case champion_id::Ryze:
			ryze::load( );
			return true;
		case champion_id::Cassiopeia:
			cassiopeia::load( );
			return true;
		case champion_id::Orianna:
			orianna::load( );
			return true;
		case champion_id::Syndra:
			syndra::load( );
			return true;
		case champion_id::Annie:
			annie::load( );
			return true;
		case champion_id::Azir:
			azir::load( );
			return true;
		default:
			break;
	}

	return false;
}

PLUGIN_API void on_sdk_unload( )
{
	switch ( myhero->get_champion( ) )
	{
		case champion_id::Ryze:
			ryze::unload( );
			return;
		case champion_id::Cassiopeia:
			cassiopeia::unload( );
			return;
		case champion_id::Orianna:
			orianna::unload( );
			return;
		case champion_id::Syndra:
			syndra::unload( );
			return;
		case champion_id::Annie:
			annie::unload( );
			return;
		case champion_id::Azir:
			azir::unload( );
			return;
		default:
			break;
	}
}