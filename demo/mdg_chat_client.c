#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
// For sleep:
#if __linux
#include <unistd.h>
#elif MDG_WINDOWS
#include <WinSock2.h>
#include <Windows.h>
#elif defined(MDG_CC3200)
#include <simplelink.h>
#elif defined(MDG_UCOS)
#include <simplelink/simplelink.h>
#endif

#include "mdg_peer_api.h"
#include "mdg_peer_storage.h"

#define SECURE_LOG_MODULE_NO 117

#ifdef MDG_WINDOWS
#define MDGEXT_NO_CLIENT_DEBUG
#define MDGEXT_NO_INVOKE_SERVICE
#define MDGEXT_NO_SECURE_LOG
#define MDGEXT_NO_SERVER_PUSH
#else
#ifndef __ANDROID__
// Include demo of file downloading - links mdgext_filedown into the binary too.
#define DEMO_FILE_DOWNLOAD
#endif
#endif

#ifdef MDG_DYNAMIC_LIBRARY
// Hex encode / decode:
#include "mdg_util.c"
#else
#include "mdg_util.h"
#endif
#include "devismart.h"
#include "devismart_protocol.h"
#include "logging.h"
#include "mdg_chat_client.h"

extern void mdg_chat_client_exit();

typedef struct {
  uint8_t peer_id[MDG_PEER_ID_SIZE];
  char peer_name[256];
} pairing_t;

#define MAX_CHAT_PAIRINGS 32
static pairing_t chat_pairings[MAX_CHAT_PAIRINGS];
static int chat_pairings_count = 0;

static int pcr_timeout = 10;
static int incomingcall_testdelay = 0;
static int hex_output_mode = 1;
static int ap_mode = 0;

struct cmd {
  char *short_name;
  char *long_name;
  char *help_text;
  void (*handler)(char *args_buf, unsigned int len);
};

static void save_all_pairings_to_file()
{
#if defined(MDG_CC3200) ||defined(MDG_UCOS)
  _i32 s, f;
  s = sl_FsOpen("chat_demo_pairings.bin", FS_MODE_OPEN_CREATE(3584, 0), NULL, &f);
  if (s == SL_FS_FILE_NAME_EXIST) {
    s = sl_FsOpen("chat_demo_pairings.bin", FS_MODE_OPEN_WRITE, NULL, &f);
    if (s != 0) {
      sl_FsClose(f, NULL, NULL, 0);
    }
  }
  if (s == 0) {
    sl_FsWrite(f, 0, (unsigned char*) &chat_pairings_count, sizeof(chat_pairings_count));
    sl_FsWrite(f, 0, (unsigned char*) chat_pairings, sizeof(chat_pairings));
    sl_FsClose(f, NULL, NULL, 0);
  }
#else
  FILE *f = fopen("chat_demo_pairings.bin", "wb");
  if (f != NULL) {
    fwrite(&chat_pairings_count, 1, sizeof(chat_pairings_count), f);
    fwrite(chat_pairings, 1, sizeof(chat_pairings), f);
    fclose(f);
  }
#endif
}

static void load_all_pairings_from_file()
{
  static int has_loaded = 0;
  if (!has_loaded) {
    has_loaded = 1;
#if defined(MDG_CC3200) || defined(MDG_UCOS)
    _i32 s, f;
    s = sl_FsOpen("chat_demo_pairings.bin", FS_MODE_OPEN_READ, NULL, &f);
    if (s == 0) {
      s = sl_FsRead(f, 0, (unsigned char*) &chat_pairings_count, sizeof(chat_pairings_count));
      if (s == sizeof(chat_pairings_count)) {
        s = sl_FsRead(f, 0, (unsigned char*) chat_pairings, sizeof(chat_pairings));
        if (s != sizeof(chat_pairings)) {
          // Make warning on s get ignored.
          has_loaded = 2;
        }
      }
      sl_FsClose(f, NULL, NULL, 0);
    }
#else
    FILE *f = fopen("chat_demo_pairings.bin", "rb");
    if (f != NULL) {
      int s = fread(&chat_pairings_count, 1, sizeof(chat_pairings_count), f);
      if (s == sizeof(chat_pairings_count)) {
        s = fread(chat_pairings, 1, sizeof(chat_pairings), f);
        if (s != sizeof(chat_pairings)) {
          // Make warning on s get ignored.
          has_loaded = 2;
        }
      }
      fclose(f);
    }
#endif
  }
}

char client_email[256];
extern char mdg_chat_platform[];

/* Compatibility wrapper for API version, used by DEVISmart. */
static int get_connection_info(uint32_t connection_id,
                               mdg_peer_id_t sender_device_id,
                               char protocol[MAX_PROTOCOL_BYTES])
{
#ifdef DEVISMART_API
    /* Meaning of this extra parameter was never understood, returned ID always contains zeroes */
    return mdg_get_connection_info(connection_id, sender_device_id, protocol, NULL);
#else
    return mdg_get_connection_info(connection_id, sender_device_id, protocol);
#endif
}											  

mdg_property_t chatclient_client_props[] = {
  // Example properties. Make up your own for your application.
  {"client_app_prop1", "mdg_chat_client" },
  {"platform", mdg_chat_platform },
  {"client_email", client_email },
  {0, 0 },
};

static int arg_decode_gotonext(char **args_buf, unsigned int *lenp)
{
  int i, len = *lenp;
  char *p = *args_buf;
  if (len <= 0) {
    return 1;
  }
  for (i = 0; i < len; i++) {
    if (p[i] == ' ') {
      break;
    }
  }
  p[i] = 0;
  if (i < len) {
    i++;
  }
  for ( ; i < len; i++) {
    if (p[i] != ' ') {
      break;
    }
  }
  *args_buf = &p[i];
  *lenp -= i;
  return 0;
}

static int arg_decode_device_id_hex(char **args_buf, unsigned int *len, uint8_t *target)
{
  char *dev_id_arg = *args_buf;
  char *first_space = strstr(dev_id_arg, " ");
  int arg_len;
  if (*len < MDG_PEER_ID_SIZE * 2) {
    return 2;
  }
  if (arg_decode_gotonext(args_buf, len)) {
    return 1;
  }
  if (first_space != 0) {
    arg_len = first_space - dev_id_arg;
  } else {
    arg_len = *args_buf - dev_id_arg;
  }
  if (arg_len != MDG_PEER_ID_SIZE * 2) {
    return 3;
  }
  if (hex_decode_bytes(dev_id_arg, target, MDG_PEER_ID_SIZE)) {
    return 4;
  }

  return 0;
}

static void quit_handler(char *args_buf, unsigned int len)
{
  mdg_chat_output_fprintf("got quit command, exit(0)\n");
  mdg_chat_client_exit();
}

void whoami_hex(char *hexed)
{
  mdg_peer_id_t device_id;

  if (mdg_whoami(&device_id) != 0) {
    mdg_chat_output_fprintf("whoami failed.\n");
    return;
  }

  hex_encode_bytes(device_id, hexed, MDG_PRIVATE_KEY_DATA_SIZE);
}

static void whoami_handler(char *args_buf, unsigned int len)
{
  char hexed[AS_HEX(MDG_PEER_ID_SIZE)];

  whoami_hex(hexed);
  mdg_chat_output_fprintf("whoami says %s\n", hexed);
}



