#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iomplx.h>
#include <iomplx_inet.h>

int demo_receive(iomplx_item *item)
{
	char buf[64];
	int r;
	puts("READ");
	r = read(item->fd, buf, sizeof(buf)-1);
	buf[r] = 0;
	printf("buf=%s", buf);

	return 0;
}

int demo_timeout(iomplx_item *item)
{
	puts("TIMEOUT");
	return -1;
}

int demo_disconnect(iomplx_item *item)
{
	puts("DISCONNECT");
	return 0;
}

int demo_accept(iomplx_item *item)
{
	item->cb.ev_read = demo_receive;
	item->cb.ev_timeout = demo_timeout;
	item->cb.ev_close = demo_disconnect;
	item->timeout = 2;
	return 0;
}

int main()
{
	iomplx_instance m;
	
	iomplx_init(&m, malloc, free, NULL, 4, 4);
	iomplx_inet_listen(&m, "0.0.0.0", 2222, demo_accept, NULL);
	iomplx_launch(&m);

	return 0;
}
