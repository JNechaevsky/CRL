//
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
// Common code shared between the client and server
//

#ifndef NET_COMMON_H
#define NET_COMMON_H

#include "d_mode.h"
#include "net_defs.h"
#include "net_packet.h"

typedef enum
{
    // Client has sent a SYN, is waiting for a SYN in response.
    NET_CONN_STATE_CONNECTING,

    // Successfully connected.
    NET_CONN_STATE_CONNECTED,

    // Sent a DISCONNECT packet, waiting for a DISCONNECT_ACK reply
    NET_CONN_STATE_DISCONNECTING,

    // Client successfully disconnected
    NET_CONN_STATE_DISCONNECTED,

    // We are disconnected, but in a sleep state, waiting for several
    // seconds.  This is in case the DISCONNECT_ACK we sent failed
    // to arrive, and we need to send another one.  We keep this as
    // a valid connection for a few seconds until we are sure that
    // the other end has successfully disconnected as well.
    NET_CONN_STATE_DISCONNECTED_SLEEP,

} net_connstate_t;

// Reason a connection was terminated

typedef enum
{
    // As the result of a local disconnect request

    NET_DISCONNECT_LOCAL,

    // As the result of a remote disconnect request

    NET_DISCONNECT_REMOTE,

    // Timeout (no data received in a long time)

    NET_DISCONNECT_TIMEOUT,

} net_disconnect_reason_t;

#define MAX_RETRIES 5

typedef struct net_reliable_packet_s net_reliable_packet_t;

typedef struct 
{
    net_connstate_t state;
    net_disconnect_reason_t disconnect_reason;
    net_addr_t *addr;
    net_protocol_t protocol;
    int last_send_time;
    int num_retries;
    int keepalive_send_time;
    int keepalive_recv_time;
    net_reliable_packet_t *reliable_packets;
    int reliable_send_seq;
    int reliable_recv_seq;
} net_connection_t;


void NET_Conn_SendPacket(net_connection_t *conn, net_packet_t *packet);
void NET_Conn_InitClient(net_connection_t *conn, net_addr_t *addr,
                         net_protocol_t protocol);
void NET_Conn_InitServer(net_connection_t *conn, net_addr_t *addr,
                         net_protocol_t protocol);
boolean NET_Conn_Packet(net_connection_t *conn, net_packet_t *packet,
                        unsigned int *packet_type);
void NET_Conn_Disconnect(net_connection_t *conn);
void NET_Conn_Run(net_connection_t *conn);
net_packet_t *NET_Conn_NewReliable(net_connection_t *conn, int packet_type);

// Other miscellaneous common functions
unsigned int NET_ExpandTicNum(unsigned int relative, unsigned int b);
boolean NET_ValidGameSettings(GameMode_t mode, GameMission_t mission,
                              const net_gamesettings_t *settings);

void NET_OpenLog(void);
void NET_Log(const char *fmt, ...);
void NET_LogPacket(const net_packet_t *packet);

#endif /* #ifndef NET_COMMON_H */

