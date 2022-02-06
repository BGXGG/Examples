#include "cassiopeia.h"
#include "../plugin_sdk/plugin_sdk.hpp"

script_spell* q = nullptr;
script_spell* w = nullptr;
script_spell* e = nullptr;
script_spell* r = nullptr;

#define CASSIO_Q_RANGE 850.f
#define CASSIO_W_RANGE 700.f
#define CASSIO_E_RANGE 700.f
#define CASSIO_R_RANGE 825.f

#define CASSIO_E_SPEED 2500.f

#define Q_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 62, 129, 237 ))
#define W_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 227, 203, 20 ))
#define E_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 235, 12, 223 ))
#define R_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 224, 77, 13 ))

namespace farm_settings
{
	TreeEntry* use_q = nullptr;
	TreeEntry* use_w = nullptr;
	TreeEntry* use_e = nullptr;

	TreeEntry* q_min_minions = nullptr;
	TreeEntry* w_min_minions = nullptr;

	TreeEntry* q_min_minions_jungle = nullptr;
	TreeEntry* w_min_minions_jungle = nullptr;

	TreeEntry* use_multi_e = nullptr;
	TreeEntry* use_multi_e_when_health_below = nullptr;

	TreeEntry* use_spells_mana = nullptr;
}

namespace hitchance_settings
{
	TreeEntry* q_hitchance = nullptr;
	TreeEntry* w_hitchance = nullptr;
	TreeEntry* r_hitchance = nullptr;
}


namespace harras_settings
{
	TreeEntry* use_q = nullptr;
	TreeEntry* use_e = nullptr;

	TreeEntry* use_spells_when_mana = nullptr;
}

namespace combo_settings
{
	TreeEntry* use_q = nullptr;
	TreeEntry* use_w = nullptr;
	TreeEntry* use_e = nullptr;
	TreeEntry* use_r = nullptr;

	TreeEntry* min_r_targets = nullptr;

	TreeEntry* r_manual_cast = nullptr;

	std::map<uint32_t, TreeEntry*> always_use_r;
}

namespace draw_settings
{
	TreeEntry* draw_range_q = nullptr;
	TreeEntry* draw_range_w = nullptr;
	TreeEntry* draw_range_e = nullptr;
	TreeEntry* draw_range_r = nullptr;
}

TreeTab* main_tab = nullptr;

float aa_missile_speed = 0.f;

hit_chance get_hitchance_by_config( TreeEntry* hit )
{
	switch ( hit->get_int( ) )
	{
		case 0:
			return hit_chance::low;
			break;
		case 1:
			return hit_chance::medium;
			break;
		case 2:
			return hit_chance::high;
			break;
		case 3:
			return hit_chance::very_high;
			break;
	}
	return hit_chance::medium;
}

void e_logic_minion_last_hit( )
{
	if ( entitylist->get_enemy_minions( ).empty( ) && entitylist->get_jugnle_mobs_minions( ).empty( ) )
		return;

	std::vector<game_object_script> minions_list;
	minions_list.reserve( entitylist->get_enemy_minions( ).size( ) + entitylist->get_jugnle_mobs_minions( ).size( ) );
	minions_list.insert( minions_list.end( ), entitylist->get_enemy_minions( ).begin( ), entitylist->get_enemy_minions( ).end( ) );
	minions_list.insert( minions_list.end( ), entitylist->get_jugnle_mobs_minions( ).begin( ), entitylist->get_jugnle_mobs_minions( ).end( ) );

	for ( auto& minion : minions_list )
	{
		if ( minion->get_distance( myhero ) > CASSIO_E_RANGE )
			continue;

		auto t = 0.125f + fmax( 0.f, myhero->get_position( ).distance( minion->get_position( ) ) - myhero->get_bounding_radius( ) )
			/ CASSIO_E_SPEED;

		auto health_predicted = health_prediction->get_health_prediction( minion, t );

		if ( health_predicted <= 0.f )
			continue;

		auto e_dmg = e->get_damage( minion );

		if ( health_predicted - e_dmg < 0 )
		{
			e->cast( minion );
			break;
		}

		if ( orbwalker->lane_clear_mode( ) && farm_settings::use_multi_e->get_bool( ) )
		{
			if ( minion->get_team( ) == game_object_team::neutral || ( myhero->get_health_percent( ) < farm_settings::use_multi_e_when_health_below->get_int( ) && myhero->get_mana_percent( ) > farm_settings::use_spells_mana->get_int( ) ) )
			{
				if ( minion->get_buff_by_type( buff_type::Poison ) )
				{
					e->cast( minion );
					break;
				}
			}
		}
	}
}

