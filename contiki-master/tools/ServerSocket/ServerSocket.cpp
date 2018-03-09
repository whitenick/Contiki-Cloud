#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string>

static std::string serverIP = "127.0.0.1";
static int port = 60001;



int main (int argc, char* argv[])
{
	int server_handle;

	server_handle = socket(AF_INET, SOCK_STREAM, 0);

	if (server_handle < 0)
	{
		printf("Error\n");
		return -1;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = port;

	int result;

	result = bind(server_handle, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if (result < 0)
	{
		printf("Bind error\n");
		return -1;
	}

	result = listen(server_handle, SOMAXCONN);

	while (1)
	{
		int new_socket = accept(server_handle, (struct sockaddr*)&serverAddr, INADDR_ANY);
		int valRead = read(new_socket, buffer, 1024);
		printf("%s\n", buffer);

	}




	return 0;
}
