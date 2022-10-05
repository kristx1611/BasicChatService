#include "send_packet.h"

#include <time.h>
#include <ctype.h>

#define IP "127.0.0.1"
#define BUFSIZE 1401
#define LOOKUPSIZE 36
#define MIN_INPUT_SIZE 4
#define MAX_NAME_BYTE_SIZE 20
#define ACKSIZE 32
#define HEARTBEAT 10
#define MAIN_LOOP_DOWNTIME 1

static int server_seq_num;

struct blocked {
  char* name;
  struct blocked* next;
};

struct block_list {
  int size;
  struct blocked* head;
  struct blocked* tail;
};

struct message {
  int repeat;
  char* msg;
  time_t last_time_sent;
  struct message* next;
  //struct message* prev;
};

struct client {
  int size;
  int next_seq_num;
  int expected_seq_num;
  char* name;
  char* ip;
  int port;
  struct message* head;
  struct message* tail;
  struct client* next;
  //struct client* prev;
};

struct message_queue { // A linked list containing client_linked_list
  int size;
  struct client* head;
  struct client* tail;
};

void destroy_blocked(struct blocked* b) {
  free(b->name);
  free(b);
}

void destroy_block_list(struct block_list* bl) {
  struct blocked* current = bl->head;
  struct blocked* temp;
  while (current != NULL) {
    temp = current;
    current = current->next;
    destroy_blocked(temp);
  }
  free(bl);
}

int is_blocked(struct block_list* bl, char* name) {
  struct blocked* current = bl->head;
  while (current != NULL) {
    if (!strcmp(current->name, name))
      return 1;
    current = current->next;
  }
  return 0;
}

void add_block(struct block_list* bl, char* name) {
  struct blocked* current = bl->head;
  while (current != NULL) {
    if (!strcmp(current->name, name)) {
      fprintf(stderr, "%s IS ALREADY ON YOUR BLOCKLIST\n", name);
      return;
    }
    current = current->next;
  }

  struct blocked* b = malloc(sizeof(struct blocked));
  b->name = strdup(name);
  b->next = NULL;

  if (bl->tail != NULL)
    bl->tail->next = b;
  else
    bl->head = b;
  bl->tail = b;
  bl->size += 1;
}

void remove_block(struct block_list* bl, char* name) {
  struct blocked* current = bl->head;
  struct blocked* prev;
  if (bl->size == 1 && !strcmp(current->name, name)) {
    bl->tail = NULL;
    bl->head = NULL;
    bl->size = 0;
    destroy_blocked(current);
  } else if (!strcmp(current->name, name)) {
    bl->head = current->next;
    bl->size -= 1;
    destroy_blocked(current);
  } else {
    prev = bl->head;
    current = prev->next;


    while (current != NULL) {
      if (!strcmp(current->name, name)) {
        if (current->next != NULL) {
          prev->next = current->next;;
        } else {
          bl->tail = prev;
        }
        bl->size -= 1;
        destroy_blocked(current);
        return;
      }
      prev = current;
      current = current->next;
    }
  }
}

struct block_list* create_block_list() {
  struct block_list* bl = malloc(sizeof(struct block_list));
  bl->size = 0;
  bl->head = NULL;
  bl->tail = NULL;
  return bl;
}

struct message_queue* create_message_queue() {
  struct message_queue* mq = malloc(sizeof(struct message_queue));
  mq->size = 0;
  mq->head = NULL;
  mq->tail = NULL;
  return mq;
}

struct client* find_client(struct message_queue* mq, char* name) {
  struct client* current = mq->head;
  while (current != NULL) {
    if (!strcmp(current->name, name))
      return current;
    current = current->next;
  }
  return NULL;
}

struct client* find_client_by_port(struct message_queue* mq, int port) {
  struct client* current = mq->head;
  while (current != NULL) {
    if (current->port == port)
      return current;
    current = current->next;
  }
  return NULL;
}

void update_message_info(struct message* message) {
  message->repeat += 1;
  message->last_time_sent = time(NULL);
}

void push_back_message(struct client* client, char* msg) {
  struct message* message = malloc(sizeof(struct message));
  message->msg = strdup(msg);
  message->next = NULL;
  message->repeat = 0;
  message->last_time_sent = 0;

  if (client->tail != NULL)
    client->tail->next = message;
  else
    client->head = message;
  client->tail = message;
  client->size += 1;
}

