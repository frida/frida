package frida

//#include <frida-core.h>
import "C"
import (
	"unsafe"
)

// SpawnOptions struct is responsible for setting/getting argv, envp etc
type SpawnOptions struct {
	opts *C.FridaSpawnOptions
}

// NewSpawnOptions create new instance of SpawnOptions.
func NewSpawnOptions() *SpawnOptions {
	opts := C.frida_spawn_options_new()
	return &SpawnOptions{
		opts: opts,
	}
}

// SetArgv set spawns argv with the argv provided.
func (s *SpawnOptions) SetArgv(argv []string) {
	arr, sz := stringSliceToCharArr(argv)

	C.frida_spawn_options_set_argv(s.opts, arr, sz)
}

// Argv returns argv of the spawn.
func (s *SpawnOptions) Argv() []string {
	var count C.gint
	argvC := C.frida_spawn_options_get_argv(s.opts, &count)

	argv := cArrayToStringSlice(argvC, C.int(count))

	return argv
}

// SetEnvp set spawns envp with the envp provided.
func (s *SpawnOptions) SetEnvp(envp map[string]string) {
	i := 0
	sl := make([]string, len(envp))

	for k, v := range envp {
		sl[i] = k + "=" + v
		i++
	}

	arr, sz := stringSliceToCharArr(sl)
	C.frida_spawn_options_set_envp(s.opts, arr, sz)
}

// Envp returns envp of the spawn.
func (s *SpawnOptions) Envp() []string {
	var count C.gint
	envpC := C.frida_spawn_options_get_argv(s.opts, &count)

	envp := cArrayToStringSlice(envpC, C.int(count))

	return envp
}

// SetEnv set spawns env with the env provided.
func (s *SpawnOptions) SetEnv(env map[string]string) {
	i := 0
	sl := make([]string, len(env))

	for k, v := range env {
		sl[i] = k + "=" + v
		i++
	}

	arr, sz := stringSliceToCharArr(sl)
	C.frida_spawn_options_set_env(s.opts, arr, sz)
}

// Env returns env of the spawn.
func (s *SpawnOptions) Env() []string {
	var count C.gint
	envpC := C.frida_spawn_options_get_env(s.opts, &count)

	env := cArrayToStringSlice(envpC, C.int(count))

	return env
}

// SetCwd sets current working directory (CWD) for the spawn.
func (s *SpawnOptions) SetCwd(cwd string) {
	cwdC := C.CString(cwd)
	defer C.free(unsafe.Pointer(cwdC))

	C.frida_spawn_options_set_cwd(s.opts, cwdC)
}

// Cwd returns current working directory (CWD) of the spawn.
func (s *SpawnOptions) Cwd() string {
	return C.GoString(C.frida_spawn_options_get_cwd(s.opts))
}

// SetStdio sets standard input/output of the spawn with the stdio provided.
func (s *SpawnOptions) SetStdio(stdio Stdio) {
	C.frida_spawn_options_set_stdio(s.opts, C.FridaStdio(stdio))
}

// Stdio returns spawns stdio.
func (s *SpawnOptions) Stdio() Stdio {
	return Stdio(int(C.frida_spawn_options_get_stdio(s.opts)))
}

// Aux returns aux of the spawn.
func (s *SpawnOptions) Aux() map[string]any {
	ht := C.frida_spawn_options_get_aux(s.opts)
	aux := gHashTableToMap(ht)
	return aux
}

// Clean will clean the resources held by the spawn options.
func (s *SpawnOptions) Clean() {
	clean(unsafe.Pointer(s.opts), unrefFrida)
}
