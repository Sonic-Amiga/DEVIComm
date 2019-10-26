#ifndef __MDG_PEER_API
#define __MDG_PEER_API
#include <stdint.h>

#ifndef MDG_MAX_PAIRING_COUNT
# define MDG_MAX_PAIRING_COUNT 32
#endif
#ifndef MDG_MAX_CONNECTION_COUNT
# define MDG_MAX_CONNECTION_COUNT 16
#endif

#define PAIRING_DURATION_LIMIT 120

#define MDG_PEER_ID_SIZE 32
#define MDG_PEER_PUBLIC_KEY_SIZE 32
#define MDG_LICENSEKEY_SIZE 128
#define MAX_ENDPOINT_BYTES 48
#define MAX_PROTOCOL_BYTES 40
#define MAX_TOKEN_BYTES 40
#define MAX_TICKET_BYTES 40
#define MAX_ENDPOINT_ROUTES 4
#define MAX_OTP_BYTES 32
/* Lenght of part of otp, that is never sent to server: */
#define LOCAL_PART_OTP_LEN 3
/* Rest of OTP is for server (when remote) and at least this many bytes: */
#define SERVER_PART_OTP_MIN_LEN 4

#define MDG_CONNECTION_STATUS_PEER_OFFLINE 0
#define MDG_CONNECTION_STATUS_PEER_LOCAL   1
#define MDG_CONNECTION_STATUS_PEER_GRID    2

/* Holds internal type definitions for portability: */
#include "mdg_internal_t.h"

typedef struct {
  char *name;
  uint16_t port;
} mdg_control_server_endpoint;

typedef struct {
  const mdg_control_server_endpoint **servers;
  const uint8_t **trusted_server_keys;
  const char *mdns_name;
} mdg_configuration;

_MDG_API_ int32_t mdg_set_configuration(const mdg_configuration *conf);
#if 0
// Example configuration for two servers:
static const mdg_control_server_endpoint dkhc01mdg03 = { "77.66.11.95", 443 };
static const mdg_control_server_endpoint localhost = { "127.0.0.1", 443 };
static const mdg_control_server_endpoint *mdg_control_hosts_demo[] = {
  &dkhc01mdg03,
  &localhost,
  0
};

// Test server public key.
static const uint8_t test_ctrl_server_key[32] = {
  81,13,101,52,29,109,136,196,
  86,91,34,91,3,19,150,3,
  215,43,210,9,242,146,119,188,
  153,245,78,232,94,113,37,47
};
// Example config for trust server. Lists only test server key.
static const uint8_t *trusted_server_keys_default[] = {
  test_ctrl_server_key,
  0
};

static const mdg_configuration demo_mdg_configuration = {
  mdg_control_hosts_demo,
  trusted_server_keys_default,
  0 //Use no mDNS name.
};

// And then in main...:
int main() {
  mdg_set_configuration(&demo_mdg_configuration);
  if ((s = mdg_init(0)) != 0) {
    fprintf(stderr, "mdg_init failed with %d\n", s);
    return -1;
  }
}
#endif
_MDG_API_ int32_t mdg_init(uint32_t flags);
_MDG_API_ void mdg_run_main_loop();

typedef void* mdg_peer_id_t;
typedef enum {
  mdg_pairing_mode_disabled = 0,
  mdg_pairing_mode_waiting_for_pairing_attempt = 1,
  mdg_pairing_mode_pairing_attempt_in_progress = 2,
} mdg_pairing_mode;

typedef struct {
  uint32_t unixtime_utc;    /* Zero when not known, otherwise "UTC seconds since epoch=1970". */
  uint8_t connect_switch;   /* 0/1 does client desire a mdg grid connection */
  uint8_t connected_to_mdg; /* 0/1 does mdg consider this device to be connected to mdg grid? */
  mdg_pairing_mode pairing_mode;     /* State of pairing state machine */
  uint8_t aggressive_ping;  /* 0/1 is aggressive ping enabled? */
  uint8_t remote_logging_enabled; /* Is remote logging enabled at the moment? */
} mdg_status_t;

_MDG_API_ int32_t mdg_status(mdg_status_t *status);
_MDG_API_ int32_t mdg_aggressive_ping(uint32_t duration);
_MDG_API_ int32_t mdg_make_private_key(void *private_key);
_MDG_API_ int32_t mdg_whoami(mdg_peer_id_t *device_id);
typedef struct {
  char *name;
  char *value;
} mdg_property_t;
_MDG_API_ int32_t mdg_connect(mdg_property_t *properties);
_MDG_API_ int32_t mdg_disconnect(void);

