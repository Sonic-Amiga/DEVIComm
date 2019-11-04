#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

#include "mdg_chat_client.h"
#include "pktdump.h"

#ifdef __ANDROID__

/*
 * On Android the binary calls its functions directly, so this won't work :(
 */
void pktdump_open_file(const char *filename)
{
    mdg_chat_output_fprintf("Sorry, packet dump doesn't work on Android\n");
}

void pktdump_close_file(void)
{

}

#else

#include <sodium.h>

static FILE *f = NULL;

void pktdump_open_file(const char *filename)
{
    mdg_chat_output_fprintf("Decrypted packets will be dumped to %s\n", filename);
    f = fopen(filename, "w");
    if (!f)
        perror("Failed to open packet dump file");
}

void pktdump_close_file(void)
{
    if (f)
    {
        fclose(f);
        f = NULL;
    }
}

static void dump(const unsigned char *data, unsigned int len)
{
    unsigned int i;

    while (len)
    {
        unsigned int part = len > 16 ? 16 : len;

        for (i = 0; i < part; i++)
            fprintf(f, "%02x ", data[i]);
        for (i = part; i < 16; i++)
            fputs("   ", f);
        for (i = 0; i < part; i++)
            fputc(isprint(data[i]) ? data[i] : '.', f);
        fputc('\n', f);

        len -= part;
        data += part;
    }
}    

/*
 * This works by overriding functions in the PLT. Underlying functions are also
 * exported, making this an exceptionally easy task.
 */
int
crypto_box_afternm(unsigned char *c, const unsigned char *m,
                   unsigned long long mlen, const unsigned char *n,
                   const unsigned char *k)
{
    if (f)
    {
        fprintf(f, "Encrypting %lld bytes:\n", mlen);
        dump(m, mlen);        
    }
    return crypto_box_curve25519xsalsa20poly1305_afternm(c, m, mlen, n, k);
}

int
crypto_box_open_afternm(unsigned char *m, const unsigned char *c,
                        unsigned long long clen, const unsigned char *n,
                        const unsigned char *k)
{
    int ret = crypto_box_curve25519xsalsa20poly1305_open_afternm(m, c, clen, n, k);

    if (f)
    {
        if (ret == 0)
        {
            fprintf(f, "Decrypted %lld bytes:\n", clen);
            dump(m, clen);
        }
        else
        {
            fprintf(f, "Error decrypting %lld bytes!\n", clen);
        }
    }

    return ret;
}

#endif

