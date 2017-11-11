//
// Created by root on 11/10/17.
//

#include "TCPVPN.h"
#include "tun.h"
#include "simple-event/src/simpleEvent.h"
#include "simple-event/src/simpleEventEpollAdapter.h"
#include "simple-event/src/simpleEventHashTableAdapter.h"
#include "simple-event/src/simpleTCPServer.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

static const size_t tunDevNameBufferSize = 32;
static const int tunDevFlags = IFF_TUN | IFF_NO_PI;

typedef enum TCPVPN_TYPE TCPVPN_TYPE;
typedef enum readWriteTag readWriteTag;

enum TCPVPN_TYPE{
    TCPVPN_SERVER,
    TCPVPN_TUNNEL,
};

enum readWriteTag {
    EOF_TAG = 1,
    BLOCK_TAG = 1 < 1,
    OTHER_TAG = 1 < 2,
};


struct TCPVPN {
    TCPVPN_TYPE type;
    int fd;
    void *instance;
};

typedef struct TCPVPN TCPVPN;

static TCPTunnel *createTCPVPNFromServer(int serverSocketFD);
static int TCPTunnelFDAdd(simpleEvent *simpleEventInstance, TCPTunnel *TCPTunnelInstance);
static size_t readTillBlock(int fd, void *buffer, size_t needToRead, int *tag);
static size_t writeTillBlock(int fd, void *buffer, size_t needToWrite, int *tag);


TCPVPNServer *createTCPVPNServer(const char *bindAddress, short port, int backlog) {
    /*
    // Create tun device
    // char tunDevNameBuffer[tunDevNameBufferSize];
    int tunFD = createTun(tunDevName, tunDevFlags, NULL, NULL, tunDevNameBufferSize);
    if (tunFD < 0)
        return NULL;
    */

    // Create TCP Server
    simpleTCPServer *simpleTCPServerInstance = createSimpleTCPServer(bindAddress, port, backlog);
    if (simpleTCPServerInstance == NULL)
        return NULL;

    // Create TCP VPN Server
    TCPVPNServer *TCPVPNServerInstance = malloc(sizeof(TCPVPNServer));
    TCPVPNServerInstance->TCPServer = simpleTCPServerInstance;

    simpleEventHandler eventHandler = createEpollHandler();
    simpleEventContainerHandler containerHandler = createHashTableHandler();
    TCPVPNServerInstance->event = createSimpleEvent(100, &eventHandler, &containerHandler);
    if (TCPVPNServerInstance->event == NULL)
        goto DESTROY_TCP_VPN_SERVER;

    TCPVPN *TCPVPNInstance = malloc(sizeof(TCPVPN));
    TCPVPNInstance->type = TCPVPN_SERVER;
    TCPVPNInstance->fd = getServerSocketFD(TCPVPNServerInstance->TCPServer);
    TCPVPNInstance->instance = TCPVPNServerInstance;

    simpleEventAddFD(TCPVPNServerInstance->event, getServerSocketFD(TCPVPNServerInstance->TCPServer), SIMPLE_EVENT_READ | SIMPLE_EVENT_EXCEPTION, TCPVPNInstance);

    return TCPVPNServerInstance;

    DESTROY_TCP_VPN_SERVER:
    free(TCPVPNServerInstance);
    CLOSE_TUN_FD:
    // close(tunFD);
    RETURN_NULL:
    return NULL;
}