static void status_handler(char *args_buf, unsigned int len)
{
  int s;
  mdg_status_t status;
  s = mdg_status(&status);
  if (s != 0) {
    mdg_chat_output_fprintf("status failed with return %d\n", s);
    return;
  }
  mdg_chat_output_fprintf("=status:\n");
  mdg_chat_output_fprintf("unixtime_utc:     %d\n", (int) status.unixtime_utc);
  mdg_chat_output_fprintf("connect_switch:   %d\n", (int)status.connect_switch);
  mdg_chat_output_fprintf("connected_to_mdg: %d\n", (int)status.connected_to_mdg);
  mdg_chat_output_fprintf("pairing_mode:     %d\n", (int)status.pairing_mode);
  mdg_chat_output_fprintf("aggressive_ping:  %d\n", (int)status.aggressive_ping);
  mdg_chat_output_fprintf("remote_logging:   %d\n", (int)status.remote_logging_enabled);
}

static void connect_handler(char *args_buf, unsigned int len)
{
  if (mdg_connect(chatclient_client_props) != 0) {
    mdg_chat_output_fprintf("mdg_chat_client: connect failed\n");
  }
}

void mdg_chat_init()
{
  connect_handler(0, 0);
}

static void disconnect_handler(char *args_buf, unsigned int len)
{
  if (mdg_disconnect() != 0) {
    mdg_chat_output_fprintf("mdg_chat_client: disconnect failed\n");
  }
}

static int arg_parse_device_public_key(char **args_buf, unsigned int *len, uint8_t *device_id)
{
  if (*len < 1) {
    mdg_chat_output_fprintf("could not parse arg as public key\n");
    return 1;
  }
  if (arg_decode_device_id_hex(args_buf, len, device_id)) {
    mdg_chat_output_fprintf("could not parse arg as public key\n");
    return 1;
  }
#if DEBUG
  char hexed[128];
  hex_encode_bytes(device_id, hexed, MDG_PRIVATE_KEY_DATA_SIZE);
  mdg_chat_output_fprintf("parsed arg:device with public id %s\n", hexed);
#endif
  return 0;
}

static void pair_remote_handler(char *args_buf, unsigned int len)
{
  char *otp_arg = args_buf;
  int s;
  if (arg_decode_gotonext(&args_buf, &len)) {
    mdg_chat_output_fprintf("pair remote: Missing required arg, OTP.\n");
    return;
  }
  s = mdg_pair_remote(otp_arg);
  if (s) {
    mdg_chat_output_fprintf("mdg_pair_remote failed with %d\n", s);
  }
}

static void set_hex_output_mode_handler(char *args_buf, unsigned int len)
{
  char *flag_arg = args_buf;
  long flag;
  if (!arg_decode_gotonext(&args_buf, &len)) {
    flag = strtol(flag_arg, 0, 10);
    hex_output_mode = (int)flag;
  } else {
    hex_output_mode = !hex_output_mode;
  }

  switch (hex_output_mode) {
  case 0: mdg_chat_output_fprintf("outputting data from peers as text\n"); break;
  case 1: mdg_chat_output_fprintf("outputting data from peers as one-line hex\n"); break;
  case 2: mdg_chat_output_fprintf("outputting data from peers as hex\n"); break;
  }
}

static int32_t accept_all_peers(const char *protocol, const mdg_peer_id_t calling_device_id) {
  return 0;
}

static void set_ap_mode_handler(char *args_buf, unsigned int len)
{
  char *flag_arg = args_buf;
  long flag;
  if (!arg_decode_gotonext(&args_buf, &len)) {
    flag = strtol(flag_arg, 0, 10);
    ap_mode = (flag != 0);
  } else {
    ap_mode ^= 1;
  }

  if (ap_mode != 0) {
    mdg_set_peer_verifying_cb(accept_all_peers);
    mdg_chat_output_fprintf("access point simulation mode enabled (Accept any peer)\n");
  } else {
    mdg_set_peer_verifying_cb(0);
    mdg_chat_output_fprintf("access point simulation mode disabled (Accept only paired peers)\n");
  }
}

static void aggressive_ping_handler(char *args_buf, unsigned int len)
{
  char *duration_arg = args_buf;
  long duration;
  int s;
  if (!arg_decode_gotonext(&args_buf, &len)) {
    duration = strtol(duration_arg, 0, 10);
  } else {
    duration = 120;
  }

  s = mdg_aggressive_ping(duration);

  if (s != 0) {
    mdg_chat_output_fprintf("mdg_aggressive_ping failed, s=%d\n", s);
  } else if (duration == 0) {
    mdg_chat_output_fprintf("mdg_aggressive_ping stopped.\n");
  } else {
    mdg_chat_output_fprintf("mdg_aggressive_ping for %ld seconds started\n", duration);
  }
}

static char preset_otp_buffer[MDG_PRESET_PAIR_ID_SIZE];
static void open_for_pairing_handler(char *args_buf, unsigned int len)
{
  char *duration_arg = args_buf;
  long duration;
  int s;
  if (!arg_decode_gotonext(&args_buf, &len)) {
    duration = strtol(duration_arg, 0, 10);
  } else {
    duration = 120;
  }

  preset_otp_buffer[0] = 0; // Do not use preset otp.
  mdg_disable_pairing_mode();
  s = mdg_enable_pairing_mode(duration);

  if (s != 0) {
    mdg_chat_output_fprintf("Open for pairing failed, s=%d\n", s);
  } else {
    mdg_chat_output_fprintf("Open for pairing for %ld seconds\n", duration);
  }
}

static void open_for_pairing_handler_preset_otp(char *args_buf, unsigned int len)
{
  char *otp_arg = args_buf;
  long duration = 120;
  int s;
  if (!arg_decode_gotonext(&args_buf, &len)) {
    int otp_len = strlen(otp_arg);
    if (otp_len < MDG_PRESET_PAIR_ID_SIZE) {
      memcpy(preset_otp_buffer, otp_arg, otp_len);
      preset_otp_buffer[otp_len] = 0;
    } else {
      mdg_chat_output_fprintf("OTP argument too long. Limit=%d\n", MDG_PRESET_PAIR_ID_SIZE);
      return;
    }
  } else {
    mdg_chat_output_fprintf("OTP argument required.\n");
    return;
  }

  mdg_disable_pairing_mode();
  s = mdg_enable_pairing_mode(duration);

  if (s != 0) {
    mdg_chat_output_fprintf("Open for pairing failed, s=%d\n", s);
  } else {
    mdg_chat_output_fprintf("Open for pairing for %ld seconds\n", duration);
  }
}


static int set_intparam_handler(char **args_buf, unsigned int *len,
                                 char *param_name,
                                 int *target, int min, int max)
{
  long value;
  char *value_arg = *args_buf;

  if (arg_decode_gotonext(args_buf, len)) {
    mdg_chat_output_fprintf("Missing required arg, %s.\n", param_name);
    return 1;
 }
  value = strtol(value_arg, 0, 10);
  if (min <= value && value <= max) {
    *target = (int)value;
    return 0;
  } else {
    mdg_chat_output_fprintf("Illegal %s value.\n", param_name);
    return 1;
  }
}

