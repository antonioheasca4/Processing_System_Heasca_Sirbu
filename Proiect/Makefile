all: server agent client

server: server.c
	gcc server.c -o server

agent: agent.c
	gcc agent.c -o agent

client: client.c
	gcc client.c -o client

clean:
	rm -f server agent client
