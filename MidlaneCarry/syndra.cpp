#include "syndra.h"
#include "../plugin_sdk/plugin_sdk.hpp"
#include <bitset>
#include <iostream>
#include <regex>

namespace syndra
{
#define Q_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 62, 129, 237 ))
#define W_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 227, 203, 20 ))
#define E_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 235, 12, 223 ))
#define R_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 224, 77, 13 ))

#define Q_RADIUS 125.f
#define Q_DELAY 0.7f
#define Q_SPEED FLT_MAX

#define W_RADIUS 100.f
#define W_DELAY 0.4f
#define W_SPEED 1800.f

#define E_DELAY 0.25f
#define E_SPEED 2500.f

	script_spell* q = nullptr;
	script_spell* w = nullptr;
	script_spell* e = nullptr;
	script_spell* r = nullptr;
	script_spell* eq = nullptr;

	TreeTab* main_tab = nullptr;

	namespace hitchances
	{
		TreeEntry* q = nullptr;
		TreeEntry* w = nullptr;
		TreeEntry* e = nullptr;
		TreeEntry* qe = nullptr;
	}

	namespace combo_settings
	{
		TreeEntry* q = nullptr;
		TreeEntry* w = nullptr;
		TreeEntry* e = nullptr;
		TreeEntry* qe = nullptr;
		TreeEntry* r = nullptr;
	}

	namespace harass_settings
	{
		TreeEntry* q = nullptr;
		TreeEntry* w = nullptr;
		TreeEntry* e = nullptr;
		TreeEntry* qe = nullptr;
		TreeEntry* mana = nullptr;
	}

	namespace misc_settings
	{
		TreeEntry* interrupter = nullptr;
		TreeEntry* castQE = nullptr;
		TreeEntry* instantQE = nullptr;

		std::map<game_object_script, TreeEntry*> ult_blacklist;
	}

	namespace draw_settings
	{
		TreeEntry* q = nullptr;
		TreeEntry* w = nullptr;
		TreeEntry* e = nullptr;
		TreeEntry* r = nullptr;
		TreeEntry* qe = nullptr;
		TreeEntry* wobj = nullptr;
	}

	vector last_we_pos;

	float qeComboTime = 0.f;
	float weComboTime = 0.f;
	float w_last_cast_attempt_time = 0.f;

	void detect_spells( bool is_q, bool is_w, vector pos )
	{
		if ( gametime->get_time( ) - qeComboTime < 0.5f && is_q )
		{
			w_last_cast_attempt_time = gametime->get_time( ) + 0.4f;
			e->cast( pos );
		}

		if ( gametime->get_time( ) - weComboTime < 0.5f
			&& is_w )
		{
			w_last_cast_attempt_time = gametime->get_time( ) + 0.4f;
			e->cast( pos );
		}
	}

	hit_chance get_hitchance( script_spell* spell )
	{
		if ( spell == q ) return ( ( hit_chance ) ( hitchances::q->get_int( ) + 3 ) );
		if ( spell == w ) return ( ( hit_chance ) ( hitchances::w->get_int( ) + 3 ) );
		if ( spell == e ) return ( ( hit_chance ) ( hitchances::e->get_int( ) + 3 ) );
		if ( spell == eq ) return ( ( hit_chance ) ( hitchances::qe->get_int( ) + 3 ) );

		return hit_chance::medium;
	}

	namespace orb_manager
	{
		std::regex q_cas_name( "^Syndra_.+_Q.*_aoe_gather", std::regex_constants::icase );
		std::regex w_cas_name( "^Syndra_.+_W.*_cas$", std::regex_constants::icase );
		std::regex particle_name( "^Syndra_.+_E_sphereIndicator$", std::regex_constants::icase );

		vector q_orb_pos;
		float	   q_orb_timee = 0.f;

		game_object_script w_object( bool only_orb )
		{
			if ( w->toogle_state( ) == 1 )
			{
				return nullptr;
			}

			for ( auto&& obj : entitylist->get_other_minion_objects( ) )
			{
				if ( !obj->is_dead( ) )
				{
					std::bitset<32> b( ( int ) obj->get_action_state( ) );

					if ( b.test( 10 ) )
					{
						if ( obj->get_base_skin_name( ) == "SyndraSphere" || !only_orb )
						{
							return obj;
						}
					}
				}
			}

			return nullptr;
		}

