#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mplx3.h>

int demo_receive(mplx3_endpoint *ep)
{
	char buf[64];
	int r;
	puts("READ");
	r = read(ep->sockfd, buf, sizeof(buf)-1);
	buf[r] = 0;
	printf("buf=%s", buf);

	return 0;
}

int demo_timeout(mplx3_endpoint *ep)
{
	puts("TIMEOUT");
	return -1;
}

int demo_disconnect(mplx3_endpoint *ep)
{
	puts("DISCONNECT");
	return 0;
}

int demo_accept(mplx3_endpoint *ep)
{
	ep->cb.ev_receive = demo_receive;
	ep->cb.ev_timeout = demo_timeout;
	ep->cb.ev_disconnect = demo_disconnect;
	ep->timeout = 2;
	return 0;
}

int main()
{
	mplx3_multiplexer m;
	
	mplx3_init(&m, malloc, free, 4, 4);
	mplx3_listen(&m, "0.0.0.0", 2222, demo_accept, NULL);
	mplx3_launch(&m);

	return 0;
}
