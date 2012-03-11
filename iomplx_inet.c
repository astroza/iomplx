#include <netinet/in.h>
#include <arpa/inet.h>
#include <iomplx.h>
#include <iomplx_inet.h>
#include <unistd.h>

int iomplx_inet_listen(iomplx_instance *mplx, const char *addr, unsigned short port, ev_call1 ev_accept, void *data)
{
	iomplx_item local_item, *item;
	struct sockaddr_in *sa;
	int ret;
	int status = 1;

	local_item.fd = socket(AF_INET, SOCK_STREAM, 0);
        if (local_item.fd == -1 )
                return -1;

	sa = (struct sockaddr_in *)&local_item.sa;
	sa->sin_family = AF_INET;
	sa->sin_port = htons(port);
	sa->sin_addr.s_addr = inet_addr(addr);
	local_item.sa_size = sizeof(struct sockaddr_in);

	if(setsockopt(local_item.fd, SOL_SOCKET, SO_REUSEADDR, &status, sizeof(status)) == -1)
		goto error;

	ret = bind(local_item.fd, &local_item.sa, local_item.sa_size);
	if(ret == -1)
		goto error;

	if(listen(local_item.fd, IOMPLX_CONF_BACKLOG) == -1)
		goto error;

	local_item.data = data;
	local_item.new_filter = IOMPLX_READ;
	iomplx_callbacks_init(&local_item);
	local_item.cb.ev_accept = ev_accept;

	item = iomplx_item_add(mplx, &local_item, 1);
	if(!item)
		goto error;

	return 0;
error:
	close(local_item.fd);
	return -1;
}