typedef struct {
  mdg_pairing_mode pairing_mode;
  uint32_t start_timestamp;
  uint32_t end_timestamp;
  char otp[MAX_OTP_BYTES];
  mdg_peer_id_t new_peer;
} mdg_pairing_mode_state_t;


typedef enum {
        mdg_pairing_starting = 0,
        mdg_pairing_otp_ready = 1,
        mdg_pairing_completed = 2,
        mdg_pairing_failed_generic = 3,
        mdg_pairing_failed_otp = 4
} mdg_pairing_status;

/* Callback invoked by MDG on state changes. Do not refer state after returning. */
extern void mdguser_pairing_state(mdg_pairing_mode_state_t* state, mdg_pairing_status status);
_MDG_API_ int32_t mdg_enable_pairing_mode(uint32_t duration);
_MDG_API_ void mdg_disable_pairing_mode(void);
_MDG_API_ int32_t mdg_revoke_pairing(mdg_peer_id_t device_id);
_MDG_API_ int32_t mdg_add_pairing(mdg_peer_id_t device_id);
_MDG_API_ int32_t mdg_revoke_all_pairings(void);
_MDG_API_ void mdg_notify_pairings_changed();
_MDG_API_ int32_t mdg_pair_remote(char *otp);
_MDG_API_ int32_t mdg_pair_local(char *otp, char *peer_ip, uint16_t port);
typedef enum {
        mdg_control_not_desired = 0,
        mdg_control_connecting = 1,
        mdg_control_connected = 2,
        mdg_control_failed = 3
} mdg_control_status_t;

/* Implement this callback: */
void mdguser_control_status(mdg_control_status_t state);
typedef enum {
        mdg_routing_disconnected = 0,
        mdg_routing_connected = 1,
        mdg_routing_failed = -1,
        mdg_routing_peer_not_available = -2,
        mdg_routing_peer_not_paired = -3
} mdg_routing_status_t;

/* Implement this callback: */
void mdguser_routing(uint32_t connection_id, mdg_routing_status_t state);
_MDG_API_ int32_t mdg_place_call_remote(mdg_peer_id_t device_id, char *protocol, uint32_t *connection_id, uint32_t timout);
_MDG_API_ int32_t mdg_place_call_local(mdg_peer_id_t device_id, char *protocol, char *peer_ip, uint16_t port, uint32_t *connection_id, uint32_t timeout);
typedef void (*mdg_receive_data)(const unsigned char *data, uint32_t count, uint32_t connection_id);
/* Arguments sent when invoking mdg_receive_data:
 * `data` = memory buffer holding data received. Only valid during invocation of call.
 * `count` = number of bytes in buffer
 * `connection_id` that identifies this connection. (Use in `mdg_send_to_peer` when sending data)
 */
_MDG_API_ int32_t mdg_receive_from_peer(uint32_t connection_id, mdg_receive_data cb);
_MDG_API_ int32_t mdg_send_to_peer(uint8_t *data, uint32_t count, uint32_t connection_id);
_MDG_API_ int32_t mdg_send_to_peer_append(uint8_t *data, uint32_t count,
                                          uint32_t connection_id, uint32_t flush_now);
_MDG_API_ int32_t mdg_close_peer_connection(uint32_t connection_id);
extern int32_t mdguser_incoming_call(const char *protocol);
typedef int32_t (*mdg_peer_verifying_cb)(const char *protocol, const mdg_peer_id_t device_id);
_MDG_API_ void mdg_set_peer_verifying_cb(mdg_peer_verifying_cb cb);
#ifdef DEVISMART_API
_MDG_API_ int32_t mdg_get_connection_info(uint32_t connection_id,
                                          mdg_peer_id_t sender_device_id,
                                          char protocol[MAX_PROTOCOL_BYTES],
										  mdg_peer_id_t unknown);
#else
_MDG_API_ int32_t mdg_get_connection_info(uint32_t connection_id,
                                          mdg_peer_id_t sender_device_id,
                                          char protocol[MAX_PROTOCOL_BYTES]);
#endif
_MDG_API_ int32_t mdg_enable_remote_logging(uint32_t duration);
_MDG_API_ int32_t mdg_set_debug_log_target(int32_t target);
_MDG_API_ int32_t mdg_start_local_listener(uint16_t *port);
_MDG_API_ int32_t mdg_stop_local_listener();
typedef struct {
  char *name;
  int64_t value;
} mdg_metric_t;

_MDG_API_ int32_t mdg_send_client_metrics(mdg_property_t *properties,
                                          mdg_metric_t *metrics);
