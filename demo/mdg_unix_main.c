#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "mdg_peer_api.h"
#include "mdg_peer_storage.h"
#ifdef MDG_DYNAMIC_LIBRARY
#include "mdg_callback_functions.h"
#endif
#include "logging.h"

#ifdef MDG_ANDROID
char mdg_chat_platform[] = "Android";
#else
char mdg_chat_platform[] = "Unix";
#endif

#include "mdg_config.c"

extern void hex_encode_bytes(const uint8_t *input, char *dst, int input_size);
extern int hex_decode_bytes(char *input, uint8_t *dst, int output_size);
extern void chatclient_parse_cmd_args(int c, char**a);
extern void mdg_chat_client_input(char *stdin_buf, int len);

void mdg_chat_output_fprintf(const char *fmt, ...);

#define BUFFER_LENGTH (1024+30) // A bit extra added to make room for both a 512-byte bytestring in hex plus the rest of a command.
static char stdin_buf[BUFFER_LENGTH], *rp = stdin_buf;
int io_port = -1;
int io_socket = -1;

static int wait_for_connection(short port, int *connected_socket)
{
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  int listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket == -1) {
    fprintf(stderr, "socket error (%d)", errno);
    return -1;
  }

  int enable = 1;
  if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    return 7;
  }
#ifdef SO_REUSEPORT
  if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0) {
    return 7;
  }
#endif

  if (bind(listen_socket, (struct sockaddr*) &address, sizeof(address)) == -1) {
    fprintf(stderr, "bind error (%d)", errno);
    close(listen_socket);
    return -1;
  }

  if (listen(listen_socket, 1) == -1) {
    fprintf(stderr, "listen error (%d)", errno);
    close(listen_socket);
    return -1;
  }

  *connected_socket = accept(listen_socket, NULL, NULL);
  if (*connected_socket == -1) {
    fprintf(stderr, "accept error (%d)", errno);
    close(listen_socket);
    return -1;
  }

  close(listen_socket);
  fprintf(stderr, "Connected to i/o socket\n");
  mdg_chat_output_fprintf("Connected!\n");

  return 0;
}

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
    } else {
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
    } else {
      break;
    }
  }
}

void mdg_chat_output_fprintf(const char *fmt, ...)
{
  va_list argp;
  va_start(argp, fmt);
  if (io_socket != -1) {
    socket_mdg_chat_output_fprintf(fmt, argp);
  } else {
    vfprintf(stdout, fmt, argp);
  }
  va_end(argp);
}


static int read_from_sock(int in)
{
  int s;
  while (rp < stdin_buf + sizeof(stdin_buf)) {
    s = read(in, rp, 1);
    if (s == 0) {
      fprintf(stderr, "eof\n");
      return 1;
    }
    if (s <= 0) {
      if (errno == EAGAIN) {
        return 0;
      }
      fprintf(stderr, "s=%d, errno=%d on stdin\n", s, errno);
      return -1;
    }
    s = *rp;
    if (s == 10) {
      mdg_chat_client_input(stdin_buf, rp - stdin_buf);
      rp = stdin_buf;
    } else if (s == 13) {
      //Ignore char, overwrite on next read.
    } else {
      rp++;
    }
  }
  return 0;
}

void mdg_abort() {
  fprintf(stderr, "mdg_abort invoked!");
  abort();
}

#ifdef DEVISMART_API

/*
 * Undocumented function, present in DEVISmart version of the library.
 */
_MDG_API_ int32_t mdg_add_to_buffer_pool(void *memory, uint32_t memorySize, uint32_t singleBufferSize);

#endif

extern void mdg_chat_init();
int mdg_demo_start(void)
{
  int s;
  
#ifdef DEVISMART_API
  /*
   * If this is not done, mdg_init() fails with code -3. The code inside the
   * library makes sure that at least 5 buffers are available.
   * Values and algorithm have been reverse engineered from Java and JNI glue
   * layer code. Each buffer has some header internally, 76 likely represents
   * extra space in order to accomodate it.
   */
  const int numBuffers = 128;
  const int bufferSize = 1536;
  
  int rawBufferSize = bufferSize + 76;
  int totalSize = numBuffers * rawBufferSize;
  void *mem = malloc(totalSize);

  if (!mem) {
	fprintf(stderr, "Failed to allocate memory for buffers\n");
	return -1;
  }
  
  /* According to the code it always returns zero, so it's OK to ignore the result */
  mdg_add_to_buffer_pool(mem, totalSize, rawBufferSize);
#endif
  
  if ((s = mdg_init(0)) != 0) {
    fprintf(stderr, "mdg_init failed with %d\n", s);
    return -1;
  }

  mdg_chat_init();

  while (1) {
    if (io_port == -1) {
      if (read_from_sock(STDIN_FILENO) == 1) {
        exit(0);
      }
    } else {
      fprintf(stderr, "Listening for connection on port %d\n", io_port);
      if (wait_for_connection(io_port, &io_socket) == 0) {
        fprintf(stderr, "Reading input from socket...\n");
        read_from_sock(io_socket);
      }
      fprintf(stderr, "Socket closed.\n");
      close(io_socket);
    }
  }
}