int update_client(struct message_queue* mq, char* name, char* ip, char* port) {
  struct client* current = mq->head;
  struct client* temp;
  while (current != NULL) {
    temp = current;
    current = current->next;

    if (!strcmp(name, temp->name)) {
      free(temp->ip);
      temp->ip = strdup(ip);
      temp->port = atoi(port);
      return 1;
    }
  }
  return 0;
}

void push_back_client(struct message_queue* mq, char* name, char* ip, char* port) {
  struct client* client = malloc(sizeof(struct client));
  client->size = 0;
  client->next_seq_num = 0;
  client->expected_seq_num = 0;
  client->name = strdup(name);
  client->ip = strdup(ip);
  client->port = atoi(port);
  client->head = NULL;
  client->tail = NULL;
  client->next = NULL;

  if (mq->tail != NULL)
    mq->tail->next = client;
  else
    mq->head = client;
  mq->tail = client;
  mq->size += 1;
}

void destroy_message(struct message* message) {
  free(message->msg);
  free(message);
}

void destroy_client(struct client* client) {
  struct message* current = client->head;
  struct message* temp;
  while (current != NULL) {
    temp = current;
    current = current->next;
    destroy_message(temp);
  }
  free(client->name);
  free(client->ip);
  free(client);
}

void destroy_message_queue(struct message_queue* mq) {
  struct client* current = mq->head;
  struct client* temp;
  while (current != NULL) {
    temp = current;
    current = current->next;
    destroy_client(temp);
  }
  free(mq);
}

void pop_front_message(struct client* client) {
  struct message* temp;

  if (client->size == 1) {
    destroy_message(client->head);
    client->tail = NULL;
    client->head = NULL;
    client->size = 0;
  } else if (client->size > 1) {
    temp = client->head->next;
    destroy_message(client->head);
    client->head = temp;
    client->size -= 1;
  }

  //printf("size = %d\n", client->size);
}

void pop_client(struct message_queue* mq, char* name) {
  struct client* current = mq->head;
  struct client* prev;
  if (mq->size == 1 && !strcmp(current->name, name)) {
    mq->tail = NULL;
    mq->head = NULL;
    mq->size = 0;
    destroy_client(current);
  } else if (!strcmp(current->name, name)) {
    mq->head = current->next;
    mq->size -= 1;
    destroy_client(current);
  } else {
    prev = mq->head;
    current = prev->next;


    while (current != NULL) {
      if (!strcmp(current->name, name)) {
        if (current->next != NULL) {
          prev->next = current->next;;
        } else {
          mq->tail = prev;
        }
        mq->size -= 1;
        destroy_client(current);
        return;
      }
      prev = current;
      current = current->next;
    }
  }

}

void check_error(int i, char *msg) {
  if (i == -1) {
    perror(msg);
    exit(EXIT_FAILURE);
  }
}

void get_string(char buf[], int size) {
  char c;
  fgets(buf, size, stdin);

  if (buf[strlen(buf) - 1] == '\n') {
      buf[strlen(buf) - 1] = '\0';
  }

  else while ((c = getchar()) != '\n' && c != EOF);
}

int compare_seq_nums(char received, int expected) {
  int i = received - '0';
  return i == expected;
}

void swap_server_seq_num() {
  if (server_seq_num)
    server_seq_num = 0;
  else
    server_seq_num = 1;
}

void swap_client_expected_seq_num(struct client* client) {
  if (client->expected_seq_num)
    client->expected_seq_num = 0;
  else
    client->expected_seq_num = 1;
}

void swap_client_next_seq_num(struct client* client) {
  if (client->next_seq_num)
    client->next_seq_num = 0;
  else
    client->next_seq_num = 1;
}

void check_valid_nick(const char* nick) {
  if (strlen(nick) > MAX_NAME_BYTE_SIZE - 1) {
    fprintf(stderr, "<nick> WRONG FORMAT. MAXIMUM 19 LETTERS.\n");
    exit(EXIT_FAILURE);
  }

  for (size_t i = 0; i < strlen(nick); i++) {
    if (!isascii(nick[i]) || isspace(nick[i])) {
      fprintf(stderr, "<nick> WRONG FORMAT. ONLY ASCII. NO SPACES.\n");
      exit(EXIT_FAILURE);
    }
  }
}

