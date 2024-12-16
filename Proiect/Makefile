all: server agent client

server: server.c
	gcc -g server.c -o server

agent: agent.c
	gcc -g agent.c -o agent

client: client.c
	gcc -g client.c -o client

clean:
	rm -f server agent client
	rm file_*
	rm task_file*
