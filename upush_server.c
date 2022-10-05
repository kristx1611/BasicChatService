#include "send_packet.h"

#include <time.h>

#define IP "127.0.0.1"
#define BUFSIZE 256
#define ACKSIZE 64
#define HEARTBEAT 30

struct client {
  char* name;
  char* ip;
  int port;
  time_t heartbeat;
  struct client* next;
};

struct client_list {
  int size;
  struct client* head;
  struct client* tail;
};

struct client_list* create_client_list() {
  struct client_list* cl = malloc(sizeof(struct client_list));
  cl->size = 0;
  cl->head = NULL;
  cl->tail = NULL;
  return cl;
}

void check_error(int i, char* msg) {
  if (i == -1) {
    perror(msg);
    exit(EXIT_FAILURE);
  }
}

int calculate_time_interval(time_t begin, time_t end) {
  return end - begin;
}

void destroy_client(struct client* client) {
  free(client->name);
  free(client->ip);
  free(client);
}

void destroy_client_list(struct client_list* cl) {
  struct client* current = cl->head;
  struct client* temp;
  while (current != NULL) {
    temp = current;
    current = current->next;
    destroy_client(temp);
  }
  free(cl);
}

struct client* create_client(char* name, struct sockaddr_in clientaddr) {
  struct client* new_client = malloc(sizeof(struct client));
  new_client->name = strdup(name);
  new_client->ip = strdup(inet_ntoa(clientaddr.sin_addr));
  new_client->port = ntohs(clientaddr.sin_port);
  new_client->next = NULL;
  return new_client;
}

int update_client(struct client_list* cl, char* name, struct sockaddr_in clientaddr) {
  struct client* current = cl->head;
  struct client* temp;
  while (current != NULL) {
    temp = current;
    current = current->next;

    if (!strcmp(name, temp->name)) {
      free(temp->ip);
      temp->ip = strdup(inet_ntoa(clientaddr.sin_addr));
      temp->port = ntohs(clientaddr.sin_port);
      temp->heartbeat = time(NULL);
      return 1;
    }
  }
  return 0;
}

void push_back_client(struct client_list* cl, char* name, struct sockaddr_in clientaddr) {
  struct client* client = malloc(sizeof(struct client));
  client->name = strdup(name);
  client->ip = strdup(inet_ntoa(clientaddr.sin_addr));
  client->port = ntohs(clientaddr.sin_port);
  client->heartbeat = time(NULL);
  client->next = NULL;

  if (cl->tail != NULL)
    cl->tail->next = client;
  else
    cl->head = client;
  cl->tail = client;
  cl->size += 1;
}

void pop_client(struct client_list* cl, char* name) {
  struct client* current = cl->head;
  struct client* prev;
  if (cl->size == 1 && !strcmp(current->name, name)) {
    cl->tail = NULL;
    cl->head = NULL;
    cl->size = 0;
    destroy_client(current);
  } else if (!strcmp(current->name, name)) {
    cl->head = current->next;
    cl->size -= 1;
    destroy_client(current);
  } else {
    prev = cl->head;
    current = prev->next;


    while (current != NULL) {
      if (!strcmp(current->name, name)) {
        if (current->next != NULL) {
          prev->next = current->next;;
        } else {
          cl->tail = prev;
        }
        cl->size -= 1;
        destroy_client(current);
        return;
      }
      prev = current;
      current = current->next;
    }
  }

}

int is_old_registration(struct client_list* cl, struct client* client) {
  time_t current_time = time(NULL);
  if (calculate_time_interval(client->heartbeat, current_time) > HEARTBEAT) {
    pop_client(cl, client->name);
    return 1;
  }
  return 0;
}

void create_ack(char* ack, char* seq_num, char* msg) {
  memset(ack, 0, ACKSIZE);
  snprintf(ack, ACKSIZE, "ACK %s %s", seq_num, msg);
}

void print_clients(struct client_list* cl) {
  struct client* next = cl->head;

  while (next != NULL) {
    printf("%s\n", next->name);
    printf("%s\n", next->ip);
    printf("%d\n", next->port);
    printf("\n");

    next = next->next;
  }
}

struct client* find_client(struct client_list* cl, char* name) {
  struct client* current = cl->head;
  while (current != NULL) {
    if (!strcmp(current->name, name))
      return current;
    current = current->next;
  }
  return NULL;
}

int main(int argc, char const *argv[]) {
  unsigned short port;
  int so, rc;
  struct sockaddr_in my_addr;
  struct in_addr ip_addr;
  char buf[BUFSIZE];
  struct client_list* cl;

  if (argc < 3) {
      printf("Usage: ./server <port> <loss_probability>\n");
      return 0;
  }
  // valgrind ./upush_server 2000 0

  // Currently assumes command line arguments are correct.
  port = atoi(argv[1]);
  set_loss_probability(atoi(argv[2]));

  cl = create_client_list();

  so = socket(AF_INET, SOCK_DGRAM, 0);
  check_error(so, "socket");

  inet_pton(AF_INET, IP, &ip_addr);

  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(port);
  my_addr.sin_addr = ip_addr;

  rc = bind(so, (struct sockaddr*)&my_addr, sizeof(my_addr));
  check_error(rc, "bind");

  socklen_t clientaddr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in clientaddr;

  buf[0] = '\0';
  char* seq_num;
  char* command;
  char* name;
  char ack[ACKSIZE];
  struct client* lookup;
  char lookup_reply[ACKSIZE];
  while (strcmp(buf, "quit")) { // This is just here for an easy way to close the server.
    rc = recvfrom(so, buf, BUFSIZE - 1, 0, (struct sockaddr*)&clientaddr, &clientaddr_len);
    check_error(rc, "read");
    buf[rc] = '\0';
    printf("%s\n", buf); // Only for debugging.

    if (!strcmp(buf, "quit")) { // This is just here for an easy way to close the server.
      continue;
    }

    strtok(buf, " "); // Skipping over PKT
    seq_num = strtok(NULL, " ");
    command = strtok(NULL, " ");
    name = strtok(NULL, " ");

    if (!strcmp(command, "REG")) {
      create_ack(ack, seq_num, "OK");
      rc = send_packet(so, ack, strlen(ack), 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr));
      check_error(rc, "send_packet");

      if (!update_client(cl, name, clientaddr))
        push_back_client(cl, name, clientaddr);

    } else {
      lookup = find_client(cl, name);

      if (lookup == NULL || is_old_registration(cl, lookup)) {
        create_ack(ack, seq_num, "NOT FOUND");
        rc = send_packet(so, ack, strlen(ack), 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr));
        check_error(rc, "send_packet");
      } else {
        memset(lookup_reply, 0, ACKSIZE);
        snprintf(lookup_reply, ACKSIZE, "NICK %s IP %s PORT %d", lookup->name, lookup->ip, lookup->port);
        create_ack(ack, seq_num, lookup_reply);
        rc = send_packet(so, ack, strlen(ack), 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr));
        check_error(rc, "send_packet");
      }

    }

  }

  print_clients(cl); // Only for debugging.
  destroy_client_list(cl);
  close(so);
  return EXIT_SUCCESS;
}
