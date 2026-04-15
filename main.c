#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_CLIENTS   1024
#define MAX_CHANNELS  20
#define MAX_NAME_LEN  20
#define MAX_IP_LEN    16
#define BUF_SIZE      1024
#define MSG_SIZE      256

typedef struct {
    char nickname[MAX_NAME_LEN];
    char ip[MAX_IP_LEN];
    int active;
} ClientInfo;

typedef struct {
    char name[MAX_NAME_LEN];
    char topic[MAX_NAME_LEN];
    int user_count;
} ChannelInfo;

static ClientInfo clients[MAX_CLIENTS];
static ChannelInfo channels[MAX_CHANNELS];
static int channel_count = 0;
static int client_count = 0;

static void send_text(int fd, const char *text) {
    send(fd, text, strlen(text), 0);
}

static void send_reply(int fd, const char *fmt, const char *nick,
                       const char *arg1, const char *arg2) {
    char msg[MSG_SIZE];
    snprintf(msg, sizeof(msg), fmt,
             nick ? nick : "",
             arg1 ? arg1 : "",
             arg2 ? arg2 : "");
    send_text(fd, msg);
}

static int find_channel(const char *name) {
    for (int i = 0; i < channel_count; i++) {
        if (strcmp(channels[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int add_channel(const char *name) {
    if (channel_count >= MAX_CHANNELS) {
        return -1;
    }
    strncpy(channels[channel_count].name, name, MAX_NAME_LEN - 1);
    channels[channel_count].name[MAX_NAME_LEN - 1] = '\0';
    channels[channel_count].topic[0] = '\0';
    channels[channel_count].user_count = 0;
    return channel_count++;
}

static int nickname_exists(const char *name, int listener, fd_set *master, int fdmax) {
    for (int j = 0; j <= fdmax; j++) {
        if (FD_ISSET(j, master) && j != listener) {
            if (strcmp(clients[j].nickname, name) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static void trim_crlf(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

static int setup_listener(const char *port) {
    struct addrinfo hints, *ai, *p;
    int listener = -1;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            listener = -1;
            continue;
        }
        break;
    }

    freeaddrinfo(ai);

    if (listener < 0) {
        fprintf(stderr, "failed to bind\n");
        exit(2);
    }

    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    return listener;
}

static void handle_new_connection(int listener, fd_set *master, int *fdmax) {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    int newfd = accept(listener, (struct sockaddr *)&client_addr, &addrlen);
    if (newfd == -1) {
        perror("accept");
        return;
    }

    FD_SET(newfd, master);
    if (newfd > *fdmax) {
        *fdmax = newfd;
    }

    clients[newfd].active = 1;
    strncpy(clients[newfd].ip, inet_ntoa(client_addr.sin_addr), MAX_IP_LEN - 1);
    clients[newfd].ip[MAX_IP_LEN - 1] = '\0';
    clients[newfd].nickname[0] = '\0';

    client_count++;
}

static void handle_nick(int fd, char *buf, int listener, fd_set *master, int fdmax) {
    if (buf[5] == '\0') {
        send_text(fd, ":chat 431 :No nickname given\n");
        return;
    }

    char temp_name[MAX_NAME_LEN] = {0};
    strncpy(temp_name, buf + 5, MAX_NAME_LEN - 1);
    trim_crlf(temp_name);

    if (nickname_exists(temp_name, listener, master, fdmax)) {
        char msg[MSG_SIZE];
        snprintf(msg, sizeof(msg), ":chat 436 %s :Nickname collision KILL\n", temp_name);
        send_text(fd, msg);
        return;
    }

    strncpy(clients[fd].nickname, temp_name, MAX_NAME_LEN - 1);
    clients[fd].nickname[MAX_NAME_LEN - 1] = '\0';
}

static void handle_users(int fd, int listener, fd_set *master, int fdmax) {
    char msg[MSG_SIZE];

    snprintf(msg, sizeof(msg), ":chat 392 %s :UserID                   Terminal  Host\n",
             clients[fd].nickname);
    send_text(fd, msg);

    for (int j = 0; j <= fdmax; j++) {
        if (FD_ISSET(j, master) && j != listener) {
            snprintf(msg, sizeof(msg), ":chat 393 %s :%-25s-         %s\n",
                     clients[fd].nickname,
                     clients[j].nickname,
                     clients[j].ip);
            send_text(fd, msg);
        }
    }

    snprintf(msg, sizeof(msg), ":chat 394 %s :End of users\n", clients[fd].nickname);
    send_text(fd, msg);
}

static void handle_user(int fd, char *buf) {
    if (buf[4] != ' ') {
        trim_crlf(buf);
        char msg[MSG_SIZE];
        snprintf(msg, sizeof(msg), ":chat 461 %s %s :Not enough parameters\n",
                 clients[fd].nickname, buf);
        send_text(fd, msg);
        return;
    }

    char msg[MSG_SIZE];

    snprintf(msg, sizeof(msg), ":chat 001 %s :Welcome to the chat project chat room\n",
             clients[fd].nickname);
    send_text(fd, msg);

    snprintf(msg, sizeof(msg), ":chat 251 %s :There are %d users online now\n",
             clients[fd].nickname, client_count);
    send_text(fd, msg);

    snprintf(msg, sizeof(msg), ":chat 375 %s :- chat Message of the day -\n",
             clients[fd].nickname);
    send_text(fd, msg);

    snprintf(msg, sizeof(msg), ":chat 372 %s :- Hello, World!\n",
             clients[fd].nickname);
    send_text(fd, msg);

    snprintf(msg, sizeof(msg), ":chat 376 %s :End of message of the day\n",
             clients[fd].nickname);
    send_text(fd, msg);
}

static void handle_list(int fd) {
    char msg[MSG_SIZE];

    snprintf(msg, sizeof(msg), ":chat 321 %s :Channel  Users  Name\n",
             clients[fd].nickname);
    send_text(fd, msg);

    for (int i = 0; i < channel_count; i++) {
        snprintf(msg, sizeof(msg), ":chat 322 %s #%s %d :%s\n",
                 clients[fd].nickname,
                 channels[i].name,
                 channels[i].user_count,
                 channels[i].topic);
        send_text(fd, msg);
    }

    snprintf(msg, sizeof(msg), ":chat 323 %s :End of /LIST\n",
             clients[fd].nickname);
    send_text(fd, msg);
}

static void handle_join(int fd, char *buf) {
    char channel[MAX_NAME_LEN] = {0};
    strncpy(channel, buf + 6, MAX_NAME_LEN - 1);
    trim_crlf(channel);

    int idx = find_channel(channel);
    if (idx < 0) {
        idx = add_channel(channel);
        if (idx < 0) {
            return;
        }
    }

    channels[idx].user_count++;

    char msg[MSG_SIZE];
    snprintf(msg, sizeof(msg), ":%s JOIN #%s\n", clients[fd].nickname, channel);
    send_text(fd, msg);
}

static void handle_part(int fd, char *buf) {
    char channel[MAX_NAME_LEN] = {0};
    int k = 0;

    while (buf[6 + k] != '\0' && buf[6 + k] != ':' && k < MAX_NAME_LEN - 1) {
        channel[k] = buf[6 + k];
        k++;
    }
    trim_crlf(channel);

    int idx = find_channel(channel);
    if (idx < 0) {
        char msg[MSG_SIZE];
        snprintf(msg, sizeof(msg), ":chat 403 %s #%s :No such channel\n",
                 clients[fd].nickname, channel);
        send_text(fd, msg);
        return;
    }

    if (channels[idx].user_count > 0) {
        channels[idx].user_count--;
    }

    char msg[MSG_SIZE];
    snprintf(msg, sizeof(msg), ":%s PART :#%s\n", clients[fd].nickname, channel);
    send_text(fd, msg);
}

static void handle_topic(int fd, char *buf) {
    char channel[MAX_NAME_LEN] = {0};
    int pos = 0;
    int set_topic = 0;

    while (buf[7 + pos] != '\0' && pos < MAX_NAME_LEN - 1) {
        if (buf[7 + pos] == ':') {
            set_topic = 1;
            break;
        }
        channel[pos] = buf[7 + pos];
        pos++;
    }
    trim_crlf(channel);

    int idx = find_channel(channel);
    if (idx < 0) {
        return;
    }

    char msg[MSG_SIZE];

    if (set_topic) {
        char *topic = buf + 8 + pos;
        trim_crlf(topic);

        strncpy(channels[idx].topic, topic, MAX_NAME_LEN - 1);
        channels[idx].topic[MAX_NAME_LEN - 1] = '\0';

        snprintf(msg, sizeof(msg), ":chat 332 %s #%s :%s\n",
                 clients[fd].nickname,
                 channels[idx].name,
                 channels[idx].topic);
    } else {
        if (channels[idx].topic[0] == '\0') {
            snprintf(msg, sizeof(msg), ":chat 331 %s #%s :No topic is set\n",
                     clients[fd].nickname,
                     channels[idx].name);
        } else {
            snprintf(msg, sizeof(msg), ":chat 332 %s #%s :%s\n",
                     clients[fd].nickname,
                     channels[idx].name,
                     channels[idx].topic);
        }
    }

    send_text(fd, msg);
}

static void handle_privmsg(int fd, char *buf, int listener, fd_set *master, int fdmax) {
    if (buf[9] == '\0') {
        char msg[MSG_SIZE];
        snprintf(msg, sizeof(msg), ":chat 411 %s :No recipient given (PRIVMSG)\n",
                 clients[fd].nickname);
        send_text(fd, msg);
        return;
    }

    char channel[MAX_NAME_LEN] = {0};
    int k = 0;

    while (buf[9 + k] != '\0' && buf[9 + k] != ':' && k < MAX_NAME_LEN - 1) {
        channel[k] = buf[9 + k];
        k++;
    }

    if (k > 0 && channel[k - 1] == ' ') {
        channel[k - 1] = '\0';
    }
    trim_crlf(channel);

    int idx = find_channel(channel);
    if (idx < 0) {
        char msg[MSG_SIZE];
        snprintf(msg, sizeof(msg), ":chat 401 %s #%s :No such nick/channel\n",
                 clients[fd].nickname, channel);
        send_text(fd, msg);
        return;
    }

    char *text = strchr(buf, ':');
    if (text == NULL || *(text + 1) == '\0') {
        char msg[MSG_SIZE];
        snprintf(msg, sizeof(msg), ":chat 412 %s :No text to send\n",
                 clients[fd].nickname);
        send_text(fd, msg);
        return;
    }
    text++;
    trim_crlf(text);

    char msg[MSG_SIZE];
    snprintf(msg, sizeof(msg), ":%s PRIVMSG #%s :%s\n",
             clients[fd].nickname, channel, text);

    for (int j = 0; j <= fdmax; j++) {
        if (FD_ISSET(j, master) && j != listener && j != fd) {
            send_text(j, msg);
        }
    }
}

static void handle_quit(int fd, fd_set *master) {
    close(fd);
    FD_CLR(fd, master);
    clients[fd].active = 0;
    client_count--;
}

static void handle_client_message(int fd, int listener, fd_set *master, int fdmax) {
    char buf[BUF_SIZE] = {0};
    int nbytes = recv(fd, buf, sizeof(buf) - 1, 0);

    if (nbytes <= 0) {
        handle_quit(fd, master);
        return;
    }

    buf[nbytes] = '\0';
    printf("receive message: %s", buf);

    if (strncmp(buf, "NICK", 4) == 0) {
        handle_nick(fd, buf, listener, master, fdmax);
    } else if (strncmp(buf, "USERS", 5) == 0) {
        handle_users(fd, listener, master, fdmax);
    } else if (strncmp(buf, "USER", 4) == 0) {
        handle_user(fd, buf);
    } else if (strncmp(buf, "QUIT", 4) == 0) {
        handle_quit(fd, master);
    } else if (strncmp(buf, "LIST", 4) == 0) {
        handle_list(fd);
    } else if (strncmp(buf, "JOIN", 4) == 0) {
        handle_join(fd, buf);
    } else if (strncmp(buf, "PART", 4) == 0) {
        handle_part(fd, buf);
    } else if (strncmp(buf, "TOPIC", 5) == 0) {
        handle_topic(fd, buf);
    } else if (strncmp(buf, "PRIVMSG", 7) == 0) {
        handle_privmsg(fd, buf, listener, master, fdmax);
    } else {
        trim_crlf(buf);
        char msg[MSG_SIZE];
        snprintf(msg, sizeof(msg), ":chat 421 %s %s :Unknown command\n",
                 clients[fd].nickname, buf);
        send_text(fd, msg);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    fd_set master, read_fds;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    int listener = setup_listener(argv[1]);
    FD_SET(listener, &master);

    int fdmax = listener;

    while (1) {
        read_fds = master;

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        for (int i = 0; i <= fdmax; i++) {
            if (!FD_ISSET(i, &read_fds)) {
                continue;
            }

            if (i == listener) {
                handle_new_connection(listener, &master, &fdmax);
            } else {
                handle_client_message(i, listener, &master, fdmax);
            }
        }
    }

    return 0;
}