static void set_pc_timeout_handler(char *args_buf, unsigned int len)
{
  if (!set_intparam_handler(&args_buf, &len, "timeout",
                            &pcr_timeout, 1, 600)) {
    mdg_chat_output_fprintf("Set place_call timeout to %d\n",
            pcr_timeout);
  }
}

static void test_random_handler(char *args_buf, unsigned int len)
{
  uint8_t buffer[MDG_PEER_ID_SIZE];
  char hexed[2*MDG_PEER_ID_SIZE + 1];

  mdg_make_private_key(buffer);
  hex_encode_bytes(buffer, hexed, sizeof(buffer));
  mdg_chat_output_fprintf("%s\n", hexed);
}

#ifndef MDGEXT_NO_CLIENT_DEBUG
static void demo_secure_log_loc_handler(char *args_buf, unsigned int len)
{
  SECURE_LOG_LOC();
  SECURE_LOG_PRINTF("printf-demo-line %d %d", -1, 2);
  mdg_secure_log_flush();
}
#endif /* MDGEXT_NO_CLIENT_DEBUG */

#ifndef MDGEXT_NO_SERVER_PUSH
static void demo_mdgext_register_peer_token(char *args_buf, unsigned int len)
{
  uint8_t device_id[MDG_PEER_ID_SIZE];
  void* token;
  uint32_t token_len;
  char* locale_code;
  int s;

  if (arg_parse_device_public_key(&args_buf, &len, device_id)) {
    return;
  }

  token = args_buf;
  if (arg_decode_gotonext(&args_buf, &len)) {
    mdg_chat_output_fprintf("Missing required arg, token.\n");
    return;
  }
  token_len = strlen(token); // token is a byte-array, but just a string in this demo.

  locale_code = args_buf;
  if (arg_decode_gotonext(&args_buf, &len)) {
    mdg_chat_output_fprintf("Missing required arg, locale_code.\n");
    return;
  }

  s = mdgext_register_peer_token(device_id,
                                 token, token_len,
                                 mdgext_peer_kind_android,
                                 locale_code,
                                 mdgext_push_on_connection_lost_default,
                                 1);
  if (s) {
    mdg_chat_output_fprintf("mdgext_register_peer_token failed with %d\n", s);
  }
}
#endif /* MDGEXT_NO_SERVER_PUSH */

#ifndef MDGEXT_NO_INVOKE_SERVICE
static void demo_mdgext_invoke_test_service_cb(mdgext_service_invocation * invocation,
                                               mdg_service_status_t state,
                                               const unsigned char *data,
                                               const uint32_t count)
{
}

static mdgext_service_invocation test_service;
static void demo_mdgext_invoke_test_service(char *args_buf, unsigned int len)
{
  int s;

  test_service.service_id = mdg_test_internal;
  test_service.properties = 0;
  test_service.req_data = 0;
  test_service.req_data_count = 0;
  test_service.cb = demo_mdgext_invoke_test_service_cb;

  s = mdgext_invoke_service(&test_service);
  if (s) {
    mdg_chat_output_fprintf("mdgext_invoke_service failed with %d\n", s);
  }
}
#endif /* MDGEXT_NO_INVOKE_SERVICE */

static void set_incomingcall_testdelay_handler(char *args_buf, unsigned int len)
{
  if (!set_intparam_handler(&args_buf, &len, "incomingcall_testdelay",
                            &incomingcall_testdelay, 0, 6000)) {
    mdg_chat_output_fprintf("Set incomingcall_testdelay to %d\n",
            incomingcall_testdelay);
  }
}

static void close_conn_handler(char *args_buf, unsigned int len)
{
  int conn_id, s;
  if (!set_intparam_handler(&args_buf, &len, "connection_id",
                            &conn_id,
                            -1, 1000)) {
    mdg_chat_output_fprintf("Invoking mdg_close_peer_connection(%d)\n", conn_id);
    if ((s = mdg_close_peer_connection(conn_id))) {
      mdg_chat_output_fprintf("mdg_close_peer_connection returned %d\n", s);
    }
  }
}

static void conninfo_handler(char *args_buf, unsigned int len)
{
  int conn_id, s;
  uint8_t sender_device_id[MDG_PEER_ID_SIZE];
  char protocol[MAX_PROTOCOL_BYTES];
#ifdef DEVISMART_API
  uint8_t unknown_id[MDG_PEER_ID_SIZE];
#endif

  if (!set_intparam_handler(&args_buf, &len, "connection_id",
                            &conn_id,
                            -1, 1000)) {
#ifdef DEVISMART_API
    s = mdg_get_connection_info(conn_id, sender_device_id, protocol, unknown_id);
#else
    s = mdg_get_connection_info(conn_id, sender_device_id, protocol);
#endif
    if (s != 0) {
      mdg_chat_output_fprintf("mdg_get_connection_info failed, returned %d\n", s);
    } else {
      char hexed[2* MDG_PEER_ID_SIZE + 1];
      hex_encode_bytes(sender_device_id, hexed, MDG_PEER_ID_SIZE);
      mdg_chat_output_fprintf("Peer for connection #%d is %s\n", conn_id, hexed);
      mdg_chat_output_fprintf("Peer protocol for connection #%d is %s\n", conn_id, protocol);
#ifdef DEVISMART_API
      hex_encode_bytes(unknown_id, hexed, MDG_PEER_ID_SIZE);
      mdg_chat_output_fprintf("Extra id for connection #%d is %s\n", conn_id, hexed);
#endif
    }
  }
}

extern void save_demo_email();

static void email_handler(char *args_buf, unsigned int len)
{
  char *a1 = args_buf;
  a1[len] = 0;
  if (arg_decode_gotonext(&args_buf, &len)) {
    mdg_chat_output_fprintf("Email for next connect is %s\n", client_email);
    return;
  }
  strncpy(client_email, a1, sizeof(client_email));
  client_email[sizeof(client_email) - 1] = 0; // strncpy does not ensure termination in buffer overflow case.
  mdg_chat_output_fprintf("Email for next connect set to %s\n", client_email);
  disconnect_handler(0, 0);
  connect_handler(0, 0);
  save_demo_email();
}

static void send_handler(char *args_buf, unsigned int len)
{
  int conn_id, s;
  char *message;

  if (!set_intparam_handler(&args_buf, &len, "connection_id",
                            &conn_id,
                            -1, MDG_MAX_CONNECTION_COUNT + 1)) {
    message = args_buf;
    s = mdg_send_to_peer((uint8_t*)message, len, conn_id);
    if (s) {
      mdg_chat_output_fprintf("send-data-to-peer failed with %d\n", s);
    }
  }
}

static void send_append_handler(char *args_buf, unsigned int len)
{
#ifdef MDG_WINDOWS
    mdg_chat_output_fprintf("Not supported under Windows\n");
#else
  int conn_id, flush_now, s;
  char *message;

  if (!set_intparam_handler(&args_buf, &len, "connection_id",
                            &conn_id,
                            -1, MDG_MAX_CONNECTION_COUNT + 1)) {
    if (!set_intparam_handler(&args_buf, &len, "flush_now",
                              &flush_now, 0, 1)) {
      message = args_buf;
      if (len == 0) {
        message = 0;
        mdg_chat_output_fprintf("invoking mdg_send_to_peer_append with data=count=0\n");
      }
      s = mdg_send_to_peer_append((uint8_t*)message, len, conn_id, flush_now);
      if (s) {
        mdg_chat_output_fprintf("mdg_send_to_peer_append failed with %d\n", s);
      }
    }
  }
#endif
}