void logic_e_target( )
{
	std::vector<game_object_script> poisoned_targets;

	for ( auto& it : entitylist->get_enemy_heroes( ) )
	{
		if ( !it->is_valid_target( e->range( ) ) )
			continue;

		if ( auto poision = it->get_buff_by_type( buff_type::Poison ) )
		{
			auto t = poision->get_remaining_time( );

			if ( t > 0.25 )
				poisoned_targets.push_back( it );
		}
	}

	game_object_script target = nullptr;

	if ( !poisoned_targets.empty( ) || !orbwalker->harass( ) )
	{
		if ( poisoned_targets.empty( ) )
		{
			if ( ( ( e->mana_cost( ) * 5 ) + q->mana_cost( ) ) < myhero->get_mana( ) )
				target = target_selector->get_target( e->range( ), damage_type::magical );
		}
		else
			target = target_selector->get_target( poisoned_targets, damage_type::magical );
	}

	if ( target )
		e->cast( target );
}

void q_logic( )
{
	if ( myhero->get_mana( ) > e->mana_cost( ) + q->mana_cost( ) )
	{
		auto q_target = target_selector->get_target( q->range( ), damage_type::magical );

		if ( q_target )
			q->cast( q_target, get_hitchance_by_config( hitchance_settings::q_hitchance ) );
	}
}

void w_logic( )
{
	if ( myhero->get_mana( ) > w->mana_cost( ) + e->mana_cost( ) + r->mana_cost( ) )
	{
		std::vector<game_object_script> valid_targets;

		for ( auto&& target : entitylist->get_enemy_heroes( ) )
		{
			if ( target->is_valid_target( w->range( ) ) )
				valid_targets.push_back( target );
		}

		for ( auto&& target : valid_targets )
		{
			{
				auto active_spell = target->get_active_spell( );

				if ( active_spell )
				{
					if ( active_spell->get_attack_cast_delay( ) > 0.35f )
					{
						w->cast( target->get_position( ) );
						break;
					}
				}
			}

			/*MORE W LOGIC*/
			{
				if ( !target->get_buff_by_type( buff_type::Poison ) && !q->is_ready( ) )
					if ( w->cast( target, get_hitchance_by_config( hitchance_settings::w_hitchance ) ) )
						break;
			}
		}
	}
}

int32_t always_use_r_on( game_object_script obj )
{
	auto it = combo_settings::always_use_r.find( obj->get_network_id( ) );

	if ( it != combo_settings::always_use_r.end( ) )
		return it->second->get_int( );

	combo_settings::always_use_r[ obj->get_network_id( ) ] = main_tab->add_combobox( "ex_cass.combo.always.r." + std::to_string( obj->get_network_id( ) ), obj->get_model( ), { {"Never",nullptr}, {"Facing",nullptr }, {"Always",nullptr} }, 0, false );

	return 0;
}

bool try_cast_r( )
{
	auto target = r->get_target( );

	if ( target )
	{
		auto pred = r->get_prediction( target, true );
		auto hitCount = 0;

		for ( auto& hero : pred.aoe_targets_hit )
		{
			if ( hero->is_facing( myhero ) )
				hitCount++;
		}

		if ( hitCount >= combo_settings::min_r_targets->get_int( ) || ( hitCount > 0 && combo_settings::r_manual_cast->get_bool( ) ) )
		{
			if ( r->cast( pred.get_cast_position( ) ) )
				return true;
		}

		if ( hitCount == 0 && target->is_facing( myhero ) )
		{
			if ( r->cast( target, get_hitchance_by_config( hitchance_settings::r_hitchance ) ) )
				return true;
		}
	}
	return false;
}

void r_logic( )
{
	if ( myhero->count_enemies_in_range( r->range( ) ) > 1 || ( combo_settings::r_manual_cast->get_bool( ) && myhero->count_enemies_in_range( r->range( ) ) > 0 ) )
	{
		if ( try_cast_r( ) )
			return;
	}

	for ( auto&& it : entitylist->get_enemy_heroes( ) )
	{
		if ( !it->is_valid_target( r->range( ) ) )
			continue;

		auto combo_use = always_use_r_on( it );

		if ( !combo_use )
			continue;

		if ( combo_use == 1 && it->is_facing( myhero ) )
			if ( r->cast( it, get_hitchance_by_config( hitchance_settings::r_hitchance ) ) )
				return;

		if ( combo_use == 2 )
			if ( r->cast( it, get_hitchance_by_config( hitchance_settings::r_hitchance ) ) )
				return;
	}
}