int startTCPVPNServer(TCPVPNServer *TCPVPNServerInstance) {
    int eventCounter, i, returnValue = -1, tag;
    size_t readBytes, writeBytes, mtu = 1500;
    char packetBuffer[mtu];
    TCPVPNServer *serverInstance;
    TCPVPN *TCPVPNInstance, *tmpTCPVPNInstance;
    TCPTunnel *TCPTunnelInstance;
    while (1) {
        simpleEventFD *fds;
        printf("Waiting for events...\n");
        eventCounter = simpleEventWait(TCPVPNServerInstance->event, &fds, -1);
        if (eventCounter == -1) {
            switch (errno) {
                case EINTR:
                    continue;
                case EBADF:
                case EFAULT:
                case EINVAL:
                    perror("epoll_wait()");
                    goto END_LOOP;
                default:
                    fprintf(stderr, "Unrecognized error.\n");
                    goto END_LOOP;
            }
        }

        printf("%d events.\n", eventCounter);

        for (i = 0; i < eventCounter; ++i) {
            TCPVPNInstance = fds[i].data;
            switch (TCPVPNInstance->type) {
                case TCPVPN_SERVER:
                    if (fds[i].events & SIMPLE_EVENT_READ) {
                        serverInstance = TCPVPNInstance->instance;
                        while ((TCPTunnelInstance = createTCPVPNFromServer(TCPVPNInstance->fd))) {
                            if (TCPTunnelFDAdd(serverInstance->event, TCPTunnelInstance) == -1)
                                destroyTCPTunnel(TCPTunnelInstance);
                        }
                    } else if (fds[i].events & SIMPLE_EVENT_EXCEPTION)
                        goto END_LOOP;
                    break;
                case TCPVPN_TUNNEL:
                    // Get TCPTunnel instance
                    TCPTunnelInstance = TCPVPNInstance->instance;
                    if (fds[i].events & SIMPLE_EVENT_READ) {
                        readBytes = readTillBlock(fds[i].fd, packetBuffer, mtu, &tag);
                        printf("Read bytes: %zd\n", readBytes);
                        // Close FD on EOF
                        if (tag & EOF_TAG) {
                            simpleEventRemoveFD(TCPVPNServerInstance->event, TCPTunnelInstance->TCPSocketFD);
                            simpleEventRemoveFD(TCPVPNServerInstance->event, TCPTunnelInstance->tunFD);

                            close(fds[i].fd);

                            // Set the closed FD variable to -1
                            if (TCPTunnelInstance->tunFD == fds[i].fd) {
                                printf("EOF from tun.\n");
                                TCPTunnelInstance->tunFD = -1;
                            } else {
                                TCPTunnelInstance->TCPSocketFD = -1;
                            }
                        }
                        writeBytes = writeTillBlock(TCPVPNInstance->fd, packetBuffer, readBytes, &tag);
                        printf("Write bytes: %zd\n", writeBytes);
                        if (tag & OTHER_TAG)
                            perror("write()");
                        // If one side has been closed, destroy TCPTunnelInstance
                        if (TCPTunnelInstance->tunFD == -1 || TCPTunnelInstance->TCPSocketFD == -1) {
                            destroyTCPTunnel(TCPTunnelInstance);
                            free(TCPVPNInstance);
                        }
                    } else if (fds[i].events & SIMPLE_EVENT_EXCEPTION) {
                        destroyTCPTunnel(TCPTunnelInstance);
                    }
                    break;
            }
        }
    }
    END_LOOP:
    return returnValue;
}

static int TCPTunnelFDAdd(simpleEvent *simpleEventInstance, TCPTunnel *TCPTunnelInstance) {
    int tunFD = TCPTunnelInstance->tunFD, TCPSocketFD = TCPTunnelInstance->TCPSocketFD;
    TCPVPN *tmpTCPVPNInstance1, *tmpTCPVPNInstance2;
    // Add TCP Socket FD
    tmpTCPVPNInstance1 = malloc(sizeof(TCPVPN));
    if (tmpTCPVPNInstance1 == NULL)
        return -1;

    tmpTCPVPNInstance1->fd = tunFD;
    tmpTCPVPNInstance1->type = TCPVPN_TUNNEL;
    tmpTCPVPNInstance1->instance = TCPTunnelInstance;
    simpleEventAddFD(simpleEventInstance, TCPSocketFD,
                     SIMPLE_EVENT_READ | SIMPLE_EVENT_EXCEPTION,
                     tmpTCPVPNInstance1);

    // Add tun FD
    tmpTCPVPNInstance2 = malloc(sizeof(TCPVPN));
    if (tmpTCPVPNInstance2 == NULL) {
        free(tmpTCPVPNInstance1);
        return -1;
    }
    tmpTCPVPNInstance2->fd = TCPTunnelInstance->TCPSocketFD;
    tmpTCPVPNInstance2->type = TCPVPN_TUNNEL;
    tmpTCPVPNInstance2->instance = TCPTunnelInstance;
    simpleEventAddFD(simpleEventInstance, TCPTunnelInstance->tunFD,
                     SIMPLE_EVENT_READ | SIMPLE_EVENT_EXCEPTION,
                     tmpTCPVPNInstance2);

    return 0;
}