static void send_hex_handler(char *args_buf, unsigned int len)
{
  int conn_id, s;
  uint8_t data[512]; // Be carefull with large chunks on stack.

  if (!set_intparam_handler(&args_buf, &len, "connection_id",
                            &conn_id,
                            -1, MDG_MAX_CONNECTION_COUNT + 1)) {
    if (len < 2
        || (len & 1) != 0
        || (len / 2) > sizeof(data)
        || hex_decode_bytes(args_buf, data, len / 2) != 0) {
      mdg_chat_output_fprintf("data argument must be hex encoded, even number of chars, max %d bytes.\n",
                  (int)sizeof(data));
      return;
    }
    s = mdg_send_to_peer(data, len / 2, conn_id);
    if (s) {
      mdg_chat_output_fprintf("mdg_send_data_to_peer failed with %d\n", s);
    }
  }
}

static void send_hex_append_handler(char *args_buf, unsigned int len)
{
#ifdef MDG_WINDOWS
    mdg_chat_output_fprintf("Not supported under Windows\n");
#else
  int conn_id, flush_now, s;
  uint8_t data[512]; // Be carefull with large chunks on stack.

  if (!set_intparam_handler(&args_buf, &len, "connection_id",
                            &conn_id,
                            -1, MDG_MAX_CONNECTION_COUNT + 1)) {
    if (!set_intparam_handler(&args_buf, &len, "flush_now",
                              &flush_now, 0, 1)) {
      if (len < 2
          || (len & 1) != 0
          || (len / 2) > sizeof(data)
          || hex_decode_bytes(args_buf, data, len / 2) != 0) {
        mdg_chat_output_fprintf("data argument must be hex encoded, even number of chars, max %d bytes.\n",
                                (int)sizeof(data));
        return;
      }
      s = mdg_send_to_peer_append(data, len / 2, conn_id, flush_now);
      if (s) {
        mdg_chat_output_fprintf("mdg_send_data_to_peer_append failed with %d\n", s);
      }
    }
  }
#endif
}

static void pretty_print_otp(const char *format, const char *otp)
{
  char c, pretty[32], *p=pretty;
  int i = 0;
  while (1) {
    c = otp[i++];
    if (c == 0) {
      *p = 0;
      break;
    }
    if (i > 1 && (i % 3) == 1) {
      *p++ = '-';
    }
    *p++ = c;
  }
  mdg_chat_output_fprintf(format, pretty);
}

// Callback invoked by MDG:
void mdguser_control_status(mdg_control_status_t state)
{
  switch (state) {
  case mdg_control_not_desired:
    mdg_chat_output_fprintf("Callback: mdguser_control_status state=not_desired\n");
    break;
  case mdg_control_connecting:
    mdg_chat_output_fprintf("Callback: mdguser_control_status state=connecting\n");
    break;
  case mdg_control_connected:
    mdg_chat_output_fprintf("Callback: mdguser_control_status state=connected\n");
    break;
  case mdg_control_failed:
    mdg_chat_output_fprintf("Callback: mdguser_control_status state=failed\n");
    break;
  default:
    mdg_chat_output_fprintf("Callback: mdguser_control_status invoked with unknown state=%d\n", state);
  }
}

// Callback invoked by MDG:
void mdguser_pairing_state(mdg_pairing_mode_state_t* state, mdg_pairing_status status)
{
  switch (status) {
  case mdg_pairing_starting:
    mdg_chat_output_fprintf("Callback: mdguser_pairing_state status=mdg_pairing_starting\n");
    break;
  case mdg_pairing_otp_ready:
    pretty_print_otp("Chat client: One-Time-Password (OTP) for pairing is: %s\n", state->otp);
    break;
  case mdg_pairing_completed:
    {
      char hexed[2 * MDG_PEER_ID_SIZE + 1];
      hex_encode_bytes(state->new_peer, hexed, MDG_PEER_ID_SIZE);
      mdg_chat_output_fprintf("Callback: mdguser_pairing_state status=mdg_pairing_completed. Peer: \"%s\"\n", hexed);
    }
    break;
  case mdg_pairing_failed_generic:
    mdg_chat_output_fprintf("Callback: mdguser_pairing_state status=mdg_pairing_failed_generic\n");
    break;
  case mdg_pairing_failed_otp:
    mdg_chat_output_fprintf("Callback: mdguser_pairing_state status=mdg_pairing_failed_otp\n");
    break;
  default:
    mdg_chat_output_fprintf("Callback: mdguser_pairing_state invoked with unknown status=%d\n", status);
    break;
  }
}

static void cancel_pairing_mode_handler(char *args_buf, unsigned int len)
{
  mdg_disable_pairing_mode();
}

static void list_pairings_handler(char *args_buf, unsigned int len)
{
  int i;
  char hexed[2 * MDG_PEER_ID_SIZE + 1];
  load_all_pairings_from_file();
  mdg_chat_output_fprintf("Listing pairings. Count=%d\n", chat_pairings_count);
  for (i = 0; i < chat_pairings_count; i++) {
    hex_encode_bytes(chat_pairings[i].peer_id, hexed, MDG_PEER_ID_SIZE);
    mdg_chat_output_fprintf("%s %s\n", hexed, chat_pairings[i].peer_name);
  }
}

static void remove_pairing_handler(char *args_buf, unsigned int len)
{
  unsigned char peer_id[MDG_PEER_ID_SIZE];
  if (!arg_parse_device_public_key(&args_buf, &len, peer_id)) {
    int s = mdg_revoke_pairing(peer_id);
    {
      char hexed[2 * MDG_PEER_ID_SIZE + 1];
      hex_encode_bytes(peer_id, hexed, MDG_PEER_ID_SIZE);
      mdg_chat_output_fprintf("mdg_revoke_pairing: returned=%d Peer=\"%s\"\n", s, hexed);
    }
  }
}

static void remove_all_pairings_handler(char *args_buf, unsigned int len)
{
#ifdef MDG_WINDOWS
  mdg_chat_output_fprintf("Not supported under Windows\n");
#else
  int s = mdg_revoke_all_pairings();
  mdg_chat_output_fprintf("mdg_revoke_all_pairings returned %d\n", s);
#endif
}

int add_pairing(uint8_t *peer_id, const char *peer_name)
{
  int i;
  load_all_pairings_from_file();
  for (i = 0; i < chat_pairings_count; ++i) {
    if (!memcmp(&chat_pairings[i].peer_id, peer_id, MDG_PEER_ID_SIZE)) {
      return MDGSTORAGE_OK; // Existing pairing matched.
    }
  }
  memcpy(&chat_pairings[chat_pairings_count].peer_id, peer_id, MDG_PEER_ID_SIZE);
  strncpy(chat_pairings[chat_pairings_count].peer_name, peer_name,
          sizeof(chat_pairings[chat_pairings_count].peer_name) - 1);
  chat_pairings_count++;
  save_all_pairings_to_file();
  return MDGSTORAGE_OK;
}

static void add_test_pairing_handler(char *args_buf, unsigned int len)
{
  uint8_t peer_id[MDG_PEER_ID_SIZE];
  if (!arg_parse_device_public_key(&args_buf, &len, peer_id)) {
    int s = add_pairing(peer_id, "Manually added");
    mdg_chat_output_fprintf("add_pairing returned %d\n", s);
  }
}

