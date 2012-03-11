#ifndef __IOMPLX_INET__
#define __IOMPLX_INET__

#define IOMPLX_CONF_BACKLOG     10

int iomplx_inet_listen(iomplx_instance *, const char *, unsigned short, ev_call1, void *);

#endif