int startTCPVPNClient(TCPTunnel *client) {
    size_t mtu = 1500, readBytes, writeBytes;
    char packetBuffer[mtu];
    int tag;
    simpleEventHandler eventHandler = createEpollHandler();
    simpleEventContainerHandler containerHandler = createHashTableHandler();
    simpleEvent *event = createSimpleEvent(100, &eventHandler, &containerHandler);
    if (event == NULL)
        return -1;

    if (TCPTunnelFDAdd(event, client) < 0)
        return -1;

    simpleEventFD *fds;
    int eventCounter, i;

    TCPVPN *TCPVPNInstance;
    TCPTunnel *TCPTunnelInstance;
    while (1) {
        eventCounter = simpleEventWait(event, &fds, -1);
        if (eventCounter == -1) {
            if (errno == EINTR)
                continue;
            break;
        }

        printf("%d events.\n", eventCounter);

        for (i = 0; i < eventCounter; ++i) {
            TCPVPNInstance = fds[i].data;
            TCPTunnelInstance = TCPVPNInstance->instance;
            if (fds[i].events & SIMPLE_EVENT_READ) {
                readBytes = readTillBlock(fds[i].fd, packetBuffer, mtu, &tag);
                printf("Read bytes: %zd\n", readBytes);
                // Close FD on EOF
                if (tag & EOF_TAG) {
                    close(fds[i].fd);

                    // Set the closed FD variable to -1
                    if (TCPTunnelInstance->tunFD == fds[i].fd)
                        TCPTunnelInstance->tunFD = -1;
                    else
                        TCPTunnelInstance->TCPSocketFD = -1;
                }
                writeBytes = writeTillBlock(TCPVPNInstance->fd, packetBuffer, readBytes, &tag);
                printf("Write bytes: %zd\n", writeBytes);
                // If one side has been closed, destroy TCPTunnelInstance
                if (tag & OTHER_TAG)
                    perror("write()");
                if (TCPTunnelInstance->tunFD == -1 || TCPTunnelInstance->TCPSocketFD == -1) {
                    destroyTCPTunnel(TCPTunnelInstance);
                    free(TCPVPNInstance);
                    goto END_WHILE;
                }
            } else {
                destroyTCPTunnel(TCPTunnelInstance);
                goto END_WHILE;
            }
        }
    }

    END_WHILE:;
}

static size_t readTillBlock(int fd, void *buffer, size_t needToRead, int *tag) {
    size_t totalReadCounter = 0;
    ssize_t readReturn;
    int tagBuffer;

    if (tag == NULL)
        tag = &tagBuffer; // Reduce if (tag != NULL)
    *tag = 0;

    while (totalReadCounter < needToRead) {
        // Read from interrupt position
        readReturn = read(fd, buffer + totalReadCounter, needToRead - totalReadCounter);
        switch (readReturn) {
            case -1:
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    *tag |= BLOCK_TAG;
                else
                    *tag |= OTHER_TAG;
                goto END_WHILE;
            case 0:
                *tag |= EOF_TAG;
                goto END_WHILE;
            default:
                totalReadCounter += readReturn;
        }
    }
    END_WHILE:
    return totalReadCounter;
}