static void start_local_listener_handler(char *args_buf, unsigned int len)
{
  char *port_arg = args_buf;
  uint16_t port;
  if (!arg_decode_gotonext(&args_buf, &len)) {
    port = (uint16_t)strtol(port_arg, 0, 10);
  } else {
    port = 0;
  }

  int s = mdg_start_local_listener(&port);
  if (s != 0) {
    mdg_chat_output_fprintf("mdg_start_local_listener failed with %d\n", s);
  } else {
    mdg_chat_output_fprintf("mdg_start_local_listener success on port %d\n", port);
  }
}

#ifdef DEMO_FILE_DOWNLOAD
#include "mdgext_filedownload.h"
static uint32_t filedownload_cb(const unsigned char *data, uint32_t count,
                                mdgext_filedownload_status status)
{
  switch (status) {
  case mdgext_filedownload_completed_success:
    mdg_chat_output_fprintf("filedownload_cb invoked with status=completed_success\n");
    break;
  case mdgext_filedownload_size_known:
    mdg_chat_output_fprintf("filedownload_cb invoked with status=size_known, total size=%d\n", count);
    break;
  case mdgext_filedownload_datablock_included:
    mdg_chat_output_fprintf("filedownload_cb invoked with status=datablock_included of size %d\n", count);
    break;
  case mdgext_filedownload_checksum_valid:
    mdg_chat_output_fprintf("filedownload_cb invoked with status=checksum_valid of size %d\n", count);
    break;
  case mdgext_filedownload_no_update_available:
    mdg_chat_output_fprintf("filedownload_cb invoked with status=no_update_available\n");
    break;
  case mdgext_filedownload_failed:
    mdg_chat_output_fprintf("filedownload_cb invoked with status=failed (generic)\n");
    break;
  case mdgext_filedownload_checksum_failed:
    mdg_chat_output_fprintf("filedownload_cb invoked with status=checksum_failed\n");
    break;
  case mdgext_filedownload_connection_failed:
    mdg_chat_output_fprintf("filedownload_cb invoked with status=connection_failed\n");
    break;
  case mdgext_filedownload_starting_check:
    mdg_chat_output_fprintf("filedownload_cb invoked with status=starting_check\n");
    break;
  default:
    mdg_chat_output_fprintf("filedownload_cb invoked with other status=%d\n", status);
  }
  return 0;
}

mdg_property_t ota_props[] = {
  // Example properties. Make up your own for your application.
  {"domain_key", "chat-serial" },
  {"domain", "chat client" },
  {"sw", "0.0.0" },
  {0, 0 },
};

static uint8_t ota_signing_public[32] = {
97, 226, 31, 152, 143, 243, 80, 22,
79, 87, 91, 108, 128, 223, 9, 197,
177, 26, 4, 33, 105, 19, 30, 159,
226, 7, 149, 128, 66, 83, 162, 104
/* Signing secret: For REAL projects, DO NOT publish this secret key!
(Same secret hexed: feb1821f3aea327ceceab56106208e573fc96ed2bcd34c8f99d9787c4752a453)
254, 177, 130, 31, 58, 234, 50, 124,
236, 234, 181, 97, 6, 32, 142, 87,
63, 201, 110, 210, 188, 211, 76, 143,
153, 217, 120, 124, 71, 82, 164, 83
 */
};

extern const mdg_configuration mdg_configuration_tmdg82;
extern const mdg_configuration mdg_configuration_test;
extern const mdg_configuration mdg_configuration_prod01;

static void ota_demo_handler(char *args_buf, unsigned int len)
{
  static uint8_t init = 0;
  uint32_t s;
  if (!init) {
    init = 1;
    s = mdgext_filedownload_init();
    if (s != 0) {
      mdg_chat_output_fprintf("mdgext_filedownload_init failed with %d\n", s);
      return;
    }
  }

  if (init == 1) {
    init = 2;
    s = mdgext_filedownload_start(filedownload_cb,
                                  &mdg_configuration_test,
                                  ota_props,
                                  ota_signing_public,
                                  10);
    if (s != 0) {
      mdg_chat_output_fprintf("mdgext_filedownload_start failed with %d\n", s);
      return;
    }
  } else if (init == 2) {
    init = 1;
    s = mdgext_filedownload_cancel(filedownload_cb);
    if (s != 0) {
      mdg_chat_output_fprintf("mdgext_filedownload_cancel failed with %d\n", s);
      return;
    }
  }
}
#endif

static void kill_local_listener_handler(char *args_buf, unsigned int len)
{
  int s = mdg_stop_local_listener();
  if (s != 0) {
    mdg_chat_output_fprintf("mdg_stop_local_listener failed with %d\n", s);
  }
}

static void place_call_remote_handler(char *args_buf, unsigned int len)
{
  uint8_t device_id[MDG_PEER_ID_SIZE];
  char *protocol_arg;
  int s;
  uint32_t connection_id;

  arg_parse_device_public_key(&args_buf, &len, device_id);

  if (len > 0) {
      protocol_arg = args_buf;
      if (arg_decode_gotonext(&args_buf, &len)) {
        mdg_chat_output_fprintf("could not parse optional argument protocol\n");
        return;
      }
  } else {
    protocol_arg = DEVISMART_PROTOCOL_NAME;
  }

  s = mdg_place_call_remote(device_id, protocol_arg, &connection_id, pcr_timeout);
  if (s == 0) {
    mdg_chat_output_fprintf("Place call started, got connection_id=%d\n", connection_id);
  } else {
    mdg_chat_output_fprintf("Place call failed, got error=%d\n", s);
  }
}

static void pair_local_handler(char *args_buf, unsigned int len)
{
  int s;
  char *otp_arg;
  char* peer_ip;
  int port;

  otp_arg = args_buf;
  if (arg_decode_gotonext(&args_buf, &len)) {
    mdg_chat_output_fprintf("pair local: Missing required arg, OTP.\n");
    return;
  }

  peer_ip = args_buf;
  if (arg_decode_gotonext(&args_buf, &len)) {
    mdg_chat_output_fprintf("pair local: Missing required arg, host.\n");
    return;
  }

  if (set_intparam_handler(&args_buf, &len, "port", &port, 1, 0xffff) != 0) {
    return;
  }

  s = mdg_pair_local(otp_arg, peer_ip, (uint16_t) port);
  if (s != 0) {
    mdg_chat_output_fprintf("mdg_pair_local failed with %d\n", s);
  }
}

static void place_call_local_handler(char *args_buf, unsigned int len)
{
  int s;
  char* peer_ip;
  char *protocol_arg;
  uint8_t device_id[MDG_PEER_ID_SIZE];
  uint32_t connection_id;
  int port;

  if (arg_parse_device_public_key(&args_buf, &len, device_id) != 0) {
    return;
  }

  peer_ip = args_buf;
  if (arg_decode_gotonext(&args_buf, &len)) {
    mdg_chat_output_fprintf("place call local: Missing required arg, host.\n");
    return;
  }

  if (len > 0) {
    if (set_intparam_handler(&args_buf, &len, "port", &port, 1, 0xffff) != 0) {
      return;
	}
  } else {
	port = DEVISMART_LOCAL_PORT;
  }

  if (len > 0) {
    protocol_arg = args_buf;
    if (arg_decode_gotonext(&args_buf, &len)) {
      mdg_chat_output_fprintf("place call local: Could not parse optional arg, protocol.\n");
      return;
    }
  } else {
    protocol_arg = "chat-client";
  }

  s = mdg_place_call_local(device_id, protocol_arg, peer_ip, (uint16_t) port,
                           &connection_id, pcr_timeout);
  if (s == 0) {
    mdg_chat_output_fprintf("Place call started, got connection_id=%d\n", connection_id);
  } else {
    mdg_chat_output_fprintf("Place call failed, got error=%d\n", s);
  }
}



