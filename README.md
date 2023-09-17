# Multithreaded Database Server in C
A multithreaded database server written in C

## Build Instructions:
To build both the server and the client, simply run `make`.

`make dbserver` to build the server.
`make dbclient` to build the client.

## Usage Instructions
1. Open a terminal on the server machine and run `./dbserver port`
2. Open another terminal on the client machine and run `./dbclient hostname port`

The server will create a new thread for each client that connects.
