#ifdef MDG_WINDOWS

#define SECURITY_WIN32
#include <WinSock2.h>
#include <Windows.h>
#define SystemFunction036 NTAPI SystemFunction036
#include <NTSecAPI.h>
#undef SystemFunction036
#include <stdio.h>
#include <stdint.h>

#include "mdg_peer_api.h"
#include "mdg_peer_storage.h"
#include "mdg_callback_functions.h"

char mdg_chat_platform[] = "Windows";

#include "mdg_config.c"

extern void hex_encode_bytes(const uint8_t *input, char *dst, int input_size);
extern int hex_decode_bytes(char *input, uint8_t *dst, int output_size);
extern void chatclient_parse_cmd_args(int c, char**a);
extern void mdg_chat_client_input(char *stdin_buf, int len);


#define BUFFER_LENGTH 1024
static char stdin_buf[BUFFER_LENGTH], *rp = stdin_buf;
int io_port = -1;
SOCKET io_socket = INVALID_SOCKET;

static void socket_mdg_chat_output_fprintf(const char *fmt, va_list argp)
{
    char buffer[BUFFER_LENGTH] = { 0 };
    char fixedBuffer[BUFFER_LENGTH * 2] = { 0 };
    int output_count = vsnprintf(buffer, sizeof(buffer), fmt, argp);
    int i;
    for (i = 0; i < output_count; ++i) {
        if (buffer[i] == '\n') {
            fixedBuffer[i] = '\r'; //not really needed, but makes things look good in a default telnet client
            fixedBuffer[i + 1] = '\n';
            ++i;
        }
        else {
            fixedBuffer[i] = buffer[i];
        }
    }
    output_count = i;

    int send_count_total = 0;
    int send_count = 0;
    while (send_count_total < output_count) {
        send_count = send(io_socket, fixedBuffer, output_count - send_count, 0);
        if (send_count > 0) {
            send_count_total += send_count;
        }
        else {
            break;
        }
    }
}

void mdg_chat_output_fprintf(const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    if (io_socket != INVALID_SOCKET) {
        socket_mdg_chat_output_fprintf(fmt, argp);
    }
    else {
        vfprintf(stdout, fmt, argp);
    }
    va_end(argp);
}

static int wait_for_connection(short port, SOCKET *connected_socket)
{
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket == INVALID_SOCKET) {
    fprintf(stderr, "socket error (%d)", WSAGetLastError());
    return -1;
  }

  if (bind(listen_socket, (struct sockaddr*) &address, sizeof(address)) == SOCKET_ERROR) {
    fprintf(stderr, "bind error (%d)", WSAGetLastError());
    closesocket(listen_socket);
    return -1;
  }

  if (listen(listen_socket, 1) == SOCKET_ERROR) {
    fprintf(stderr, "listen error (%d)", WSAGetLastError());
    closesocket(listen_socket);
    return -1;
  }

  *connected_socket = accept(listen_socket, NULL, NULL);
  if (*connected_socket == INVALID_SOCKET) {
    fprintf(stderr, "accept error (%d)", WSAGetLastError());
    closesocket(listen_socket);
    closesocket(*connected_socket);
    return -1;
  }

  closesocket(listen_socket);
  fprintf(stderr, "Connected to i/o socket\n");
  mdg_chat_output_fprintf("Connected!\n");

  return 0;
}

static void socket_handle_input(char *data, int data_length)
{
  int i;
  for (i = 0; i < data_length; ++i) {
    *rp = data[i];
    if (*rp == 10) {
      mdg_chat_client_input(stdin_buf, rp - stdin_buf);
      rp = stdin_buf;
    }
    else if (*rp == 13) {
      //Ignore char, overwrite on next read.
    }
    else {
      ++rp;
    }
  }
}

static void socket_do_select()
{
  int ret_val;
  fd_set read_fds;
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  char buffer[BUFFER_LENGTH];

  FD_ZERO(&read_fds);
  FD_SET(io_socket, &read_fds);
  ret_val = select(0, &read_fds, 0, 0, &tv);
  if (ret_val <= 0 || !FD_ISSET(io_socket, &read_fds)) {
    return;
  }

  ret_val = recv(io_socket, buffer, BUFFER_LENGTH, 0);
  if (ret_val <= 0) {
    closesocket(io_socket);
    io_socket = INVALID_SOCKET;
    return;
  }

  socket_handle_input(buffer, ret_val);
}

static int socket_init(int port)
{
  WSADATA data;
  if (WSAStartup(0x0202, &data)) {
    fprintf(stderr, "Unable to initialize winsock (%d)\n", WSAGetLastError());
    return 1;
  }
  io_port = port;
  return 0;
}

