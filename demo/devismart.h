#define PROTOCOL_DEVISMART_CONFIG "dominion-configuration-1.0"
#define PROTOCOL_DEVISMART_THERMOSTAT "dominion-1.0"

void devismart_request_configuration(uint32_t connection_id);
uint32_t devismart_receive_config_data(const uint8_t *data, uint32_t size);

uint32_t devismart_connection_begin(uint32_t connection_id);
uint32_t devismart_receive_data(const uint8_t *data, uint32_t size, uint32_t connection_id);
