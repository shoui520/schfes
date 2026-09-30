#include <poll.h>
#include <stdlib.h>
#include <errno.h>

/* Vita libc registers each entry with epoll ADD, which silently loses later
 * entries for the same fd. libcurl polls read and write separately on one fd.
 * Merge interests for the native call, then report each caller entry. */
int sif_poll(struct pollfd *fds,nfds_t count,int timeout) {
    if(count && !fds){errno=EFAULT;return -1;}
    if(count>255){errno=EINVAL;return -1;}
    if(!count)return poll(NULL,0,timeout);
    struct pollfd *merged=calloc(count,sizeof(*merged));
    if(!merged){errno=ENOMEM;return -1;}
    nfds_t unique=0;
    for(nfds_t i=0;i<count;i++) {
        fds[i].revents=0;
        if(fds[i].fd<0)continue;
        nfds_t j=0;while(j<unique && merged[j].fd!=fds[i].fd)j++;
        if(j==unique){merged[j].fd=fds[i].fd;unique++;}
        /* Android normal/band flags collapse to Vita's IN/OUT capabilities. */
        if(fds[i].events&(1|2|0x40|0x80))merged[j].events|=POLLIN;
        if(fds[i].events&(4|0x100|0x200))merged[j].events|=POLLOUT;
    }
    int result=poll(merged,unique,timeout),saved=errno;
    if(result>=0) {
        result=0;
        for(nfds_t i=0;i<count;i++) {
            if(fds[i].fd<0)continue;
            nfds_t j=0;while(j<unique && merged[j].fd!=fds[i].fd)j++;
            short ready=merged[j].revents&(POLLERR|POLLHUP|POLLNVAL);
            if(merged[j].revents&POLLIN)ready|=fds[i].events&(1|2|0x40|0x80);
            if(merged[j].revents&POLLOUT)ready|=fds[i].events&(4|0x100|0x200);
            fds[i].revents=ready;if(ready)result++;
        }
    }
    free(merged);errno=saved;return result;
}