void mdg_abort() {
  fprintf(stderr, "mdg_abort invoked!");
  abort();
}

static void windows_chatclient_print_help(void)
{
#if !GOTOCON_DEMO
  char help[] = "/h ";
  mdg_chat_client_input(help, 3);
#endif
}

extern void mdg_chat_init();
int mdg_demo_start(void)
{
  int s;

  // If you need custom configuration, now is the time:
  // mdg_set_configuration(&mdg_configuration_test);
  // mdg_set_configuration(&mdg_configuration_prod01);
  mdg_set_configuration(&mdg_configuration_danfoss_prod);

  if ((s = mdg_init(0)) != 0) {
    fprintf(stderr, "mdg_init failed with %d\n", s);
    return -1;
  }

  mdg_chat_init();

  windows_chatclient_print_help();

  while (1) {
    if (io_port != -1) {
      if (io_socket == INVALID_SOCKET) {
        wait_for_connection(io_port, &io_socket);
        continue;
      }
      socket_do_select();
    } else {
      if (fgets(stdin_buf, sizeof(stdin_buf), stdin) != 0) {
        mdg_chat_client_input(stdin_buf, strlen(stdin_buf) - 1);
        memset(stdin_buf, 0, sizeof(stdin_buf));
      }
    }
  }
}

// Callback invoked by MDG lib.
int mdgstorage_load_random_base(uint8_t *random_base, uint32_t length)
{
  /* Library uses RtlGenRandom internally on Windows. */
  return 0;
}

extern char client_email[];
void load_demo_email()
{
  FILE *f = fopen("chat_demo_email.txt", "rb");
  if (f != NULL) {
    int r = fread(client_email, 1, 255, f);
    fclose(f);
    client_email[r] = 0;
  }
  else {
    client_email[0] = 0;
  }
}

void save_demo_email()
{
  FILE *f = fopen("chat_demo_email.txt", "wb");
  if (f != NULL) {
    fwrite(client_email, 1, strlen(client_email), f);
    fclose(f);
  }
}

//dummy callback which combines the individual arguments into a struct, then delegates to the real callback
static void pairing_state_cb_dummy(unsigned char pairing_mode, unsigned int start_timestamp, unsigned int end_timestamp, unsigned char *otp, unsigned char *new_peer, int32_t status)
{
  mdg_pairing_mode_state_t state;
  state.start_timestamp = start_timestamp;
  state.end_timestamp = end_timestamp;
  strncpy(state.otp, otp, sizeof(state.otp));
  state.new_peer = new_peer;
  state.pairing_mode = pairing_mode;

  mdguser_pairing_state(&state, status);
}

static int setup_callbacks()
{
  register_mdg_abort_callback(mdg_abort);
  register_incoming_call_from_peer_callback(mdguser_incoming_call);
  register_mdgstorage_add_pairing_callback(mdgstorage_add_pairing);
  register_mdgstorage_load_pairing_callback(mdgstorage_load_pairing);
  register_mdgstorage_load_license_key_callback(mdgstorage_load_license_key);
  register_mdgstorage_load_preset_pairing_token_callback(mdgstorage_load_preset_pairing_token);
  register_mdgstorage_load_private_key_callback(mdgstorage_load_private_key);
  register_mdgstorage_remove_pairing_callback(mdgstorage_remove_pairing);
  register_mdgstorage_load_random_base_callback(mdgstorage_load_random_base);
  register_mdguser_pairing_state_callback(pairing_state_cb_dummy);
  register_mdguser_routing_callback(mdguser_routing);
  register_mdguser_control_status(mdguser_control_status);
  return 0;
}

int main(int c, char**a)
{
  int s;
  mdg_chat_output_fprintf("Welcome to TDG-lib chat demo.\n");
  setup_callbacks();
  chatclient_parse_cmd_args(c, a);
  if (c > 2) {
    s = socket_init(atoi(a[2]));
    if (s != 0) {
      mdg_chat_output_fprintf("unable to inialize socket from command line argument\n");
      return 1;
    }
  }
  load_demo_email();
  if (client_email[0] == 0) {
    mdg_chat_output_fprintf("Please provide e-mail using \"/email test@example.org\"\n");
  }
  mdg_chat_output_fprintf("Type \"/h\" for list of commands. All commands start with \"/\"\n");
  mdg_demo_start();
  return 0;
}

void mdg_chat_client_exit() {
  exit(0);
}

#endif
