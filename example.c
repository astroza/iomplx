#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iomplx.h>
#include <iomplx_inet.h>

struct echo_buffer
{
	char buf[64];
	unsigned len;
};

int demo_receive(iomplx_item *item)
{
	struct echo_buffer *eb = item->data;

	eb->len = recv(item->fd, eb->buf, sizeof(eb->buf), 0);
	if(eb->len == -1)
		return IOMPLX_ITEM_WOULDBLOCK;

	puts("RECEIVE");
	iomplx_item_filter_set(item, IOMPLX_WRITE);

	return 0;
}

int demo_send(iomplx_item *item)
{
	struct echo_buffer *eb = item->data;
	int r;

	r = send(item->fd, eb->buf, eb->len, 0);
	if(r == -1)
		return IOMPLX_ITEM_WOULDBLOCK;

	iomplx_item_filter_set(item, IOMPLX_READ);
	return 0;
}

int demo_timeout(iomplx_item *item)
{
	puts("TIMEOUT");
	return -1;
}

int demo_disconnect(iomplx_item *item)
{
	free(item->data);
	puts("DISCONNECT");
	return 0;
}

int demo_accept(iomplx_item *item)
{
	item->cb.ev_read = demo_receive;
	item->cb.ev_write = demo_send;
	item->cb.ev_timeout = demo_timeout;
	item->cb.ev_close = demo_disconnect;
	item->timeout = 2;
	item->data = malloc(sizeof(struct echo_buffer));
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
