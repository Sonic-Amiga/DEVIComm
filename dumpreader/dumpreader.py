#!/usr/bin/python

import binascii
import json
import string
import sys
# had to rename parse.py, otherwise it conflicted with the standard parse module
import protobuf

def DumpData(data, offset):    
    size = len(data) / 2
    if size > offset:
        binary = binascii.unhexlify(data)
        messages = {}
        protobuf.ParseData(binary, offset, size - offset, messages)
        print json.dumps(messages, indent=4, sort_keys=True, ensure_ascii=False, encoding='utf-8')

if len(sys.argv) < 2:
    print "Usage :" + sys.argv[0] + " <input file> [start offset]"
    exit(1)

infile = open(sys.argv[1], "r") 
data = ""

if len(sys.argv) > 2:
    offset = sys.argv[2]
else:
    # We assume that the first packet is VOCH payload, and it has 128 bytes of inner
    # crypto box, containing the key. Skip it over.
    offset = 128

for line in infile:
    line = line.strip()
    if len(line) > 48 and line[0] in string.hexdigits and line[1] in string.hexdigits and line[2] == " ":
        data = data + line[:48].replace(" ", "")
    else:
        DumpData(data, offset)
        print line
        data = ""
        offset = 32 # Every packet has 32 bytes of padding
DumpData(data, offset)
infile.close()

