//
// Created by zhensheng on 11/10/17.
//

#ifndef LIGHTTCPVPN_TUN_H
#define LIGHTTCPVPN_TUN_H

#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>

int createTun(const char *devName, int flags, const char *tunFilePath, char *devNameBuffer, size_t devNameBufferSize);

#endif //LIGHTTCPVPN_TUN_H
