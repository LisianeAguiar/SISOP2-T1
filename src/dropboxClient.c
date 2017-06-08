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
#include <pwd.h>
#include "../include/dropboxClient.h"
#include "../include/dropboxUtil.h"

char userid[MAXNAME];
char directory[MAXNAME + 50];
char *host;
int port;
int sockfd = -1, sync_socket = -1;
int notifyfd;
int watchfd;

void initializeNotifyDescription()
{
	notifyfd = inotify_init();

	watchfd = inotify_add_watch(notifyfd, directory, IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
}

int create_sync_sock()
{
	int byteCount, connected;
	struct sockaddr_in server_addr;
	struct hostent *server;
	int client_thread = 0;
	char buffer[256];

	server = gethostbyname(host);

	if (server == NULL)
	{
  	return -1;
  }

	// tenta abrir o socket
	if ((sync_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		return -1;
	}

	// inicializa server_addr
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)server->h_addr);

	bzero(&(server_addr.sin_zero), 8);

	// tenta conectar ao socket
	if (connect(sync_socket,(struct sockaddr *) &server_addr,sizeof(server_addr)) < 0)
	{
		  return -1;
	}

	write(sync_socket, &client_thread, sizeof(client_thread));

	// envia userid para o servidor
	byteCount = write(sync_socket, userid, sizeof(userid));
}

int connect_server (char *host, int port)
{
	int byteCount, connected;
	struct sockaddr_in server_addr;
	struct hostent *server;
	int client_thread = 1;
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

	write(sockfd, &client_thread, sizeof(client_thread));

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
	int length, i = 0;
  char buffer[BUF_LEN];
	char path[200];

	create_sync_sock();
	get_all_files();

	while(1)
	{
	  length = read( notifyfd, buffer, BUF_LEN );

	  if ( length < 0 ) {
	    perror( "read" );
	  }

	  while ( i < length ) {
	    struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
	    if ( event->len ) {
				if ( event->mask & IN_CLOSE_WRITE || event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
					strcpy(path, directory);
					strcat(path, "/");
					strcat(path, event->name);
					if(exists(path) && (event->name[0] != '.'))
					{
						upload_file(path, sync_socket);
					}
				}
				else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM)
				{
					strcpy(path, directory);
					strcat(path, "/");
					strcat(path, event->name);
					if(event->name[0] != '.')
					{
						delete_file_request(path, sync_socket);
					}
				}
	    }
	    i += EVENT_SIZE + event->len;
  	}
		i = 0;

		sleep(10);
	}
}

void sync_client_first()
{
	char *homedir;
	char fileName[MAXNAME + 10] = "sync_dir_";
	pthread_t syn_th;

	if ((homedir = getenv("HOME")) == NULL)
	{
    homedir = getpwuid(getuid())->pw_dir;
  }
	// nome do arquivo
	strcat(fileName, userid);

	// forma o path do arquivo
	strcpy(directory, homedir);
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

	initializeNotifyDescription();

	//cria thread para sincronização
	if(pthread_create(&syn_th, NULL, sync_thread, NULL))
	{
		printf("ERROR creating thread\n");
	}
}

void get_all_files()
{
	int byteCount, bytesLeft, fileSize, fileNum, i;
	struct client_request clientRequest;
	FILE* ptrfile;
	char dataBuffer[KBYTE], file[MAXNAME], path[KBYTE];

	// copia nome do arquivo e comando para enviar para o servidor
	clientRequest.command = DOWNLOADALL;

	// avisa servidor que será feito um download
	byteCount = write(sync_socket, &clientRequest, sizeof(clientRequest));
	if (byteCount < 0)
		printf("Error sending DOWNLOAD message to server\n");

	byteCount = read(sync_socket, &fileNum, sizeof(fileNum));

	for(i = 0; i < fileNum; i++)
	{
		// lê nome do arquivo do servidor
		byteCount = read(sync_socket, file, sizeof(file));
		if (byteCount < 0)
			printf("Error receiving filename\n");

		strcpy(path, directory);
		strcat(path, "/");
		strcat(path, file);

		// cria arquivo no diretório do cliente
		ptrfile = fopen(path, "wb");

		read(sync_socket, &fileSize, sizeof(int));

		// número de bytes que faltam ser lidos
		bytesLeft = fileSize;

		if (fileSize > 0)
		{
			while(bytesLeft > 0)
			{
				// lê 1kbyte de dados do arquivo do servidor
				byteCount = read(sync_socket, dataBuffer, KBYTE);

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
		}

		fclose(ptrfile);
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

void delete_file_request(char* file, int socket)
{
	int byteCount;
	struct client_request clientRequest;

	getFilename(file, clientRequest.file);
	clientRequest.command = DELETE;

	byteCount = write(socket, &clientRequest, sizeof(clientRequest));

	if (byteCount < 0)
		printf("ERROR sending delete file request\n");
}

void upload_file(char *file, int socket)
{
	int byteCount, fileSize;
	FILE* ptrfile;
	char dataBuffer[KBYTE];
	struct client_request clientRequest;

	if (ptrfile = fopen(file, "rb"))
	{
			getFilename(file, clientRequest.file);
			clientRequest.command = UPLOAD;

			byteCount = write(socket, &clientRequest, sizeof(clientRequest));

			fileSize = getFileSize(ptrfile);

			// escreve número de bytes do arquivo
			byteCount = write(socket, &fileSize, sizeof(fileSize));

			if (fileSize == 0)
			{
				fclose(ptrfile);
				return;
			}

			while(!feof(ptrfile))
			{
					fread(dataBuffer, sizeof(dataBuffer), 1, ptrfile);

					byteCount = write(socket, dataBuffer, KBYTE);
					if(byteCount < 0)
						printf("ERROR sending file\n");
			}
			fclose(ptrfile);

			if (socket != sync_socket)
				printf("the file has been uploaded\n");
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
			case SYNC: get_all_files();break;
			case DOWNLOAD: get_file(file);break;
		  case UPLOAD: upload_file(file, sockfd); break;

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
	int byteCount;
	struct client_request clientRequest;

	clientRequest.command = EXIT;

	// avisa servidor que será feito um download
	byteCount = write(sockfd, &clientRequest, sizeof(clientRequest));
	if (byteCount < 0)
		printf("Error sending EXIT message to server\n");

	// avisa servidor que será feito um download
	byteCount = write(sync_socket, &clientRequest, sizeof(clientRequest));
	if (byteCount < 0)
		printf("Error sending EXIT message to server\n");

	close(sockfd);
	printf("Connection with server has been closed\n");
}
