#include <sys/inotify.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

void client_interface();

int connect_server (char *host, int port);
void sync_client();
void upload_file(char *file, int socket);
void get_file(char *file);
void close_connection();
void show_files();
void sync_client_first();
void *sync_thread();
void initializeNotifyDescription();
void get_all_files();
void delete_file_request(char* file, int socket);