static char PROTOCOL_BOOTSTRAP[] = "<wifibootstrap>";

// Example bootstrap of wifi code.
#define WIFI_BOOTSTRAP_MAX_SSID_BYTES 64
#define WIFI_BOOTSTRAP_MAX_PASS_BYTES 64
struct mdgdemo_wifi_bootstrap_info {
  char ssid[WIFI_BOOTSTRAP_MAX_SSID_BYTES];
  char pass[WIFI_BOOTSTRAP_MAX_PASS_BYTES];
};

static int bootstrap_server_got_data(const unsigned char *data,
                                     unsigned int count)
{
  if (count == sizeof(struct mdgdemo_wifi_bootstrap_info)) {
    struct mdgdemo_wifi_bootstrap_info *info =
      (struct mdgdemo_wifi_bootstrap_info*)data;
    mdg_chat_output_fprintf("Received ssid:%.*s\n",
                            WIFI_BOOTSTRAP_MAX_SSID_BYTES, info->ssid);
    mdg_chat_output_fprintf("Received pass:%.*s\n",
                            WIFI_BOOTSTRAP_MAX_PASS_BYTES, info->ssid);

  } else {
    return 1;
  }
  return 0;
}

static int auto_pair = 1;

static void auto_pair_handler(char *args_buf, unsigned int len)
{
  char *arg = args_buf;

  if (!arg_decode_gotonext(&args_buf, &len)) {
    if (strcmp(arg, "0") == 0 || stricmp(arg, "off") == 0)
      auto_pair = 0;
    else
      auto_pair = 1;
  }
  mdg_chat_output_fprintf("auto-add-pairing: %s\n", auto_pair ? "enabled" : "disabled");
}

static void basic_help_handler(char *args_buf, unsigned int len);
static void advanced_help_handler(char *args_buf, unsigned int len);
static const struct cmd advanced_commands[] = {
  {"/c", "/connect",
   "Set connection-wanted flag to true",
   connect_handler },
  {"/d", "/disconnect",
   "set connection-wanted flag to false",
   disconnect_handler },
  {"/s-a", "/send-to-peer-append",
   "Send a text message. Args: conn.-id, flush_now, rest of line is msg.",
   send_append_handler},
  {"/sx-a", "/send-hex-to-peer-append",
   "Send a binary message. Arg: conn-id, flush_now, rest of line is msg.",
   send_hex_append_handler},
  {"/atp", "/add-test-pairing",
   "Add (fake) pairing, provide device-id as arg",
   add_test_pairing_handler },
  {"/hexm", "/hex-mode",
   "Set whether input from peer should output in hex, 0/1/2 as optional arg",
   set_hex_output_mode_handler },
  {"/agp", "/agressive_ping",
   "Invokes mdg_aggressive_ping, provide duration as optional arg, 0 to cancel",
   aggressive_ping_handler },
  { "/pl", "/pair-local",
    "Pair local, Required args: OTP, target ip, target port",
    pair_local_handler },
  { "/pcl", "/place-call-local",
    "Args: device-id, target ip, target port. Optional protocol arg",
    place_call_local_handler },
  {"/spct", "/set-place-call-timeout",
   "Timeout in seconds, for place-call to complete. Default: 5",
   set_pc_timeout_handler},
  {"/t_dic", "/test_delay_call",
   "Set delay in seconds, for responding to incoming call. Default: 0",
   set_incomingcall_testdelay_handler},
  {"/t_rand", "/test_random_func",
   "Invoke random func, for testing. Prints 32 bytes data as hex.",
   test_random_handler},
#ifndef MDGEXT_NO_CLIENT_DEBUG
  {"/x_loc", "/x_test_log_loc",
   "Invoke single line remote log of file/lineno.",
   demo_secure_log_loc_handler},
#endif
#ifndef MDGEXT_NO_SERVER_PUSH
  {"/x_rpt", "/x_reg_push",
   "Register push token at server. Args: pubkey, token, locale.",
   demo_mdgext_register_peer_token},
#endif
#ifndef MDGEXT_NO_INVOKE_SERVICE
  {"/x_tsrv", "/x_test_service",
   "Test remote service api.",
   demo_mdgext_invoke_test_service},
#endif
  {"/apm", "/access-point-mode",
   "Toggles access point simulation mode, 0/1 as optional arg",
   set_ap_mode_handler},
  {"/sll", "/start-local-listener",
   "Start (or restart) local connection listener. Port as optional arg",
   start_local_listener_handler},
  {"/kll", "/kill-local-listener",
   "Kill local connection listener",
   kill_local_listener_handler},
#ifdef DEMO_FILE_DOWNLOAD
  {"/ota", "/ota-demo",
   "Download a file and verify checksum/signature",
   ota_demo_handler},
#endif
  {"/aap", "/auto-add-pairing",
   "Enable or disable to automatically add pairings after OTP pairing",
   auto_pair_handler },
  { 0, 0, 0, 0 }
};

static const struct cmd basic_commands[] = {
  {"/email", "/email",
   "Set email address in client properties",
   email_handler },
  {"/s", "/status",
   "Display status from mdg lib",
   status_handler },
  {"/who", "/whoami",
   "Print my own device_id in hex",
   whoami_handler },
  {"/op", "/open-for-pairing",
   "Open for pairing, optional timeout as arg, default 120",
   open_for_pairing_handler },
  {"/opf", "/open-for-pairing-preset",
   "Open for pairing, required OTP as arg.",
   open_for_pairing_handler_preset_otp },
  {"/cpm", "/cancel-pairing-mode",
   "Cancel any ongoing pairing", cancel_pairing_mode_handler },
  {"/pair", "/pair-remote",
   "Pair with a peer, Required OTP as arg",
   pair_remote_handler },
  {"/pcr", "/place-call-remote",
   "Connect to a paired peer. Arg: device-id. Optional protocol arg",
   place_call_remote_handler },
  {"/send", "/send-data-to-peer",
   "Send a text message. Arg: connection-id, rest of line is message.",
   send_handler},
  {"/send-hex", "/send-hex-data-to-peer",
   "Send a binary message. Arg: connection-id, rest of line is message.",
   send_hex_handler},
  {"/info", "/info-on-connection",
   "Who is at other end of connection? Arg: connection-id",
   conninfo_handler},
  {"/close", "/close-connection",
   "Close connection (Or pending connection) Arg: connection-id",
   close_conn_handler},
  {"/lp", "/list-pairings",
   "List paired peers, one device_id pr. line",
   list_pairings_handler },
  {"/rmp", "/remove-pairing",
   "Remove pairing, provide device-id as arg",
   remove_pairing_handler },
  {"/rmall", "/remove-all-pairings",
   "Remove all pairings - security reset",
   remove_all_pairings_handler },
  {"/h", "/help-basic",
   "Display this help",
   basic_help_handler },
  {"/h2", "/help-advanced",
   "Display help on advanced and testing commands",
   advanced_help_handler },
  {"/q", "/quit",
   "Quit chat client",
   quit_handler },
  { 0, 0, 0, 0 }
};

