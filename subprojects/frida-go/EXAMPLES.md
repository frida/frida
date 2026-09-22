## Channels

```golang
package main

import (
	"bufio"
	"fmt"
	"os"

	"github.com/frida/frida-go/frida"
)

func main() {
	r := bufio.NewReader(os.Stdin)
	dev := frida.USBDevice()
	channel, err := dev.OpenChannel("tcp:8080")
	if err != nil {
		panic(err)
	}
	defer channel.Close()

	dt := make([]byte, 512)
	read, err := channel.Read(&dt)
	if err != nil {
		panic(err)
	}
	fmt.Printf("Got %s; %d bytes\n", string(dt), read)

	n, err := channel.Write([]byte("what's up"))
	if err != nil {
		panic(err)
	}
	fmt.Printf("Written %d bytes\n", n)

	r.ReadLine()
}
```

## Child gating

```golang
package main

import (
	"bufio"
	"fmt"
	"os"

	"github.com/frida/frida-go/frida"
)

var sc = `
Interceptor.attach(Module.getExportByName(null, 'open'), {
	onEnter: function (args) {
	  send({
		type: 'open',
		path: Memory.readUtf8String(args[0])
	  });
	}
  });
`

func main() {
	d := frida.LocalDevice()

	instrument := func(pid int) {
		fmt.Printf("✔ attach(pid={%d})\n", pid)
		sess, err := d.Attach(pid, nil)
		if err != nil {
			panic(err)
		}

		sess.On("detached", func(reason frida.SessionDetachReason) {
			fmt.Printf("⚡ detached: pid={%d}, reason='{%s}'\n", pid, frida.SessionDetachReason(reason))
		})

		fmt.Printf("✔ enable_child_gating()\n")
		if err := sess.EnableChildGating(); err != nil {
			panic(err)
		}

		fmt.Printf("✔ create_script()\n")
		script, err := sess.CreateScript(sc)
		if err != nil {
			panic(err)
		}

		script.On("message", func(message string) {
			fmt.Printf("⚡ message: pid={%d}, payload={message['%s']}\n", pid, message)
		})

		fmt.Printf("✔ load()\n")
		script.Load()

		fmt.Printf("✔ resume(pid={%d})\n", pid)
		d.Resume(pid)
	}

	d.On("child-added", func(child *frida.Child) {
		fmt.Printf("⚡ child_added: {%d}, parent_pid: {%d}\n",
			child.PID(),
			child.PPID())
		instrument(int(child.PID()))
	})

	d.On("child-removed", func(child *frida.Child) {
		fmt.Printf("⚡ child_removed: {%v}\n", child.PID())
	})

	d.On("output", func(pid int, fd int, data []byte) {
		fmt.Printf("⚡ output: pid={%d}, fd={%d}, data={%s}\n",
			pid,
			fd,
			string(data))
	})

	fopts := frida.NewSpawnOptions()
	fopts.SetArgv([]string{
		"/bin/sh",
		"-c",
		"cat /etc/hosts",
	})
	fopts.SetStdio(frida.StdioPipe)

	fmt.Printf("✔ spawn(argv={%v})\n", fopts.Argv())
	pid, err := d.Spawn("/bin/sh", fopts)
	if err != nil {
		panic(err)
	}

	instrument(pid)

	r := bufio.NewReader(os.Stdin)
	r.ReadLine()
}
````

## Compiler build

__agent.ts:__
```typescript
import { log } from "./log.js";

log("Hello from Frida:", Frida.version);
```

__log.ts:__
```typescript
export function log(...args: any[]) {
    console.log(...args);
}
```

__main.go:__
```golang
package main

import (
	"fmt"
	"github.com/frida/frida-go/frida"
	"os"
)

func main() {
	c := frida.NewCompiler()
	c.On("starting", func() {
		fmt.Println("[*] Starting compiler")
	})
	c.On("finished", func() {
		fmt.Println("[*] Compiler finished")
	})
	c.On("bundle", func(bundle string) {
		fmt.Printf("[*] Compiler bundle: %s\n",
			bundle)
	})
	c.On("diagnostics", func(diag string) {
		fmt.Printf("[*] Compiler diagnostics: %s\n", diag)
	})

	bundle, err := c.Build("agent.ts")
	if err != nil {
		panic(err)
	}
	os.WriteFile("_agent.js", []byte(bundle), os.ModePerm)
}
```

## Compiler watch

__agent.ts:__
```typescript
import { log } from "./log.js";

log("Hello from Frida:", Frida.version);
```

__log.ts:__
```typescript
export function log(...args: any[]) {
    console.log(...args);
}
```

__main.go:__
```golang
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"

	"github.com/frida/frida-go/frida"
)

func main() {
	sess, err := frida.Attach(0)
	if err != nil {
		panic(err)
	}

	var script *frida.Script = nil

	onMessage := func(msg string) {
		msgMap := make(map[string]string)
		json.Unmarshal([]byte(msg), &msgMap)
		fmt.Printf("on_message: %s\n", msgMap["payload"])
	}

	compiler := frida.NewCompiler()
	compiler.On("output", func(bundle string) {
		if script != nil {
			fmt.Println("Unloading old bundle...")
			script.Unload()
			script = nil
		}
		fmt.Println("Loading bundle...")
		script, _ = sess.CreateScript(bundle)
		script.On("message", onMessage)
		script.Load()
	})

	compiler.Watch("agent.ts")

	r := bufio.NewReader(os.Stdin)
	r.ReadLine()
}
```

## File Monitor

```golang
package main

import (
	"bufio"
	"fmt"
	"os"
	"time"

	"github.com/frida/frida-go/frida"
)

func main() {
	mon := frida.NewFileMonitor("/tmp/test.txt")
	if err := mon.Enable(); err != nil {
		panic(err)
	}

	mon.On("change", func(changedFile, otherFile, changeType string) {
		fmt.Printf("[*] File %s has changed (%s)\n", changedFile, changeType)
	})

	fmt.Printf("[*] Monitoring path: %s\n", mon.Path())

	t := time.NewTimer(20 * time.Second)
	<-t.C

	if err := mon.Disable(); err != nil {
		panic(err)
	}

	r := bufio.NewReader(os.Stdin)
	r.ReadLine()
}
```

