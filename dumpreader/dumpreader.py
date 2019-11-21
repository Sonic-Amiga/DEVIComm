#!/usr/bin/python

import binascii
import json
import string
import sys
# had to rename parse.py, otherwise it conflicted with the standard parse module
import protobuf

def DumpData(data, offset, decodeHeader):
    size = len(data)
    if size > 128 and data[128:143] == "\x01\x0bcertificate\x00\x80":
        # VOCH payload appears to be not protobuf but something else.
        print "Skipping VOCH payload, not a protobuf"
        return
    if size > offset:
        if decodeHeader:
            if size < offset + 3:
                print "Packet is too short, no header"
                return
            msgsize = (ord(data[offset]) << 8) | ord(data[offset + 1])
            if msgsize > size - 2:
                print "Invalid payload size, not a protobuf"
                return
            msgtype = ord(data[offset + 2])
            print "Data length " + str(msgsize) + " type " + str(msgtype)
            offset = offset + 3
        messages = {}
        protobuf.ParseData(data, offset, size, messages)
        print json.dumps(messages, indent=4, sort_keys=True, ensure_ascii=False, encoding='utf-8')

if len(sys.argv) < 2:
    print "Usage :" + sys.argv[0] + " <input file> [start offset]"
    exit(1)

infile = open(sys.argv[1], "r") 
data = ""

if len(sys.argv) > 2:
    offset = int(sys.argv[2])
    decodeHeader = False
else:
    # Every packet has 32 bytes of padding plus 3 byte header:
    # 2 bytes - length of the following protobuf payload, bigendian
    # 1 byte - structure type
    offset = 32
    decodeHeader = True

for line in infile:
    line = line.strip()
    if len(line) > 48 and line[0] in string.hexdigits and line[1] in string.hexdigits and line[2] == " ":
        data = data + binascii.unhexlify(line[:48].replace(" ", ""))
    else:
        DumpData(data, offset, decodeHeader)
        data = ""
        print line
DumpData(data, offset, decodeHeader)
infile.close()