static void do_help_handler(const struct cmd *cmd)
{
  mdg_chat_output_fprintf("Usage:\n");
  mdg_chat_output_fprintf("  In general, commands and their output incl errors show up on stdout.\n"
          "  stdout is for help text and internal debug logging output only.\n"
          "  stdout is for remote control readability by a program.\n================\n"
          "SHORT\tLONG\n");
  for (; cmd->short_name != 0; cmd++) {
    mdg_chat_output_fprintf(" %-9s%-25s%s\n", cmd->short_name, cmd->long_name, cmd->help_text);
  }
}

static void basic_help_handler(char *args_buf, unsigned int len)
{
  do_help_handler(basic_commands);
}
static void advanced_help_handler(char *args_buf, unsigned int len)
{
  do_help_handler(advanced_commands);
}


static const struct cmd* find_command(const struct cmd *cmd, char *input)
{
  for (; cmd->short_name != 0; cmd++) {
    if (!strcmp(cmd->short_name, input)) {
      return cmd;
    } else if (!strcmp(cmd->long_name, input)) {
      return cmd;
    }
  }
  return 0;
}

void mdg_chat_client_input(char *in_buf, unsigned int len)
{
  const struct cmd *cmd;
  unsigned int i, args_len;
  char *args;
  for (i = 0; i < len; i++) {
    if (in_buf[i] == ' ') {
      break;
    }
  }
  in_buf[i] = 0; // make cmd a string of it's own.
  if (i == len) {
    args = "";
    args_len = 0;
  } else {
    i++;
    for ( ; i < len; i++) {
      if (in_buf[i] != ' ') {
        break;
      }
    }
    args = &in_buf[i];
    args_len = len - i;
    args[args_len] = 0; // Teminate args string as well.
  }
  cmd = find_command(basic_commands, in_buf);
  if (cmd == 0) {
    cmd = find_command(advanced_commands, in_buf);
  }
  if (cmd != 0) {
    cmd->handler(args, args_len);
  } else {
    mdg_chat_output_fprintf("Unknown command %s\n", in_buf);
  }
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))
void chatclient_hexdump(const uint8_t *data, uint32_t count)
{
  char buf[512];
  int bytes_left;

  for (bytes_left = count; bytes_left > 0; ) {
    int bytes_now = MIN(bytes_left, 512/2);

    hex_encode_bytes(data, buf, bytes_now);
    mdg_chat_output_fprintf("%.*s", bytes_now * 2, buf);
    bytes_left -= bytes_now;
    data += bytes_now;
  }
  mdg_chat_output_fprintf("\n");
}

static int chatclient_print_data_received(const uint8_t *data,
                                           const uint32_t count,
                                           const uint32_t connection_id)
{
  switch (hex_output_mode) {
  case 2: {
    char buf[512];
    mdg_chat_output_fprintf("Received data from peer on connection %d, "
                            "count=%u hexbytes follow:\n",
                            connection_id, count);
    int bytes_left;
    for (bytes_left = count; bytes_left > 0; ) {
      int bytes_now = MIN(bytes_left, 4);
      hex_encode_bytes(data, buf, bytes_now);
      mdg_chat_output_fprintf("  %.*s\n", bytes_now * 2, buf);
      bytes_left -= bytes_now;
      data += bytes_now;
    }
  } break;

  case 1: {
    mdg_chat_output_fprintf("Received data from peer on connection %d, "
                            "count=%u, hexbytes: ",
                            connection_id, count);
    chatclient_hexdump(data, count);
  } break;

  case 0:
  default: {
    mdg_chat_output_fprintf("Received data from peer on connection %d: %.*s\n",
                            connection_id, count, data);
  } break;
  }
  return 0;
}

static void chatclient_data_received(const uint8_t *data, const uint32_t count, const uint32_t connection_id)
{
  uint8_t sender_device_id[MDG_PEER_ID_SIZE];
  char protocol[MAX_PROTOCOL_BYTES];
  int s;

  s = get_connection_info(connection_id, sender_device_id, protocol);
  if (s == 0) {
    if (!memcmp(PROTOCOL_BOOTSTRAP, protocol, MAX_PROTOCOL_BYTES)) {
      s = bootstrap_server_got_data(data, count);
	} else if (!strcmp(PROTOCOL_DEVISMART_CONFIG, protocol)) {
	  s = devismart_receive_config_data(data, count);
	} else if (!strcmp(DEVISMART_PROTOCOL_NAME, protocol)) {
	  s = devismart_receive_data(data, count, connection_id);
    } else {
      s = chatclient_print_data_received(data, count, connection_id);
    }
  }
  if (s == 0) {
    s = mdg_receive_from_peer(connection_id, chatclient_data_received);
    if (s != 0) {
      mdg_chat_output_fprintf("mdg_receive_from_peer failed\n");
    }
  }

  if (s != 0) {
    mdg_close_peer_connection(connection_id);
  }
}

// Callbacks invoked by MDG lib.
void mdguser_routing(uint32_t connection_id, mdg_routing_status_t state)
{
  switch (state) {
  case mdg_routing_disconnected:
    mdg_chat_output_fprintf("Callback: mdguser_routing connection_id=%d, state=disconnected\n", connection_id);
    break;
  case mdg_routing_connected:
      mdg_chat_output_fprintf("Callback: mdguser_routing connection_id=%d, state=connected\n", connection_id);
      break;
  case mdg_routing_failed:
    mdg_chat_output_fprintf("Callback: mdguser_routing connection_id=%d, state=failed\n", connection_id);
    break;
  case mdg_routing_peer_not_available:
    mdg_chat_output_fprintf("Callback: mdguser_routing connection_id=%d, state=peer_not_available\n", connection_id);
    break;
  case mdg_routing_peer_not_paired: // Note! server is allowed to respond "not available"...
    mdg_chat_output_fprintf("Callback: mdguser_routing connection_id=%d, state=peer_not_paired\n", connection_id);
    break;
  default:
    mdg_chat_output_fprintf("Callback: mdguser_routing connection_id=%d, state=%d\n", connection_id, state);
    break;
  }

  if (state == mdg_routing_connected) {
    if (mdg_receive_from_peer(connection_id, chatclient_data_received)) {
      mdg_chat_output_fprintf("mdg_receive_from_peer failed\n");
    }
	
    char protocol[MAX_PROTOCOL_BYTES];
	int s = get_connection_info(connection_id, NULL, protocol);
	if (s == 0) {
	  if (!strcmp(protocol, PROTOCOL_DEVISMART_CONFIG))
	    devismart_request_configuration(connection_id);
	  else if (!strcmp(protocol, DEVISMART_PROTOCOL_NAME))
		devismart_connection_begin(connection_id);
	}
  }
}

