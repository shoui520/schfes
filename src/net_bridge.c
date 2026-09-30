#include "net_bridge.h"
#include "net_address.h"
#include "reimpl/errno.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
void sif_log(const char *,...);
_Static_assert(sizeof(struct sockaddr_in)==16,"Vita IPv4 ABI");
_Static_assert(sizeof(struct sockaddr_in6)==32,"Vita IPv6 ABI");
_Static_assert(offsetof(struct sockaddr_in6,sin6_scope_id)==28,"Vita IPv6 scope ABI");
_Static_assert(sizeof(struct addrinfo)==32,"Android ARM addrinfo ABI");
_Static_assert(offsetof(struct addrinfo,ai_addr)==24,"Android addrinfo address ABI");
static int host_family(int family) {
    if(family==0)return AF_UNSPEC;
    if(family==2)return AF_INET;
    if(family==10)return AF_INET6;
    errno=EAFNOSUPPORT;return -1;
}
int sif_socket(int family,int type,int protocol) {
    int native=host_family(family);if(native<0)return -1;
    /* Creation flags and non-IP domains need separate contracts. */
    if(type<1 || type>5){errno=EINVAL;return -1;}
    int r=socket(native,type,protocol),e=errno;
    sif_log("net socket family=%d type=%d result=%d errno=%d",family,type,r,r<0?e:0);
    errno=e;return r;
}
static int address_in(const void *src,socklen_t len,unsigned char *dst) {
    int n=sif_address_to_vita(src,len,dst);
    if(n<0)errno=(!src?EFAULT:len<2?EINVAL:EAFNOSUPPORT);
    return n;
}
int sif_connect(int fd,const void *addr,socklen_t len) {
    _Alignas(4) unsigned char native[32];int n=address_in(addr,len,native);if(n<0)return -1;
    if(native[1]==AF_INET) {
        const struct sockaddr_in *v=(const struct sockaddr_in *)native;
        char ip[INET_ADDRSTRLEN];inet_ntop(AF_INET,&v->sin_addr,ip,sizeof(ip));
        sif_log("net destination address=%s port=%u",ip,(unsigned)ntohs(v->sin_port));
    }
    int r=connect(fd,(struct sockaddr *)native,n),e=errno;
    sif_log("net connect fd=%d family=%u result=%d errno=%d",fd,native[1],r,r<0?e:0);
    errno=e;return r;
}
int sif_bind(int fd,const void *addr,socklen_t len) {
    _Alignas(4) unsigned char native[32];int n=address_in(addr,len,native);if(n<0)return -1;
    return bind(fd,(struct sockaddr *)native,n);
}
static int address_out(const void *src,socklen_t len,void *dst,socklen_t *capacity) {
    unsigned char android[28];int n=sif_address_to_android(src,len,android);
    if(n<0){errno=EAFNOSUPPORT;return -1;}
    memcpy(dst,android,*capacity<(unsigned)n?*capacity:(unsigned)n);*capacity=n;return 0;
}
static int name_out(int fd,void *dst,socklen_t *len,int peer) {
    if(!dst || !len){errno=EFAULT;return -1;}
    _Alignas(4) unsigned char native[32];socklen_t n=sizeof(native);
    int r=peer?getpeername(fd,(struct sockaddr *)native,&n):getsockname(fd,(struct sockaddr *)native,&n);
    return r<0?r:address_out(native,n,dst,len);
}
int sif_getsockname(int fd,void *dst,socklen_t *len){return name_out(fd,dst,len,0);}
int sif_getpeername(int fd,void *dst,socklen_t *len){return name_out(fd,dst,len,1);}
int sif_accept(int fd,void *dst,socklen_t *len) {
    if(!dst)return accept(fd,NULL,NULL);
    if(!len){errno=EFAULT;return -1;}
    _Alignas(4) unsigned char native[32];socklen_t n=sizeof(native);
    int client=accept(fd,(struct sockaddr *)native,&n);
    if(client>=0 && address_out(native,n,dst,len)<0){int e=errno;close(client);errno=e;return -1;}
    return client;
}
static int android_eai(int e) {
    switch(e) {
    case 0:return 0;case EAI_ADDRFAMILY:return 1;case EAI_AGAIN:return 2;
    case EAI_BADFLAGS:return 3;case EAI_FAIL:return 4;case EAI_FAMILY:return 5;
    case EAI_MEMORY:return 6;case EAI_NODATA:return 7;case EAI_NONAME:return 8;
    case EAI_SERVICE:return 9;case EAI_SOCKTYPE:return 10;case EAI_SYSTEM:return 11;
    case EAI_OVERFLOW:return 14;default:return 4;
    }
}
void sif_freeaddrinfo(struct addrinfo *p) {
    while(p){struct addrinfo *next=p->ai_next;free(p->ai_addr);free(p->ai_canonname);free(p);p=next;}
}
int sif_getaddrinfo(const char *node,const char *service,const struct addrinfo *hints,struct addrinfo **result) {
    if(!result)return 12;*result=NULL;
    struct addrinfo h={0},*native=NULL;
    if(hints) {
        h.ai_flags=hints->ai_flags;h.ai_socktype=hints->ai_socktype;h.ai_protocol=hints->ai_protocol;
        h.ai_family=host_family(hints->ai_family);if(h.ai_family<0)return 5;
    }
    int r=getaddrinfo(node,service,hints?&h:NULL,&native);
    if(r){int e=errno;sif_log("net resolver result=%d android=%d",r,android_eai(r));errno=e;return android_eai(r);}
    struct addrinfo **tail=result;
    for(struct addrinfo *p=native;p;p=p->ai_next) {
        unsigned char address[28];int n=sif_address_to_android(p->ai_addr,p->ai_addrlen,address);
        if(n<0){r=5;break;}
        struct addrinfo *q=calloc(1,sizeof(*q));if(!q){r=6;break;}
        *tail=q;tail=&q->ai_next;
        q->ai_flags=p->ai_flags;q->ai_family=p->ai_family==AF_INET?2:10;
        q->ai_socktype=p->ai_socktype;q->ai_protocol=p->ai_protocol;q->ai_addrlen=n;
        q->ai_addr=malloc(n);if(!q->ai_addr){r=6;break;}memcpy(q->ai_addr,address,n);
        if(p->ai_canonname){q->ai_canonname=strdup(p->ai_canonname);if(!q->ai_canonname){r=6;break;}}
    }
    freeaddrinfo(native);
    if(r){sif_freeaddrinfo(*result);*result=NULL;}
    sif_log("net resolver translated result=%d",r);return r;
}
static int socket_option(int *level,int *option) {
    if(*level==1) {
        *level=SOL_SOCKET;
        switch(*option) {
#define OPT(a,b) case a:*option=b;return 0
        OPT(1,SO_DEBUG);OPT(2,SO_REUSEADDR);OPT(3,SO_TYPE);OPT(4,SO_ERROR);
        OPT(5,SO_DONTROUTE);OPT(6,SO_BROADCAST);OPT(7,SO_SNDBUF);OPT(8,SO_RCVBUF);
        OPT(9,SO_KEEPALIVE);OPT(10,SO_OOBINLINE);OPT(13,SO_LINGER);
        OPT(18,SO_RCVLOWAT);OPT(19,SO_SNDLOWAT);OPT(30,SO_ACCEPTCONN);
#undef OPT
        }
    } else if(*level==6) {
        if(*option==1){*option=TCP_NODELAY;return 0;}
        if(*option==2){*option=TCP_MAXSEG;return 0;}
    }
    /* Timeout structs and IP-level options require their own ABI mapping. */
    errno=ENOPROTOOPT;return -1;
}
int sif_setsockopt(int fd,int level,int option,const void *value,socklen_t len) {
    if(socket_option(&level,&option)<0)return -1;
    return setsockopt(fd,level,option,value,len);
}
int sif_getsockopt(int fd,int level,int option,void *value,socklen_t *len) {
    int is_error=level==1 && option==4;
    if(socket_option(&level,&option)<0)return -1;
    int r=getsockopt(fd,level,option,value,len);
    if(is_error)sif_log("net SO_ERROR fd=%d result=%d native=%d",fd,r,r==0 && value?*(int*)value:-1);
    if(!r && is_error && value && len && *len>=sizeof(int)) {
        int e;memcpy(&e,value,sizeof(e));e=errno_to_bionic(e);memcpy(value,&e,sizeof(e));
    }
    return r;
}
static int message_flags(int flags) {
    int allowed=1|2|4|8|0x20|0x40|0x80|0x100|0x4000;
    if(flags&~allowed){errno=EOPNOTSUPP;return -1;}
    int n=flags&7;
    if(flags&8)n|=MSG_CTRUNC;if(flags&0x20)n|=MSG_TRUNC;
    if(flags&0x40)n|=MSG_DONTWAIT;if(flags&0x80)n|=MSG_EOR;if(flags&0x100)n|=MSG_WAITALL;
    /* Vita socket sends do not generate POSIX SIGPIPE, so NOSIGNAL needs no bit. */
    return n;
}
ssize_t sif_send(int fd,const void *p,size_t n,int flags){
    int f=message_flags(flags);ssize_t r=f<0?-1:send(fd,p,n,f);int e=errno;static unsigned logs;
    if(logs++<24)sif_log("net send fd=%d bytes=%u flags=%x result=%d errno=%d",fd,(unsigned)n,flags,(int)r,r<0?e:0);
    errno=e;return r;
}
ssize_t sif_recv(int fd,void *p,size_t n,int flags){
    int f=message_flags(flags);ssize_t r=f<0?-1:recv(fd,p,n,f);int e=errno;static unsigned logs;
    if(logs++<24)sif_log("net recv fd=%d bytes=%u flags=%x result=%d errno=%d",fd,(unsigned)n,flags,(int)r,r<0?e:0);
    errno=e;return r;
}
ssize_t sif_sendto(int fd,const void *p,size_t n,int flags,const void *addr,socklen_t len) {
    int f=message_flags(flags);if(f<0)return -1;
    if(!addr)return sendto(fd,p,n,f,NULL,0);
    _Alignas(4) unsigned char native[32];int size=address_in(addr,len,native);if(size<0)return -1;
    return sendto(fd,p,n,f,(struct sockaddr *)native,size);
}
int sif_inet_pton(int family,const char *src,void *dst){int f=host_family(family);return f<0?-1:inet_pton(f,src,dst);}
const char *sif_inet_ntop(int family,const void *src,char *dst,socklen_t size){int f=host_family(family);return f<0?NULL:inet_ntop(f,src,dst,size);}