		std::vector < vector > get_orbs( bool only_to_grab = false )
		{
			std::vector < vector > result;

			for ( auto&& obj : entitylist->get_other_minion_objects( ) )
			{
				if ( !obj->is_dead( ) && obj->is_ally( ) && obj->get_base_skin_name( ) == "SyndraSphere" )
				{
					std::bitset<32> b( ( int ) obj->get_action_state( ) );

					if ( b.test( 10 ) )
					{
						continue;
					}

					if ( obj->get_owner( ) == myhero )
					{
						{
							result.push_back( obj->get_position( ) );
						}
					}
				}
			}

			if ( !only_to_grab && gametime->get_time( ) - q_orb_timee < 0.6f )
			{
				result.push_back( q_orb_pos );
			}

			return result;
		}

		vector get_object_to_grab( int range )
		{
			auto orbs = get_orbs( );
			auto from = myhero->get_position( );

			for ( auto&& orb : orbs )
			{
				if ( orb.distance( from ) < range )
				{
					return orb;
				}
			}

			return vector::zero;
		}

		void on_create( game_object_script sender )
		{
			if ( sender == nullptr || !sender->is_valid( ) || !sender->is_general_particle_emitter( ) )
			{
				return;
			}

			if ( std::regex_match( sender->get_name( ), w_cas_name ) )
			{
				detect_spells( false, true, last_we_pos );
			}

			if ( std::regex_match( sender->get_name( ), q_cas_name ) )
			{
				q_orb_pos = sender->get_position( );
				q_orb_timee = gametime->get_time( );

				detect_spells( true, false, q_orb_pos );
			}
		}
	}

	vector get_grabable_object_pos( bool only_orbs )
	{
		if ( !only_orbs )
		{
			for ( auto&& minion : entitylist->get_enemy_minions( ) )
			{
				if ( minion->is_valid_target( w->range( ) ) )
				{
					return minion->get_position( );
				}
			}

			for ( auto&& minion : entitylist->get_jugnle_mobs_minions( ) )
			{
				if ( minion->is_valid_target( w->range( ) ) )
				{
					return minion->get_position( );
				}
			}
		}
		return orb_manager::get_object_to_grab( ( int ) w->range( ) );
	}

	void use_e( game_object_script enemy )
	{
		auto from = myhero->get_position( );

		for ( auto&& orb : orb_manager::get_orbs( true ) )
		{
			if ( from.distance( orb ) < e->range( ) + 100.f )
			{
				auto startPoint = orb.extend( from, 100.f );
				auto endPoint = from.extend( orb, from.distance( orb ) > 200.f ? 1300.f : 1000.f );

				eq->set_delay( E_DELAY + from.distance( orb ) / E_SPEED );
				eq->set_range_check_from( orb );

				auto enemyPred = eq->get_prediction( enemy );

				if ( enemyPred.hitchance >= get_hitchance( e )
					&& enemyPred.get_unit_position( ).distance( startPoint, endPoint, false )
					< 55 + enemy->get_bounding_radius( ) )
				{
					e->cast( orb );
					w_last_cast_attempt_time = gametime->get_time( );
					return;
				}
			}
		}
	}

	void use_qe( game_object_script enemy )
	{
		eq->set_delay( E_DELAY + q->range( ) / E_SPEED );
		eq->set_range_check_from( myhero->get_position( ).extend( enemy->get_position( ), q->range( ) ) );

		auto prediction = eq->get_prediction( enemy );

		if ( prediction.hitchance >= get_hitchance( eq ) )
		{
			if ( enemy->is_valid_target( q->range( ) ) )
			{
				if ( q->cast( enemy, get_hitchance( q ) ) )
				{
					qeComboTime = gametime->get_time( );
					w_last_cast_attempt_time = gametime->get_time( );
				}
			} else
			{
				q->cast( myhero->get_position( ).extend( prediction.get_cast_position( ), q->range( ) - 100 ) );

				qeComboTime = gametime->get_time( );
				w_last_cast_attempt_time = gametime->get_time( );
			}

		}
	}

