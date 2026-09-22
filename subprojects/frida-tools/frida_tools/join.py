import argparse
from typing import Any, List, MutableMapping


def main() -> None:
    from frida_tools.application import ConsoleApplication, await_ctrl_c

    class JoinApplication(ConsoleApplication):
        def __init__(self) -> None:
            ConsoleApplication.__init__(self, await_ctrl_c)
            self._parsed_options: MutableMapping[str, Any] = {}

        def _usage(self) -> str:
            return "%(prog)s [options] target portal-location [portal-certificate] [portal-token]"

        def _add_options(self, parser: argparse.ArgumentParser) -> None:
            parser.add_argument(
                "--portal-location", help="join portal at LOCATION", metavar="LOCATION", dest="portal_location"
            )
            parser.add_argument(
                "--portal-certificate",
                help="speak TLS with portal, expecting CERTIFICATE",
                metavar="CERTIFICATE",
                dest="portal_certificate",
            )
            parser.add_argument(
                "--portal-token", help="authenticate with portal using TOKEN", metavar="TOKEN", dest="portal_token"
            )
            parser.add_argument(
                "--portal-acl-allow",
                help="limit portal access to control channels with TAG",
                metavar="TAG",
                action="append",
                dest="portal_acl",
            )

        def _initialize(self, parser: argparse.ArgumentParser, options: argparse.Namespace, args: List[str]) -> None:
            location = args[0] if len(args) >= 1 else options.portal_location
            certificate = args[1] if len(args) >= 2 else options.portal_certificate
            token = args[2] if len(args) >= 3 else options.portal_token
            acl = options.portal_acl

            if location is None:
                parser.error("portal location must be specified")

            if certificate is not None:
                self._parsed_options["certificate"] = certificate
            if token is not None:
                self._parsed_options["token"] = token
            if acl is not None:
                self._parsed_options["acl"] = acl

            self._location = location

        def _needs_target(self) -> bool:
            return True

        def _start(self) -> None:
            self._update_status("Joining portal...")
            try:
                assert self._session is not None
                self._session.join_portal(self._location, **self._parsed_options)
            except Exception as e:
                self._update_status("Unable to join: " + str(e))
                self._exit(1)
                return
            self._update_status("Joined!")
            self._exit(0)

        def _stop(self) -> None:
            pass

    app = JoinApplication()
    app.run()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
