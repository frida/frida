# releng

Build system infrastructure to ensure fast and consistent builds across projects.

Intended to be used as a git submodule at `/releng` in projects.

## Setting up a new project

1. Set up the repo:

  ```sh
$ git init my-project
$ cd my-project
$ git submodule add https://github.com/frida/releng.git
$ cp releng/meson-scripts/* .
$ echo -e '/build/\n/deps/' > .gitignore
  ```

2. Create `meson.build` containing:

  ```meson
project('my-project', 'vala', version: '1.0.0')
executable('hello', 'hello.vala', dependencies: dependency('glib-2.0'))
  ```

3. Create `hello.vala` containing:

  ```vala
int main (string[] args) {
	print ("Hello World from Vala!\n");
	return 0;
}
  ```

4. Build and run:

  ```sh
$ make
$ ./build/hello
Hello World from Vala!
$
  ```

## Cross-compiling

### iOS

  ```sh
$ ./configure --host=ios-arm64
$ make
  ```

### Android

  ```sh
$ ./configure --host=android-arm64
$ make
  ```

### Raspberry Pi

  ```sh
$ sudo apt-get install g++-arm-linux-gnueabihf
$ ./configure --host=arm-linux-gnueabihf
$ make
  ```
