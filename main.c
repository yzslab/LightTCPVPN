#include <stdio.h>
#include "src/TCPVPN.h"

int main(int argc, char *argv[]) {
    TCPVPNServer *server;
    TCPTunnel *tunnel;
    short port;
    if (argc == 1) {
        server = createTCPVPNServer(NULL, 2095, 1024);
        if (server == NULL) {
            perror("error");
            return -1;
        }
        startTCPVPNServer(server);
    } else {
        sscanf(argv[2], "%hd", &port);
        tunnel = createTCPVPNClient(NULL, argv[1], port);
        if (tunnel == NULL) {
            perror("error");
            return -1;
        }
        startTCPVPNClient(tunnel);
    }
    return 0;
}