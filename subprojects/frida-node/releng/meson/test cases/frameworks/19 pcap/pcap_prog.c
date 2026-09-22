#include <pcap/pcap.h>

int
main()
{
    char errbuf[PCAP_ERRBUF_SIZE];
#ifdef __APPLE__
    // source = NULL for "any" doesn't work on macOS (linux only?)
    char *source = "en0";
#else
    char *source = NULL;
#endif
    pcap_t *p = pcap_create(source, errbuf);
    return p == NULL;
}
