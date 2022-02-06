#include "ryze.h"
#include "../plugin_sdk/plugin_sdk.hpp"

#define RYZE_Q_RANGE 1000
#define RYZE_W_RANGE 550
#define RYZE_E_RANGE 550
#define RYZE_R_MIN_RANGE 1000
#define RYZE_R_RANGE 3000

#define RYZE_Q_DELAY 0.25f
#define RYZE_Q_SPEED 1700.f
#define RYZE_Q_RADIUS 55.f

#define RYZE_E_DELAY 0.25f
#define RYZE_E_SPEED 2500.f

namespace ryze
{
	script_spell* q = nullptr;
	script_spell* w = nullptr;
	script_spell* e = nullptr;
	script_spell* r = nullptr;

	TreeTab* main_tab = nullptr;

	namespace combo_settings
	{
		TreeEntry* aa_block = nullptr;
		TreeEntry* min_aa_block = nullptr;
		TreeEntry* aa_block_after_6 = nullptr;
		TreeEntry* root_combo = nullptr;
	}

	namespace mixed_settings
	{
		TreeEntry* min_mana = nullptr;
		TreeEntry* use_q = nullptr;
		TreeEntry* use_q_lh = nullptr;
		TreeEntry* use_e = nullptr;
		TreeEntry* use_w = nullptr;
		TreeEntry* use_auto_q = nullptr;
	}

	namespace lane_clear_settings
	{
		TreeEntry* disable_lane = nullptr;
		TreeEntry* min_mana = nullptr;
		TreeEntry* use_q = nullptr;
		TreeEntry* use_e = nullptr;
	}

	namespace jungle_settings
	{
		TreeEntry* min_mana = nullptr;
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;
		TreeEntry* use_e = nullptr;
	}

