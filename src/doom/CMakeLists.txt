add_library(doom STATIC
            am_map.c        am_map.h
            crlfunc.c       crlfunc.h
            ct_chat.c
            deh_ammo.c
            deh_bexstr.c
            deh_cheat.c
            deh_doom.c
            deh_frame.c
            deh_misc.c      deh_misc.h
            deh_ptr.c
            deh_sound.c
            deh_thing.c
            deh_weapon.c
                            d_englsh.h
            d_items.c       d_items.h
            d_main.c        d_main.h
            d_net.c
            doomdef.c       doomdef.h
            doomstat.c      doomstat.h
                            d_player.h
            dstrings.c      dstrings.h
                            d_think.h
            f_finale.c      f_finale.h
            f_wipe.c        f_wipe.h
            g_game.c        g_game.h
            info.c          info.h
            m_menu.c        m_menu.h
            m_random.c      m_random.h
            p_ceilng.c
            p_doors.c
            p_enemy.c
            p_floor.c
            p_inter.c
            p_lights.c
                            p_local.h
            p_map.c
            p_maputl.c
            p_mobj.c
            p_plats.c
            p_pspr.c
            p_saveg.c
            p_setup.c
            p_sight.c
            p_spec.c
            p_switch.c
            p_telept.c
            p_tick.c
            p_user.c
            r_bsp.c
            r_data.c
            r_draw.c
                            r_local.h
            r_main.c
            r_plane.c
            r_segs.c
            r_things.c
            s_sound.c       s_sound.h
            sounds.c        sounds.h
            statdump.c      statdump.h
            st_bar.c        st_bar.h
            wi_stuff.c      wi_stuff.h)

target_include_directories(doom PRIVATE "../" "${CMAKE_CURRENT_BINARY_DIR}/../../")
target_link_libraries(doom SDL2::SDL2)
if(ENABLE_SDL2_MIXER)
    target_link_libraries(doom SDL2_mixer::SDL2_mixer)
endif()
if(ENABLE_SDL2_NET)
    target_link_libraries(doom SDL2_net::SDL2_net)
endif()
