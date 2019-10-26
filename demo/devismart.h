#define PROTOCOL_DEVISMART_CONFIG "dominion-configuration-1.0"

void devismart_request_configuration(uint32_t connection_id);
uint32_t devismart_receive_config_data(const uint8_t *data, uint32_t size);
