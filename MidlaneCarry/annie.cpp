#include "annie.h"
#include "../plugin_sdk/plugin_sdk.hpp"

namespace annie
{
	script_spell* q = nullptr;
	script_spell* w = nullptr;
	script_spell* e = nullptr;
	script_spell* r = nullptr;

	script_spell* flash = nullptr;

#define ANNIE_Q_RANGE 625
#define ANNIE_W_RANGE 590
#define ANNIE_E_RANGE 0
#define ANNIE_R_RANGE 600

#define SPELL_FLASH_RANGE 400

#define Q_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 62, 129, 237 ))
#define W_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 227, 203, 20 ))
#define E_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 235, 12, 223 ))
#define R_DRAW_COLOR (D3DCOLOR_ARGB ( 255, 224, 77, 13 ))

	namespace draw_settings
	{
		TreeEntry* draw_range_q = nullptr;
		TreeEntry* draw_range_w = nullptr;
		TreeEntry* draw_range_e = nullptr;
		TreeEntry* draw_range_r = nullptr;
	}

	namespace farm_settings
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;

		TreeEntry* use_w_min_minions = nullptr;
		TreeEntry* use_w_min_minions_jungle = nullptr;

		TreeEntry* use_spells_mana = nullptr;
	}

	namespace misc_settings
	{
		TreeEntry* use_e = nullptr;
	}

	namespace harras_settings
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;

		TreeEntry* use_spells_when_mana = nullptr;
	}

	namespace combo_settings
	{
		TreeEntry* use_q = nullptr;
		TreeEntry* use_w = nullptr;
		TreeEntry* use_e = nullptr;
		TreeEntry* use_r = nullptr;
		TreeEntry* auto_tibers = nullptr;

		TreeEntry* r_only_stun = nullptr;

		TreeEntry* rCount = nullptr;
		TreeEntry* flashComboKey = nullptr;
		TreeEntry* rManualCast = nullptr;
		TreeEntry* flashRCount = nullptr;
		TreeEntry* OverKill = nullptr;

		std::map<uint16_t, TreeEntry*> use_on;
	}

	TreeTab* main_tab = nullptr;

	float last_tibers_command = 0.f;
	float q_missile_speed = 0.f;

	bool was_blockin_aa = false;

	game_object_script my_spawn_point;
	void on_update( )
	{
		if ( myhero->is_dead( ) )
			return;

		auto has_stun = myhero->has_buff( buff_hash( "anniepassiveprimed" ) );
		auto has_tibers = myhero->has_buff( buff_hash( "AnnieRController" ) );

		if ( myhero->get_position( ).distance( my_spawn_point->get_position( ) ) < 700.f )
		{
			if ( !has_stun )
			{
				if ( e->is_ready( ) )
					e->cast( );
				if ( w->is_ready( ) )
					w->cast( myhero->get_position( ) );
			}
		}

		if ( !has_tibers && flash && flash->is_ready( ) && ( !combo_settings::r_only_stun->get_bool( ) || has_stun ) && combo_settings::flashComboKey->get_bool( ) )
		{
			auto target = target_selector->get_target( r->range( ) + SPELL_FLASH_RANGE, damage_type::magical );
			if ( target )
			{
				auto pred = r->get_prediction( target, true, ANNIE_R_RANGE + SPELL_FLASH_RANGE );
				if ( pred.aoe_targets_hit_count( ) >= combo_settings::flashRCount->get_int( ) )
				{
					r->cast( pred.get_cast_position( ) );
					flash->cast( pred.get_cast_position( ) );
					return;
				}
			}
		}

		if ( !has_tibers && combo_settings::rManualCast->get_bool( ) && r->is_ready( ) && ( !combo_settings::r_only_stun->get_bool( ) || has_stun ) )
		{
			auto target = target_selector->get_target( r->range( ), damage_type::magical );
			if ( target )
			{
				if ( r->cast( target, hit_chance::high ) )
					return;
			}
		}

		if ( misc_settings::use_e->get_bool( ) && e->is_ready( ) )
		{
			auto dmg = health_prediction->get_incoming_damage( myhero, 0.35f, true );
			if ( dmg > 100 )
			{
				e->cast( );
			}
		}

		if ( has_tibers && combo_settings::auto_tibers->get_bool( ) && r->is_ready( ) )
		{
			auto target = target_selector->get_target( 1500.f, damage_type::magical );
			if ( target )
			{
				auto time = gametime->get_time( );
				if ( time > last_tibers_command )
				{
					r->cast( target );
					last_tibers_command = time + 0.5f;
				}
			}
		}

		if ( orbwalker->combo_mode( ) )
		{
			if ( combo_settings::use_e->get_bool( ) && e->is_ready( ) )
			{
				if ( !has_stun )
					e->cast( );
			}

			if ( !has_tibers && combo_settings::use_r->get_bool( ) && r->is_ready( ) && ( !combo_settings::r_only_stun->get_bool( ) || has_stun ) )
			{
				auto target = target_selector->get_target( r->range( ), damage_type::magical );
				if ( target )
				{
					auto pred = r->get_prediction( target, true );

					if ( pred.aoe_targets_hit_count( ) >= combo_settings::rCount->get_int( ) && combo_settings::rCount->get_int( ) > 1 )
					{
						r->cast( pred.get_cast_position( ) );
						return;
					}
				}

				for ( auto&& t : entitylist->get_enemy_heroes( ) )
				{
					if ( t->is_valid_target( r->range( ) ) )
					{
						if ( combo_settings::use_on[ t->get_id( ) ]->get_int( ) == 2 )
							continue;

						if ( combo_settings::use_on[ t->get_id( ) ]->get_int( ) == 1 )
							r->cast( t, hit_chance::high );

						if ( combo_settings::use_on[ t->get_id( ) ]->get_int( ) == 3 && has_stun )
							r->cast( t, hit_chance::high );

						auto combo_dmg = r->get_damage( t ) + health_prediction->get_incoming_damage( t, 0.2f, false );
						auto q_damage = 0.f;
						auto w_damage = 0.f;

						if ( myhero->get_mana( ) > r->mana_cost( ) + q->mana_cost( ) && q->is_ready( ) )
							q_damage += q->get_damage( t );

						if ( myhero->get_mana( ) > r->mana_cost( ) + q->mana_cost( ) + w->mana_cost( ) && w->is_ready( ) )
							w_damage += w->get_damage( t );

						combo_dmg += q_damage;
						combo_dmg += w_damage;
						combo_dmg += myhero->get_auto_attack_damage( t ) * 2;

						auto tHp = t->get_real_health( false, true );

						if ( combo_settings::OverKill->get_bool( ) && tHp < combo_dmg && tHp >( q_damage + w_damage ) && t->get_position( ).count_allys_in_range( 350.f, myhero ) == 0 )
						{
							r->cast( t, hit_chance::high );
						} else if ( tHp < combo_dmg )
						{
							r->cast( t, hit_chance::high );
						}
					}
				}
			}

			auto q_ready = false;
			auto w_ready = false;

			if ( combo_settings::use_q->get_bool( ) && q->is_ready( ) )
			{
				q_ready = true;

				auto q_target = target_selector->get_target( q->range( ), damage_type::magical );
				if ( q_target )
					q->cast( q_target );
			}

			if ( combo_settings::use_w->get_bool( ) && w->is_ready( ) )
			{
				w_ready = true;

				auto w_target = target_selector->get_target( w->range( ), damage_type::magical );
				if ( w_target )
					w->cast( w_target->get_position( ) );
			}

			if ( q_ready || w_ready )
			{
				was_blockin_aa = true;
				orbwalker->set_attack( false );
			} else if ( was_blockin_aa )
			{
				orbwalker->set_attack( true );
				was_blockin_aa = false;
			}
		} else if ( was_blockin_aa )
		{
			orbwalker->set_attack( true );
			was_blockin_aa = false;
		}

		if ( orbwalker->mixed_mode( ) )
		{
			if ( myhero->get_mana_percent( ) > harras_settings::use_spells_when_mana->get_int( ) )
			{
				if ( harras_settings::use_q->get_bool( ) && q->is_ready( ) )
				{
					auto q_target = target_selector->get_target( q->range( ), damage_type::magical );
					if ( q_target )
						q->cast( q_target );
				}

				if ( harras_settings::use_w->get_bool( ) && w->is_ready( ) )
				{
					auto w_target = target_selector->get_target( w->range( ), damage_type::magical );
					if ( w_target )
						w->cast( w_target->get_position( ) );
				}
			}
		}

		if ( orbwalker->lane_clear_mode( ) || orbwalker->last_hit_mode( ) || orbwalker->mixed_mode( ) )
		{
			if ( farm_settings::use_q->get_bool( ) && q->is_ready( ) )
			{
				std::vector<game_object_script> minions_list;
				minions_list.reserve( entitylist->get_enemy_minions( ).size( ) + entitylist->get_jugnle_mobs_minions( ).size( ) );
				minions_list.insert( minions_list.end( ), entitylist->get_enemy_minions( ).begin( ), entitylist->get_enemy_minions( ).end( ) );
				minions_list.insert( minions_list.end( ), entitylist->get_jugnle_mobs_minions( ).begin( ), entitylist->get_jugnle_mobs_minions( ).end( ) );

				if ( !minions_list.empty( ) )
				{
					auto dmg = q->get_damage( minions_list.front( ) );

					for ( auto& minion : minions_list )
					{
						if ( minion->get_distance( myhero ) > q->range( ) )
							continue;

						if ( !minion->is_valid_target( ) )
							continue;

						if ( minion->get_team( ) == game_object_team::neutral )
						{
							if ( q->cast( minion ) )
								break;
						} else
						{
							auto t = ( fmaxf( 0.f, minion->get_distance( myhero ) - myhero->get_bounding_radius( ) ) / q_missile_speed ) + 0.25f;
							auto minion_hp = health_prediction->get_health_prediction( minion, t - 0.1f );
							if ( minion_hp - dmg < 0 )
							{
								if ( q->cast( minion ) )
									break;
							}
						}
					}
				}
			}

			if ( orbwalker->lane_clear_mode( ) )
			{
				if ( myhero->get_mana_percent( ) > farm_settings::use_spells_mana->get_int( ) )
				{
					if ( farm_settings::use_w->get_bool( ) && w->is_ready( ) )
					{
						if ( !w->cast_on_best_farm_position( farm_settings::use_w_min_minions->get_int( ) ) )
						{
							w->cast_on_best_farm_position( farm_settings::use_w_min_minions_jungle->get_int( ), true );
						}
					}
				}
			}
		}
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

		if ( combo_settings::r_only_stun->get_bool( ) )
			draw_manager->add_text_on_screen( { 50,50 }, MAKE_COLOR( 0, 255, 0, 255 ), 17, "R stun only: ON" );
		else
			draw_manager->add_text_on_screen( { 50,50 }, MAKE_COLOR( 255, 0, 0, 255 ), 17, "R stun only: OFF" );
	}

	void load( )
	{
		q = plugin_sdk->register_spell( spellslot::q, ANNIE_Q_RANGE );
		w = plugin_sdk->register_spell( spellslot::w, ANNIE_W_RANGE );
		e = plugin_sdk->register_spell( spellslot::e, ANNIE_E_RANGE );
		r = plugin_sdk->register_spell( spellslot::r, ANNIE_R_RANGE );

		if ( myhero->get_spell( spellslot::summoner1 )->get_spell_data( )->get_name_hash( ) == spell_hash( "SummonerFlash" ) )
			flash = plugin_sdk->register_spell( spellslot::summoner1, SPELL_FLASH_RANGE );
		else if ( myhero->get_spell( spellslot::summoner2 )->get_spell_data( )->get_name_hash( ) == spell_hash( "SummonerFlash" ) )
			flash = plugin_sdk->register_spell( spellslot::summoner2, SPELL_FLASH_RANGE );

		q_missile_speed = myhero->get_spell( spellslot::q )->get_spell_data( )->MissileSpeed( );

		w->set_skillshot( .25f, degrees_to_radians(49.52f), FLT_MAX, {}, skillshot_type::skillshot_cone );
		r->set_skillshot( .25f, 250.f, FLT_MAX, {}, skillshot_type::skillshot_circle );

		for ( auto& it : entitylist->get_all_spawnpoints( ) )
			if ( it->is_ally( ) )
				my_spawn_point = it;

		main_tab = menu->create_tab( "carry.annie", "Annie" );

		auto combo = main_tab->add_tab( "carry.annie.combo", "Combo settings" );
		{
			combo_settings::use_q = combo->add_checkbox( "carry.annie.combo.q", "Use Q", true );
			combo_settings::use_w = combo->add_checkbox( "carry.annie.combo.w", "Use W", true );
			combo_settings::use_e = combo->add_checkbox( "carry.annie.combo.e", "Use E", true );
			combo_settings::use_r = combo->add_checkbox( "carry.annie.combo.r", "Use R", true );

			combo_settings::auto_tibers = combo->add_checkbox( "carry.annie.combo.r.tibers", "Auto tibers", true );

			combo_settings::r_only_stun = combo->add_hotkey( "combo.r.stun", "R stun only", TreeHotkeyMode::Toggle, 0x41, false );

			combo_settings::OverKill = combo->add_checkbox( myhero->get_model( ) + "r_config::OverKill", "No overkill", true );

			combo_settings::flashComboKey = combo->add_hotkey( "carry.annie.combo.r.flash.key", "Use R+flash key", TreeHotkeyMode::Hold, 0x54, false );
			combo_settings::rManualCast = combo->add_hotkey( "carry.annie.combo.r.manual", "R manual cast", TreeHotkeyMode::Hold, 0x53, false );

			combo_settings::rCount = combo->add_slider( "carry.annie.combo.rcount", "Auto R x enemys", 2, 0, 5 );
			combo_settings::flashRCount = combo->add_slider( "carry.annie.combo.rflashcombo", "flash + R stun x enemys", 2, 0, 5 );

			auto combo_use_r = combo->add_tab( "combo.r", "R config" );

			for ( auto&& enemy : entitylist->get_enemy_heroes( ) )
			{
				combo_settings::use_on[ enemy->get_id( ) ] = combo_use_r->add_combobox( "annie.use_on." + enemy->get_model( ), " " + enemy->get_model( ), { {"KS",nullptr}, {"Always",nullptr }, {"Never", nullptr}, {"Stun",nullptr} }, 0 );
			}
		};

		auto harras = main_tab->add_tab( "carry.annie.harras", "Harras settings" );
		{
			harras_settings::use_q = harras->add_checkbox( "carry.annie.harras.q", "Use Q", true );
			harras_settings::use_w = harras->add_checkbox( "carry.annie.harras.w", "Use W", true );

			harras_settings::use_spells_when_mana = harras->add_slider( "carry.annie.harras.mana", "Use spells when mana > %", 40, 1, 100 );
		};

		auto misc = main_tab->add_tab( "carry.annie.misc", "Misc settings" );
		{
			misc_settings::use_e = misc->add_checkbox( "carry.annie.misc.e", "Use E", true );
		}

		auto farm = main_tab->add_tab( "carry.annie.farm", "Farm settings" );
		{
			farm_settings::use_q = farm->add_checkbox( "carry.annie.farm.q", "Use Q", true );
			farm_settings::use_w = farm->add_checkbox( "carry.annie.farm.w", "Use W", true );

			farm_settings::use_w_min_minions = farm->add_slider( "carry.annie.farm.w.m", "Min W minions", 3, 1, 6 );
			farm_settings::use_w_min_minions_jungle = farm->add_slider( "carry.annie.farm.w.m.j", "Min W minions jungle", 1, 1, 6 );

			farm_settings::use_spells_mana = farm->add_slider( "carry.annie.farm.mana", "Use spells when mana > %", 60, 1, 100 );
		}

		auto draw = main_tab->add_tab( "carry.annie.draw", "Draw Settings" );
		{
			draw_settings::draw_range_q = draw->add_checkbox( "carry.annie.draw.q", "Draw Q range", true );
			draw_settings::draw_range_w = draw->add_checkbox( "carry.annie.draw.w", "Draw W range", true );
			draw_settings::draw_range_e = draw->add_checkbox( "carry.annie.draw.e", "Draw E range", true );
			draw_settings::draw_range_r = draw->add_checkbox( "carry.annie.draw.r", "Draw R range", true );
		}

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

		if ( flash )
			plugin_sdk->remove_spell( flash );

		event_handler<events::on_update>::remove_handler( on_update );
		event_handler<events::on_draw>::remove_handler( on_draw );
	}
};