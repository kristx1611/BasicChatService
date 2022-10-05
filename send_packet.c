#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include <time.h>

#include "send_packet.h"

static float loss_probability = 0.0f;

void set_loss_probability( float x )
{
    srand48(time(NULL));
    loss_probability = x / 100.0f;
}

ssize_t send_packet( int sock, void* buffer, size_t size, int flags, struct sockaddr* addr, socklen_t addrlen )
{
    float rnd = drand48();
    if( rnd < loss_probability)
    {
        fprintf(stderr, "Randomly dropping a packet\n");
        return size;
    }

    return sendto( sock,
                   buffer,
                   size,
                   flags,
                   addr,
                   addrlen );
}
