build: memory-scanner-arm64.elf marker-x86.elf

memory-scanner-arm64.elf:
	RUSTFLAGS="-C code-model=tiny -C link-arg=--emit-relocs -C link-arg=-n -C link-arg=--discard-all" \
		cargo build -r --target aarch64-unknown-none
	cp target/aarch64-unknown-none/release/memory-scanner $@.tmp
	rust-strip \
		--strip-unneeded \
		--remove-section=.rela.text \
		$@.tmp
	mv $@.tmp $@

marker-x86.elf:
	RUSTC_BOOTSTRAP=1 RUSTFLAGS="-C link-arg=--emit-relocs -C link-arg=-n -C link-arg=--discard-all" \
		cargo build -r --bin marker \
		--target ../agent/i686-unknown-none.json -Z json-target-spec \
		-Z build-std=core
	cp target/i686-unknown-none/release/marker $@.tmp
	rust-strip --strip-unneeded $@.tmp
	mv $@.tmp $@

.PHONY: build
