import frida

session = frida.attach("Twitter")
script = session.create_script("""\
rpc.exports = {
  hello: function () {
    return 'Hello';
  },
  failPlease: function () {
    oops;
  }
};
""")
script.load()
api = script.exports_sync
print("api.hello() =>", api.hello())
api.fail_please()