// Callbacks invoked by MDG lib.
int mdgstorage_load_pairing(int pairings_index, uint8_t *peer_id)
{
  Log("%s(%d) called\n", __FUNCTION__, pairings_index);

  load_all_pairings_from_file();
  if (pairings_index < chat_pairings_count) {
    memcpy(peer_id, chat_pairings[pairings_index].peer_id, MDG_PEER_ID_SIZE);
    return MDGSTORAGE_OK;
  }
  return 1;
}

// Callbacks invoked by MDG lib.
int mdgstorage_add_pairing(uint8_t *peer_id)
{
  char buf[AS_HEX(MDG_PEER_ID_SIZE)];

  hex_encode_bytes(peer_id, buf, MDG_PEER_ID_SIZE);
  if (auto_pair)
  {
    Log("%s(%s) called\n", __FUNCTION__, buf);
    return add_pairing(peer_id, "Paired by MDG");
  }
  else
  {
    mdg_chat_output_fprintf("IGNORED adding peer ID %s\n", buf);
    return 0;
  }
}

// Callbacks invoked by MDG lib.
int mdgstorage_remove_pairing(unsigned char *peer_id)
{
  Log("%s(", __FUNCTION__); Hexdump(peer_id, MDG_PEER_ID_SIZE); Log(") called\n");
 
  int i = 0, j;
  load_all_pairings_from_file();
  while (i < chat_pairings_count) {
    if (!memcmp(&chat_pairings[i].peer_id, peer_id, MDG_PEER_ID_SIZE)) {
      j = i+1;
      if (j < chat_pairings_count) {
        // overwrite pairing with remaining pairings.
        while (j < chat_pairings_count) {
          memcpy(&chat_pairings[j-1], &chat_pairings[j], sizeof(pairing_t));
          j++;
        }
        memset(&chat_pairings[j-1], 0, sizeof(pairing_t));
        chat_pairings_count--;
      } else {
        // Wipe pairing.
        memset(&chat_pairings[i], 0, sizeof(pairing_t));
        i++;
        if (i == chat_pairings_count) {
          chat_pairings_count--;
        }
      }
    } else {
      i++;
    }
  }

  save_all_pairings_to_file();
  return MDGSTORAGE_OK;
}

// Callbacks invoked by MDG lib.
int32_t mdguser_incoming_call(const char *protocol)
{
  mdg_chat_output_fprintf("Got incoming call for protocol %s\n", protocol);
  if (incomingcall_testdelay > 0) {
    mdg_chat_output_fprintf("Delaying response for incoming call for %d seconds\n", incomingcall_testdelay);
#if __linux
	sleep(incomingcall_testdelay);
#elif MDG_WINDOWS
	Sleep(incomingcall_testdelay * 1000);
#else
	mdg_chat_output_fprintf("The response for incoming call could not be delayed as sleep is not supported on the current platform.\n");//TODO
	return 0;
#endif
    mdg_chat_output_fprintf("Delaying response for incoming call for %d seconds completed.\n", incomingcall_testdelay);
  }
  return 0; // Accept call.
}

// Callbacks invoked by MDG lib.
int mdgstorage_load_preset_pairing_token(char *printedLabelOtp)
{
  Log("%s() called\n", __FUNCTION__);

  if (preset_otp_buffer[0] != 0) {
    int otp_len = strlen(preset_otp_buffer);
    if (otp_len < MDG_PRESET_PAIR_ID_SIZE) {
      mdg_chat_output_fprintf("mdgstorage_load_preset_pairing_token: "
                              "using preset OTP og len %d\n", otp_len);
      memcpy(printedLabelOtp, preset_otp_buffer, otp_len);
      printedLabelOtp[otp_len] = 0;
    } else {
      // Pre-def otp too long. Fail.
      return 1;
    }
  } else {
      mdg_chat_output_fprintf("mdgstorage_load_preset_pairing_token: "
                              "using random OTP\n");
    printedLabelOtp[0] = 0;
  }
  return 0;
}

// Callbacks invoked by MDG lib.
int mdgstorage_load_license_key(void *license_key)
{
  Log("%s() called\n", __FUNCTION__);
  return 1;
}

// Load/create+store private key for this instance in local filesystem.
static void chatclient_load_or_create_private_key(uint8_t *pk)
{
#if defined(MDG_CC3200) || defined(MDG_UCOS)
  _i32 s = 0;
  _i32 f;
  s = sl_FsOpen("chat_demo_private.key", FS_MODE_OPEN_READ, NULL, &f);
  if (s == 0) {
    s = sl_FsRead(f, NULL, pk, MDG_PEER_ID_SIZE);
    sl_FsClose(f, NULL, NULL, 0);
  }
  if (s != MDG_PEER_ID_SIZE)
  {
    mdg_make_private_key(pk);
    s = sl_FsOpen("chat_demo_private.key", FS_MODE_OPEN_CREATE(3584, 0), NULL, &f);
    if (s == 0) {
      s = sl_FsWrite(f, 0, pk, MDG_PEER_ID_SIZE);
      sl_FsClose(f, NULL, NULL, 0);
    }
  }
#else
  FILE *f = fopen("chat_demo_private.key", "rb");
  int s = 0;
  if (f != NULL) {
    s = fread(pk, 1, MDG_PEER_ID_SIZE, f);
    fclose(f);
  }
  if (s != MDG_PEER_ID_SIZE) {
    mdg_make_private_key(pk);
    f = fopen("chat_demo_private.key", "wb");
    if (f != NULL) {
      s = fwrite(pk, 1, MDG_PEER_ID_SIZE, f);
      fclose(f);
    }
  }
#endif
}

static uint8_t chatclient_private_key[MDG_PRIVATE_KEY_DATA_SIZE];
static uint8_t chatclient_private_key_set = 0;

uint8_t *chatclient_get_private_key(void)
{
  if (!chatclient_private_key_set) {
    chatclient_load_or_create_private_key(chatclient_private_key);
	chatclient_private_key_set = 1;
  }
  
  return chatclient_private_key;
}

int mdgstorage_load_private_key(void *private_key)
{
  Log("%s() called\n", __FUNCTION__);

  memcpy(private_key, chatclient_get_private_key(), MDG_PRIVATE_KEY_DATA_SIZE);
  return 0;
}

void chatclient_set_private_key(uint8_t *pk)
{
  if (pk == 0) {
    memset(chatclient_private_key, 0, MDG_PRIVATE_KEY_DATA_SIZE);
    chatclient_private_key_set = 0;
  } else {
    memcpy(chatclient_private_key, pk, MDG_PRIVATE_KEY_DATA_SIZE);
    chatclient_private_key_set = 1;
  }
}

void chatclient_parse_cmd_args(int c, char**a)
{
  // Never replace key again...
  if (chatclient_private_key_set == 0) {
    if (c > 1) {
      uint8_t pk[MDG_PRIVATE_KEY_DATA_SIZE];
      char hexed[MDG_PRIVATE_KEY_DATA_SIZE * 2 + 1];
      memset(pk, 0, MDG_PRIVATE_KEY_DATA_SIZE);
      hex_decode_bytes(a[1], pk, MDG_PRIVATE_KEY_DATA_SIZE);
      hex_encode_bytes(pk, hexed, MDG_PRIVATE_KEY_DATA_SIZE);
      mdg_chat_output_fprintf("Chat-client, my private key %s\n", hexed);
      chatclient_set_private_key(pk);
    } else {
      chatclient_set_private_key(0);
    }
  }
}
