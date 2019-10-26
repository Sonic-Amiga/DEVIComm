#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdg_peer_api.h"

#include "logging.h"
#include "mdg_chat_client.h"
#include "mdg_util.h"

static const char *user_name = "DEVIComm test";

static void getMyIdHexed(char *hexed)
{
  hex_encode_bytes(chatclient_get_private_key(), hexed, MDG_PEER_ID_SIZE);
}

void devismart_request_configuration(uint32_t connection_id)
{
  char hexed[2 * MDG_PEER_ID_SIZE + 1];
  char json[1024];
  int len, s;
  
  Log("Requesting DEVISmart config on connection %d\n", connection_id);

  getMyIdHexed(hexed);
  len = sprintf(json, "{\"phoneName\":\"%s\",\"phonePublicKey\":\"%s\",\"chunkedMessage\":true}", user_name, hexed);
  
  Log("Sending request: %s\n", json);
  s = mdg_send_to_peer((uint8_t *)json, len, connection_id);

  if (s != 0)
	mdg_chat_output_fprintf("Error %d sending config request\n, s");
}

struct ConfigDataHeader
{
	uint32_t start;  // Always 0
	uint32_t length; // Total length of the following data
};

uint32_t devismart_receive_config_data(const uint8_t *data, uint32_t size)
{
  if (size > sizeof(struct ConfigDataHeader))
  {
    const struct ConfigDataHeader *header = (struct ConfigDataHeader *)data;
  
    if (header->start == 0)
    {
	  Log("Full data size: %d\n", header->length);
	  data += sizeof(struct ConfigDataHeader);
	  size -= sizeof(struct ConfigDataHeader);
	}
  }

  char *buf = malloc(size + 1);

  memcpy(buf, data, size);
  buf[size] = 0;

  Log("Received configuration data:\n%s\n", buf);  
  
  return 0;
}
