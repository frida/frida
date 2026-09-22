# Frida CLI tools

CLI tools for [Frida](https://frida.re).

### Installing Fish completions

Currently there is no mechanism to install Fish completions through the setup.py
script so if you want to have completions in Fish you will have to install it
manually. Unless you've changed your XDG_CONFIG_HOME location, you should just
copy the completion file into `~/.config/fish/completions` like so:

    cp completions/frida.fish ~/.config/fish/completions

### frida-itrace file format

File starts with a 4-byte magic: "ITRC"
https://github.com/frida/frida-tools/blob/1ea077fdb49440e5807cf25fae41e389e3d2bd4a/frida_tools/itracer.py#L365-L366

Then, following that, there are two different types of records, MESSAGE and
CHUNK. Each record starts with a big-endian uint32 specifying the type of
record, where 1 means MESSAGE, 2 means CHUNK.

#### MESSAGE

- `length`: uint32 (big-endian)
- `message`: JSON, UTF-8 encoded
- `data_size`: uint32 (big-endian)
- `data_values`: uint8[data_size]

Generated [here](https://github.com/frida/frida-tools/blob/1ea077fdb49440e5807cf25fae41e389e3d2bd4a/frida_tools/itracer.py#L451-L458).

There are three different kinds of MESSAGEs:

- ["itrace:start"](https://github.com/frida/frida-tools/blob/1ea077fdb49440e5807cf25fae41e389e3d2bd4a/agents/itracer/agent.ts#L68-L76):
  Signals that the trace is starting, providing the initial register values.
  Contains register names and sizes in the JSON portion, and register values in
  the data portion.
  Generated [here](https://github.com/frida/frida-itrace/blob/ad7780bde9e518e325d7aaf848e9a29e1a53b7d2/lib/backend.ts#L341-L359).
- "itrace:end": Signals that the endpoint was reached, when specifying a range
  with an end address included.
- "itrace:compile": Signals that a basic block was discovered, providing the
  schema of future CHUNKs pertaining to it.
  Generated [here](https://github.com/frida/frida-itrace/blob/ad7780bde9e518e325d7aaf848e9a29e1a53b7d2/lib/backend.ts#L277-L323)
  and by the [code](https://github.com/frida/frida-itrace/blob/ad7780bde9e518e325d7aaf848e9a29e1a53b7d2/lib/backend.ts#L398-L401)
  above it that computes the "writes" array.

  The "writes" array contains tuples (arrays) that look like this:

    (block_offset, cpu_ctx_offset)

  Where `block_offset` is how many bytes into the basic block the write happens,
  and `cpu_ctx_offset` is the index into the registers declared by
  "itrace:start".

#### CHUNK

- `size`: uint32 (big-endian)
- `data`: uint8[size]

Generated [here](https://github.com/frida/frida-tools/blob/1ea077fdb49440e5807cf25fae41e389e3d2bd4a/frida_tools/itracer.py#L464-L465).

The CHUNK records combine to a stream of raw register values at different parts
of the given basic block. Each record looks like this:

- `block_start_address`: uint64 (target-endian, i.e. little-endian on arm64)
- `link_register_value`: uint64 (target-endian)
- `block_register_values`: uint64[n], where n depends on the specific basic
  block. (See above docs on "itrace:compile" and its "writes" array.)