// Callback invoked by MDG lib.
int mdgstorage_load_random_base(uint8_t *random_base, uint32_t length)
{
  Log("%s(%d) called\n", __FUNCTION__, length);

  /* Library uses /dev/urandom internally on unix. */
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
  } else {
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

// Cmd-line provided server settings

#define IPSTRLEN 4*3 + 3*1 + 1 //XXX.XXX.XXX.XXX\0
static char server_endpoint_name[IPSTRLEN];
static mdg_control_server_endpoint  server_endpoint = {
  .name = server_endpoint_name
};

static const mdg_control_server_endpoint *server_endpoints[] = {
  &server_endpoint,
  0
};

static uint8_t server_pubkey[32];
static const uint8_t *server_pubkeys[] = {
  server_pubkey,
  0
};

static const mdg_configuration server_configuration = {
  .servers = server_endpoints,
  .trusted_server_keys = server_pubkeys
};

void configure_server(char* ip, int port, char* pubkey){
  if (strlen(pubkey) != 64){
    mdg_chat_output_fprintf("ERROR pubkey length not 64\n");
    exit(1);
  }
  hex_decode_bytes(pubkey, server_pubkey, 32);
  strncpy(server_endpoint_name, ip,  IPSTRLEN);
  server_endpoint_name[IPSTRLEN - 1] = 0; // strncpy does not ensure
                                          // termination in buffer
                                          // overflow case
  server_endpoint.port = port;
  mdg_set_configuration(&server_configuration);
}

#ifdef MDG_DYNAMIC_LIBRARY
//dummy callback which combines the individual arguments into a struct, then delegates to the real callback
static void pairing_state_cb(unsigned char pairing_mode, unsigned int start_timestamp, unsigned int end_timestamp, char *otp, unsigned char *new_peer, int32_t status)
{
  mdg_pairing_mode_state_t state;
  state.start_timestamp = start_timestamp;
  state.end_timestamp = end_timestamp;
  strncpy(state.otp, otp, sizeof(state.otp));
  state.new_peer = new_peer;
  state.pairing_mode = pairing_mode;

  mdguser_pairing_state(&state, status);
}

void mdguser_control_status_cb(int i)
{
  mdg_control_status_t state = i;
  mdguser_control_status(state);
}

static void setup_callbacks()
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
  register_mdguser_pairing_state_callback(pairing_state_cb);
  register_mdguser_routing_callback(mdguser_routing);
  register_mdguser_control_status(mdguser_control_status_cb);
}
#else
static void setup_callbacks(){} // static builds refer the functions directly.
#endif /*MDG_DYNAMIC_LIBRARY */

void mdg_chat_client_exit() {
  exit(0);
}

#if DEBUG_TO_UDP
extern char mdg_debug_log_target_ip[];
extern int mdg_debug_log_target_port;
extern int mdg_debug_log_target;
#endif

int main(int c, char**a) {
#if DEBUG_TO_UDP
  strcpy(mdg_debug_log_target_ip, "77.66.11.94");
  mdg_debug_log_target_port = 9999;
  mdg_debug_log_target = 0;
#endif

  mdg_chat_output_fprintf("Welcome to TDG-lib chat demo.\n");
  setup_callbacks();
  chatclient_parse_cmd_args(c, a);
  if (c == 3) {
    // Use TCP based IO instead of stdIO
    io_port = atoi(a[2]);
  }

  if (c == 5) {
    // Server config from cmd-line args.
    configure_server(a[2], atoi(a[3]), a[4]);
  } else {
    // mdg_set_configuration(&mdg_configuration_test);
    // mdg_set_configuration(&mdg_configuration_tmdg82);
    // mdg_set_configuration(&mdg_configuration_localhost);
    // mdg_set_configuration(&mdg_configuration_prod01);
    mdg_set_configuration(&mdg_configuration_danfoss_prod);
    // mdg_set_configuration(&mdg_configuration_danfoss_qa);
  }

  load_demo_email();
  if (client_email[0] == 0) {
      mdg_chat_output_fprintf("Please provide e-mail using \"/email test@example.org\"\n");
  }
  mdg_chat_output_fprintf("Type \"/h\" for list of commands. All commands start with \"/\"\n");
  mdg_demo_start();
  return 0;
}

