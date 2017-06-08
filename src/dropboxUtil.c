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
#include "../include/dropboxUtil.h"

char commands[5][13] = {"upload", "download", "list", "get_sync_dir", "exit"};

void newList(struct client_list *client_list)
{
	client_list = NULL;
}

void insertList(struct client_list **client_list, struct client client)
{
	struct client_list *client_node;
	struct client_list *client_list_aux = *client_list;

	client_node = malloc(sizeof(struct client_list));

	client_node->client = client;
	client_node->next = NULL;

	if (*client_list == NULL)
	{
		*client_list = client_node;
	}
	else
	{
		while(client_list_aux->next != NULL)
			client_list_aux = client_list_aux->next;

		client_list_aux->next = client_node;
	}
}

int isEmpty(struct client_list *client_list)
{
	return client_list == NULL;
}

int findNode(char *userid, struct client_list *client_list, struct client_list **client_node)
{
	struct client_list *client_list_aux = client_list;

	while(client_list_aux != NULL)
	{
		if (strcmp(userid, client_list_aux->client.userid) == 0)
		{
			*client_node = client_list_aux;
			return 1;
		}
		else
			client_list_aux = client_list_aux->next;
	}
	return 0;
}

int getFileSize(FILE *ptrfile)
{
	int size;

	fseek(ptrfile, 0L, SEEK_END);
	size = ftell(ptrfile);

	rewind(ptrfile);

	return size;
}

int commandRequest(char *request, char *file)
{
	char *requestAux, *fileAux;
	int strLen;

	strLen = strlen(request);

	if ((strLen > 0) && (request[strLen-1] == '\n'))
	{
		  request[strLen-1] = '\0';
	}

	if (!strcmp(request, commands[LIST]))
		return LIST;
	else if (!strcmp(request, commands[EXIT]))
		return EXIT;
	else if (!strcmp(request, commands[SYNC]))
		return SYNC;

	requestAux = strtok(request, " ");
	//if (requestAux != NULL)
	//puts("AAA");
	fileAux = strtok(NULL, "\n");
	if (fileAux != NULL)
		strcpy(file, fileAux);
	else
		return -1;

	if (file != NULL)
	{
		if (!strcmp(requestAux, commands[DOWNLOAD]))
			return DOWNLOAD;
		else if (!strcmp(requestAux, commands[UPLOAD]))
			return UPLOAD;
	}
	else
		return -1;
}

// função que extrai o nome do arquivo a partir de um pathname
void getFilename(char *pathname, char *filename)
{
	char *filenameAux;

	filenameAux = strtok(pathname, "/");

	strcpy(filename, filenameAux);

	while(filenameAux != NULL)
	{
		strcpy(filename, filenameAux);

		filenameAux = strtok(NULL, "/");
	}
}

time_t getFileModifiedTime(char *path)
{
    struct stat attr;
    if (stat(path, &attr) == 0)
    {
        return attr.st_mtime;
    }
    return 0;
}

int exists(const char *fname)
{
    FILE *file;
    if (file = fopen(fname, "rb"))
    {
        fclose(file);
        return 1;
    }
    return 0;
}
