//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2011-2017 RestlessRodent
// Copyright(C) 2018-2025 Julia Nechaevskaya
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//


#pragma once


//
// Keyboard controls
//

// Movement

extern int key_up, key_up2;
extern int key_down, key_down2;
extern int key_right, key_right2;
extern int key_left, key_left2;
extern int key_strafeleft, key_strafeleft2;
extern int key_straferight, key_straferight2;
extern int key_speed, key_speed2;
extern int key_strafe, key_strafe2;
extern int key_jump, key_jump2; // Hexen
extern int key_180turn, key_180turn2;

// Action

extern int key_fire, key_fire2;
extern int key_use, key_use2;

// View

extern int key_lookup, key_lookup2;
extern int key_lookdown, key_lookdown2;
extern int key_lookcenter, key_lookcenter2;

// Flying

extern int key_flyup, key_flyup2;
extern int key_flydown, key_flydown2;
extern int key_flycenter, key_flycenter2;

// Inventory

extern int key_invleft, key_invleft2;
extern int key_invright, key_invright2;
extern int key_useartifact, key_useartifact2;

// CRL controls

extern int key_crl_menu, key_crl_menu2;               // RestlessRodent -- CRL
extern int key_crl_prevlevel, key_crl_prevlevel2;     // [PN]
extern int key_crl_nextlevel, key_crl_nextlevel2;     // [crispy]
extern int key_crl_reloadlevel, key_crl_reloadlevel2; // [crispy]
extern int key_crl_demospeed, key_crl_demospeed2;     // [crispy]
extern int key_crl_extendedhud, key_crl_extendedhud2;
extern int key_crl_limits, key_crl_limits2;

// Game modes

extern int key_crl_spectator, key_crl_spectator2;
extern int key_crl_cameraup, key_crl_cameraup2;
extern int key_crl_cameradown, key_crl_cameradown2;
extern int key_crl_cameramoveto, key_crl_cameramoveto2;
extern int key_crl_freeze, key_crl_freeze2;
extern int key_crl_buddha, key_crl_buddha2;
extern int key_crl_notarget, key_crl_notarget2;
extern int key_crl_nomomentum, key_crl_nomomentum2;

// Advanced movement
extern int key_crl_autorun, key_crl_autorun2;
extern int key_crl_mlook; // [crispy]
extern int key_crl_novert, key_crl_novert2;
extern int key_crl_vilebomb, key_crl_vilebomb2;
extern int key_crl_vilefly, key_crl_vilefly2;

// Visplanes MAX value

extern int key_crl_clearmax, key_crl_clearmax2;
extern int key_crl_movetomax, key_crl_movetomax2;

// Cheat shortcuts

extern int key_crl_iddqd, key_crl_iddqd2;
extern int key_crl_idkfa, key_crl_idkfa2;
extern int key_crl_idfa, key_crl_idfa2;
extern int key_crl_idclip, key_crl_idclip2;
extern int key_crl_iddt, key_crl_iddt2;
extern int key_crl_mdk, key_crl_mdk2;

// Weapons

extern int key_weapon1, key_weapon1_2;
extern int key_weapon2, key_weapon2_2;
extern int key_weapon3, key_weapon3_2;
extern int key_weapon4, key_weapon4_2;
extern int key_weapon5, key_weapon5_2;
extern int key_weapon6, key_weapon6_2;
extern int key_weapon7, key_weapon7_2;
extern int key_weapon8, key_weapon8_2;
extern int key_prevweapon, key_prevweapon2;
extern int key_nextweapon, key_nextweapon2;

// Artifacts

extern int key_arti_quartz, key_arti_quartz2;
extern int key_arti_urn, key_arti_urn2;
extern int key_arti_bomb, key_arti_bomb2;
extern int key_arti_tome, key_arti_tome2;
extern int key_arti_ring, key_arti_ring2;
extern int key_arti_chaosdevice, key_arti_chaosdevice2;
extern int key_arti_shadowsphere, key_arti_shadowsphere2;
extern int key_arti_wings, key_arti_wings2;
extern int key_arti_torch, key_arti_torch2;
extern int key_arti_morph, key_arti_morph2;
extern int key_arti_health, key_arti_health2;
extern int key_arti_poisonbag, key_arti_poisonbag2;
extern int key_arti_blastradius, key_arti_blastradius2;
extern int key_arti_teleport, key_arti_teleport2;
extern int key_arti_teleportother, key_arti_teleportother2;
extern int key_arti_egg, key_arti_egg2;
extern int key_arti_invulnerability, key_arti_invulnerability2;
extern int key_arti_all, key_arti_all2;

