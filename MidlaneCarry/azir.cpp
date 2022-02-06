#include "azir.h"
#include "../plugin_sdk/plugin_sdk.hpp"

namespace azir
{
#define Q_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 62, 129, 237 ))
#define W_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 227, 203, 20 ))
#define E_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 235, 12, 223 ))
#define R_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 224, 77, 13 ))

#define AZIR_Q_RANGE 740
#define AZIR_W_RANGE 560
#define AZIR_E_RANGE 1100
#define AZIR_R_RANGE 300

#define AZIR_SOLIDER_AA_RANGE 310

	script_spell* q = nullptr;
	script_spell* w = nullptr;
	script_spell* e = nullptr;
	script_spell* r = nullptr;

	TreeTab* main_tab = nullptr;


	namespace draw_settings
	{
		TreeEntry* draw_range_q = nullptr;
		TreeEntry* draw_range_w = nullptr;
		TreeEntry* draw_range_e = nullptr;
		TreeEntry* draw_range_r = nullptr;
	}

	namespace combo_settings
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;
		TreeEntry* use_e = nullptr; //todo
		TreeEntry* use_r = nullptr; //todo

		TreeEntry* cast_w_when_target_near = nullptr;

		TreeEntry* q_min_soliders = nullptr;
		TreeEntry* dont_cast_q_soliders_in_range = nullptr;
	};

	namespace flee_settings
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;
		TreeEntry* use_e = nullptr;

		TreeEntry* extra_delay = nullptr;
	};

	namespace harras_settings
	{
		TreeEntry* use_w = nullptr;
		TreeEntry* use_spells_when_mana = nullptr;
	}

	namespace farm_settings
	{
		TreeEntry* use_w_laneclear = nullptr;
		TreeEntry* use_w_last_hit = nullptr;
		TreeEntry* use_q = nullptr;

		TreeEntry* max_soliders_in_place = nullptr;
		TreeEntry* use_w_min_minions = nullptr;

		TreeEntry* use_spells_when_mana = nullptr;
	};


	namespace insec_settings
	{
		TreeEntry* insec_key = nullptr;

		TreeEntry* insec_extra_key = nullptr;
	};

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

	void fill_list( std::vector<game_object_script>* my_soliders, std::vector<game_object_script>* valid_enemy_targets )
	{
		if ( my_soliders )
		{
			for ( auto& it : entitylist->get_other_minion_objects( ) )
			{
				if ( it->is_dead( ) )
					continue;

				if ( it->get_object_owner( ) != myhero->get_base( ) )
					continue;

				if ( it->get_name( ) != "AzirSoldier" )
					continue;

				if ( it->get_attack_range( ) == 0.f )
					continue;

				my_soliders->push_back( it );
			}
		}

		if ( valid_enemy_targets )
		{
			for ( auto& it : entitylist->get_enemy_heroes( ) )
			{
				if ( !it->is_valid_target( ) )
					continue;

				valid_enemy_targets->push_back( it );
			}
		}
	}

	bool doing_insec = false;
	game_object_script insec_target = nullptr;
	void on_new_path( game_object_script sender, const std::vector<vector>& path, bool is_dash, float dash_speed )
	{
		if ( sender->is_me( ) )
		{
			if ( is_dash )
			{
				auto is_insec = ( insec_settings::insec_key->get_bool( ) && insec_target );
				if ( orbwalker->flee_mode( ) || is_insec )
				{
					if ( q->is_ready( ) )
					{
						std::vector<game_object_script> my_soliders;
						fill_list( &my_soliders, nullptr );

						bool is_dashing_to_soliders = false;

						for ( auto& it : my_soliders )
						{
							if ( it->get_distance( path.back( ) ) <= 10.f )
							{
								is_dashing_to_soliders = true;
								break;
							}
						}

						if ( is_dashing_to_soliders )
						{
							auto travel_time = ( myhero->get_distance( path.back( ) ) / dash_speed );

							if ( is_insec )
							{
								doing_insec = true;
								scheduler->delay_action( travel_time - 0.1f - (insec_settings::insec_extra_key->get_int( ) / 1000.f), [ ]( )
									{
										q->cast( insec_target->get_position( ) );
										doing_insec = false;
									} );
							} else if ( flee_settings::use_q->get_bool( ) )
							{
								scheduler->delay_action( travel_time - 0.1f - (flee_settings::extra_delay->get_int( ) / 1000.f), [ ]( )
									{
										auto cursor_pos = hud->get_hud_input_logic( )->get_game_cursor_position( );
										auto direction = ( cursor_pos - myhero->get_position( ) ).normalized( );
										auto final_pos = myhero->get_position( ) + ( direction * q->range( ) );
										q->cast( final_pos );
									} );
							}
						}
					}
				}
			}
		}
	}

	game_object_script get_insec_target( )
	{
		return target_selector->get_target( AZIR_W_RANGE + AZIR_Q_RANGE, damage_type::magical );
	}

	void on_update( )
	{
		if ( myhero->is_dead( ) )
			return;

		std::vector<game_object_script> my_soliders;
		std::vector<game_object_script> valid_enemy_targets;

		if ( insec_settings::insec_key->get_bool( ) )
		{
			auto target = get_insec_target( );

			if ( target )
			{
				if ( r->is_ready( ) )
				{
					if ( target->get_distance( myhero ) < r->range( ) )
					{
						game_object_script best_ally_turret = nullptr;

						for ( auto& it : entitylist->get_ally_turrets( ) )
							if ( !it->is_dead( ) )
								if ( !best_ally_turret || best_ally_turret->get_distance( myhero ) > it->get_distance( myhero ) )
									best_ally_turret = it;

						if ( best_ally_turret )
						{
							r->cast( best_ally_turret->get_position( ) );
						}
					} else if ( e->is_ready( ) && q->is_ready( ) )
					{
						auto total_mana_cost = q->mana_cost( ) + w->mana_cost( ) + e->mana_cost( ) + r->mana_cost( );

						if ( myhero->get_mana( ) > total_mana_cost )
						{
							fill_list( &my_soliders, &valid_enemy_targets );
							game_object_script best_solider_escape = nullptr;

							for ( auto& it : my_soliders )
							{
								if ( it->get_distance( myhero ) > e->range( ) )
									continue;

								auto v1 = it->get_position( ) - myhero->get_position( );
								auto v2 = target->get_position( ) - myhero->get_position( );
								auto angle = v1.angle_between( v2 );

								if ( angle > 90.f )
									continue;

								if ( !best_solider_escape || best_solider_escape->get_distance( target->get_position( ) ) < it->get_distance( target->get_position( ) ) )
									best_solider_escape = it;
							}

							if ( best_solider_escape )
							{
								insec_target = target;
								e->cast( best_solider_escape->get_position( ) );
							} else if ( w->is_ready( ) )
								w->cast( myhero->get_position( ).extend( target->get_position( ), w->range( ) ) );

						}
					}
				}
			}
		} else if ( !doing_insec )
			insec_target = nullptr;

		if ( orbwalker->flee_mode( ) )
		{
			fill_list( &my_soliders, &valid_enemy_targets );

			game_object_script best_solider_escape = nullptr;
			auto cursor_pos = hud->get_hud_input_logic( )->get_game_cursor_position( );

			if ( e->is_ready( ) && flee_settings::use_e->get_bool( ) )
			{
				for ( auto& it : my_soliders )
				{
					if ( it->get_distance( myhero ) > e->range( ) )
						continue;

					auto v1 = it->get_position( ) - myhero->get_position( );
					auto v2 = cursor_pos - myhero->get_position( );
					auto angle = v1.angle_between( v2 );

					if ( angle > 90.f )
						continue;

					if ( !best_solider_escape || best_solider_escape->get_distance( cursor_pos ) < it->get_distance( cursor_pos ) )
						best_solider_escape = it;
				}

				if ( best_solider_escape )
					e->cast( best_solider_escape->get_position( ) );
				else if ( flee_settings::use_w->get_bool( ) && w->is_ready( ) && q->is_ready( ) )
					w->cast( myhero->get_position( ).extend( cursor_pos, w->range( ) ) );
			}
		}

		if ( orbwalker->combo_mode( ) )
		{
			fill_list( &my_soliders, &valid_enemy_targets );

			if ( combo_settings::use_w->get_bool( ) && w->is_ready( ) )
			{
				auto w_target = target_selector->get_target( w->range( ) + AZIR_SOLIDER_AA_RANGE, damage_type::magical );
				if ( w_target )
					w->cast( w_target->get_position( ) );
				else if ( combo_settings::cast_w_when_target_near->get_bool( ) )
				{
					float closets_target = FLT_MAX;
					vector best_pos = vector( );

					for ( auto& it : valid_enemy_targets )
					{
						auto dist = it->get_distance( myhero );
						if ( dist < closets_target )
						{
							closets_target = dist;
							best_pos = it->get_position( );
						}
					}

					if ( closets_target < 1100 )
					{
						w->cast( myhero->get_position( ).extend( best_pos, w->range( ) ) );
					}
				}
			}

			if ( combo_settings::use_q->get_bool( ) && q->is_ready( ) && !my_soliders.empty( ) )
			{
				auto q_target = target_selector->get_target( q->range( ) + ( AZIR_SOLIDER_AA_RANGE / 3 ), damage_type::magical );
				if ( q_target )
				{
					int is_in_solider_range = 0;

					for ( auto& it : my_soliders )
					{
						if ( it->is_in_auto_attack_range( q_target, it->get_bounding_radius( ) * -2.f - 15.f ) )
							is_in_solider_range++;
					}

					if ( ( is_in_solider_range < combo_settings::dont_cast_q_soliders_in_range->get_int( ) && (int)my_soliders.size( ) >= combo_settings::q_min_soliders->get_int( ) ) || ( q->get_damage( q_target ) + w->get_damage( q_target ) >= q_target->get_health( ) ) )
					{
						if ( q_target->get_path_controller( )->is_moving( ) )
						{
							auto velocity = q_target->get_pathing_direction( ) * q_target->get_move_speed( );
							velocity = velocity * 0.15f;

							auto target_posistion = q_target->get_position( ) + velocity;

							if ( target_posistion.distance( myhero->get_position( ) ) > q->range( ) )
								q->cast( q_target->get_position( ) );
							else
								q->cast( target_posistion );
						} else
							q->cast( q_target->get_position( ) );
					}
				}
			}
		}

		if ( orbwalker->lane_clear_mode( ) || orbwalker->last_hit_mode( ) || orbwalker->harass( ) )
		{
			fill_list( &my_soliders, &valid_enemy_targets );

			if ( orbwalker->harass( ) && harras_settings::use_w->get_bool( ) )
			{
				if ( myhero->get_mana_percent( ) > harras_settings::use_spells_when_mana->get_int( ) )
				{
					if ( w->is_ready( ) )
					{
						auto target_w = target_selector->get_target( w->range( ), damage_type::magical );
						if ( target_w )
							w->cast( target_w->get_position( ) );
					}
				}
			}

			if ( myhero->get_mana_percent( ) > farm_settings::use_spells_when_mana->get_int( ) )
			{
				if ( ( orbwalker->lane_clear_mode( ) && farm_settings::use_w_laneclear->get_bool( ) ) || ( ( orbwalker->last_hit_mode( ) || orbwalker->harass( ) ) && farm_settings::use_w_last_hit->get_bool( ) ) )
				{
					if ( w->is_ready( ) )
					{
						auto pos = w->get_cast_on_best_farm_position( farm_settings::use_w_min_minions->get_int( ) );

						if ( pos.is_valid( ) && !pos.is_under_enemy_turret( ) )
						{
							int soliders_in_range = 0;
							for ( auto& it : my_soliders )
							{
								if ( it->get_distance( pos ) < AZIR_SOLIDER_AA_RANGE )
									soliders_in_range++;
							}

							if ( soliders_in_range < farm_settings::max_soliders_in_place->get_int( ) )
							{
								w->cast( pos );
							}
						} else if ( !pos.is_valid( ) )
						{
							auto pos = w->get_cast_on_best_farm_position( 1, true );

							if ( pos.is_valid( ) )
							{
								int soliders_in_range = 0;
								for ( auto& it : my_soliders )
								{
									if ( it->get_distance( pos ) < AZIR_SOLIDER_AA_RANGE )
									{
										soliders_in_range++;
										break;
									}
								}

								if ( soliders_in_range == 0 )
									w->cast( pos );
							}
						}
					}
				}

				if ( orbwalker->lane_clear_mode( ) && farm_settings::use_q->get_bool( ) && !my_soliders.empty( ) )
				{
					if ( q->is_ready( ) )
					{
						auto pos = w->get_cast_on_best_farm_position( 2 );
						if ( pos.is_valid( ) && !pos.is_under_enemy_turret( ) )
						{
							int soliders_in_range = 0;
							for ( auto& it : my_soliders )
							{
								if ( it->get_distance( pos ) < AZIR_SOLIDER_AA_RANGE )
									soliders_in_range++;
							}

							if ( soliders_in_range < (int)my_soliders.size( ) )
							{
								q->cast( pos );
							}
						}
					}
				}
			}
		}
	}

	void on_spell( game_object_script obj, spell_instance_script spell )
	{
		if ( !obj->is_me( ) )
			return;

		console->print( "%s => %f => %f", spell->get_spell_data( )->get_name( ).c_str( ), spell->get_attack_cast_delay( ), obj->get_distance( entitylist->get_enemy_heroes( ).front( ) ) );
	}

	void load( )
	{
		q = plugin_sdk->register_spell( spellslot::q, AZIR_Q_RANGE );
		w = plugin_sdk->register_spell( spellslot::w, AZIR_W_RANGE );
		e = plugin_sdk->register_spell( spellslot::e, AZIR_E_RANGE );
		r = plugin_sdk->register_spell( spellslot::r, AZIR_R_RANGE );

		w->set_skillshot( 0.25f, AZIR_SOLIDER_AA_RANGE, FLT_MAX, {}, skillshot_type::skillshot_circle );

		main_tab = menu->create_tab( "carry.azir", "Azir" );

		auto combo = main_tab->add_tab( "carry.azir.combo", "Combo Settings" );
		{
			combo_settings::use_q = combo->add_checkbox( "carry.azir.combo.q", "Use Q", true );
			combo_settings::use_w = combo->add_checkbox( "carry.azir.combo.w", "Use W", true );
			combo_settings::use_e = combo->add_checkbox( "carry.azir.combo.e", "Use E", false );
			combo_settings::use_r = combo->add_checkbox( "carry.azir.combo.r", "Use R", false );

			combo_settings::cast_w_when_target_near = combo->add_checkbox( "carry.azir.combo.w.near", "Use W when target near", true );

			combo_settings::q_min_soliders = combo->add_slider( "carry.azir.combo.w.min", "W min soliders", 2, 1, 4 );
			combo_settings::dont_cast_q_soliders_in_range = combo->add_slider( "carry.azir.combo.w.max", "Dont cast if x soliders in range", 2, 1, 4 );
		};

		auto insec = main_tab->add_tab( "carry.azir.insec", "Insec Settings" );
		{
			insec_settings::insec_key = insec->add_hotkey( "carry.azir.insec.key", "Insec key", TreeHotkeyMode::Hold, 0x54, false );
			insec_settings::insec_extra_key = insec->add_slider( "carry.azir.insec.delay", "Extra delay", 0, 0, 100 );
			//more option target, position
		}

		auto harras = main_tab->add_tab( "carry.azir.harras", "Harras Settings" );
		{
			harras_settings::use_w = harras->add_checkbox( "carry.azir.harras.w", "Use W", true );
			harras_settings::use_spells_when_mana = harras->add_slider( "carry.azir.harras.mana", "Use spells when mana > %", 40, 1, 100 );
		};

		auto farm = main_tab->add_tab( "carry.azir.farm", "Farm Settings" );
		{
			farm_settings::use_w_laneclear = farm->add_checkbox( "carry.azir.farm.w.v", "Use W (laneclear)", true );
			farm_settings::use_w_last_hit = farm->add_checkbox( "carry.azir.farm.w.c", "Use W (last hit)", false );

			farm_settings::use_q = farm->add_checkbox( "carry.azir.farm.q", "Use Q", false );

			farm_settings::max_soliders_in_place = farm->add_slider( "carry.azir.farm.w.max", "Max soliders in place", 1, 1, 5 );
			farm_settings::use_w_min_minions = farm->add_slider( "carry.azir.farm.w.min", "W min minions", 3, 1, 6 );

			farm_settings::use_spells_when_mana = farm->add_slider( "carry.azir.farm.mana", "Use spells when mana > %", 40, 1, 100 );
		};

		auto flee = main_tab->add_tab( "carry.azir.flee", "Flee Settings" );
		{
			flee_settings::use_q = flee->add_checkbox( "carry.azir.flee.q", "Use Q", true );
			flee_settings::use_w = flee->add_checkbox( "carry.azir.flee.w", "Use W", true );
			flee_settings::use_e = flee->add_checkbox( "carry.azir.flee.e", "Use E", true );

			flee_settings::extra_delay = main_tab->add_slider( "carry.azir.flee.delay", "Extra delay", 0, 0, 100 );
		};

		auto draw = main_tab->add_tab( "carry.azir.draw", "Draw Settings" );
		{
			draw_settings::draw_range_q = draw->add_checkbox( "carry.azir.draw.q", "Draw Q range", true );
			draw_settings::draw_range_w = draw->add_checkbox( "carry.azir.draw.w", "Draw W range", true );
			draw_settings::draw_range_e = draw->add_checkbox( "carry.azir.draw.e", "Draw E range", true );
			draw_settings::draw_range_r = draw->add_checkbox( "carry.azir.draw.r", "Draw R range", true );
		}

		event_handler<events::on_update>::add_callback( on_update );
		event_handler<events::on_draw>::add_callback( on_draw );
		event_handler<events::on_new_path>::add_callback( on_new_path );
		//event_handler<events::on_process_spell_cast>::add_callback(on_spell);
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
		event_handler<events::on_new_path>::remove_handler( on_new_path );
		//event_handler<events::on_process_spell_cast>::remove_handler(on_spell);

	}

};