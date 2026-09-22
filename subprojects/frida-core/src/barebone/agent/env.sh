export FRIDA_BAREBONE_CONFIG=$PWD/etc/xnu.json
export PYTHONPATH=$HOME/src/frida-python

sdk=$HOME/src/frida-core/deps/sdk-none-arm64-softfloat_pic
export CC="clang --target=aarch64-none-elf -ffixed-x18 -I$sdk/include -I$sdk/include/glib-2.0 -I$sdk/lib/glib-2.0/include \
    -I$sdk/include/capstone -I$sdk/include/json-glib-1.0"
export RUSTFLAGS="-L native=$sdk/lib"
export GUMJS_DEVKIT_DIR=$HOME/src/frida-gum/build/none-arm64-softfloat_pic/bindings/gumjs/devkit
