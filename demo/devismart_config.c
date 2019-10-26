#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdg_peer_api.h"

#include "logging.h"
#include "mdg_chat_client.h"
#include "mdg_util.h"

/*
 * What is going on here is NOT communication with thermostats.
 * This code handles peer-to-peer communication with the DEVISmart app,
 * used to share houses. This is the only legitimate way to add a client
 * to an already running installation.
 * Communication here is done in JSON and is pretty self-explanatory.
 */
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
  /*
   * If chunkedMessage parameter is set to true, the whole data will be split
   * into 512-byte long chunks and sent as separate packets; we would have
   * to put them together during receiving; we don't want to do that.
   * In this case a header is sent in the first packet, describing total
   * length of the configuration data. See struct ConfigDataHeader.
   * As of DEVISmart v1.2 this parameter is optional and can be completely
   * omitted; default value is false.
   */
  len = sprintf(json, "{\"phoneName\":\"%s\",\"phonePublicKey\":\"%s\",\"chunkedMessage\":false}", user_name, hexed);
  
  Log("Sending request: %s\n", json);
  s = mdg_send_to_peer((uint8_t *)json, len, connection_id);

  if (s != 0)
	mdg_chat_output_fprintf("Error %d sending config request\n, s");
}

struct ConfigDataHeader
{
	uint32_t start;  // Always 0, used for presence detection
	uint32_t length; // Total length of the following data
};

uint32_t devismart_receive_config_data(const uint8_t *data, uint32_t size)
{
  if (size > sizeof(struct ConfigDataHeader))
  {
    const struct ConfigDataHeader *header = (struct ConfigDataHeader *)data;
  
    if (header->start == 0)
    {
	  Log("Full size of chunked data: %d\n", header->length);
	  data += sizeof(struct ConfigDataHeader);
	  size -= sizeof(struct ConfigDataHeader);
	}
  }

  char *buf = malloc(size + 1);

  memcpy(buf, data, size);
  buf[size] = 0;

  Log("Received configuration data:\n%s\n", buf);  

/*
 * TODO: Parse this JSON and add peers
  {"houseName":"My Flat","houseEditUsers":true,"rooms":[{"roomName":"Гостиная","peerId":"<undisclosed>","zone":"Living","sortOrder":0}]}
*/

  return 0;
}
