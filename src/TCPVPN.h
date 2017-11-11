//
// Created by root on 11/10/17.
//

#ifndef LIGHTTCPVPN_TCPVPN_H
#define LIGHTTCPVPN_TCPVPN_H

#include <simple-event/src/simpleEvent.h>
#include "simple-event/src/simpleTCPServer.h"

typedef struct TCPTunnel TCPTunnel;
typedef struct TCPVPNServer TCPVPNServer;

struct TCPTunnel {
    int tunFD;
    int TCPSocketFD;
    struct sockaddr_in clientAddr;
    socklen_t length;
};

struct TCPVPNServer {
    simpleTCPServer *TCPServer;
    simpleEvent *event;
};

TCPVPNServer *createTCPVPNServer(const char *bindAddress, short port, int backlog);
int startTCPVPNServer(TCPVPNServer *TCPVPNServerInstance);
int startTCPVPNClient(TCPTunnel *client);
TCPTunnel *createTCPVPNClient(const char *tunDevName, const char *address, short port);
void destroyTCPVPNServer(TCPVPNServer *TCPVPNServerInstance);
void destroyTCPTunnel(TCPTunnel *instance);

#endif //LIGHTTCPVPN_TCPVPN_H
