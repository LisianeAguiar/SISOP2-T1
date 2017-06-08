#define MAXNAME 64
#define MAXFILES 20
#define FREEDEV -1

struct file_info
{
  char name[MAXNAME];
  char extension[MAXNAME];
  char last_modified[MAXNAME];
  time_t lst_modified;
  int size;
  pthread_mutex_t file_mutex;
};

struct client
{
  int devices[2];
  char userid[MAXNAME];
  struct file_info file_info[MAXFILES];
  int logged_in;
};

struct client_list
{
  struct client client;
  struct client_list *next;
};

struct client_request
{
  char file[200];
  int command;
};

void sync_server(int socket, char *userid);
void receive_file(char *file, int socket, char*userid);
void send_file(char *file, int socket, char *userid);
void send_all_files(int client_socket, char *userid);
int initializeClient(int client_socket, char *userid, struct client *client);
void *client_thread (void *socket);
void *sync_thread_sv(void *socket);
void listen_client(int client_socket, char *userid);
void initializeClientList();
void send_file_info(int socket, char *userid);
void updateFileInfo(char *userid, struct file_info file_info);
void listen_sync(int client_socket, char *userid);
void close_client_connection();
void delete_file(char *file, int socket, char *userid);
