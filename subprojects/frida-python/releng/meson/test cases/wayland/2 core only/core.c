#include <xdg-shell-client-protocol.h>

int main() {
#if defined(XDG_SHELL_CLIENT_PROTOCOL_H) && !defined(WAYLAND_CLIENT_H) && !defined(WAYLAND_CLIENT_PROTOCOL_H)
    return 0;
#else
    return 1;
#endif
}
