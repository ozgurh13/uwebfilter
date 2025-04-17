#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "utils.h"

noreturn void
die(const char* restrict fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");

	exit(EXIT_FAILURE);
}




#define   MASK1(value)      ((value & 0xFF000000) >> 24)
#define   MASK2(value)      ((value & 0x00FF0000) >> 16)
#define   MASK3(value)      ((value & 0x0000FF00) >>  8)
#define   MASK4(value)      ((value & 0x000000FF)      )
static int
uint_to_string(char *ipaddr, uint32_t u32_ip, int index)
{
	if (u32_ip > 99) {
		int u32_ip_1 = u32_ip % 10;
		u32_ip /= 10;
		int u32_ip_10 = u32_ip % 10;
		u32_ip /= 10;
		int u32_ip_100 = u32_ip % 10;

		ipaddr[index++] = u32_ip_100 + '0';
		ipaddr[index++] = u32_ip_10 + '0';
		ipaddr[index++] = u32_ip_1 + '0';
	}
	else if (u32_ip > 9){
		int u32_ip_1 = u32_ip % 10;
		u32_ip /= 10;
		int u32_ip_10 = u32_ip % 10;

		ipaddr[index++] = u32_ip_10 + '0';
		ipaddr[index++] = u32_ip_1 + '0';
	}
	else {
		int u32_ip_1 = u32_ip % 10;

		ipaddr[index++] = u32_ip_1 + '0';
	}

	ipaddr[index++] = '.';
	return index;
}

void
ipaddr_uint32_to_string(char ipaddr[IPADDR_LENGTH], uint32_t u32_ipaddr)
{
	uint32_t u32_ip1 = MASK1(u32_ipaddr);
	uint32_t u32_ip2 = MASK2(u32_ipaddr);
	uint32_t u32_ip3 = MASK3(u32_ipaddr);
	uint32_t u32_ip4 = MASK4(u32_ipaddr);

	int index = 0;

	index = uint_to_string(ipaddr, u32_ip4, index);
	index = uint_to_string(ipaddr, u32_ip3, index);
	index = uint_to_string(ipaddr, u32_ip2, index);
	index = uint_to_string(ipaddr, u32_ip1, index);
	ipaddr[index - 1] = '\0';
}
