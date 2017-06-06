#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include "../include/dropboxClient.h"
#include "../include/dropboxUtil.h"

char userid[MAXNAME];
char directory[MAXNAME + 50] = "/home/";
int sockfd = -1;
int notifyfd;
int watchfd;

void initializeNotifyDescription()
{
	notifyfd = inotify_init();

	watchfd = inotify_add_watch(notifyfd, ".", IN_MODIFY);
}

int connect_server (char *host, int port)
{
	int byteCount, connected;
	struct sockaddr_in server_addr;
	struct hostent *server;
	char buffer[256];

	server = gethostbyname(host);

	if (server == NULL)
	{
  	printf("ERROR, no such host\n");
  	return -1;
  }

	// tenta abrir o socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("ERROR opening socket\n");
		return -1;
	}

	// inicializa server_addr
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)server->h_addr);

	bzero(&(server_addr.sin_zero), 8);

	// tenta conectar ao socket
	if (connect(sockfd,(struct sockaddr *) &server_addr,sizeof(server_addr)) < 0)
	{
  		printf("ERROR connecting\n");
		  return -1;
	}

	// envia userid para o servidor
	byteCount = write(sockfd, userid, sizeof(userid));

	if (byteCount < 0)
	{
		printf("ERROR sending userid to server\n");
		return -1;
	}

	// envia userid para o servidor
	byteCount = read(sockfd, &connected, sizeof(int));

	if (byteCount < 0)
	{
		printf("ERROR receiving connected message\n");
		return -1;
	}
	else if (connected == 1)
	{
		printf("connected\n");
		return 1;
	}
	else
	{
		printf("You already have two devices connected\n");
		return -1;
	}
}

void main(int argc, char *argv[])
{
	int port;
	char *host;

	if (argc < 3)
	{
		printf("Insufficient arguments\n");
		exit(0);
	}

	// primeiro argumento nome do usuário
	if (strlen(argv[1]) <= MAXNAME)
		strcpy(userid, argv[1]);

	// segundo argumento host
	host = malloc(sizeof(argv[2]));
	strcpy(host, argv[2]);

	// terceiro argumento porta
	port = atoi(argv[3]);

	initializeNotifyDescription();

	// tenta conectar ao servidor
	if ((connect_server(host, port)) > 0)
	{
		// sincroniza diretório do servidor com o do cliente
		sync_client_first();

		// espera por um comando de usuário
		client_interface();
	}
}

void *sync_thread()
{

}

void sync_client_first()
{
	char *user = getenv("USER");
	char fileName[MAXNAME + 10] = "sync_dir_";
	pthread_t syn_th;

	if(user == NULL)
	{
		printf("ERROR getting current user\n");
		exit(0);
	}

	// nome do arquivo
	strcat(fileName, userid);

	// forma o path do arquivo
	strcat(directory, user);
	strcat(directory, "/");
	strcat(directory, fileName);

	if (mkdir(directory, 0777) < 0)
	{
		// erro
		if (errno != EEXIST)
			printf("ERROR creating directory\n");
	}
	// diretório não existe
	else
	{
		printf("Creating %s directory in your home\n", fileName);
	}

	// cria thread para atender o cliente
	if(pthread_create(&syn_th, NULL, sync_thread, NULL))
	{
		printf("ERROR creating thread\n");
	}
}

void get_file(char *file)
{
	int byteCount, bytesLeft, fileSize;
	struct client_request clientRequest;
	FILE* ptrfile;
	char dataBuffer[KBYTE];

	// copia nome do arquivo e comando para enviar para o servidor
	strcpy(clientRequest.file, file);
	clientRequest.command = DOWNLOAD;

	// avisa servidor que será feito um download
	byteCount = write(sockfd, &clientRequest, sizeof(clientRequest));
	if (byteCount < 0)
		printf("Error sending DOWNLOAD message to server\n");

	// lê estrutura do arquivo que será lido do servidor
	byteCount = read(sockfd, &fileSize, sizeof(fileSize));
	if (byteCount < 0)
		printf("Error receiving filesize\n");

	if (fileSize < 0)
	{
		printf("The file doesn't exist\n\n\n");
		return;
	}
	// cria arquivo no diretório do cliente
	ptrfile = fopen(file, "wb");

	// número de bytes que faltam ser lidos
	bytesLeft = fileSize;

	while(bytesLeft > 0)
	{
		// lê 1kbyte de dados do arquivo do servidor
		byteCount = read(sockfd, dataBuffer, KBYTE);

		// escreve no arquivo do cliente os bytes lidos do servidor
		if(bytesLeft > KBYTE)
		{
			byteCount = fwrite(dataBuffer, KBYTE, 1, ptrfile);
		}
		else
		{
			fwrite(dataBuffer, bytesLeft, 1, ptrfile);
		}
		// decrementa os bytes lidos
		bytesLeft -= KBYTE;
	}

	fclose(ptrfile);
	printf("File %s has been downloaded\n\n", file);
}

void upload_file(char *file)
{
	int byteCount, fileSize;
	FILE* ptrfile;
	char dataBuffer[KBYTE];
	struct client_request clientRequest;

	if (ptrfile = fopen(file, "rb"))
	{
			getFilename(file, clientRequest.file);
			clientRequest.command = UPLOAD;

			byteCount = write(sockfd, &clientRequest, sizeof(clientRequest));

			fileSize = getFileSize(ptrfile);

			// escreve número de bytes do arquivo
			byteCount = write(sockfd, &fileSize, sizeof(fileSize));

			while(!feof(ptrfile))
			{
					fread(dataBuffer, sizeof(dataBuffer), 1, ptrfile);

					byteCount = write(sockfd, dataBuffer, KBYTE);
					if(byteCount < 0)
						printf("ERROR sending file\n");
			}
	}
	// arquivo não existe
	else
	{
		printf("ERROR this file doesn't exist\n\n");
	}
}

void client_interface()
{
	int command = 0;
	char request[200], file[200];

	printf("\nCommands:\nupload <path/filename.ext>\ndownload <filename.ext>\nlist\nget_sync_dir\nexit\n");
	do
	{
		printf("\ntype your command: ");

		fgets(request, sizeof(request), stdin);

		command = commandRequest(request, file);

		// verifica requisição do cliente
		switch (command)
		{
			case LIST: show_files(); break;
			case EXIT: close_connection();break;
			//case SYNC: sync_client();break;
			case DOWNLOAD: get_file(file);break;
		  case UPLOAD: upload_file(file); break;

			default: printf("ERROR invalid command\n");
		}
	}while(command != EXIT);
}

void show_files()
{
	int byteCount, fileNum, i;
	struct client_request clientRequest;
	struct file_info file_info;

	clientRequest.command = LIST;

	// avisa servidor que será feito um download
	byteCount = write(sockfd, &clientRequest, sizeof(clientRequest));
	if (byteCount < 0)
		printf("Error sending LIST message to server\n");

	// lê número de arquivos existentes no diretório
	byteCount = read(sockfd, &fileNum, sizeof(fileNum));
	if (byteCount < 0)
		printf("Error receiving filesize\n");

	if (fileNum == 0)
	{
		printf("Empty directory\n\n\n");
		return;
	}

	for (i = 0; i < fileNum; i++)
	{
		byteCount = read(sockfd, &file_info, sizeof(file_info));

		printf("\nFile: %s \nLast modified: %ssize: %d\n", file_info.name, file_info.last_modified, file_info.size);
	}
}

void close_connection()
{
	close(sockfd);
	printf("Connection with server has been closed\n");
}
