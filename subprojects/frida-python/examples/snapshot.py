import frida

embed_script = """
const button = {
  color: 'blue',
};

function mutateButton() {
  button.color = 'red';
}
"""

warmup_script = """
mutateButton();
"""

test_script = """
console.log('Button before:', JSON.stringify(button));
mutateButton();
console.log('Button after:', JSON.stringify(button));
"""

runtime = "v8"


session = frida.attach(0)

snapshot = session.snapshot_script(embed_script, warmup_script=warmup_script, runtime=runtime)


def on_message(message, data):
    print("on_message:", message)


script = session.create_script(test_script, snapshot=snapshot, runtime=runtime)
script.on("message", on_message)
script.load()
