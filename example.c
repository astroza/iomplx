#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <iomplx.h>
#include <iomplx_inet.h>

struct echo_buffer
{
	char buf[64];
	int len;
};

int demo_receive(iomplx_item *item)
{
	struct echo_buffer *eb = item->data;

	printf("thread=%lu> enter recv: item->fd=%d\n", (unsigned long)pthread_self(), item->fd);
	eb->len = recv(item->fd, eb->buf, sizeof(eb->buf), 0);
	printf("thread=%lu> exit recv: item->fd=%d eb->len=%d\n", (unsigned long)pthread_self(), item->fd, eb->len);
	if(eb->len <= 0)
		return IOMPLX_ITEM_WOULDBLOCK;

	iomplx_item_filter_set(item, IOMPLX_WRITE);

	return 0;
}

int demo_send(iomplx_item *item)
{
	struct echo_buffer *eb = item->data;
	int r;

	printf("thread=%lu> enter send item->fd=%d eb->buf=%p eb->len=%d\n", (unsigned long)pthread_self(), item->fd, eb->buf, eb->len);
	r = send(item->fd, eb->buf, eb->len, 0);
	printf("thread=%lu> exit send item->fd=%d\n", (unsigned long)pthread_self(), item->fd);
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
	printf("DISCONNECT item->fd=%d\n", item->fd);
	return 0;
}

int demo_accept(iomplx_item *item)
{
	item->cb.ev_read = demo_receive;
	item->cb.ev_write = demo_send;
	item->cb.ev_timeout = demo_timeout;
	item->cb.ev_close = demo_disconnect;
	item->timeout = 3;
	item->data = malloc(sizeof(struct echo_buffer));
	puts("accept");
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
