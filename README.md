# ChatServer｜Simple TCP Chat Server

A simple multi-client chat server written in **C**, using **TCP sockets** and `select()` for concurrent connection handling.

This project implements a lightweight IRC-style chat server that supports basic user management, channel operations, topic settings, and channel messaging.

## Overview

ChatServer is a console-based chat server built for networking programming practice.

The server accepts multiple client connections and processes text-based commands from connected users. It supports nickname registration, online user listing, channel creation and joining, channel topic management, and basic message broadcasting.

This project demonstrates the core ideas of network programming, including:

- TCP socket communication
- multi-client handling with `select()`
- command parsing
- simple chat protocol design
- server-side state management

## Features

- Multi-client TCP server
- Concurrent connection handling with `select()`
- Nickname registration
- Online user listing
- Channel creation
- Channel listing
- Join / leave channel support
- Channel topic query and update
- Channel messaging with `PRIVMSG`
- Client quit / disconnect handling
- IRC-style numeric reply messages

## Supported Commands

### `NICK <nickname>`
Set or change the user's nickname.

Example:
```text
NICK Alice
```

### `USER <anything>`
Complete user registration and receive welcome messages.

Example:
```text
USER Alice
```

### `USERS`
List all currently connected users and their IP addresses.

Example:
```text
USERS
```

### `LIST`
List all available channels.

Example:
```text
LIST
```

### `JOIN <channel>`
Join a channel.  
If the channel does not exist, it will be created automatically.

Example:
```text
JOIN general
```

### `PART <channel>`
Leave a channel.

Example:
```text
PART general
```

### `TOPIC <channel>`
Show the current topic of a channel.

Example:
```text
TOPIC general
```

### `TOPIC <channel> :<topic text>`
Set the topic of a channel.

Example:
```text
TOPIC general :Welcome to the room
```

### `PRIVMSG <channel> :<message>`
Send a message to a channel.

Example:
```text
PRIVMSG general :Hello everyone
```

### `QUIT`
Disconnect from the server.

Example:
```text
QUIT
```

## File Structure

```text
ChatServer/
├── main.c
└── README.md
```

## Build

Compile the program with `gcc`:

```bash
gcc -o chatserver main.c
```

## Run

Start the server by providing a port number:

```bash
./chatserver 8080
```

If the server starts successfully, it will listen for incoming client connections on the specified port.

## How to Test

You can test the server using **telnet** or **netcat** from another terminal.

### Using telnet

```bash
telnet 127.0.0.1 8080
```

### Using netcat

```bash
nc 127.0.0.1 8080
```

After connecting, try the following commands:

```text
NICK Alice
USER Alice
JOIN general
PRIVMSG general :Hello everyone
LIST
USERS
```

To test multi-user behavior, open multiple terminal windows and connect several clients at the same time.

## Example Workflow

Client 1:
```text
NICK Alice
USER Alice
JOIN general
TOPIC general :Welcome to the room
PRIVMSG general :Hello everyone
```

Client 2:
```text
NICK Bob
USER Bob
JOIN general
PRIVMSG general :Hi Alice
```

## Implementation Details

This project is implemented in C and uses standard socket programming functions such as:

- `socket`
- `bind`
- `listen`
- `accept`
- `recv`
- `send`
- `select`

The server maintains internal data structures for:

- connected clients
- nicknames
- IP addresses
- channels
- channel topics
- channel user counts

## Limitations

This is a lightweight educational project, so it currently has several limitations:

- no authentication or password system
- no encryption
- no persistent storage or chat history
- no separate client application
- channel membership is handled in a simplified way
- `PRIVMSG` behavior is simplified and is not a full IRC implementation