bool was_block_attacks = false;
void on_update( )
{
	if ( myhero->is_dead( ) )
		return;

	if ( was_block_attacks && !orbwalker->combo_mode( ) )
	{
		orbwalker->set_attack( true );
		was_block_attacks = false;
	}

	orbwalker->set_orbwalking_target( nullptr );

	if ( orbwalker->lane_clear_mode( ) || orbwalker->last_hit_mode( ) )
	{
		if ( farm_settings::use_e->get_bool( ) && e->is_ready( ) )
			e_logic_minion_last_hit( );

		if ( orbwalker->lane_clear_mode( ) )
		{
			if ( myhero->get_mana_percent( ) > farm_settings::use_spells_mana->get_int( ) )
			{
				if ( farm_settings::use_q->get_bool( ) && q->is_ready( ) )
				{
					if ( !q->cast_on_best_farm_position( farm_settings::q_min_minions->get_int( ) ) )
					{
						q->cast_on_best_farm_position( farm_settings::q_min_minions_jungle->get_int( ), true );
					}
				}
				if ( farm_settings::use_w->get_bool( ) && w->is_ready( ) )
				{
					if ( !w->cast_on_best_farm_position( farm_settings::w_min_minions->get_int( ) ) )
					{
						w->cast_on_best_farm_position( farm_settings::w_min_minions_jungle->get_int( ), true );
					}
				}
			}
		}
	}
	else if ( orbwalker->harass( ) )
	{
		if ( myhero->get_mana_percent( ) > harras_settings::use_spells_when_mana->get_int( ) )
		{
			if ( harras_settings::use_q->get_bool( ) && q->is_ready( ) )
				q_logic( );

			if ( harras_settings::use_e->get_bool( ) && e->is_ready( ) )
				logic_e_target( );
		}

		if ( farm_settings::use_e->get_bool( ) && e->is_ready( ) )
			e_logic_minion_last_hit( );

	}
	else if ( orbwalker->combo_mode( ) )
	{
		if ( myhero->get_mana( ) > ( e->mana_cost( ) * 2.f ) )
		{
			orbwalker->set_attack( false );
			was_block_attacks = true;
		}
		else if ( was_block_attacks )
		{
			orbwalker->set_attack( true );
			was_block_attacks = false;
		}

		if ( combo_settings::use_q->get_bool( ) && q->is_ready( ) )
			q_logic( );

		if ( combo_settings::use_w->get_bool( ) && w->is_ready( ) )
			w_logic( );

		if ( combo_settings::use_e->get_bool( ) && e->is_ready( ) )
			logic_e_target( );

		if ( combo_settings::use_r->get_bool( ) && r->is_ready( ) )
			r_logic( );
	}
	else if ( combo_settings::r_manual_cast->get_bool( ) && myhero->count_enemies_in_range( r->range( ) ) > 0 && r->is_ready( ) )
		try_cast_r( );
}