static size_t writeTillBlock(int fd, void *buffer, size_t needToWrite, int *tag) {
    size_t totalWriteCounter = 0;
    ssize_t writeReturn;
    int tagBuffer;

    if (tag == NULL)
        tag = &tagBuffer;
    *tag = 0;

    while (totalWriteCounter < needToWrite) {
        // Write from interrupt position
        writeReturn = write(fd, buffer + totalWriteCounter, needToWrite - totalWriteCounter);
        if (writeReturn == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                *tag |= BLOCK_TAG;
            else
                *tag |= OTHER_TAG;
            goto END_WHILE;
        }
        totalWriteCounter += writeReturn;
    }

    END_WHILE:
    return totalWriteCounter;
}

static TCPTunnel *createTCPVPNFromServer(int serverSocketFD) {
    TCPTunnel *TCPTunnelInstance = malloc(sizeof(TCPTunnel));
    if (TCPTunnelInstance == NULL)
        goto RETURN_NULL;
    int socketFD = accept(serverSocketFD, &TCPTunnelInstance->clientAddr, &TCPTunnelInstance->length);
    if (socketFD < 0)
        goto DESTROY_TCP_TUNNEL;

    char IPBuffer[16];
    inet_ntop(AF_INET, &TCPTunnelInstance->clientAddr.sin_addr, IPBuffer, 16);
    printf("Accept connection from %s.\n", IPBuffer);

    TCPTunnelInstance->TCPSocketFD = socketFD;

    int tunFD = createTun(NULL, tunDevFlags, NULL, NULL, NULL);
    if (tunFD < 0)
        goto CLOSE_SOCKET_FD;

    TCPTunnelInstance->tunFD = tunFD;

    return TCPTunnelInstance;

    CLOSE_SOCKET_FD:
    close(socketFD);
    DESTROY_TCP_TUNNEL:
    free(TCPTunnelInstance);
    RETURN_NULL:
    return NULL;
}

TCPTunnel *createTCPVPNClient(const char *tunDevName, const char *address, short port) {
    TCPTunnel *TCPTunnelInstance = malloc(sizeof(TCPTunnel));
    if (TCPTunnelInstance == NULL)
        goto RETURN_NULL;

    struct sockaddr_in remoteAddr;
    memset(&remoteAddr, 0, sizeof(struct sockaddr_in));
    remoteAddr.sin_family = AF_INET;
    struct in_addr binaryAddr;
    if (inet_pton(AF_INET, address, &binaryAddr) == -1)
        goto DESTROY_TCP_TUNNEL_INSTANCE;
    remoteAddr.sin_addr.s_addr = binaryAddr.s_addr;
    remoteAddr.sin_port = htons((unsigned short) port);

    int TCPSocketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (TCPSocketFD < 0)
        goto DESTROY_TCP_TUNNEL_INSTANCE;

    if (connect(TCPSocketFD, &remoteAddr, sizeof(struct sockaddr_in)) < 0)
        goto CLOSE_TCP_SOCKET_FD;

    int tunFD = createTun(tunDevName, tunDevFlags, NULL, NULL, 0);
    if (tunFD < 0)
        goto CLOSE_TCP_SOCKET_FD;

    TCPTunnelInstance->tunFD = tunFD;
    TCPTunnelInstance->TCPSocketFD = TCPSocketFD;
    return TCPTunnelInstance;

    CLOSE_TCP_SOCKET_FD:
    close(TCPSocketFD);
    DESTROY_TCP_TUNNEL_INSTANCE:
    free(TCPTunnelInstance);
    RETURN_NULL:
    return NULL;
}

void destroyTCPVPNServer(TCPVPNServer *TCPVPNServerInstance) {
    destroySimpleEvent(TCPVPNServerInstance->event);
    free(TCPVPNServerInstance);
}

void destroyTCPTunnel(TCPTunnel *instance) {
    if (instance->tunFD >= 0)
        close(instance->tunFD);
    if (instance->TCPSocketFD >= 0)
        close(instance->TCPSocketFD);
    free(instance);
}