	void instant_qe_to_enemy( )
	{
		myhero->issue_order( hud->get_hud_input_logic( )->get_game_cursor_position( ) );

		auto t = target_selector->get_target( eq->range( ), damage_type::magical );

		if ( t && e->is_ready( ) && q->is_ready( ) )
		{
			use_qe( t );
		}
	}

	float get_combo_damage( game_object_script enemy )
	{
		auto damage = 0.f;
		damage += q->is_ready( ) ? q->get_damage( enemy ) : 0.f;
		damage += w->is_ready( ) ? w->get_damage( enemy ) : 0.f;
		damage += e->is_ready( ) ? e->get_damage( enemy ) : 0.f;
		damage += r->is_ready( ) ? r->get_damage( enemy ) : 0.f;

		return ( float ) damage;
	}

	prediction_output get_pred( int spell, game_object_script target, bool aoe = true )
	{
		prediction_input input;
		input.aoe = aoe;
		input.collision_objects.clear( );
		input.unit = target;
		input.use_bounding_radius = true;
		input.type = skillshot_type::skillshot_circle;

		if ( spell == 0 )
		{
			input._from = myhero->get_position( );
			input._range_check_from = myhero->get_position( );

			input.radius = Q_RADIUS;
			input.range = q->range( );
			input.delay = Q_DELAY;
			input.speed = Q_SPEED;
		}

		if ( spell == 1 )
		{
			input._from = w->range_check_from( );
			//input._range_check_from = w->range_check_from();

			input.radius = W_RADIUS;
			input.range = w->range( );
			input.delay = W_DELAY;
			input.speed = W_SPEED;
		}

		return prediction->get_prediction( &input );
	}

	vector last_q_pred;

