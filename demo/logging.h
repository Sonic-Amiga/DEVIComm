#define ENABLE_LOGGING

#ifdef ENABLE_LOGGING

#define Log(...) fprintf(stderr, __VA_ARGS__)
static inline void Hexdump(const uint8_t *bytes, int len)
{
    int i;
	
    for (i = 0; i < len; i++)
	  fprintf(stderr, "%02x", bytes[i]);
}

#else

#define Log(...)
#define Hexdump(bytes, len)

#endif
