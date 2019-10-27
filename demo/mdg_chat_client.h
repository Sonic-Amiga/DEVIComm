#define AS_HEX(size) (2 * size + 1)

void mdg_chat_output_fprintf(const char *fmt, ...);
void chatclient_hexdump(const uint8_t *data, uint32_t count);
uint8_t *chatclient_get_private_key(void);
int add_pairing(uint8_t *peer_id, const char *peer_name);
void whoami_hex(char *hexed);