	void use_spells( bool useQ, bool useW, bool useE, bool useR, bool useQe,
		bool useIgnite, bool isHarass )
	{
		useQ = useQ && q->is_ready( );
		useW = useW && w->is_ready( );
		useE = useE && e->is_ready( );
		useR = useR && r->is_ready( );

		auto qTarget = target_selector->get_target( q->range( ), damage_type::magical );
		auto wTarget = target_selector->get_target( w->range( ) + 25, damage_type::magical );
		auto rTarget = target_selector->get_target( r->range( ), damage_type::magical );
		auto qeTarget = target_selector->get_target( eq->range( ), damage_type::magical );
		auto comboDamage = rTarget != nullptr ? get_combo_damage( rTarget ) : 0;

		//Q
		if ( qTarget != nullptr && useQ )
		{
			auto pred = get_pred( 0, qTarget, false );

			if ( pred.hitchance >= get_hitchance( q ) )
			{
				last_q_pred = pred.get_cast_position( );

				if ( q->cast( pred.get_cast_position( ) ) )
				{
					return;
				}
			}
		}

		//E
		if ( gametime->get_time( ) - w_last_cast_attempt_time > ::ping->get_ping( ) / 1000.f + 0.15f && e->is_ready( ) && useE )
		{
			for ( auto&& enemy : entitylist->get_enemy_heroes( ) )
			{
				if ( enemy->is_valid_target( eq->range( ) ) )
				{
					use_e( enemy );
				}
			}
		}

		//W
		if ( useW )
		{
			if ( w->toogle_state( ) == 1 && w->is_ready( ) && qeTarget != nullptr )
			{
				auto object_pos = get_grabable_object_pos( wTarget == nullptr );

				if ( object_pos.is_valid( ) && gametime->get_time( ) - w_last_cast_attempt_time > ::ping->get_ping( ) / 1000.f + 0.3f
					&& gametime->get_time( ) - e->get_last_cast_spell_time( ) > (::ping->get_ping( ) / 1000.f) + 0.6f )
				{
					w->cast( object_pos );
					w_last_cast_attempt_time = gametime->get_time( );
					/*
					if (get_pred(1, qeTarget).hitchance >= get_hitchance(w))
					{
						w->cast(object_pos);
						w_last_cast_attempt = gametime->get_tick_count();
					}
					else if (wTarget != nullptr && get_pred(1, wTarget).hitchance >= get_hitchance(w))
					{
						w->cast(object_pos);
						w_last_cast_attempt = gametime->get_tick_count();
					}*/
				}
			} else if ( wTarget != nullptr && w->toogle_state( ) != 1 && w->is_ready( )
				&& gametime->get_time( ) - w_last_cast_attempt_time > ::ping->get_ping( ) / 1000.f + 0.1f )
			{
				auto w_object = orb_manager::w_object( false );
				if ( w_object != nullptr )
				{
					w->set_range_check_from( w_object->get_position( ) );

					auto pred = get_pred( 1, wTarget );

					if ( pred.hitchance >= get_hitchance( w ) )
					{
						w_last_cast_attempt_time = gametime->get_time( );

						if ( w->cast( pred.get_cast_position( ) ) )
						{
							return;
						}
					}
				}
			}
		}

		if ( rTarget != nullptr && useR )
		{
			useR = false;

			if ( misc_settings::ult_blacklist.find( rTarget ) != misc_settings::ult_blacklist.end( ) )
			{
				useR = !misc_settings::ult_blacklist[ rTarget ]->get_bool( );
			}
		}


		if ( rTarget != nullptr && useR && r->is_ready( ) && !rTarget->is_zombie( ) )
		{
			auto r_prediction = r->get_prediction( rTarget );

			if ( r_prediction.collision_objects.empty( ) )
			{
				if ( (comboDamage > rTarget->get_health( ) && !q->is_ready( )) || r->get_damage( rTarget ) > rTarget->get_health( ) )
				{
					r->cast( rTarget );
				}
			}
		}

		//QE
		if ( wTarget == nullptr && qeTarget != nullptr && q->is_ready( ) && e->is_ready( ) && useQe )
		{
			use_qe( qeTarget );
		}

		//WE
		if ( wTarget == nullptr && qeTarget != nullptr && e->is_ready( ) && w->is_ready( ) && useE )
		{
			auto w_object = orb_manager::w_object( true );

			if ( w_object )
			{
				eq->set_delay( E_DELAY + q->range( ) / W_SPEED );
				eq->set_range_check_from( myhero->get_position( ).extend( qeTarget->get_position( ), q->range( ) ) );

				auto prediction = eq->get_prediction( qeTarget );

				if ( prediction.hitchance >= get_hitchance( eq ) )
				{
					last_we_pos = myhero->get_position( ).extend( prediction.get_cast_position( ), q->range( ) - 100 );

					w->cast( last_we_pos );
					weComboTime = gametime->get_time( );
				}
			}
		}
	}

	void combo( )
	{
		use_spells(
			combo_settings::q->get_bool( ),
			combo_settings::w->get_bool( ),
			combo_settings::e->get_bool( ),
			combo_settings::r->get_bool( ),
			combo_settings::qe->get_bool( ),
			false,
			false );
	}

	void harass( )
	{
		if ( myhero->get_mana_percent( ) < harass_settings::mana->get_int( ) )
		{
			return;
		}

		use_spells(
			harass_settings::q->get_bool( ),
			harass_settings::w->get_bool( ),
			harass_settings::e->get_bool( ),
			false,
			harass_settings::qe->get_bool( ),
			false,
			true );
	}

	void on_draw( )
	{
		draw_manager->add_circle( last_q_pred, 65, Q_DRAW_COLOR );

		if ( draw_settings::q->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), q->range( ), Q_DRAW_COLOR );

