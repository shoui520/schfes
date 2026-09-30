#ifndef SIF_NET_BRIDGE_H
#define SIF_NET_BRIDGE_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
int sif_poll(struct pollfd *,nfds_t,int);
int sif_socket(int,int,int);
int sif_connect(int,const void *,socklen_t);
int sif_bind(int,const void *,socklen_t);
int sif_accept(int,void *,socklen_t *);
int sif_getsockname(int,void *,socklen_t *);
int sif_getpeername(int,void *,socklen_t *);
int sif_getaddrinfo(const char *,const char *,const struct addrinfo *,struct addrinfo **);
void sif_freeaddrinfo(struct addrinfo *);
int sif_setsockopt(int,int,int,const void *,socklen_t);
int sif_getsockopt(int,int,int,void *,socklen_t *);
ssize_t sif_send(int,const void *,size_t,int);
ssize_t sif_recv(int,void *,size_t,int);
ssize_t sif_sendto(int,const void *,size_t,int,const void *,socklen_t);
int sif_inet_pton(int,const char *,void *);
const char *sif_inet_ntop(int,const void *,char *,socklen_t);
#endif
