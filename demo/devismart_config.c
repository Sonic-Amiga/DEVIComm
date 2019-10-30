#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsmn.h"
#include "mdg_peer_api.h"

#include "logging.h"
#include "mdg_chat_client.h"
#include "mdg_util.h"

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

static char *jsonstrdup(const char *json, jsmntok_t *tok)
{
  /* Reinvent strndup(); Windows doesn't have it. */
  int len = tok->end - tok->start;
  char *str = malloc(len + 1);

  memcpy(str, json + tok->start, len);
  str[len] = 0;
  return str;
}

/*
 * What is going on here is NOT communication with thermostats.
 * This code handles peer-to-peer communication with the DEVISmart app,
 * used to share houses. This is the only legitimate way to add a client
 * to an already running installation.
 * Communication here is done in JSON and is pretty self-explanatory.
 */
static const char *user_name = "DEVIComm test";

void devismart_request_configuration(uint32_t connection_id)
{
  char hexed[AS_HEX(MDG_PEER_ID_SIZE)];
  char json[1024];
  int len, s;
  
  Log("Requesting DEVISmart config on connection %d\n", connection_id);

  whoami_hex(hexed);
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

  Log("Received configuration data:\n%.*s\n", size, data);  

/*
 * Received data is a JSON and it looks like this (i've removed my peer ID):
 * {"houseName":"My Flat","houseEditUsers":true,"rooms":[{"roomName":"Living room","peerId":"<undisclosed>","zone":"Living","sortOrder":0}]}
 */
  jsmn_parser p;
  jsmntok_t t[1024];
  int r, i, j, k;
  const char *json = (const char *)data;

  jsmn_init(&p);
  r = jsmn_parse(&p, json, size, t, sizeof(t) / sizeof(t[0]));

  if (r < 0) {
    mdg_chat_output_fprintf("Failed to parse configuration JSON: %d\n", r);
    return 1;
  }

  for (i = 1; i < r; i++)
  {
    if ((jsoneq(json, &t[i], "rooms") == 0) && (t[i + 1].type == JSMN_ARRAY))
	{
      for (j = 0; j < t[i + 1].size; j++) {
        jsmntok_t *g = &t[i + j + 2];
	    char *roomName = NULL;
		char *roomPeer = NULL;
		
		if (g[0].type != JSMN_OBJECT)
		{
          mdg_chat_output_fprintf("JSON error: \"rooms\" value is not an object!\n");
		  return 1;
		}
		
		for (k = 1; k < g[0].size; k+= 2)
		{
		  if (jsoneq(json, &g[k], "roomName") == 0) {
			roomName = jsonstrdup(json, &g[k + 1]);
		  } else if (jsoneq(json, &g[k], "peerId") == 0) {
			roomPeer = jsonstrdup(json, &g[k + 1]);
		  }
		}
		
		if (roomName && roomPeer)
		{
		  uint8_t device_id[MDG_PEER_ID_SIZE];

		  mdg_chat_output_fprintf("Adding room %s %s\n", roomPeer, roomName);

		  if (!hex_decode_bytes(roomPeer, device_id, MDG_PEER_ID_SIZE)) {
            add_pairing(device_id, roomName);            
          } else {
			mdg_chat_output_fprintf("Malformed room %s peer ID: %s\n", roomName, roomPeer);
		  }
		}
		
		free(roomName);
		free(roomPeer);
      }

      i += t[i + 1].size + 1;
    }
  }

  return 0;
}