		if ( draw_settings::w->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), w->range( ), W_DRAW_COLOR );

		if ( draw_settings::e->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), e->range( ), E_DRAW_COLOR );

		if ( draw_settings::r->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), r->range( ), R_DRAW_COLOR );

		if ( draw_settings::qe->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), eq->range( ), D3DCOLOR_ARGB( 255, 27, 227, 61 ) );

		if ( draw_settings::wobj->get_bool( ) )
		{
			auto obj = orb_manager::w_object( false );

			if ( obj )
				draw_manager->add_circle( obj->get_position( ), 100, D3DCOLOR_ARGB( 255, 50, 168, 82 ) );
		}
	}

	void before_attack( game_object_script target, bool* process )
	{
		if ( orbwalker->combo_mode( ) )
		{
			*process = !( q->is_ready( ) || w->is_ready( ) );
		}
	}

	void on_update( )
	{
		if ( myhero->is_dead( ) )
			return;

		if ( !orbwalker->combo_mode( ) && !orbwalker->can_move( 0.025f ) )
		{
			return;
		}

		r->set_range( r->level( ) == 3 ? 750.f : 675.f );

		if ( e->is_ready( ) && q->is_ready( ) &&
			myhero->get_mana( ) > q->mana_cost( ) + e->mana_cost( ) )
		{
			if ( misc_settings::castQE->get_bool( ) )
			{
				for ( auto&& enemy : entitylist->get_enemy_heroes( ) )
				{
					if ( enemy->is_valid_target( eq->range( ) ) && hud->get_hud_input_logic( )->get_game_cursor_position( ).
						distance( enemy->get_position( ) ) < 400 )
					{
						use_qe( enemy );
					}
				}

				return;
			} else if ( misc_settings::instantQE->get_bool( ) )
			{
				instant_qe_to_enemy( );
				return;
			}
		}

		if ( misc_settings::interrupter->get_bool( ) )
		{
			for ( auto&& enemy : entitylist->get_enemy_heroes( ) )
			{
				if ( enemy->is_valid_target( ) && enemy->is_casting_interruptible_spell( ) )
				{
					if ( myhero->get_distance( enemy ) < e->range( ) && e->is_ready( ) )
					{
						if ( q->is_ready( ) )
						{
							q->cast( enemy->get_position( ) );
						}

						e->cast( enemy->get_position( ) );
					} else if ( myhero->get_distance( enemy ) < eq->range( ) && e->is_ready( ) && q->is_ready( ) )
					{
						use_qe( enemy );
					}
				}
			}
		}

		if ( orbwalker->combo_mode( ) )
		{
			combo( );
		} else if ( orbwalker->harass( ) )
		{
			harass( );
		}
	}

	void load( )
	{
		q = plugin_sdk->register_spell( spellslot::q, 790 );
		w = plugin_sdk->register_spell( spellslot::w, 925 );
		e = plugin_sdk->register_spell( spellslot::e, 700 );
		r = plugin_sdk->register_spell( spellslot::r, 675 );
		eq = plugin_sdk->register_spell( spellslot::e, q->range( ) + 400 );

		q->set_skillshot( 0.6f, 180.f, FLT_MAX, { }, skillshot_type::skillshot_circle );
		w->set_skillshot( 0.25f, 225.f, 1600, { }, skillshot_type::skillshot_circle );
		e->set_skillshot( E_DELAY, ( float ) ( 45 * 0.5f ), E_SPEED, { }, skillshot_type::skillshot_circle );
		eq->set_skillshot( FLT_MAX, 55, 2000, { collisionable_objects::yasuo_wall }, skillshot_type::skillshot_circle );
		r->set_skillshot( 0, 55, FLT_MAX, { collisionable_objects::yasuo_wall }, skillshot_type::skillshot_line );

		main_tab = menu->create_tab( "carry.syndra", "Syndra" );

		auto hitchance = main_tab->add_tab( "carry.syndra.hitchance", "Hitchance Settings" );
		{
			hitchances::q = hitchance->add_combobox( "carry.syndra.hitchance_q", "Q Hitchance", { {"Low",nullptr},{"Medium",nullptr },{"High", nullptr},{"Very High",nullptr} }, ( ( int ) ( hit_chance::medium ) -3 ) );
			hitchances::w = hitchance->add_combobox( "carry.syndra.hitchance_w", "W Hitchance", { {"Low",nullptr},{"Medium",nullptr },{"High", nullptr},{"Very High",nullptr} }, ( ( int ) ( hit_chance::medium ) -3 ) );
			hitchances::e = hitchance->add_combobox( "carry.syndra.hitchance_e", "E Hitchance", { {"Low",nullptr},{"Medium",nullptr },{"High", nullptr},{"Very High",nullptr} }, ( ( int ) ( hit_chance::high ) -3 ) );
			hitchances::qe = hitchance->add_combobox( "carry.syndra.hitchance_qe", "QE Hitchance", { {"Low",nullptr},{"Medium",nullptr },{"High", nullptr},{"Very High",nullptr} }, ( ( int ) ( hit_chance::high ) -3 ) );
		}

		auto combo = main_tab->add_tab( "carry.syndra.combo", "Combo" );
		{
			combo_settings::q = combo->add_checkbox( "carry.syndra.combo.q", "Use Q", true );
			combo_settings::w = combo->add_checkbox( "carry.syndra.combo.w", "Use W", true );
			combo_settings::e = combo->add_checkbox( "carry.syndra.combo.e", "Use E", true );
			combo_settings::qe = combo->add_checkbox( "carry.syndra.combo.qe", "Use QE", true );
			combo_settings::r = combo->add_checkbox( "carry.syndra.combo.r", "Use R", true );
		}

		auto harass = main_tab->add_tab( "carry.syndra.harass", "Harass" );
		{
			harass_settings::q = harass->add_checkbox( "carry.syndra.harass.q", "Use Q", true );
			harass_settings::w = harass->add_checkbox( "carry.syndra.harass.w", "Use W", false );
			harass_settings::e = harass->add_checkbox( "carry.syndra.harass.e", "Use E", false );
			harass_settings::qe = harass->add_checkbox( "carry.syndra.harass.qe", "Use QE", false );
			harass_settings::mana = harass->add_slider( "carry.syndra.harass.mana", "Don't harass if mana < %", 20, 0, 100 );
		}

		auto misc = main_tab->add_tab( "carry.syndra.misc", "Misc" );
		{
			misc_settings::interrupter = misc->add_checkbox( "carry.syndra.misc.interrupter", "Interrupt spells", true );
			misc_settings::castQE = misc->add_hotkey( "carry.syndra.misc.castQE", "QE closest to cursor", TreeHotkeyMode::Hold, 'T', false, false );
			misc_settings::instantQE = misc->add_hotkey( "carry.syndra.misc.instantQE", "QE INSTANT", TreeHotkeyMode::Hold, 'G', false, false );

			misc->add_separator( "carry.syndra.dont_ult", "Dont Use R on:" );
			{
				for ( auto&& enemy : entitylist->get_enemy_heroes( ) )
				{
					misc_settings::ult_blacklist[ enemy ] = misc->add_checkbox( "carry.syndra.dont_ult." + enemy->get_base_skin_name( ), enemy->get_base_skin_name( ), false );
				}
			}
		}

		auto draw = main_tab->add_tab( "carry.syndra.draw", "Draw Settings" );
		{
			draw_settings::q = draw->add_checkbox( "carry.syndra.draw.q", "Draw Q range", false );
			draw_settings::w = draw->add_checkbox( "carry.syndra.draw.w", "Draw W range", true );
			draw_settings::e = draw->add_checkbox( "carry.syndra.draw.e", "Draw E range", false );
			draw_settings::r = draw->add_checkbox( "carry.syndra.draw.r", "Draw R range", false );
			draw_settings::qe = draw->add_checkbox( "carry.syndra.draw.qe", "Draw QE range", true );
			draw_settings::wobj = draw->add_checkbox( "carry.syndra.draw.wobj", "Draw W object", true );
		}

		event_handler<events::on_update>::add_callback( on_update );
		event_handler<events::on_draw>::add_callback( on_draw );
		event_handler<events::on_before_attack_orbwalker>::add_callback( before_attack );
		event_handler<events::on_create_object>::add_callback( orb_manager::on_create );
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
		event_handler<events::on_before_attack_orbwalker>::remove_handler( before_attack );
		event_handler<events::on_create_object>::remove_handler( orb_manager::on_create );
	}
}
