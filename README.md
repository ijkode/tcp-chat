# TCP Chat Server
Event-driven TCP chat server made in C. Concurrency is done with `select` instead of a static thread pool.

## Usage
Compile:
```sh
gcc chatServer.c -o chatServer
```

Run:
```sh
./chatServer <port>
```

port: The port number you want the chat to bind to


After running the server, clients can connect to the port (i.e. with `telnet`) and communicate with each other.