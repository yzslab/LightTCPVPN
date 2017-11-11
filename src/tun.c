//
// Created by zhensheng on 11/10/17.
//

#include "tun.h"

static const char const *defaultTunFilePath = "/dev/net/tun";

int createTun(const char *devName, int flags, const char *tunFilePath, char *devNameBuffer, size_t devNameBufferSize) {
    struct ifreq ifr;
    int fd, err;
    const char *clonedev = tunFilePath;

    if (clonedev == NULL)
        clonedev = defaultTunFilePath;

    /* Arguments taken by the function:
     *
     * char *dev: the name of an interface (or '\0'). MUST have enough
     *   space to hold the interface name if '\0' is passed
     * int flags: interface flags (eg, IFF_TUN etc.)
     */

    /* open the clone device */
    if( (fd = open(clonedev, O_RDWR)) < 0 ) {
        return fd;
    }

    /* preparation of the struct ifr, of type "struct ifreq" */
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = flags;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

    if (devName != NULL) {
        /* if a device name was specified, put it in the structure; otherwise,
         * the kernel will try to allocate the "next" device of the
         * specified type */
        strncpy(ifr.ifr_name, devName, IFNAMSIZ);
    }

    /* try to create the device */
    if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
        close(fd);
        return err;
    }

    /* if the operation was successful, write back the name of the
     * interface to the buffer "devNameBuffer", so the caller can know
     * it.
     */
    if (devNameBuffer != NULL) {
        strncpy(devNameBuffer, ifr.ifr_name, devNameBufferSize);
        devNameBuffer[devNameBufferSize - 1] = '\0';
    }

    /* this is the special file descriptor that the caller will use to talk
     * with the virtual interface */
    return fd;
}