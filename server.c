/*
 * Copyright (C) 2016 Ingemar Ådahl
 *
 * This file is part of remote-input.
 *
 * remote-input is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * remote-input is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with remote-input.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "server.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "logging.h"


int server_create(const char* local_ip, uint16_t port) {
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%u", port);

    struct addrinfo bind_hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };

    struct addrinfo* local_addrs;
    int res = getaddrinfo(local_ip, port_str, &bind_hints, &local_addrs);
    if (res != 0) {
        LOG(ERROR, "getaddrinfo: %s", gai_strerror(res));
        return -1;
    }

    int socket_fd = -1;
    struct addrinfo* addr = local_addrs;
    do {
        socket_fd = socket(addr->ai_family, addr->ai_socktype,
                addr->ai_protocol);
    } while (socket_fd < 0 && (addr = addr->ai_next));

    if (socket_fd < 0) {
        LOG_ERRNO("socket error");
        freeaddrinfo(local_addrs);
        return -1;
    }

    if (bind(socket_fd, addr->ai_addr, addr->ai_addrlen) < 0) {
        LOG_ERRNO("bind error");
        goto cleanup;
    }

    void* bound_address;
    uint16_t bound_port;
    if (addr->ai_family == AF_INET) {
        struct sockaddr_in* ipv4_addr = (struct sockaddr_in*)addr->ai_addr;
        bound_address = &(ipv4_addr->sin_addr);
        bound_port = ipv4_addr->sin_port;

    } else {
        struct sockaddr_in6* ipv6_addr = (struct sockaddr_in6*)addr->ai_addr;
        bound_address = &(ipv6_addr->sin6_addr);
        bound_port = ipv6_addr->sin6_port;
    }

    char bound_ip[INET6_ADDRSTRLEN];
    inet_ntop(addr->ai_family, bound_address, bound_ip, sizeof(bound_ip));
    LOG(INFO, "listening for connections on %s:%d", bound_ip,
            ntohs(bound_port));
    if (listen(socket_fd, 1) < 0) {
        LOG_ERRNO("listen error");
        goto cleanup;
    }

    freeaddrinfo(local_addrs);

    return socket_fd;

cleanup:
    freeaddrinfo(local_addrs);
    close(socket_fd);
    return -1;
}

void server_close(int server_fd) {
    close(server_fd);
}

int server_accept(int server_fd) {
    // TODO: Not neccecarily a sockaddr_in, could be sockaddr_in6
    struct sockaddr_in client_in_addr;
    struct sockaddr* client_addr = (struct sockaddr*)&client_in_addr;
    socklen_t in_addr_len = sizeof(client_in_addr);

    int client_fd = accept(server_fd, client_addr, &in_addr_len);
    if (client_fd < 0) {
        if (errno != EINTR) {
            LOG_ERRNO("accept error");
        }
        return -1;
    }

    LOG(INFO, "accepted connection from %s",
            inet_ntoa(client_in_addr.sin_addr));

    return client_fd;
}

int read_client_event(int client_fd, struct client_event* event) {
    int read_length = read(client_fd, event, sizeof(struct client_event));
    if (read_length < 0) {
        LOG_ERRNO("error reading from client");
        return -1;
    }

    if (read_length < sizeof(struct client_event)) {
        return 0;
    }

    return read_length;
}