void on_draw( )
{
	if ( draw_settings::draw_range_q->get_bool( ) )
		draw_manager->add_circle( myhero->get_position( ), q->range( ), Q_DRAW_COLOR );

	if ( draw_settings::draw_range_w->get_bool( ) )
		draw_manager->add_circle( myhero->get_position( ), w->range( ), W_DRAW_COLOR );

	if ( draw_settings::draw_range_e->get_bool( ) )
		draw_manager->add_circle( myhero->get_position( ), e->range( ), E_DRAW_COLOR );

	if ( draw_settings::draw_range_r->get_bool( ) )
		draw_manager->add_circle( myhero->get_position( ), r->range( ), R_DRAW_COLOR );
}
namespace cassiopeia
{
	void load( )
	{
		q = plugin_sdk->register_spell( spellslot::q, CASSIO_Q_RANGE );
		w = plugin_sdk->register_spell( spellslot::w, CASSIO_W_RANGE );
		e = plugin_sdk->register_spell( spellslot::e, CASSIO_E_RANGE );
		r = plugin_sdk->register_spell( spellslot::r, CASSIO_R_RANGE );

		q->set_skillshot( 0.65f, 200.f, FLT_MAX, {}, skillshot_type::skillshot_circle );
		w->set_skillshot( 0.25, 160.f, FLT_MAX, { collisionable_objects::yasuo_wall }, skillshot_type::skillshot_circle );
		r->set_skillshot( 0.5f, degrees_to_radians( 80.f ), FLT_MAX, { collisionable_objects::yasuo_wall }, skillshot_type::skillshot_cone );

		main_tab = menu->create_tab( "ex_cassio", "Cassiopeia" );

		auto combo = main_tab->add_tab( "ex_cass.combo", "Combo Settings" );
		{
			combo_settings::use_q = combo->add_checkbox( "ex_cass.combo.q", "Use Q", true );
			combo_settings::use_w = combo->add_checkbox( "ex_cass.combo.w", "Use W", true );
			combo_settings::use_e = combo->add_checkbox( "ex_cass.combo.e", "Use E", true );
			combo_settings::use_r = combo->add_checkbox( "ex_cass.combo.r", "Use R", true );

			combo_settings::min_r_targets = combo->add_slider( "ex_cass.combo.r.targets", "Use R min targets", 2, 1, 5 );

			combo_settings::r_manual_cast = combo->add_hotkey( "ex_cass.combo.r.manualcast", "R Manual Cast", TreeHotkeyMode::Hold, 0x54, false );

			auto use_r_on = combo->add_tab( "ex_cass.combo", "Use R on" );

			for ( auto&& it : entitylist->get_enemy_heroes( ) )
				combo_settings::always_use_r[ it->get_network_id( ) ] = use_r_on->add_combobox( "ex_cass.combo.always.r." + std::to_string( it->get_network_id( ) ), it->get_model( ), { {"Never",nullptr}, {"Facing",nullptr }, {"Always",nullptr} }, 0, false );
		}

		auto harras = main_tab->add_tab( "ex_cass.harras", "Harras Settings" );
		{
			harras_settings::use_q = harras->add_checkbox( "ex_cass.harras.q", "Use Q", true );
			harras_settings::use_e = harras->add_checkbox( "ex_cass.harras.e", "Use E", false );

			harras_settings::use_spells_when_mana = harras->add_slider( "ex_cass.harras.mana", "Use spells when mana > %", 40, 1, 100 );
		}

		auto farm = main_tab->add_tab( "ex_cass.farm", "Farm Settings" );
		{
			farm_settings::use_q = farm->add_checkbox( "ex_cass.farm.q", "Use Q", true );
			farm_settings::use_w = farm->add_checkbox( "ex_cass.farm.w", "Use W", false );
			farm_settings::use_e = farm->add_checkbox( "ex_cass.farm.e", "Use E", true );

			farm_settings::q_min_minions = farm->add_slider( "ex_cass.farm.q.m", "Min Q minions", 3, 1, 6 );
			farm_settings::w_min_minions = farm->add_slider( "ex_cass.farm.w.m", "Min W minions", 3, 1, 6 );

			farm_settings::q_min_minions_jungle = farm->add_slider( "ex_cass.farm.q.m.j", "Min Q minions jungle", 1, 1, 6 );
			farm_settings::w_min_minions_jungle = farm->add_slider( "ex_cass.farm.w.m.j", "Min W minions jungle", 3, 1, 6 );

			farm_settings::use_multi_e = farm->add_checkbox( "ex_cass.farm.me", "Use Multi E", true );

			farm_settings::use_multi_e_when_health_below = farm->add_slider( "ex_cass.farm.me_hp", "Use Multi E below HP", 80, 1, 100 );

			farm_settings::use_spells_mana = farm->add_slider( "ex_cass.farm.mana", "Use spells when mana > %", 50, 1, 100 );
		}

		auto hitchance = main_tab->add_tab( "ex_cass.hitchance", "Hitchance settings" );
		{
			hitchance_settings::q_hitchance = hitchance->add_combobox( "ex_cass.hitchance.q", "Hitchance Q", { {"Low",nullptr},{"Medium",nullptr },{"High", nullptr},{"Very High",nullptr} }, 2 );
			hitchance_settings::w_hitchance = hitchance->add_combobox( "ex_cass.hitchance.w", "Hitchance W", { {"Low",nullptr},{"Medium",nullptr },{"High", nullptr},{"Very High",nullptr} }, 2 );
			hitchance_settings::r_hitchance = hitchance->add_combobox( "ex_cass.hitchance.r", "Hitchance R", { {"Low",nullptr},{"Medium",nullptr },{"High", nullptr},{"Very High",nullptr} }, 2 );
		}

		auto draw = main_tab->add_tab( "ex_cass.draw", "Draw Settings" );
		{
			draw_settings::draw_range_q = draw->add_checkbox( "ex_cass.draw.q", "Draw Q range", true );
			draw_settings::draw_range_w = draw->add_checkbox( "ex_cass.draw.w", "Draw W range", true );
			draw_settings::draw_range_e = draw->add_checkbox( "ex_cass.draw.e", "Draw E range", true );
			draw_settings::draw_range_r = draw->add_checkbox( "ex_cass.draw.r", "Draw R range", true );
		}

		aa_missile_speed = myhero->get_auto_attack( )->MissileSpeed( );

		event_handler<events::on_update>::add_callback( on_update );
		event_handler<events::on_draw>::add_callback( on_draw );
	}

	void unload( )
	{
		menu->delete_tab( main_tab );

		plugin_sdk->remove_spell( q );
		plugin_sdk->remove_spell( w );
		plugin_sdk->remove_spell( e );
		plugin_sdk->remove_spell( r );

		event_handler<events::on_update>::remove_handler( on_update );
		event_handler<events::on_draw>::remove_handler( on_draw );
	}
}