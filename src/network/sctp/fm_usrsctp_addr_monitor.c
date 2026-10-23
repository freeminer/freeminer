/*
Copyright (C) 2026 Freeminer contributors
SPDX-License-Identifier: BSD-2-Clause

Included by bundled usrsctp's user_recv_thread.c on Linux. Polling also works
when containers or Android deny routing sockets. No kernel privileges needed.
*/
#include <ifaddrs.h>
#include <net/if.h>
#include <time.h>

struct fm_sctp_interface_address
{
	uint32_t index;
	char name[IF_NAMESIZE];
	struct sockaddr_storage address;
	struct fm_sctp_interface_address *next;
};

static pthread_mutex_t fm_sctp_monitor_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fm_sctp_monitor_cond = PTHREAD_COND_INITIALIZER;
static int fm_sctp_monitor_stop;
static int fm_sctp_monitor_started;

static void fm_sctp_free_addresses(struct fm_sctp_interface_address *entry)
{
	while (entry != NULL) {
		struct fm_sctp_interface_address *next = entry->next;
		free(entry);
		entry = next;
	}
}

static int fm_sctp_collect_addresses(struct fm_sctp_interface_address **result)
{
	struct ifaddrs *interfaces, *ifa;
	struct fm_sctp_interface_address *head = NULL;
	if (getifaddrs(&interfaces) != 0)
		return -1;
	for (ifa = interfaces; ifa != NULL; ifa = ifa->ifa_next) {
		size_t length;
		uint32_t index;
		struct fm_sctp_interface_address *entry;
		if (ifa->ifa_addr == NULL || !(ifa->ifa_flags & IFF_UP))
			continue;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			length = sizeof(struct sockaddr_in);
			break;
#endif
#ifdef INET6
		case AF_INET6:
			length = sizeof(struct sockaddr_in6);
			break;
#endif
		default:
			continue;
		}
		index = if_nametoindex(ifa->ifa_name);
		if (index == 0)
			continue;
		entry = calloc(1, sizeof(*entry));
		if (entry == NULL) {
			freeifaddrs(interfaces);
			fm_sctp_free_addresses(head);
			return -1; /* Never reconcile an incomplete snapshot. */
		}
		entry->index = index;
		strncpy(entry->name, ifa->ifa_name, sizeof(entry->name) - 1);
		memcpy(&entry->address, ifa->ifa_addr, length);
#ifdef INET6
		if (entry->address.ss_family == AF_INET6) {
			struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&entry->address;
			if (IN6_IS_ADDR_LINKLOCAL(&ipv6->sin6_addr) && ipv6->sin6_scope_id == 0)
				ipv6->sin6_scope_id = index;
		}
#endif
		entry->next = head;
		head = entry;
	}
	freeifaddrs(interfaces);
	*result = head;
	return 0;
}

static int fm_sctp_has_address(const struct fm_sctp_interface_address *list,
		const struct fm_sctp_interface_address *entry)
{
	for (; list != NULL; list = list->next) {
		if (list->index != entry->index ||
				list->address.ss_family != entry->address.ss_family)
			continue;
#ifdef INET
		if (entry->address.ss_family == AF_INET &&
				((const struct sockaddr_in *)&list->address)->sin_addr.s_addr ==
						((const struct sockaddr_in *)&entry->address)->sin_addr.s_addr)
			return 1;
#endif
#ifdef INET6
		if (entry->address.ss_family == AF_INET6 &&
				memcmp(&((const struct sockaddr_in6 *)&list->address)->sin6_addr,
						&((const struct sockaddr_in6 *)&entry->address)->sin6_addr,
						sizeof(struct in6_addr)) == 0)
			return 1;
#endif
	}
	return 0;
}

static void *fm_sctp_monitor_addresses(void *unused)
{
	struct fm_sctp_interface_address *previous = NULL;
	(void)unused;
	sctp_userspace_set_threadname("SCTP addr mon");
	for (;;) {
		struct fm_sctp_interface_address *current = NULL, *entry;
		int additions_ok = 1;
		struct timespec deadline;
		pthread_mutex_lock(&fm_sctp_monitor_mutex);
		if (fm_sctp_monitor_stop) {
			pthread_mutex_unlock(&fm_sctp_monitor_mutex);
			break;
		}
		pthread_mutex_unlock(&fm_sctp_monitor_mutex);
		if (fm_sctp_collect_addresses(&current) == 0) {
			/* Add replacements before removing old addresses, allowing handoff. */
			for (entry = current; entry != NULL; entry = entry->next) {
				if (!fm_sctp_has_address(previous, entry) &&
						sctp_add_addr_to_vrf(SCTP_DEFAULT_VRFID, NULL, entry->index, 0,
								entry->name, NULL, (struct sockaddr *)&entry->address, 0,
								1) == NULL) {
					additions_ok = 0;
					break;
				}
			}
			if (additions_ok) {
				for (entry = previous; entry != NULL; entry = entry->next) {
					if (!fm_sctp_has_address(current, entry))
						sctp_del_addr_from_vrf(SCTP_DEFAULT_VRFID,
								(struct sockaddr *)&entry->address, NULL, entry->index);
				}
				fm_sctp_free_addresses(previous);
				previous = current;
			} else {
				// Retain the old snapshot so failed additions are retried.
				fm_sctp_free_addresses(current);
			}
		}
		clock_gettime(CLOCK_REALTIME, &deadline);
		deadline.tv_sec += 1;
		pthread_mutex_lock(&fm_sctp_monitor_mutex);
		while (!fm_sctp_monitor_stop && pthread_cond_timedwait(&fm_sctp_monitor_cond,
												&fm_sctp_monitor_mutex, &deadline) == 0) {
		}
		pthread_mutex_unlock(&fm_sctp_monitor_mutex);
	}
	fm_sctp_free_addresses(previous);
	return NULL;
}

static void fm_sctp_start_address_monitor(void)
{
	int error;
	if (fm_sctp_monitor_started)
		return;
	fm_sctp_monitor_stop = 0;
	error = sctp_userspace_thread_create(
			&SCTP_BASE_VAR(recvthreadroute), &fm_sctp_monitor_addresses);
	if (error == 0)
		fm_sctp_monitor_started = 1;
	else
		fprintf(stderr, "SCTP: cannot start address monitor: %d\n", error);
}

static void fm_sctp_stop_address_monitor(void)
{
	if (!fm_sctp_monitor_started)
		return;
	pthread_mutex_lock(&fm_sctp_monitor_mutex);
	fm_sctp_monitor_stop = 1;
	pthread_cond_signal(&fm_sctp_monitor_cond);
	pthread_mutex_unlock(&fm_sctp_monitor_mutex);
	pthread_join(SCTP_BASE_VAR(recvthreadroute), NULL);
	fm_sctp_monitor_started = 0;
}
