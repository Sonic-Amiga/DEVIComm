#define STR_PEER_ID_SIZE (2 * MDG_PEER_ID_SIZE + 1)

void mdg_chat_output_fprintf(const char *fmt, ...);
uint8_t *chatclient_get_private_key(void);
int add_pairing(uint8_t *peer_id, const char *peer_name);