// Automap

extern int key_map_toggle, key_map_toggle2;
extern int key_map_zoomin, key_map_zoomin2;
extern int key_map_zoomout, key_map_zoomout2;
extern int key_map_maxzoom, key_map_maxzoom2;
extern int key_map_follow, key_map_follow2;
extern int key_crl_map_rotate, key_crl_map_rotate2;
extern int key_crl_map_overlay, key_crl_map_overlay2;
extern int key_crl_map_mousepan, key_crl_map_mousepan2;
extern int key_crl_map_sndprop, key_crl_map_sndprop2;
extern int key_map_grid, key_map_grid2;
extern int key_map_mark, key_map_mark2;
extern int key_map_clearmark, key_map_clearmark2;
extern int key_map_north;
extern int key_map_south;
extern int key_map_east;
extern int key_map_west;

// Function keys

extern int key_menu_help, key_menu_help2;
extern int key_menu_save, key_menu_save2;
extern int key_menu_load, key_menu_load2;
extern int key_menu_volume, key_menu_volume2;
extern int key_menu_detail, key_menu_detail2;
extern int key_menu_qsave, key_menu_qsave2;
extern int key_menu_endgame, key_menu_endgame2;
extern int key_menu_messages, key_menu_messages2;
extern int key_menu_qload, key_menu_qload2;
extern int key_menu_quit, key_menu_quit2;
extern int key_menu_gammad, key_menu_gammad2;
extern int key_menu_gamma, key_menu_gamma2;
extern int key_spy, key_spy2;

// Shortcut keys

extern int key_pause, key_pause2;
extern int key_menu_screenshot, key_menu_screenshot2;
extern int key_message_refresh, key_message_refresh2;
extern int key_demo_quit, key_demo_quit2;

// Multiplayer

extern int key_multi_msg, key_multi_msg2;
extern int key_multi_msgplayer[8], key_multi_msgplayer2[8];

// Special menu keys, not available for rebinding

extern int key_menu_activate;
extern int key_menu_up;
extern int key_menu_down;
extern int key_menu_left;
extern int key_menu_right;
extern int key_menu_back;
extern int key_menu_forward;
extern int key_menu_confirm;
extern int key_menu_abort;
extern int key_menu_incscreen;
extern int key_menu_decscreen;
extern int key_menu_del; // [crispy]

//
// Strife key controls
//

// villsa [STRIFE] strife keys
/*
extern int key_usehealth;
extern int key_invquery;
extern int key_mission;
extern int key_invpop;
extern int key_invkey;
extern int key_invhome;
extern int key_invend;
extern int key_invuse;
extern int key_invdrop;
*/

//
// Mouse controls
//

extern int mousebfire, mousebfire2;
extern int mousebforward, mousebforward2;
extern int mousebspeed, mousebspeed2;
extern int mousebstrafe, mousebstrafe2;
extern int mousebbackward, mousebbackward2;
extern int mousebuse, mousebuse2;
extern int mousebstrafeleft, mousebstrafeleft2;
extern int mousebstraferight, mousebstraferight2;
extern int mousebprevweapon, mousebprevweapon2;
extern int mousebnextweapon, mousebnextweapon2;

// Heretic & Hexen: Inventory
extern int mousebinvleft;
extern int mousebinvright;
extern int mousebuseartifact;
// extern int mousebjump;

extern int dclick_use;

//
// Joystick controls
//

extern int joybfire;
extern int joybstrafe;
extern int joybuse;
extern int joybspeed;
// extern int joybjump;
extern int joybstrafeleft;
extern int joybstraferight;
extern int joybprevweapon;
extern int joybnextweapon;
extern int joybmenu;
extern int joybautomap;
extern int novert;

extern void M_BindControls (void);
extern void M_BindHereticControls (void);
extern void M_BindChatControls (unsigned int num_players);
