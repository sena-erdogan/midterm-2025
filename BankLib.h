#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <semaphore.h>

#define READ_FLAGS O_RDONLY
#define WRITE_FLAGS O_WRONLY
#define PERMS 0666

#define MAX_CLIENT 99
#define MAX_LOGFILE_LENGTH 4096
#define MAX_BANKID_LENGTH 10
#define FIFO_NAME_LEN 50
#define BANK_ID_STARTER "BankID_"
#define CLIENT_FIFO "ClientFIFO"
#define LOG_EXTENSION ".bankLog"

typedef struct {
  char id[MAX_BANKID_LENGTH];
  int number;
  int operation;
  int amount;
  int closed;
}
Client;

char * get_timestamp() {
  static char buffer[25];
  time_t now = time(NULL);
  struct tm * time_info = localtime( & now);
  strftime(buffer, sizeof(buffer), "%H:%M %B %d %Y", time_info);

  return buffer;
}

int is_integer(char * str) {
  if (strlen(str) < 1) {
    return 0;
  }

  for (int i = 0; i < strlen(str); i++) {
    if (str[i] < '0' || str[i] > '9') {
      return 0;
    }
  }
  return 1;
}

void itoa(int num, char * str) {
  int i = 0, sign = num;

  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return;
  }

  if (num < 0) num = -num;

  while (num) {
    str[i++] = (num % 10) + '0';
    num /= 10;
  }

  if (sign < 0) str[i++] = '-';

  str[i] = '\0';

  for (int j = 0, k = i - 1; j < k; j++, k--) {
    char temp = str[j];
    str[j] = str[k];
    str[k] = temp;
  }
}

