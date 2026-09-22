import frida from "frida";

async function main() {
    const session = await frida.attach("hello");
    const membership = await session.joinPortal("127.0.0.1:1337", {
        certificate: "/home/oleavr/src/cert.pem",
        token: "hunter2",
        //acl: ["admin"],
    });
    console.log("Joined!");

    /*
    await membership.terminate();
    console.log("Left!");
    */
}

main()
    .catch(e => {
        console.error(e);
    });
