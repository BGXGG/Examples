#include "orianna.h"
#include "../plugin_sdk/plugin_sdk.hpp"

namespace orianna
{
#define Q_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 62, 129, 237 ))
#define W_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 227, 203, 20 ))
#define E_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 235, 12, 223 ))
#define R_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 224, 77, 13 ))

#define ORI_Q_RANGE 825.f
#define ORI_W_RANGE 225.f
#define ORI_E_RANGE 1100.f
#define ORI_R_RANGE 415.f

	script_spell* q = nullptr;
	script_spell* w = nullptr;
	script_spell* e = nullptr;
	script_spell* r = nullptr;

	TreeTab* main_tab = nullptr;

	namespace kill_steal
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;
		TreeEntry* use_e = nullptr;
	}

	namespace draw_settings
	{
		TreeEntry* draw_range_q = nullptr;
		TreeEntry* draw_range_w = nullptr;
		TreeEntry* draw_range_e = nullptr;
		TreeEntry* draw_range_r = nullptr;

		TreeEntry* draw_ball_glow = nullptr;
		TreeEntry* draw_ball_position = nullptr;
	}

	namespace hitchance_settings
	{
		TreeEntry* q_hitchance = nullptr;
	}

	namespace farm_settings
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;

		TreeEntry* q_min_minions = nullptr;
		TreeEntry* w_min_minions = nullptr;

		TreeEntry* q_min_minions_jungle = nullptr;
		TreeEntry* w_min_minions_jungle = nullptr;

		TreeEntry* use_spells_mana = nullptr;
	}

	namespace combo_settings
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;
		TreeEntry* use_e = nullptr;
		TreeEntry* use_r = nullptr;
		TreeEntry* use_r_killable = nullptr;

		TreeEntry* min_r_targets = nullptr;

		TreeEntry* teamfightmode_r = nullptr;

		std::map<uint32_t, TreeEntry*> always_use_r;
	}

	namespace misc_settings
	{
		TreeEntry* shield_yourself = nullptr;
		TreeEntry* shield_yourself_only_when_ball_is_on_you = nullptr;
		TreeEntry* shields_allies = nullptr;
		TreeEntry* auto_e_when_ball_behind_movment = nullptr;
		TreeEntry* auto_e_when_ball_missing_enemy = nullptr;
	};

	namespace harras_settings
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;
		TreeEntry* use_e = nullptr;

		TreeEntry* use_spells_when_mana = nullptr;
	}

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

	int32_t always_use_r_on( game_object_script obj )
	{
		auto it = combo_settings::always_use_r.find( obj->get_network_id( ) );

		if ( it != combo_settings::always_use_r.end( ) )
			return it->second->get_int( );

		combo_settings::always_use_r[ obj->get_network_id( ) ] = main_tab->add_combobox( "carry.orianna.combo.always.r." + std::to_string( obj->get_network_id( ) ), obj->get_model( ), { {"Never",nullptr }, {"Always",nullptr} }, 0, false );

		return 0;
	}

	game_object_script ball_object = nullptr;

	void remove_glove_for_ball( )
	{
		if ( !draw_settings::draw_ball_glow->get_bool( ) )
			return;

		if ( ball_object && glow->is_glowed( ball_object ) )
			glow->remove_glow( ball_object );
	}

	void on_create( game_object_script obj )
	{
		if ( !obj->is_ally( ) )
			return;

		if ( !obj->is_ward( ) && obj->get_name( ) == "TheDoomBall" )
		{
			remove_glove_for_ball( );
			ball_object = obj;
			return;
		}

		if ( !obj->is_missile( ) )
			return;

		if ( obj->get_name( ) == "OrianaIzuna" )
		{
			remove_glove_for_ball( );
			ball_object = obj;
			return;
		}

		if ( obj->get_name( ) == "OrianaRedact" )
		{
			remove_glove_for_ball( );
			ball_object = entitylist->get_object( obj->missile_get_target_id( ) );
			return;
		}
	}

	void on_buff_gain( game_object_script sender, buff_instance_script buff )
	{
		if ( !sender->is_me( ) )
			return;

		if ( buff->get_hash_name( ) != buff_hash( "orianaghostself" ) )
			return;

		remove_glove_for_ball( );
		ball_object = myhero;
	}

	vector get_ball_position( bool draw = false )
	{
		if ( !ball_object )
			return myhero->get_position( );

		if ( draw )
			return ball_object->get_position( );

		if ( ball_object->is_missile( ) )
		{
			if ( ball_object->get_distance( ball_object->missile_get_end_position( ) ) > 100.f )
				return vector( 0, 0, 0 );
		}

		return ball_object->is_missile( ) ? ball_object->missile_get_end_position( ) : ball_object->get_position( );
	}

	void on_delete( game_object_script obj )
	{
		if ( obj == ball_object )
		{
			remove_glove_for_ball( );
			ball_object = myhero;
		}
	}

	void on_draw( )
	{
		if ( combo_settings::teamfightmode_r->get_bool( ) )
		{
			vector screen = myhero->get_position( );
			renderer->world_to_screen( screen, screen );

			if ( screen.is_on_screen( ) )
				draw_manager->add_text_on_screen( { screen.x,screen.y + 20 }, MAKE_COLOR( 255, 0, 0, 255 ), 15, "Teamfight MODE: ON" );
		}

		if ( draw_settings::draw_range_q->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), q->range( ), Q_DRAW_COLOR );

		if ( draw_settings::draw_range_w->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), w->range( ), W_DRAW_COLOR );

		if ( draw_settings::draw_range_e->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), e->range( ), E_DRAW_COLOR );

		if ( draw_settings::draw_range_r->get_bool( ) )
			draw_manager->add_circle( get_ball_position( ), r->range( ), R_DRAW_COLOR );

		if ( draw_settings::draw_ball_position->get_bool( ) )
			draw_manager->add_circle( get_ball_position( true ), 150.f, Q_DRAW_COLOR );

		if ( draw_settings::draw_ball_glow->get_bool( ) && ball_object && ball_object->is_ai_minion( ) && !glow->is_glowed( ball_object ) )
			glow->apply_glow( ball_object, MAKE_COLOR( 255, 0, 0, 255 ) );
	}

	void q_normal_logic( hit_chance hc )
	{
		auto q_target = target_selector->get_target( q->range( ), damage_type::magical );

		if ( q_target )
		{
			q->set_range_check_from( get_ball_position( ) );
			q->cast( q_target, hc );
		}
	}

	void killsteal( )
	{
		auto q_isready = kill_steal::use_q->get_bool( ) ? q->is_ready( ) : false;
		auto w_isready = kill_steal::use_w->get_bool( ) ? w->is_ready( ) : false;
		auto e_isready = kill_steal::use_e->get_bool( ) ? e->is_ready( ) : false;

		std::vector<game_object_script> valid_targets_enemy;

		for ( auto&& it : entitylist->get_enemy_heroes( ) )
		{
			if ( !it->is_valid_target( ) )
				continue;

			valid_targets_enemy.push_back( it );
		}

		if ( q_isready )
		{
			for ( auto&& it : valid_targets_enemy )
			{
				if ( it->get_distance( myhero ) > q->range( ) )
					continue;

				auto total_dmg = q->get_damage( it );
				if ( w_isready )
					total_dmg += w->get_damage( it ) + myhero->get_total_attack_damage( );

				if ( total_dmg >= it->get_health( ) )
				{
					q->set_range_check_from( get_ball_position( ) );
					if ( q->cast( it, get_hitchance_by_config( hitchance_settings::q_hitchance ) ) )
						break;
				}
			}
		}


		if ( w_isready )
		{
			for ( auto&& it : valid_targets_enemy )
			{
				if ( it->get_position( ).distance( get_ball_position( ) ) > ORI_W_RANGE )
					continue;

				auto wdmg = w->get_damage( it ) + myhero->get_total_attack_damage( );

				if ( wdmg < it->get_health( ) )
					continue;

				if ( w->cast( ) )
					break;
			}
		}

		if ( e_isready )
		{
			bool break_loop = false;

			for ( auto&& it : valid_targets_enemy )
			{
				for ( auto&& ally : entitylist->get_ally_heroes( ) )
				{
					if ( it->get_position( )
						.distance( get_ball_position( ), ally->get_position( ), true, true )
						<= std::pow( ( 40 ), 2 ) )
					{
						if ( e->get_damage( it ) >= it->get_health( ) )
						{
							if ( e->cast( ally ) )
							{
								break_loop = true;
								break;
							}
						}
					}
				}

				if ( break_loop )
					break;
			}
		}
	}

	void on_update( )
	{
		if ( myhero->is_dead( ) )
			return;

		if ( ball_object && ball_object->is_missile( ) )
			ball_object->update( );

		killsteal( );

		if ( orbwalker->combo_mode( ) )
		{
			auto r_is_ready = r->is_ready( );

			std::vector<game_object_script> valid_targets_enemy;

			for ( auto&& it : entitylist->get_enemy_heroes( ) )
			{
				if ( !it->is_valid_target( ) )
					continue;

				valid_targets_enemy.push_back( it );
			}

			std::vector<vector> enemeyPositions;
			if ( r_is_ready )
			{
				for ( auto&& it : valid_targets_enemy )
				{
					auto input = prediction_input( );
					input.unit = it;
					input.delay = 0.5;
					input._from = get_ball_position( );
					input._range_check_from = get_ball_position( );
					input.type = skillshot_type::skillshot_circle;
					input.speed = FLT_MAX;

					enemeyPositions.push_back( prediction->get_prediction( &input ).get_unit_position( ) );
				}
			}

			if ( misc_settings::auto_e_when_ball_missing_enemy->get_bool( ) && combo_settings::use_e->get_bool( ) && e->is_ready( ) )
			{
				if ( ball_object && ball_object != myhero )
				{
					auto best_distance = FLT_MAX;
					for ( auto& enemy : valid_targets_enemy )
					{
						auto distance = enemy->get_distance( ball_object );
						if ( distance < best_distance )
							best_distance = distance;
					}

					if ( best_distance > q->range( ) && ball_object->get_distance( myhero ) > 200.f )
					{
						e->cast( myhero );
					}
				}
			}

			if ( r_is_ready && combo_settings::use_e->get_bool( ) && e->is_ready( ) )
			{
				for ( auto& ally : entitylist->get_ally_heroes( ) )
				{
					auto DistToAlly = ally->get_distance( myhero );
					if ( ally->is_dead( ) || DistToAlly - ally->get_bounding_radius( ) > e->range( ) )
						continue;

					if ( ally->get_path_controller( )->is_dashing( ) )
					{
						auto DashInfo = ally->get_path_controller( );
						auto startPos = DashInfo->get_end_vec( );
						auto count = 0;
						auto width = ORI_R_RANGE;

						for ( auto&& pos : enemeyPositions )
							if ( startPos.distance( pos ) <= width )
								count++;

						if ( count >= combo_settings::min_r_targets->get_int( ) &&
							( DashInfo->get_end_vec( ) - myhero->get_position( ) ).length( ) <= 1100.f )
						{
							e->cast( ally );
						}
					} else
					{
						auto startPos = ally->get_position( );
						auto count = 0;
						auto width = ORI_R_RANGE;

						for ( auto&& pos : enemeyPositions )
							if ( startPos.distance( pos ) <= width )
								count++;

						if ( count >= combo_settings::min_r_targets->get_int( ) )
						{
							e->cast( ally );
						}
					}
				}
			}

			if ( combo_settings::use_q->get_bool( ) && q->is_ready( ) )
			{
				if ( !r_is_ready )
				{
					auto hitchance = get_hitchance_by_config( hitchance_settings::q_hitchance );
					auto low_hit = ( hit_chance ) ( ( int ) hitchance - 1 );
					if ( low_hit < hit_chance::low )
						low_hit = hit_chance::low;

					q_normal_logic( myhero->get_mana_percent( ) > 70.f ? low_hit : hitchance );
				} else
				{
					auto bestPos = vector( );
					auto count = 0;
					float best_distance = 0.f;
					auto startPos = myhero->get_position( );
					auto width = ORI_R_RANGE;
					auto range = ORI_Q_RANGE;

					if ( enemeyPositions.size( ) > 0 )
					{
						if ( enemeyPositions.size( ) > 1 )
						{
							for ( auto&& pos : enemeyPositions )
							{
								auto points = geometry::geometry::circle_points( pos, width / 2.f, 6 );

								for ( auto&& point : points )
								{
									if ( point.distance( startPos ) <= range )
									{
										auto countPosition = 0;
										std::vector<float> distances_calc;
										for ( auto&& pos2 : enemeyPositions )
										{
											if ( point.distance( pos2 ) <= width )
											{
												distances_calc.push_back( point.distance( pos2 ) );
												countPosition++;
											}
										}

										if ( countPosition > 0 )
										{
											float mid_distance = 0.f;
											for ( auto&& it : distances_calc )
												mid_distance += it;
											mid_distance /= distances_calc.size( );

											if ( countPosition > count || ( countPosition == count && mid_distance < best_distance ) )
											{
												best_distance = mid_distance;
												bestPos = point;
												count = countPosition;
											}
										}
									}
								}
							}
						} else
						{
							bestPos = enemeyPositions.front( );
							count = 1;
						}
					}

					if ( count >= combo_settings::min_r_targets->get_int( ) )
						q->cast( bestPos );
					else if ( !combo_settings::teamfightmode_r->get_bool( ) )
						q_normal_logic( get_hitchance_by_config( hitchance_settings::q_hitchance ) );
				}
			}

			if ( combo_settings::use_w->get_bool( ) && w->is_ready( ) && ( !ball_object || !ball_object->is_missile( ) ) )
			{
				for ( auto&& it : valid_targets_enemy )
				{
					if ( it->get_position( ).distance( get_ball_position( ) ) > ORI_W_RANGE )
						continue;

					if ( w->cast( ) )
						break;
				}
			}

			if ( combo_settings::use_e->get_bool( ) && !r_is_ready && e->is_ready( ) )
			{
				bool break_loop = false;

				for ( auto&& it : valid_targets_enemy )
				{
					for ( auto&& ally : entitylist->get_ally_heroes( ) )
					{
						if ( it->get_position( )
							.distance( get_ball_position( ), ally->get_position( ), true, true )
							<= std::pow( ( 40 ), 2 ) )
						{
							if ( e->get_damage( it ) >= it->get_health( ) )
							{
								if ( e->cast( ally ) )
								{
									break_loop = true;
									break;
								}
							}
						}
					}

					if ( break_loop )
						break;
				}
			}

			if ( r_is_ready && combo_settings::use_r->get_bool( ) )
			{
				std::vector<std::pair<vector, game_object_script>> enemeyPositions;
				for ( auto&& it : valid_targets_enemy )
				{
					auto input = prediction_input( );
					input.unit = it;
					input.delay = 0.5;
					input._from = get_ball_position( );
					input._range_check_from = get_ball_position( );
					input.type = skillshot_type::skillshot_circle;
					input.speed = FLT_MAX;

					enemeyPositions.push_back( { prediction->get_prediction( &input ).get_unit_position( ),it } );
				}

				if ( enemeyPositions.size( ) > 0 )
				{
					auto killable = 0;
					auto count = 0;
					auto startPos = get_ball_position( );
					auto width = ORI_R_RANGE;

					auto FORCE_USE = false;

					for ( auto&& pos : enemeyPositions )
					{
						if ( startPos.distance( pos.first ) <= width )
						{
							if ( always_use_r_on( pos.second ) == 1 )
								FORCE_USE = true;

							count++;
							auto dmg = r->get_damage( pos.second ) + myhero->get_total_attack_damage( ) + q->get_damage( pos.second ) + w->get_damage( pos.second );
							if ( dmg >= pos.second->get_health( ) )
								killable++;
						}
					}

					if ( killable > 0 && combo_settings::use_r_killable->get_bool( ) )
					{
						if ( startPos.count_allys_in_range( 650.f ) <= 1 && ( myhero->get_distance( startPos ) > 350 || myhero->get_health_percent( ) < 40.f ) )
						{
							FORCE_USE = true;
						}
					}

					if ( FORCE_USE || count >= combo_settings::min_r_targets->get_int( ) )
						r->cast( );
				}
			}
		} else if ( orbwalker->mixed_mode( ) )
		{
			if ( myhero->get_mana_percent( ) > harras_settings::use_spells_when_mana->get_int( ) )
			{
				std::vector<game_object_script> valid_targets_enemy;

				for ( auto&& it : entitylist->get_enemy_heroes( ) )
				{
					if ( !it->is_valid_target( ) )
						continue;

					valid_targets_enemy.push_back( it );
				}

				if ( harras_settings::use_q->get_bool( ) && q->is_ready( ) )
					q_normal_logic( get_hitchance_by_config( hitchance_settings::q_hitchance ) );

				if ( harras_settings::use_w->get_bool( ) && w->is_ready( ) && ( !ball_object || !ball_object->is_missile( ) ) )
				{
					for ( auto&& it : valid_targets_enemy )
					{
						if ( it->get_position( ).distance( get_ball_position( ) ) > ORI_W_RANGE )
							continue;

						if ( w->cast( ) )
							break;
					}
				}

				if ( harras_settings::use_e->get_bool( ) && e->is_ready( ) )
				{
					bool break_loop = false;

					for ( auto&& it : valid_targets_enemy )
					{
						for ( auto&& ally : entitylist->get_ally_heroes( ) )
						{
							if ( it->get_position( )
								.distance( get_ball_position( ), ally->get_position( ), true, true )
								<= std::pow( ( 40 ), 2 ) )
							{
								if ( e->cast( ally ) )
								{
									break_loop = true;
									break;
								}
							}
						}

						if ( break_loop )
							break;
					}
				}
			}
		} else if ( orbwalker->lane_clear_mode( ) )
		{
			if ( myhero->get_mana_percent( ) > farm_settings::use_spells_mana->get_int( ) )
			{
				if ( farm_settings::use_q->get_bool( ) && q->is_ready( ) )
				{
					q->set_range_check_from( get_ball_position( ) );
					if ( !q->cast_on_best_farm_position( farm_settings::q_min_minions->get_int( ) ) )
					{
						q->cast_on_best_farm_position( farm_settings::q_min_minions_jungle->get_int( ), true );
					}
				}

				if ( farm_settings::use_w->get_bool( ) && w->is_ready( ) )
				{
					auto minions_w_count = 0;
					for ( auto&& it : entitylist->get_enemy_minions( ) )
					{
						if ( it->get_position( ).distance( get_ball_position( ) ) > ORI_W_RANGE )
							continue;

						if ( !it->is_valid_target( ) )
							continue;

						minions_w_count++;
					}

					if ( minions_w_count >= farm_settings::w_min_minions->get_int( ) )
						w->cast( );
					else
					{
						minions_w_count = 0;
						for ( auto&& it : entitylist->get_jugnle_mobs_minions( ) )
						{
							if ( it->get_position( ).distance( get_ball_position( ) ) > ORI_W_RANGE )
								continue;

							if ( !it->is_valid_target( ) )
								continue;

							minions_w_count++;
						}

						if ( minions_w_count >= farm_settings::w_min_minions_jungle->get_int( ) )
							w->cast( );
					}
				}
			}
		}

		if ( orbwalker->none_mode( ) && e->is_ready( ) )
		{
			if ( misc_settings::shield_yourself->get_bool( ) )
			{
				bool ball_check = true;
				if ( misc_settings::shield_yourself_only_when_ball_is_on_you->get_bool( ) )
					ball_check = ball_object == myhero;

				if ( ball_check )
				{
					if ( myhero->get_buff_by_type( { buff_type::Poison,buff_type::Damage } ) )
					{
						e->cast( myhero );
						return;
					}

					float min_dmg_for_shd = 120;
					min_dmg_for_shd = fminf( min_dmg_for_shd, ( ( ( float ) 25 / 100.f ) * myhero->get_max_health( ) ) );

					auto dmg_pred = health_prediction->get_incoming_damage( myhero, 1.f, true );
					if ( dmg_pred >= min_dmg_for_shd )
					{
						e->cast( myhero );
						return;
					}
				}
			}

			if ( misc_settings::shields_allies->get_bool( ) )
			{
				for ( auto& Ally : entitylist->get_ally_heroes( ) )
				{
					if ( Ally->get_distance( myhero ) > e->range( ) )
						continue;

					if ( !Ally->is_targetable( ) )
						continue;

					bool ball_check = true;
					if ( misc_settings::shield_yourself_only_when_ball_is_on_you->get_bool( ) )
						ball_check = ball_object == Ally;

					if ( ball_check )
					{
						if ( Ally->get_buff_by_type( { buff_type::Poison,buff_type::Damage } ) )
						{
							e->cast( Ally );
							return;
						}

						float min_dmg_for_shd = 120;
						min_dmg_for_shd = fminf( min_dmg_for_shd, ( ( ( float ) 25 / 100.f ) * Ally->get_max_health( ) ) );

						auto dmg_pred = health_prediction->get_incoming_damage( Ally, 1.f, true );
						if ( dmg_pred >= min_dmg_for_shd )
						{
							e->cast( Ally );
							return;
						}
					}
				}
			}

			if ( misc_settings::auto_e_when_ball_behind_movment->get_bool( ) && ball_object && ball_object != myhero && ball_object->get_distance( myhero ) >= 450.f && !myhero->is_facing( ball_object ) )
			{
				auto Path = myhero->get_real_path( );
				float PathLen = 0.f;
				for ( std::size_t i = 1; i < Path.size( ); i++ )
					PathLen += ( Path[ i ] - Path[ i - 1 ] ).length( );

				if ( PathLen >= 650.f )
				{
					e->cast( myhero );
					return;
				}
			}
		}
	}

	void load( )
	{
		q = plugin_sdk->register_spell( spellslot::q, ORI_Q_RANGE );
		w = plugin_sdk->register_spell( spellslot::w, ORI_W_RANGE );
		e = plugin_sdk->register_spell( spellslot::e, ORI_E_RANGE );
		r = plugin_sdk->register_spell( spellslot::r, ORI_R_RANGE );

		q->set_skillshot( 0.f, 175.f, 1400, { collisionable_objects::yasuo_wall }, skillshot_type::skillshot_circle );

		ball_object = myhero;

		main_tab = menu->create_tab( "carry.orianna", "Orianna" );

		auto combo = main_tab->add_tab( "carry.orianna.combo", "Combo settings" );
		{
			combo_settings::use_q = combo->add_checkbox( "carry.orianna.combo.q", "Use Q", true );
			combo_settings::use_w = combo->add_checkbox( "carry.orianna.combo.w", "Use W", true );
			combo_settings::use_e = combo->add_checkbox( "carry.orianna.combo.e", "Use E", true );
			combo_settings::use_r = combo->add_checkbox( "carry.orianna.combo.r", "Use R", true );
			combo_settings::use_r_killable = combo->add_checkbox( "carry.orianna.combo.r.killable", "Use R when killable", true );

			combo_settings::teamfightmode_r = combo->add_hotkey( "carry.orianna.combo.teamfight", "Teamfight mode", TreeHotkeyMode::Toggle, 0x54, false, false );

			combo_settings::min_r_targets = combo->add_slider( "carry.orianna.combo.r.targets", "Use R min targets", 2, 1, 5 );

			auto use_r_on = combo->add_tab( "carry.orianna.combo.user", "Use R on" );

			for ( auto&& it : entitylist->get_enemy_heroes( ) )
				combo_settings::always_use_r[ it->get_network_id( ) ] = use_r_on->add_combobox( "carry.orianna.combo.always.r." + std::to_string( it->get_network_id( ) ), it->get_model( ), { {"Never",nullptr }, {"Always",nullptr} }, 0, false );
		};

		auto killsteal = main_tab->add_tab( "carry.orianna.killsteal", "Killsteal settings" );
		{
			kill_steal::use_q = killsteal->add_checkbox( "carry.orianna.kill.q", "Use Q", true );
			kill_steal::use_w = killsteal->add_checkbox( "carry.orianna.kill.w", "Use W", true );
			kill_steal::use_e = killsteal->add_checkbox( "carry.orianna.kill.e", "Use E", false );
		};

		auto harras = main_tab->add_tab( "carry.orianna.harras", "Harras settings" );
		{
			harras_settings::use_q = harras->add_checkbox( "carry.orianna.harras.q", "Use Q", true );
			harras_settings::use_w = harras->add_checkbox( "carry.orianna.harras.w", "Use W", true );
			harras_settings::use_e = harras->add_checkbox( "carry.orianna.harras.e", "Use E", false );

			harras_settings::use_spells_when_mana = harras->add_slider( "carry.orianna.harras.mana", "Use spells when mana > %", 40, 1, 100 );
		};

		auto farm = main_tab->add_tab( "carry.orianna.farm", "Farm settings" );
		{
			farm_settings::use_q = farm->add_checkbox( "carry.orianna.farm.q", "Use Q", true );
			farm_settings::use_w = farm->add_checkbox( "carry.orianna.farm.w", "Use W", true );

			farm_settings::q_min_minions = farm->add_slider( "carry.orianna.farm.q.m", "Min Q minions", 3, 1, 6 );
			farm_settings::w_min_minions = farm->add_slider( "carry.orianna.farm.w.m", "Min W minions", 3, 1, 6 );

			farm_settings::q_min_minions_jungle = farm->add_slider( "carry.orianna.farm.q.m.j", "Min Q minions jungle", 1, 1, 6 );
			farm_settings::w_min_minions_jungle = farm->add_slider( "carry.orianna.farm.w.m.j", "Min W minions jungle", 3, 1, 6 );

			farm_settings::use_spells_mana = farm->add_slider( "carry.orianna.farm.mana", "Use spells when mana > %", 60, 1, 100 );
		};

		auto misc = main_tab->add_tab( "carry.orianna.misc", "Misc settings" );
		{
			misc_settings::shield_yourself = misc->add_checkbox( "carry.orianna.misc.e", "Shield yourself", true );
			misc_settings::shield_yourself_only_when_ball_is_on_you = misc->add_checkbox( "carry.orianna.misc.e.onyou", "Only when ball is on you", true );
			misc_settings::shields_allies = misc->add_checkbox( "carry.orianna.misc.e.allies", "Shield allies", false );
			misc_settings::auto_e_when_ball_behind_movment = misc->add_checkbox( "carry.orianna.misc.e.behind", "E when ball was left behind", false );
			misc_settings::auto_e_when_ball_missing_enemy = misc->add_checkbox( "carry.orianna.misc.e.miss.enemy", "E when ball is not near enemy", true );
		};

		auto hitchance = main_tab->add_tab( "carry.orianna.hitchance", "Hitchance settings" );
		{
			hitchance_settings::q_hitchance = hitchance->add_combobox( "carry.orianna.hitchance.q", "Hitchance Q", { {"Low",nullptr},{"Medium",nullptr },{"High", nullptr},{"Very High",nullptr} }, 2 );
		};

		auto draw = main_tab->add_tab( "carry.orianna.draw", "Draw Settings" );
		{
			draw_settings::draw_range_q = draw->add_checkbox( "carry.orianna.draw.q", "Draw Q range", true );
			draw_settings::draw_range_w = draw->add_checkbox( "carry.orianna.draw.w", "Draw W range", true );
			draw_settings::draw_range_e = draw->add_checkbox( "carry.orianna.draw.e", "Draw E range", true );
			draw_settings::draw_range_r = draw->add_checkbox( "carry.orianna.draw.r", "Draw R range", true );

			draw_settings::draw_ball_glow = draw->add_checkbox( "carry.orianna.draw.ball.glow", "Draw Ball glow", true );
			draw_settings::draw_ball_position = draw->add_checkbox( "carry.orianna.draw.ball", "Draw Ball position", true );
		}

		event_handler<events::on_update>::add_callback( on_update );
		event_handler<events::on_draw>::add_callback( on_draw );
		event_handler<events::on_buff_gain>::add_callback( on_buff_gain );
		event_handler<events::on_create_object>::add_callback( on_create );
		event_handler<events::on_delete_object>::add_callback( on_delete );
	}

	void unload( )
	{
		menu->delete_tab( main_tab );

		plugin_sdk->remove_spell( q );
		plugin_sdk->remove_spell( w );
		plugin_sdk->remove_spell( e );
		plugin_sdk->remove_spell( r );

		event_handler<events::on_update>::remove_handler( on_update );
		event_handler<events::on_buff_gain>::remove_handler( on_buff_gain );
		event_handler<events::on_draw>::remove_handler( on_draw );
		event_handler<events::on_delete_object>::remove_handler( on_delete );
		event_handler<events::on_create_object>::remove_handler( on_create );
	}
};