_MDG_API_ int32_t mdg_mutex_lock(void);
_MDG_API_ void mdg_mutex_unlock(void);
_MDG_API_ int32_t mdg_mutex_init(void);
typedef enum {
        mdg_service_running = 0,
        mdg_service_release = 1,
        mdg_service_completed = 2,
        mdg_service_failed = -1,
} mdg_service_status_t;

typedef enum {
        mdg_test_internal = 0,
        mdg_push_json_android = 1,
        mdg_push_json_ios = 2
} mdg_service_id;

typedef struct mdgext_service_invocation mdgext_service_invocation;

typedef void (*mdg_service_cb)(mdgext_service_invocation * invocation,
                               mdg_service_status_t state,
                               const unsigned char *data,
                               const uint32_t count);

struct mdgext_service_invocation {
        uint32_t internal_id;
        uint32_t internal_state;
        mdg_service_id service_id;
        mdg_property_t *properties;
        void *req_data;
        uint32_t req_data_count;
        mdg_service_cb cb;
};

/* Arguments set when invoking mdg_service_data:
 * `internal_*` are internal to the library - do not modify these.
 * `invocation` that identifies this connection to the caller. (The pointer given to `mdg_invoke_service`).
 * `state` indicates the state change. (Not all state changes occur for all services.)
 * `data` = memory buffer holding data received. Only valid during invocation of call.
 * `count` = number of bytes in buffer
 * `data` and `count` are both zero when no data is relevant, such as for status=mdg_service_failed.
 */

_MDG_API_ int32_t mdgext_invoke_service(mdgext_service_invocation *invocation);
_MDG_API_ int32_t mdgext_abort_service(mdgext_service_invocation *invocation);
/* DRAFT API for logging - Macros in public API is perhaps not so great... */
_MDG_API_ void mdg_secure_log_flush();
#if defined(MDG_CC3200)
#define SECURE_LOG_LOC() mdgext_secure_log_line(SECURE_LOG_MODULE_NO,__LINE__)
#define SECURE_LOG_PRINTF(a) mdgext_secure_log_line_printf(SECURE_LOG_MODULE_NO,__LINE__,a)
#define SECURE_LOG_PRINTF(a,b) mdgext_secure_log_line_printf(SECURE_LOG_MODULE_NO,__LINE__,a,b)
#define SECURE_LOG_PRINTF(a,b,c) mdgext_secure_log_line_printf(SECURE_LOG_MODULE_NO,__LINE__,a,b,c)
#define SECURE_LOG_HEXBLOB(data,count) mdgext_secure_log_line_hexblob(SECURE_LOG_MODULE_NO,__LINE__,data,count)
#elif __STDC_VERSION__ >= 199901L || defined(MDG_WINDOWS) || defined(MDG_UCOS)
/* C99 version: */
#include <stdio.h>
#define SECURE_LOG_LOC() mdgext_secure_log_line(SECURE_LOG_MODULE_NO,__LINE__)
#define SECURE_LOG_PRINTF(...) mdgext_secure_log_line_printf(SECURE_LOG_MODULE_NO,__LINE__,__VA_ARGS__)
#define SECURE_LOG_HEXBLOB(data,count) mdgext_secure_log_line_hexblob(SECURE_LOG_MODULE_NO,__LINE__,data,count)
#else
/* GCC version: */
#include <stdio.h>
#define SECURE_LOG_LOC() mdgext_secure_log_line(SECURE_LOG_MODULE_NO,__LINE__)
#define SECURE_LOG_PRINTF(args...) mdgext_secure_log_line_printf(SECURE_LOG_MODULE_NO,__LINE__,args)
#define SECURE_LOG_HEXBLOB(data,count) mdgext_secure_log_line_hexblob(SECURE_LOG_MODULE_NO,__LINE__,data,count)
#endif


_MDG_API_ void mdgext_secure_log_line(uint32_t module,uint32_t lineno);
_MDG_API_ void mdgext_secure_log_line_printf(uint32_t module,uint32_t lineno, const char *fmt,...);
_MDG_API_ void mdgext_secure_log_line_hexblob(uint32_t module,uint32_t lineno,void *data,uint32_t count);
typedef enum {
  mdgext_peer_kind_android = 0,
  mdgext_peer_kind_ios = 1,
  mdgext_peer_kind_web = 2,
} mdgext_peer_kind_t;

typedef enum {
  mdgext_push_on_connection_lost_default = 0,
} mdgext_push_subscription_kind;

_MDG_API_ int32_t mdgext_register_peer_token(mdg_peer_id_t peer,
                                             void *token, uint32_t token_len,
                                             mdgext_peer_kind_t peer_kind,
                                             char *locale_code,
                                             mdgext_push_subscription_kind sub_kind,
                                             uint32_t flags);
#endif
