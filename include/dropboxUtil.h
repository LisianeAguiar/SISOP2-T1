#include "../include/dropboxServer.h"
#include <time.h>

#define KBYTE 1024
#define DELETE 6
#define DOWNLOADALL 5
#define EXIT 4
#define SYNC 3
#define LIST 2
#define DOWNLOAD 1
#define UPLOAD 0


void newList(struct client_list *client_list);
void insertList(struct client_list **client_list, struct client client);
int isEmpty(struct client_list *client_list);
int findNode(char *userid, struct client_list *client_list, struct client_list **client);
int getFileSize(FILE* ptrfile);
int commandRequest(char *request, char*file);
void getFilename(char *pathname, char *filename);
time_t getFileModifiedTime(char *path);
int exists(const char *fname);