	namespace lasthit_settings
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;
		TreeEntry* use_e = nullptr;
	}

	namespace event_settings
	{
		TreeEntry* antidash = nullptr;
		TreeEntry* auto_w = nullptr;
	}

	namespace ks_settings
	{
		TreeEntry* ks = nullptr;
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;
		TreeEntry* use_e = nullptr;
	}

	namespace draw_settings
	{
		TreeEntry* draw_q = nullptr;
		TreeEntry* draw_w = nullptr;
		TreeEntry* draw_e = nullptr;
		TreeEntry* draw_r = nullptr;
	}

	std::uint32_t forced_w_target_nid = 0;
	std::uint32_t last_e_target_nid = 0;
	std::uint32_t q_missile_target_nid = 0;

	bool has_e( game_object_script target )
	{
		if ( target == nullptr || !target->is_valid_target( ) )
			return false;

		if ( gametime->get_time( ) - e->get_last_cast_spell_time( ) < 0.5f )
		{
			if ( last_e_target_nid == target->get_network_id( ) )
			{
				return true;
			}

			if ( q_missile_target_nid == target->get_network_id( ) )
			{
				return true;
			}
		}

		return target->has_buff( buff_hash( "RyzeE" ) );
	}

	bool should_root( game_object_script unit )
	{
		if ( has_e( unit ) )
		{
			auto distance = unit->get_distance( myhero );

			if ( distance < 300 && unit->is_melee( ) )
			{
				return true;
			}

			if ( distance > 500 )
			{
				auto q_dmg = q->get_damage( unit );
				auto dmg = e->get_damage( unit ) + w->get_damage( unit );

				if ( dmg + q_dmg * 2 < unit->get_health( ) + unit->get_magical_shield( ) &&
					dmg + q_dmg * 3 > unit->get_health( ) + unit->get_magical_shield( ) )
				{
					return false;
				}

				return true;
			}
		}

		return false;
	}

	bool get_qe_target( game_object_script target, game_object_script& flux_target, prediction_output& prediction_out )
	{
		if ( target == nullptr || !has_e( target ) )
		{
			flux_target = nullptr;
			return false;
		}

		const int QDamageRange = 500;

		auto pred = q->get_prediction( target );
		game_object_script bestTarget = nullptr;
		float delta = 0;
		prediction_output predB;

		auto minions = entitylist->get_enemy_minions( );
		auto heroes = entitylist->get_enemy_heroes( );
		auto monsters = entitylist->get_jugnle_mobs_minions( );

		std::vector < game_object_script > possible_targets;
		{
			possible_targets.reserve( minions.size( ) + heroes.size( ) + monsters.size( ) );
			possible_targets.insert( possible_targets.end( ), heroes.begin( ), heroes.end( ) );
			possible_targets.insert( possible_targets.end( ), minions.begin( ), minions.end( ) );
			possible_targets.insert( possible_targets.end( ), monsters.begin( ), monsters.end( ) );
		}

		for ( auto&& possible_target : possible_targets )
		{
			if ( !possible_target->is_valid_target( q->range( ) ) || !has_e( possible_target ) )
				continue;

			float d = possible_target->get_distance( pred.get_unit_position( ) );

			if ( d > QDamageRange )
				continue;

			auto p = q->get_prediction( possible_target );

			if ( p.hitchance < hit_chance::medium )
				continue;

			if ( bestTarget == nullptr || delta > d )
			{
				bestTarget = possible_target;
				delta = d;
				predB = p;
			}
		}

		if ( bestTarget != nullptr )
		{
			flux_target = bestTarget;
			prediction_out = predB;
			return true;
		}

		flux_target = nullptr;

		return false;
	}

	void killsteal( )
	{
		if ( !ks_settings::ks->get_bool( ) )
			return;

		auto target = target_selector->get_target( q->range( ), damage_type::magical );

		if ( target == nullptr || target->is_invulnerable( ) )
			return;

		auto qSpell = ks_settings::use_q->get_bool( );
		auto wSpell = ks_settings::use_w->get_bool( );
		auto eSpell = ks_settings::use_e->get_bool( );
		auto health = ( target->get_health( ) + target->get_all_shield( ) + target->get_magical_shield( ) );

		if ( qSpell
			&& q->is_ready( )
			&& q->get_damage( target ) > health )
			q->cast( target, hit_chance::medium );

		if ( wSpell
			&& w->is_ready( )
			&& w->get_damage( target ) > health
			&& target->get_distance( myhero ) < ( w->range( ) + myhero->get_bounding_radius( ) ) )
			w->cast( target );

		if ( eSpell
			&& e->is_ready( )
			&& e->get_damage( target ) > health
			&& target->get_distance( myhero ) < ( e->range( ) + myhero->get_bounding_radius( ) ) )
			e->cast( target );
	}

	void last_hit( )
	{
		auto qlchSpell = lasthit_settings::use_q->get_bool( );
		auto elchSpell = lasthit_settings::use_e->get_bool( );
		auto wlchSpell = lasthit_settings::use_w->get_bool( );

		auto minions = entitylist->get_enemy_minions( );

		for ( auto&& minion : minions )
		{
			if ( minion->is_valid_target( q->range( ) - 50 ) )
			{
				if ( qlchSpell
					&& q->is_ready( )
					&& minion->get_health( ) < q->get_damage( minion ) )
					if ( q->cast( minion, hit_chance::medium ) )
						return;

				if ( wlchSpell
					&& w->is_ready( )
					&& minion->get_health( ) < w->get_damage( minion ) )
					if ( w->cast( minion ) )
						return;

				if ( elchSpell
					&& e->is_ready( )
					&& minion->get_health( ) < e->get_damage( minion ) )
					if ( e->cast( minion ) )
						return;
			}
		}
	}

	void lane_clear( )
	{
		if ( !lane_clear_settings::disable_lane->get_bool( ) || !orbwalker->can_move( 0.04f ) )
			return;

		if ( myhero->get_mana_percent( ) < lane_clear_settings::min_mana->get_int( ) )
			return;

		::vector my_position = myhero->get_position( );
		std::vector < game_object_script > q_minions, e_minions, flux_minions;

		for ( auto&& minion : entitylist->get_enemy_minions( ) )
		{
			auto pos = minion->get_position( );

			if ( my_position.distance_squared( pos ) < q->range( ) * q->range( ) )
			{
				q_minions.push_back( minion );

				if ( has_e( minion ) )
					flux_minions.push_back( minion );
			}

			if ( my_position.distance_squared( pos ) < e->range( ) * e->range( ) )
				e_minions.push_back( minion );
		}

		game_object_script bestEMinion = nullptr;
		int bestMinionAround = 0;

		for ( auto minion : e_minions )
		{
			auto poss = std::count_if( q_minions.begin( ), q_minions.end( ), [ &minion ]( game_object_script x )
 {
	 return x->get_network_id( ) != minion->get_network_id( ) && x->is_valid_target( 500, minion->get_position( ) );
			} );

			if ( bestEMinion == nullptr || poss > bestMinionAround )
			{
				bestEMinion = minion;
				bestMinionAround = poss;
			}
		}

		if ( e->is_ready( ) && lane_clear_settings::use_e->get_bool( ) )
		{
			if ( bestEMinion != nullptr )
			{
				bool canCast = true;

				for ( auto&& minion : q_minions )
				{
					if ( minion->get_distance( bestEMinion ) > 500 )
						continue;

					auto d = fmaxf( 0, minion->get_distance( myhero ) - myhero->get_bounding_radius( ) );
					auto delay = RYZE_E_DELAY + d / RYZE_E_SPEED + myhero->get_attack_cast_delay( ) + d / orbwalker->get_my_projectile_speed( ) + ping->get_ping( ) / 2000.f;
					auto delay2 = RYZE_E_DELAY + d / RYZE_E_SPEED + ping->get_ping( ) / 2000.f - 0.1f;

					if ( health_prediction->get_health_prediction( minion, delay2 ) < e->get_damage( minion ) )
						continue;

					if ( health_prediction->get_lane_clear_health_prediction( minion, delay ) - e->get_damage( minion ) < 0 )
					{
						if ( minion->get_network_id( ) == bestEMinion->get_network_id( )
							&& bestEMinion->get_health( ) < e->get_damage( minion ) )
							continue;

						canCast = false;
						break;
					}
				}

				auto dmg = e->get_damage( bestEMinion );

				if ( canCast )
				{
					if ( bestMinionAround != 0 ||
						( bestEMinion->get_health( ) > myhero->get_auto_attack_damage( bestEMinion, true ) &&
							bestEMinion->get_health( ) < dmg + ( q->get_prediction( bestEMinion ).hitchance >= hit_chance::medium ? q->get_damage( bestEMinion ) : 0 ) ) )
					{
						e->cast( bestEMinion );
						return;
					}
				}
			}
		}

		if ( q->is_ready( ) && lane_clear_settings::use_q->get_bool( ) )
		{
			if ( flux_minions.size( ) != 0 )
			{
				game_object_script bestMinion = nullptr;
				std::vector < game_object_script > bestMinions;

				for ( auto&& minion : flux_minions )
				{
					if ( q->get_prediction( minion ).hitchance >= hit_chance::medium )
					{
						bestMinions.push_back( minion );
					}
				}

				std::sort( bestMinions.begin( ), bestMinions.end( ), [ &flux_minions ]( game_object_script a, game_object_script b )
					{
						auto count_a = std::count_if( flux_minions.begin( ), flux_minions.end( ), [ &a ]( game_object_script x )
 {
	 return x->get_network_id( ) != a->get_network_id( ) && a->get_distance( x ) < 500;
						} );

						auto count_b = std::count_if( flux_minions.begin( ), flux_minions.end( ), [ &b ]( game_object_script x )
 {
	 return x->get_network_id( ) != b->get_network_id( ) && b->get_distance( x ) < 500;
						} );

						auto is_killable_a = a->get_health( ) <= q->get_damage( a );
						auto is_killable_b = b->get_health( ) <= q->get_damage( b );

						return std::tie( count_a, is_killable_b ) > std::tie( count_b, is_killable_a );
					} );

				if ( !bestMinions.empty( ) )
					bestMinion = bestMinions.front( );

				if ( bestMinion != nullptr )
				{
					bool canCast = true;

					for ( auto&& minion : q_minions )
					{
						if ( minion->get_distance( bestMinion ) > 500 )
							continue;

						auto d = fmaxf( 0, minion->get_distance( myhero ) - myhero->get_bounding_radius( ) );
						auto delay = RYZE_Q_DELAY + d / RYZE_Q_SPEED + myhero->get_attack_cast_delay( ) + d / orbwalker->get_my_projectile_speed( ) + ping->get_ping( ) / 2000.f;
						auto delay2 = RYZE_Q_DELAY + d / RYZE_Q_SPEED + ping->get_ping( ) / 2000.f - 0.1f;

						if ( health_prediction->get_health_prediction( minion, delay2 ) < q->get_damage( minion ) )
							continue;

						if ( health_prediction->get_lane_clear_health_prediction( minion, delay ) - q->get_damage( minion ) < 0.f )
						{
							if ( minion->get_network_id( ) == bestMinion->get_network_id( )
								&& bestMinion->get_health( ) < q->get_damage( bestMinion ) )
								continue;

							canCast = false;
							break;
						}
					}

					if ( canCast )
						q->cast( bestMinion, hit_chance::medium );
				} else
				{
					for ( auto minion : q_minions )
					{
						if ( minion->get_health( ) > ( q->get_damage( minion ) * 0.9f ) )
							continue;

						q->cast( minion, hit_chance::medium );
					}
				}
			} else
			{
				for ( auto minion : q_minions )
				{
					if ( minion->get_health( ) > q->get_damage( minion ) * 0.9 )
						continue;

					q->cast( minion, hit_chance::medium );
				}
			}
		}
	}

	void jungle_clear( )
	{
		auto qlchSpell = jungle_settings::use_q->get_bool( );
		auto elchSpell = jungle_settings::use_e->get_bool( );
		auto wlchSpell = jungle_settings::use_w->get_bool( );

		if ( myhero->get_mana_percent( ) < jungle_settings::min_mana->get_int( ) )
			return;

		auto monsters = entitylist->get_jugnle_mobs_minions( );

		monsters.erase( std::remove_if( monsters.begin( ), monsters.end( ), [ ]( game_object_script x )
			{
				return !x->is_valid_target( q->range( ) ) || x->get_max_health( ) < 20;
			} ), monsters.end( ) );

		std::sort( monsters.begin( ), monsters.end( ), [ ]( game_object_script a, game_object_script b )
		{
			return a->get_max_health( ) > b->get_max_health( );
		} );

		for ( auto&& minion : monsters )
		{
			if ( qlchSpell && q->is_ready( ) )
			{
				if ( q->cast( minion, hit_chance::medium ) )
				{
					return;
				}
			}

			if ( elchSpell && e->is_ready( ) && minion->is_valid_target( e->range( ) + myhero->get_bounding_radius( ) ) )
			{
				if ( e->cast( minion ) )
				{
					return;
				}
			}

			if ( wlchSpell && w->is_ready( ) && minion->is_valid_target( e->range( ) + myhero->get_bounding_radius( ) ) )
			{
				if ( w->cast( minion ) )
				{
					return;
				}
			}
		}
	}

	void mixed_mode( )
	{
		auto qSpell = mixed_settings::use_q->get_bool( );
		auto qlSpell = mixed_settings::use_q_lh->get_bool( );
		auto eSpell = mixed_settings::use_e->get_bool( );
		auto wSpell = mixed_settings::use_w->get_bool( );

		if ( myhero->get_mana_percent( ) < mixed_settings::min_mana->get_int( ) )
			return;

		auto target = target_selector->get_target( 900, damage_type::magical );

		if ( target != nullptr )
		{
			if ( qSpell
				&& q->is_ready( )
				&& target->is_valid_target( q->range( ) ) )
				q->cast( target, hit_chance::medium );

			if ( eSpell
				&& e->is_ready( )
				&& target->is_valid_target( e->range( ) + myhero->get_bounding_radius( ) ) )
				e->cast( target );

			if ( wSpell
				&& w->is_ready( )
				&& target->is_valid_target( w->range( ) + myhero->get_bounding_radius( ) ) )
				w->cast( target );
		}

		auto minions = entitylist->get_enemy_minions( );

		if ( myhero->get_mana_percent( ) < lane_clear_settings::min_mana->get_int( ) )
			return;

		for ( auto&& minion : minions )
		{
			if ( minion->is_valid_target( q->range( ) ) )
			{
				if ( qlSpell && q->is_ready( ) && minion->get_health( ) < q->get_damage( minion ) )
				{
					if ( q->cast( minion, hit_chance::low ) )
					{
						return;
					}
				}
			}
		}
	}

	void combo_mode( )
	{
		auto q_target = target_selector->get_target( q->range( ), damage_type::magical );
		auto q_target_no_collision = target_selector->get_target( q->range( ), damage_type::magical );
		{
			if ( q_target_no_collision )
			{
				auto pred = q->get_prediction( q_target_no_collision );
				if ( pred.hitchance == hit_chance::collision )
					q_target_no_collision = nullptr;
			}
		}
		auto target = target_selector->get_target( -1, damage_type::magical );

		if ( q_target == nullptr )
			return;

		if ( combo_settings::root_combo->get_bool( ) )
		{
			if ( target != nullptr )
			{
				if ( w->is_ready( ) && has_e( target ) )
				{
					w->cast( target );
					return;
				}

				if ( e->is_ready( ) && !has_e( target ) )
				{
					last_e_target_nid = target->get_network_id( );
					e->cast( target );
					return;
				}
			} else
			{
				auto cursor_pos = hud->get_hud_input_logic( )->get_game_cursor_position( );
				auto longer_range_target = target_selector->get_target( w->range( ) + myhero->get_bounding_radius( ) + 100, damage_type::magical );;

				if ( longer_range_target != nullptr && ( cursor_pos - myhero->get_position( ) ).normalized( )
					.angle_between( ( longer_range_target->get_position( ) - myhero->get_position( ) ) ) < 60 )
				{
					//block other spells if we are chasing..

					return;
				}
			}
		}

		if ( q->is_ready( ) )
		{
			if ( target != nullptr && q_target->get_network_id( ) == target->get_network_id( ) )
			{
				if ( q_target_no_collision != nullptr && q_target_no_collision->get_network_id( ) == q_target->get_network_id( ) )
				{
					//no collision
					q->cast( q_target, hit_chance::medium );
				} else
				{
					//collision
					game_object_script flux_target = nullptr;
					prediction_output flux_prediction;

					auto p = q_target_no_collision != nullptr ? q->get_prediction( q_target_no_collision ) : prediction_output( );

					if ( get_qe_target( q_target, flux_target, flux_prediction ) )
					{
						q->cast( flux_prediction.get_cast_position( ) );
					} else if ( p.hitchance >= hit_chance::medium )
					{
						q->cast( p.get_cast_position( ) );
					} else
					{
						for ( auto enemy : entitylist->get_enemy_heroes( ) )
						{
							if ( enemy->is_valid_target( q->range( ) ) )
							{
								if ( get_qe_target( enemy, flux_target, flux_prediction ) )
								{
									q->cast( flux_prediction.get_cast_position( ) );
								}
							}
						}
					}
				}
			} else
			{
				game_object_script flux_target = nullptr;
				prediction_output flux_prediction;

				if ( q_target_no_collision != nullptr )
				{
					if ( q_target->get_network_id( ) != q_target_no_collision->get_network_id( ) && get_qe_target( q_target, flux_target, flux_prediction ) )
					{
						q->cast( flux_prediction.get_cast_position( ) );
					} else if ( target == nullptr || q_target->get_network_id( ) == q_target_no_collision->get_network_id( ) || q_target_no_collision->get_health( ) <= q->get_damage( q_target_no_collision ) )
					{
						q->cast( q_target_no_collision, hit_chance::medium );
					}
				} else
				{

					for ( auto enemy : entitylist->get_enemy_heroes( ) )
					{
						if ( enemy->is_valid_target( q->range( ) ) )
						{
							if ( get_qe_target( enemy, flux_target, flux_prediction ) )
							{
								q->cast( flux_prediction.get_cast_position( ) );
							}
						}

					}
				}

				if ( target != nullptr )
				{
					auto pred = q->get_prediction( target );

					if ( pred.hitchance >= hit_chance::medium )
					{
						q->cast( pred.get_cast_position( ) );
					} else
					{
						if ( get_qe_target( target, flux_target, flux_prediction ) )
						{
							q->cast( flux_prediction.get_cast_position( ) );
						}
					}
				}
			}
		}

		if ( target != nullptr )
		{
			if ( e->is_ready( ) )
			{
				last_e_target_nid = target->get_network_id( );
				e->cast( target );
			}

			if ( w->is_ready( ) )
			{
				if ( has_e( target ) )
				{
					if ( should_root( target ) )
					{
						w->cast( target );
					} else if ( q->is_ready( ) )
					{
						w->cast( target );
					}
				} else if ( !e->is_ready( ) )
				{
					w->cast( target );
				}
			}
		}
	}

	void aa_block( )
	{
		if ( myhero->get_level( ) >= 6 || !combo_settings::aa_block_after_6->get_bool( ) )
			orbwalker->set_attack( !combo_settings::aa_block->get_bool( ) );
	}

	void on_process_spell( game_object_script sender, spell_instance_script spell )
	{
		if ( sender->is_me( ) && w->is_ready( ) )
		{
			auto target = spell->get_last_target_id( ) > 0 ? entitylist->get_object( spell->get_last_target_id( ) ) : nullptr;

			if ( target == nullptr || !target->is_ai_hero( ) )
				return;

			if ( spell->get_spellslot( ) == spellslot::e )
			{
				if ( combo_settings::root_combo->get_bool( ) )
					w->cast( target );
				else if ( should_root( target ) && orbwalker->combo_mode( ) )
					w->cast( target );
				else if ( forced_w_target_nid == target->get_network_id( ) )
				{
					w->cast( target );
					forced_w_target_nid = 0;
				}
			}
		}
	}

	void on_create( game_object_script sender )
	{
		if ( sender->is_missile( ) && sender->get_name( ) == "RyzeQ" )
		{
			auto spell_caster = sender->missile_get_sender_id( ) > 0 ? entitylist->get_object( sender->missile_get_sender_id( ) ) : nullptr;

			if ( spell_caster != nullptr && spell_caster->is_me( ) )
			{
				auto target = sender->missile_get_target_id( ) > 0 ? entitylist->get_object( sender->missile_get_target_id( ) ) : nullptr;

				if ( target != nullptr )
				{
					q_missile_target_nid = target->get_network_id( );
				}
			}
		}
	}


	void on_delete( game_object_script sender )
	{
		if ( sender->is_missile( ) && sender->get_name( ) == "RyzeQ" )
		{
			auto spell_caster = sender->missile_get_sender_id( ) > 0 ? entitylist->get_object( sender->missile_get_sender_id( ) ) : nullptr;

			if ( spell_caster != nullptr && spell_caster->is_me( ) )
			{
				auto target = sender->missile_get_target_id( ) > 0 ? entitylist->get_object( sender->missile_get_target_id( ) ) : nullptr;

				if ( target != nullptr )
				{
					q_missile_target_nid = 0;
				}
			}
		}
	}

	void on_before_attack( game_object_script target, bool* process )
	{
		auto heroTarget = target != nullptr && target->is_ai_hero( ) ? target : nullptr;

		if ( heroTarget != nullptr && orbwalker->combo_mode( ) )
		{
			if ( myhero->get_level( ) >= 6 || !combo_settings::aa_block_after_6->get_bool( ) )
			{
				auto aaDmg = myhero->get_auto_attack_damage( heroTarget );

				if ( heroTarget->get_health( ) > aaDmg * combo_settings::min_aa_block->get_int( ) )
				{
					*process = false;
				}
			}
		}

		if ( w->is_ready( ) && orbwalker->combo_mode( ) )
		{
			if ( heroTarget != nullptr && ( has_e( heroTarget ) || e->is_ready( ) ) )
			{
				*process = false;
			}
		} else if ( e->is_ready( ) && orbwalker->combo_mode( ) )
		{
			*process = false;
		}
	}

	void on_update( )
	{
		if ( myhero->is_dead( ) ||
			myhero->is_recalling( ) )
			return;

		if ( orbwalker->combo_mode( ) )
		{
			combo_mode( );
			aa_block( );
		}

		if ( orbwalker->mixed_mode( ) )
		{
			mixed_mode( );
			orbwalker->set_attack( true );
		}

		if ( orbwalker->lane_clear_mode( ) )
		{
			jungle_clear( );
			lane_clear( );
		}

		if ( orbwalker->last_hit_mode( ) )
		{
			last_hit( );
		}

		if ( orbwalker->none_mode( ) )
		{
			orbwalker->set_attack( true );
		}

		auto target = target_selector->get_target( q->range( ), damage_type::magical );

		if ( mixed_settings::use_auto_q->get_bool( ) && target != nullptr )
		{
			if ( myhero->get_mana_percent( ) >= mixed_settings::min_mana->get_int( ) )
				q->cast( target, hit_chance::medium );
		}

		killsteal( );

		if ( !event_settings::auto_w->get_bool( ) || !w->is_ready( ) || target == nullptr || myhero->get_distance( target ) > w->range( ) + myhero->get_bounding_radius( ) )
		{
			return;
		}

		if ( target->get_position( ).is_under_ally_turret( ) )
		{
			if ( has_e( target ) )
			{
				w->cast( target );
			} else if ( e->is_ready( ) )
			{
				e->cast( target );
				w->cast( target );
			}
		}
	}

	void on_gapcloser( game_object_script sender, vector const& dash_start, vector const& dash_end, float dash_speed, bool is_ally_grab )
	{
		if ( event_settings::antidash->get_bool( ) && w->is_ready( ) )
		{
			if ( sender->is_valid_target( w->range( ) + sender->get_bounding_radius( ) ) )
			{
				if ( has_e( sender ) )
				{
					w->cast( sender );
				} else if ( e->is_ready( ) )
				{
					e->cast( sender );
					w->cast( sender );
					forced_w_target_nid = sender->get_network_id( );
				}
			}
		}
	}

	void on_draw( )
	{
		if ( draw_settings::draw_q->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), q->range( ), D3DCOLOR_ARGB( 255, 62, 129, 237 ) );

		if ( draw_settings::draw_w->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), w->range( ), D3DCOLOR_ARGB( 255, 227, 203, 20 ) );

		if ( draw_settings::draw_e->get_bool( ) )
			draw_manager->add_circle( myhero->get_position( ), e->range( ), D3DCOLOR_ARGB( 255, 235, 12, 223 ) );

		if ( draw_settings::draw_r->get_bool( ) )
		{
			draw_manager->add_circle( myhero->get_position( ), RYZE_R_MIN_RANGE, D3DCOLOR_ARGB( 255, 224, 77, 13 ) );
			draw_manager->add_circle( myhero->get_position( ), RYZE_R_RANGE, D3DCOLOR_ARGB( 255, 224, 77, 13 ) );
		}

		auto pos = myhero->get_position( );
		renderer->world_to_screen( pos, pos );

		auto lc = lane_clear_settings::disable_lane->get_bool( );
		auto rc = combo_settings::root_combo->get_bool( );
		auto aq = mixed_settings::use_auto_q->get_bool( );

		draw_manager->add_text_on_screen( pos, ( lc ? 0xFF006400 : 0xFF0000FF ), 16, "Lane Clear[%c]: %s", lane_clear_settings::disable_lane->get_int( ), ( lc ? "ON" : "OFF" ) );
		draw_manager->add_text_on_screen( pos + vector( 0, 20 ), ( rc ? 0xFF006400 : 0xFF0000FF ), 16, "Force Root combo[%c]: %s", combo_settings::root_combo->get_int( ), ( rc ? "ON" : "OFF" ) );
		draw_manager->add_text_on_screen( pos + vector( 0, 40 ), ( aq ? 0xFF006400 : 0xFF0000FF ), 16, "Auto Q[%c]: %s", mixed_settings::use_auto_q->get_int( ), ( aq ? "ON" : "OFF" ) );

	}

	void load( )
	{
		q = plugin_sdk->register_spell( spellslot::q, RYZE_Q_RANGE );
		w = plugin_sdk->register_spell( spellslot::w, RYZE_W_RANGE );
		e = plugin_sdk->register_spell( spellslot::e, RYZE_E_RANGE );
		r = plugin_sdk->register_spell( spellslot::r, RYZE_R_RANGE );

		q->set_skillshot( RYZE_Q_DELAY, RYZE_Q_RADIUS, RYZE_Q_SPEED, { collisionable_objects::heroes, collisionable_objects::minions, collisionable_objects::yasuo_wall }, skillshot_type::skillshot_line );

		main_tab = menu->create_tab( "mcarry_ryze", "Ryze" );

		auto combo = main_tab->add_tab( "mcarry_ryze.combo", "Combo Settings" );
		{
			combo_settings::aa_block = combo->add_checkbox( "mcarry_ryze.combo.AAblock", "Block always Auto Attack in Combo", false );
			combo_settings::min_aa_block = combo->add_slider( "mcarry_ryze.combo.minAABlock", "Disable AA If Target Health > X AA", 4, 0, 10 );
			combo_settings::aa_block_after_6 = combo->add_checkbox( "mcarry_ryze.combo.AAblockAfter6", "Distable AA only after lvl 6", true );
			combo_settings::root_combo = combo->add_hotkey( "mcarry_ryze.combo.rootcombo", "Force Root combo (E->W->?)", TreeHotkeyMode::Toggle, 'G', false );
		}

		auto mixed = main_tab->add_tab( "mcarry_ryze.mixed", "Mixed Settings" );
		{
			mixed_settings::min_mana = mixed->add_slider( "mcarry_ryze.mixed.mMin", "Min. Mana for Spells", 40, 0, 100 );
			mixed_settings::use_q = mixed->add_checkbox( "mcarry_ryze.mixed.UseQM", "Use Q", true );
			mixed_settings::use_q_lh = mixed->add_checkbox( "mcarry_ryze.mixed.UseQMl", "Use Q to Last Hit Minions", true );
			mixed_settings::use_e = mixed->add_checkbox( "mcarry_ryze.mixed.UseEM", "Use E", false );
			mixed_settings::use_w = mixed->add_checkbox( "mcarry_ryze.mixed.UseWM", "Use W", false );
			mixed_settings::use_auto_q = mixed->add_hotkey( "mcarry_ryze.mixed.UseQautoh", "Auto Use Q", TreeHotkeyMode::Toggle, 'T', false );
		}

		auto farm = main_tab->add_tab( "mcarry_ryze.farm", "Farming Settings" );
		{
			auto lane_menu = farm->add_tab( "mcarry_ryze.farm.lane", "Lane Clear" );
			{
				lane_clear_settings::disable_lane = lane_menu->add_hotkey( "mcarry_ryze.farm.lane.disablelane", "Toggle Spell Usage in LaneClear", TreeHotkeyMode::Toggle, 'H', false );
				lane_clear_settings::min_mana = lane_menu->add_slider( "mcarry_ryze.farm.lane.useEPL", "Min. % Mana For Lane Clear", 50, 0, 100 );
				lane_clear_settings::use_q = lane_menu->add_checkbox( "mcarry_ryze.farm.lane.useQ2L", "Use Q", true );
				lane_clear_settings::use_e = lane_menu->add_checkbox( "mcarry_ryze.farm.lane.useE2L", "Use E", true );
			}
			auto jungle_menu = farm->add_tab( "mcarry_ryze.farm.jungle", "Jungle Settings" );
			{
				jungle_settings::min_mana = jungle_menu->add_slider( "mcarry_ryze.farm.jungle.useJM", "Min. % Mana for Jungle Clear", 50, 0, 100 );
				jungle_settings::use_q = jungle_menu->add_checkbox( "mcarry_ryze.farm.jungle.useQj", "Use Q", true );
				jungle_settings::use_w = jungle_menu->add_checkbox( "mcarry_ryze.farm.jungle.useWj", "Use W", true );
				jungle_settings::use_e = jungle_menu->add_checkbox( "mcarry_ryze.farm.jungle.useEj", "Use E", true );
			}
			auto lashit_menu = farm->add_tab( "mcarry_ryze.farm.lhit", "Last Hit Settings" );
			{
				lasthit_settings::use_q = lashit_menu->add_checkbox( "mcarry_ryze.farm.lhit.useQl2h", "Use Q to Last Hit", true );
				lasthit_settings::use_w = lashit_menu->add_checkbox( "mcarry_ryze.farm.lhit.useWl2h", "Use W to Last Hit", false );
				lasthit_settings::use_e = lashit_menu->add_checkbox( "mcarry_ryze.farm.lhit.useEl2h", "Use E to Last Hit", false );
			}
		}

		auto misc = main_tab->add_tab( "mcarry_ryze.misc", "Miscellaneous" );
		{
			auto event_menu = misc->add_tab( "mcarry_ryze.misc.ev", "Events" );
			{
				event_settings::antidash = event_menu->add_checkbox( "mcarry_ryze.misc.ev.useQW2D", "W/Q on Dashing", true );
				event_settings::auto_w = event_menu->add_checkbox( "mcarry_ryze.misc.ev.autow", "Auto W Enemy Under Ally Turret", true );
			}
			auto ks_menu = farm->add_tab( "mcarry_ryze.misc.kssettings", "Kill Steal" );
			{
				ks_settings::ks = ks_menu->add_checkbox( "mcarry_ryze.misc.kssettings.KS", "Killsteal", true );
				ks_settings::use_q = ks_menu->add_checkbox( "mcarry_ryze.misc.kssettings.useQ2KS", "Use Q to KS", true );
				ks_settings::use_w = ks_menu->add_checkbox( "mcarry_ryze.misc.kssettings.useW2KS", "Use W to KS", true );
				ks_settings::use_e = ks_menu->add_checkbox( "mcarry_ryze.misc.kssettings.useE2KS", "Use E to KS", true );
			}
		}

		auto draw = main_tab->add_tab( "mcarry_ryze.draw", "Draw Settings" );
		{
			draw_settings::draw_q = draw->add_checkbox( "mcarry_ryze.draw.q", "Draw Q range", true );
			draw_settings::draw_w = draw->add_checkbox( "mcarry_ryze.draw.w", "Draw W range", true );
			draw_settings::draw_e = draw->add_checkbox( "mcarry_ryze.draw.e", "Draw E range", true );
			draw_settings::draw_r = draw->add_checkbox( "mcarry_ryze.draw.r", "Draw R range", true );
		}

		antigapcloser::add_event_handler( on_gapcloser );

		event_handler<events::on_update>::add_callback( on_update );
		event_handler<events::on_draw>::add_callback( on_draw );
		event_handler<events::on_process_spell_cast>::add_callback( on_process_spell );
		event_handler<events::on_create_object>::add_callback( on_create );
		event_handler<events::on_delete_object>::add_callback( on_delete );
		event_handler<events::on_before_attack_orbwalker>::add_callback( on_before_attack );
	}

	void unload( )
	{
		antigapcloser::remove_event_handler( on_gapcloser );

		event_handler<events::on_update>::remove_handler( on_update );
		event_handler<events::on_draw>::remove_handler( on_draw );
		event_handler<events::on_process_spell_cast>::remove_handler( on_process_spell );
		event_handler<events::on_create_object>::remove_handler( on_create );
		event_handler<events::on_delete_object>::remove_handler( on_delete );
		event_handler<events::on_before_attack_orbwalker>::remove_handler( on_before_attack );

		menu->delete_tab( main_tab );

		plugin_sdk->remove_spell( q );
		plugin_sdk->remove_spell( w );
		plugin_sdk->remove_spell( e );
		plugin_sdk->remove_spell( r );
	}
}