int register_client(int sockfd, struct sockaddr_in server_addr, long seconds,
                    const char* nick) {
  int rc, ack;
  fd_set set;
  struct timeval timeout;
  char buf[BUFSIZE];
  int max_reg_size = 32;
  char registration[max_reg_size];

  FD_ZERO(&set);
  FD_SET(sockfd, &set);
  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;

  memset(registration, 0, max_reg_size);
  snprintf(registration, max_reg_size, "PKT %d REG %s", server_seq_num, nick);
  rc = send_packet(sockfd, registration, max_reg_size, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
  check_error(rc, "send_packet");
  ack = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
  check_error(ack, "select");

  if (ack) {
    rc = read(sockfd, buf, BUFSIZE - 1);
    check_error(rc, "read");
    buf[rc] = '\0';

    if (!strcmp(buf, "ACK 0 OK")) {
      printf("REGISTRATION COMPLETE.\n");
      server_seq_num = 1;
    } else {
      fprintf(stderr, "INVALID REPLY RECEIVED. EXITING.\n");
      return -1;
    }

  } else {
    fprintf(stderr, "NO SERVER ACKNOWLEDGEMENT RECEIVED. EXITING.\n");
    return -1;
  }

  return 1;
}

int check_user_input(char* buf) {
  // 3 = Unblock
  // 2 = Block
  // 1 = Valid message
  // 0 = Quit
  // -1 = Wrong format
  char* first;
  char* second;
  char buf_copy[strlen(buf) + 1];

  if (strlen(buf) < MIN_INPUT_SIZE)
    return -1;

  strcpy(buf_copy, buf);

  first = strtok(buf_copy, " ");
  second = strtok(NULL, " ");

  if (!strcmp(buf, "QUIT"))
    return 0;
  else if (first == NULL || second == NULL)
    return -1;
  else if (strlen(first) > MAX_NAME_BYTE_SIZE)
    return -1;
  else if (buf[0] == '@' && !isspace(buf[1]))
    return 1;
  else if (!strcmp(buf, "QUIT SERVER")) // Remember to remove.
    return 69;
  else if (!strcmp(first, "BLOCK") && strlen(second) <= MAX_NAME_BYTE_SIZE)
    return 2;
  else if (!strcmp(first, "UNBLOCK") && strlen(second) <= MAX_NAME_BYTE_SIZE)
    return 3;
  return -1;
}

void extract_nickname(char* nick, char* valid_user_input) {
  int i = 1;
  while (!isspace(valid_user_input[i])) {
    nick[i-1] = valid_user_input[i];
    i++;
  }
  nick[i-1] = 0;
}

void extract_nickname_to_block(char* nick, char* valid_user_input) {
  char buf_copy[strlen(valid_user_input) + 1];
  strcpy(buf_copy, valid_user_input);

  strtok(buf_copy, " ");
  strcpy(nick, strtok(NULL, " "));
}

int send_lookup_to_server(char* nick, int sockfd, struct sockaddr_in server_addr,
                          long seconds, struct message_queue* mq) {
  // return 1 = Found client
  // return 0 = Not found
  // return -1 = No reply from server
  int rc, ack;
  fd_set set;
  struct timeval timeout;
  char buf[BUFSIZE];
  char lookup[LOOKUPSIZE];
  char not_found[21];
  int expected_seq_num;

  FD_ZERO(&set);
  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;

  socklen_t reply_addr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in reply_addr;

  char* address;
  char* port;
  int repeat = 2;
  while (repeat > 0) {
    expected_seq_num = server_seq_num;
    swap_server_seq_num();

    memset(lookup, 0, LOOKUPSIZE);
    snprintf(lookup, LOOKUPSIZE, "PKT %d LOOKUP %s", expected_seq_num, nick);
    rc = send_packet(sockfd, lookup, LOOKUPSIZE, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    check_error(rc, "send_packet");

    memset(not_found, 0, 21);
    snprintf(not_found, 21, "ACK %d NOT FOUND", expected_seq_num);

    FD_SET(sockfd, &set);
    ack = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
    check_error(ack, "select");

    if (ack) {
      rc = recvfrom(sockfd, buf, BUFSIZE - 1, 0, (struct sockaddr*)&reply_addr, &reply_addr_len);
      check_error(rc, "read");
      buf[rc] = '\0';
      printf("%s\n", buf);

      if (ntohs(server_addr.sin_port) == ntohs(reply_addr.sin_port)) { // From server port
        if (!strcmp(not_found, buf)) {
          fprintf(stderr, "NICK %s NOT REGISTERED\n", nick);
          return 0;
        } else if (compare_seq_nums(buf[4], expected_seq_num)) {
          strtok(buf, " ");
          for (int i = 0; i < 4; i++) {
            strtok(NULL, " "); // Skip to the address and port section
          }
          address = strtok(NULL, " ");
          strtok(NULL, " ");
          port = strtok(NULL, " ");
          if (!update_client(mq, nick, address, port))
            push_back_client(mq, nick, address, port);
          return 1;
        }
      }

    } else {
      repeat -= 1;
      timeout.tv_sec = seconds;
    }

  }

  return -1;
}

void send_message_to_client(char* msg, struct client* receiver_client,
                            int sockfd, const char* from_nick, char* to_nick) {
  struct in_addr dest_ip;
  int rc;
  char full_message[BUFSIZE];
  struct sockaddr_in dest_addr;

  inet_pton(AF_INET, receiver_client->ip, &dest_ip);
  dest_addr.sin_port = htons(receiver_client->port);
  dest_addr.sin_addr = dest_ip;
  dest_addr.sin_family = AF_INET;

  if (msg == NULL) {
    rc = send_packet(sockfd, receiver_client->head->msg, strlen(receiver_client->head->msg), 0,
                    (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    check_error(rc, "send_packet");
    update_message_info(receiver_client->head);
  } else {
    msg += strlen(to_nick) + 2; // +1 for the @ and +1 for the whitespace.
    snprintf(full_message, BUFSIZE, "PKT %d FROM %s TO %s MSG %s",
              receiver_client->next_seq_num, from_nick, to_nick, msg);
    swap_client_next_seq_num(receiver_client);

    if (receiver_client->size == 0) {
      push_back_message(receiver_client, full_message);

      rc = send_packet(sockfd, full_message, strlen(full_message), 0,
                      (struct sockaddr*)&dest_addr, sizeof(dest_addr));
      check_error(rc, "send_packet");
      update_message_info(receiver_client->head);
    } else {
      push_back_message(receiver_client, full_message);
    }
  }

}

int is_ack(char* msg) {
  if (strlen(msg) < 8)
    return 0;

  return msg[0] == 'A' && msg[1] == 'C' && msg[2] == 'K' && isdigit(msg[4]);
}

void verify_ack(struct client* client, char* ack, int sockfd, const char* from_nick) {
  if (compare_seq_nums(ack[4], client->expected_seq_num)) {
    pop_front_message(client);
    swap_client_expected_seq_num(client);
    if (client->size > 0) {
      send_message_to_client(NULL, client, sockfd, from_nick, client->name);
    }
  } else {
    fprintf(stderr, "RECEIVED OLD ACK\n");
  }
}

int is_valid_message_format(char* msg, char* from_nick, char* to_nick) {
  char msg_copy[BUFSIZE];
  char* token;

  if (strlen(msg) < 23)
    return 0;

  strcpy(msg_copy, msg);
  token = strtok(msg_copy, " ");
  if (strcmp(token, "PKT"))
    return 0;

  token = strtok(NULL, " ");
  if (!isdigit(token[0]) || strlen(token) > 1)
    return 0;

  token = strtok(NULL, " ");
  if (strcmp(token, "FROM"))
    return 0;

  token = strtok(NULL, " ");
  if (token == NULL)
    return 0;
  strcpy(from_nick, token);

  token = strtok(NULL, " ");
  if (strcmp(token, "TO"))
    return 0;

  token = strtok(NULL, " ");
  if (token == NULL)
    return 0;
  strcpy(to_nick, token);

  token = strtok(NULL, " ");
  if (strcmp(token, "MSG"))
    return 0;

  token = strtok(NULL, " ");
  if (token == NULL)
    return 0;

  return 1;
}

void send_ack(char* msg, char seq_num, struct sockaddr_in dest_addr, int sockfd) {
  char ack[ACKSIZE];
  int rc;

  memset(ack, 0, ACKSIZE);
  snprintf(ack, ACKSIZE, "ACK %c %s", seq_num, msg);
  rc = send_packet(sockfd, ack, strlen(ack), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
  check_error(rc, "send_packet");
}

void print_message_to_user(char* buf, char* from_nick, char* to_nick,
                          struct block_list* bl) {
  if (!is_blocked(bl, from_nick)) {
    buf += 20 + strlen(from_nick) + strlen(to_nick);
    printf("%s: %s\n", from_nick, buf);
  }
}

int calculate_time_interval(time_t begin, time_t end) {
  return end - begin;
}

void check_message_timeouts(struct message_queue* mq, long timeout, int sockfd,
                            struct sockaddr_in server_addr, const char* from_nick) {
  time_t current_time = time(NULL);
  struct client* current = mq->head;
  struct client* temp;
  // char fake_ack[ACKSIZE];

  while (current != NULL) {
    temp = current;
    current = current->next;

    if (temp->head != NULL &&
        calculate_time_interval(temp->head->last_time_sent, current_time) >= timeout) {

      if (temp->head->repeat == 2) {
        if (send_lookup_to_server(temp->name, sockfd, server_addr, timeout, mq) > 0) {
          send_message_to_client(NULL, temp, sockfd, from_nick, temp->name);
        } else {
          fprintf(stderr, "NICK %s NOT REGISTERED\n", temp->name);
          pop_client(mq, temp->name);
        }
      } else if (temp->head->repeat == 4) {
        fprintf(stderr, "NICK %s UNREACHABLE\n", temp->name);
        pop_client(mq, temp->name);
      } else {
        send_message_to_client(NULL, temp, sockfd, from_nick, temp->name);
      }

    }
  }
}

int send_heartbeat(time_t heartbeat, int sockfd, struct sockaddr_in server_addr,
                    const char* nick) {
  if (calculate_time_interval(heartbeat, time(NULL)) > HEARTBEAT) {
    int rc;
    int max_reg_size = 32;
    char registration[max_reg_size];

    memset(registration, 0, max_reg_size);
    snprintf(registration, max_reg_size, "PKT %d REG %s", server_seq_num, nick);
    rc = send_packet(sockfd, registration, max_reg_size, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    check_error(rc, "send_packet");
    return 1;
  }
  return 0;
}

int main(int argc, char const *argv[]) {
  int so, rc;
  long seconds;
  unsigned short serverport;
  fd_set set;
  struct sockaddr_in dest_addr, server_addr;
  struct in_addr server_ip;
  char buf[BUFSIZE];
  const char* nick;
  const char* server_ip_address;
  struct message_queue* mq;
  time_t heartbeat;
  struct block_list* bl;

  if (argc < 6) {
      printf("Usage: ./upush_client <nick> <ip-address> <port> <timeout> <loss_probability>\n");
      return 0;
  }
  // valgrind ./upush_client KRISTIAN 127.0.0.1 2000 10 10
  // valgrind ./upush_client ALICE 127.0.0.1 2000 5 10
  // valgrind ./upush_client BOB 127.0.0.1 2000 1 10
  // valgrind ./upush_client RETARD 127.0.0.1 2000 3 10

  server_seq_num = 0;

  // Currently assumes command-line arguments are correct.
  nick = argv[1];
  check_valid_nick(nick);

  server_ip_address = argv[2];
  serverport = atoi(argv[3]);
  seconds = atoi(argv[4]);

  set_loss_probability(atoi(argv[5]));

  so = socket(AF_INET, SOCK_DGRAM, 0);
  check_error(so, "socket");

  //inet_pton(AF_INET, IP, &my_ip);
  inet_pton(AF_INET, server_ip_address, &server_ip);

  //my_addr.sin_family = AF_INET;
  //my_addr.sin_port = htons(readport);
  //my_addr.sin_addr = my_ip;

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(serverport);
  server_addr.sin_addr = server_ip;

  socklen_t dest_addr_len = sizeof(struct sockaddr_in);

  //bind_available_port(so, my_addr, serverport);
  if (register_client(so, server_addr, seconds, nick) == -1) {
    close(so);
    exit(EXIT_FAILURE);
  }
  heartbeat = time(NULL);

  mq = create_message_queue();
  bl = create_block_list();
  FD_ZERO(&set);
  buf[0] = '\0';
  printf("How to quit: QUIT\n");
  printf("How to send message: @nickname <message>\n");
  printf("How to block: BLOCK <nickname>\n");
  printf("How to unblock: UNBLOCK <nickname>\n");
  // char lookup[LOOKUPSIZE];
  // char* token;
  // char* receiver_nick;
  // char msg[BUFSIZE];
  struct timeval timeout;
  struct client* receiver_client;
  struct client* sender_client;
  int input_code;
  char nick_lookup[MAX_NAME_BYTE_SIZE];
  int lookup_code;
  char from_nick[MAX_NAME_BYTE_SIZE];
  char to_nick[MAX_NAME_BYTE_SIZE];
  timeout.tv_sec = MAIN_LOOP_DOWNTIME;
  timeout.tv_usec = 0;
  int exit = 0;
  while (!exit) {
    fflush(NULL);
    FD_SET(STDIN_FILENO, &set);
    FD_SET(so, &set);
    rc = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
    check_error(rc, "select");

    if (FD_ISSET(STDIN_FILENO, &set)) {
        get_string(buf, BUFSIZE);
        //printf("You wrote: %s\n", buf);

        input_code = check_user_input(buf);
        if (input_code == 1) { // 1 = Valid message
          extract_nickname(nick_lookup, buf);

          if (is_blocked(bl, nick_lookup)) {
            fprintf(stderr, "RECIPIENT IS ON YOUR BLOCKLIST\n");
          } else {
            receiver_client = find_client(mq, nick_lookup);

            if (receiver_client == NULL) {
              lookup_code = send_lookup_to_server(nick_lookup, so, server_addr, seconds, mq);
              if (lookup_code == 1) {
                receiver_client = find_client(mq, nick_lookup);
                send_message_to_client(buf, receiver_client, so, nick, nick_lookup);
              } else if (lookup_code == -1) {
                fprintf(stderr, "NO ACKNOWLEDGEMENT FROM SERVER. EXITING\n");
                exit = 1;
              }

            } else {
              send_message_to_client(buf, receiver_client, so, nick, nick_lookup);
            }
          }

        } else if (input_code == 0) { // 0 = QUIT
          exit = 1;
        } else if (input_code == 69) { // Remember to remove.
          char* quit = "quit";
          rc = sendto(so, quit, strlen(quit), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
          check_error(rc, "sendto");
        } else if (input_code == 2) { // 2 = Block
          extract_nickname_to_block(nick_lookup, buf);
          add_block(bl, nick_lookup);
          pop_client(mq, nick_lookup);
        } else if (input_code == 3) { // 3 = Unblock
          extract_nickname_to_block(nick_lookup, buf);
          remove_block(bl, nick_lookup);
        } else { // Error
          fprintf(stderr, "WRONG FORMAT\n");
        }

    } else if (FD_ISSET(so, &set)) {
        rc = recvfrom(so, buf, BUFSIZE - 1, 0, (struct sockaddr*)&dest_addr, &dest_addr_len);
        check_error(rc, "read");
        buf[rc] = '\0';
        //printf("%s\n", buf);

        if (is_ack(buf)) {
          sender_client = find_client_by_port(mq, ntohs(dest_addr.sin_port));
          if (ntohs(dest_addr.sin_port) == serverport) {
            // printf("RECEIVED ACK FROM SERVER\n");
            // Debug, Expected to occur when timeout = 0 and after sending heartbeats
          } else if (sender_client == NULL)
            fprintf(stderr, "RECEIVED ACK FROM UNKNOWN SENDER\n");
          else
            verify_ack(sender_client, buf, so, nick);

        } else if (is_valid_message_format(buf, from_nick, to_nick)) {
          if (!strcmp(to_nick, nick)) {
            print_message_to_user(buf, from_nick, to_nick, bl);
            send_ack("OK", buf[4], dest_addr, so);
          } else {
            fprintf(stderr, "RECEIVED MESSAGE WITH WRONG NAME\n");
            send_ack("WRONG NAME", buf[4], dest_addr, so);
          }
        } else {
          fprintf(stderr, "RECEIVED INVALID MESSAGE FORMAT\n");
          send_ack("WRONG FORMAT", buf[4], dest_addr, so);
        }
    } else { // Timeout
      timeout.tv_sec = MAIN_LOOP_DOWNTIME;
      if (send_heartbeat(heartbeat, so, server_addr, nick))
        heartbeat = time(NULL);
      check_message_timeouts(mq, seconds, so, server_addr, nick);
    }
  }

  destroy_block_list(bl);
  destroy_message_queue(mq);
  close(so);
  return EXIT_SUCCESS;
}
