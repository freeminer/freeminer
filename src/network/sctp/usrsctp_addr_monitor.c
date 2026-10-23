/*
Copyright (C) 2026 Freeminer contributors
SPDX-License-Identifier: BSD-2-Clause

Included by bundled usrsctp's user_recv_thread.c on Linux. Polling also works
when containers or Android deny routing sockets. No kernel privileges needed.
*/
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <time.h>

/* Registered once by the application before usrsctp_init starts any threads. */
static void (*sctp_diagnostic_logger)(int, const char *);

void sctp_set_diagnostic_logger(void (*logger)(int, const char *))
{
	sctp_diagnostic_logger = logger;
}

void sctp_diagnostic(int warning, const char *message)
{
	if (sctp_diagnostic_logger != NULL)
		sctp_diagnostic_logger(warning, message);
	else
		fprintf(stderr, "SCTP: %s\n", message);
}

struct sctp_interface_address
{
	uint32_t index;
	char name[IF_NAMESIZE];
	struct sockaddr_storage address;
	struct sctp_interface_address *next;
};

static void sctp_log_interface(
		const char *event, const struct sctp_interface_address *entry)
{
	char address[INET6_ADDRSTRLEN] = "unknown";
	char message[256];
#ifdef INET
	if (entry->address.ss_family == AF_INET)
		inet_ntop(AF_INET, &((const struct sockaddr_in *)&entry->address)->sin_addr,
				address, sizeof(address));
#endif
#ifdef INET6
	if (entry->address.ss_family == AF_INET6)
		inet_ntop(AF_INET6, &((const struct sockaddr_in6 *)&entry->address)->sin6_addr,
				address, sizeof(address));
#endif
	snprintf(message, sizeof(message),
			"local address %s interface=%s index=%u family=%s address=%s", event,
			entry->name, (unsigned)entry->index,
			entry->address.ss_family == AF_INET ? "IPv4" : "IPv6", address);
	sctp_diagnostic(strcmp(event, "registration failed") == 0, message);
}

static pthread_mutex_t sctp_monitor_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sctp_monitor_cond = PTHREAD_COND_INITIALIZER;
static int sctp_monitor_stop;
static int sctp_monitor_started;

static void sctp_free_addresses(struct sctp_interface_address *entry)
{
	while (entry != NULL) {
		struct sctp_interface_address *next = entry->next;
		free(entry);
		entry = next;
	}
}

static int sctp_collect_addresses(struct sctp_interface_address **result)
{
	struct ifaddrs *interfaces, *ifa;
	struct sctp_interface_address *head = NULL;
	if (getifaddrs(&interfaces) != 0)
		return -1;
	for (ifa = interfaces; ifa != NULL; ifa = ifa->ifa_next) {
		size_t length;
		uint32_t index;
		struct sctp_interface_address *entry;
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
			sctp_free_addresses(head);
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

static int sctp_has_address(const struct sctp_interface_address *list,
		const struct sctp_interface_address *entry)
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

static void *sctp_monitor_addresses(void *unused)
{
	struct sctp_interface_address *previous = NULL;
	int initialized = 0, snapshot_failed = 0;
	(void)unused;
	sctp_userspace_set_threadname("SCTP addr mon");
	for (;;) {
		struct sctp_interface_address *current = NULL, *entry;
		int additions_ok = 1;
		struct timespec deadline;
		pthread_mutex_lock(&sctp_monitor_mutex);
		if (sctp_monitor_stop) {
			pthread_mutex_unlock(&sctp_monitor_mutex);
			break;
		}
		pthread_mutex_unlock(&sctp_monitor_mutex);
		if (sctp_collect_addresses(&current) == 0) {
			if (snapshot_failed)
				sctp_diagnostic(0, "local address monitoring recovered");
			snapshot_failed = 0;
			/* Add replacements before removing old addresses, allowing handoff. */
			for (entry = current; entry != NULL; entry = entry->next) {
				if (sctp_has_address(previous, entry))
					continue;
				if (sctp_add_addr_to_vrf(SCTP_DEFAULT_VRFID, NULL, entry->index, 0,
							entry->name, NULL, (struct sockaddr *)&entry->address, 0,
							1) == NULL) {
					sctp_log_interface("registration failed", entry);
					additions_ok = 0;
					break;
				}
				sctp_log_interface(initialized ? "added" : "initial", entry);
			}
			if (additions_ok) {
				for (entry = previous; entry != NULL; entry = entry->next) {
					if (!sctp_has_address(current, entry)) {
						sctp_log_interface("removed", entry);
						sctp_del_addr_from_vrf(SCTP_DEFAULT_VRFID,
								(struct sockaddr *)&entry->address, NULL, entry->index);
					}
				}
				sctp_free_addresses(previous);
				previous = current;
				initialized = 1;
			} else {
				// Retain the old snapshot so failed additions are retried.
				sctp_free_addresses(current);
			}
		} else if (!snapshot_failed) {
			sctp_diagnostic(1, "cannot enumerate local addresses; monitoring will retry");
			snapshot_failed = 1;
		}
		clock_gettime(CLOCK_REALTIME, &deadline);
		deadline.tv_sec += 1;
		pthread_mutex_lock(&sctp_monitor_mutex);
		while (!sctp_monitor_stop && pthread_cond_timedwait(&sctp_monitor_cond,
											 &sctp_monitor_mutex, &deadline) == 0) {
		}
		pthread_mutex_unlock(&sctp_monitor_mutex);
	}
	sctp_free_addresses(previous);
	return NULL;
}

static void sctp_start_address_monitor(void)
{
	int error;
	if (sctp_monitor_started)
		return;
	sctp_monitor_stop = 0;
	error = sctp_userspace_thread_create(
			&SCTP_BASE_VAR(recvthreadroute), &sctp_monitor_addresses);
	if (error == 0)
		sctp_monitor_started = 1;
	else {
		char message[96];
		snprintf(message, sizeof(message), "cannot start address monitor: %d", error);
		sctp_diagnostic(1, message);
	}
}

static void sctp_stop_address_monitor(void)
{
	if (!sctp_monitor_started)
		return;
	pthread_mutex_lock(&sctp_monitor_mutex);
	sctp_monitor_stop = 1;
	pthread_cond_signal(&sctp_monitor_cond);
	pthread_mutex_unlock(&sctp_monitor_mutex);
	pthread_join(SCTP_BASE_VAR(recvthreadroute), NULL);
	sctp_monitor_started = 0;
}
