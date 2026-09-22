#include "viewporter-client-protocol.h"
#include "viewporter-server-protocol.h"

int main() {
#if defined(VIEWPORTER_CLIENT_PROTOCOL_H) &&                                   \
    defined(VIEWPORTER_SERVER_PROTOCOL_H)
  return 0;
#else
  return 1;
#endif
}
