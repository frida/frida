package main

import (
	"context"
	"errors"
	"fmt"
	"io"
	"path/filepath"

	"github.com/frida/TypeScript/tsc/pkg/bundled"
	"github.com/frida/TypeScript/tsc/pkg/json"
	"github.com/frida/TypeScript/tsc/pkg/lsp"
	"github.com/frida/TypeScript/tsc/pkg/lsp/lsproto"
	"github.com/frida/TypeScript/tsc/pkg/vfs"
)

type LanguageServer struct {
	incoming chan *lsproto.Message
	cancel   context.CancelFunc
	done     chan struct{}
}

type LanguageServerMessageCallback func(text string)

func NewLanguageServer(projectRootPath string, onMessage LanguageServerMessageCallback) (*LanguageServer, error) {
	projectRoot, err := filepath.EvalSymlinks(projectRootPath)
	if err != nil {
		return nil, fmt.Errorf("Failed to resolve project root: %w", err)
	}

	fs := newConfiglessFS(newProjectFS(projectRoot), projectRoot)

	options, _, err := NewTSConfigCache(projectRoot, false, nil).GetCompilerOptions(projectHost{fs, projectRoot})
	if err != nil {
		return nil, fmt.Errorf("Failed to load tsconfig options: %w", err)
	}

	incoming := make(chan *lsproto.Message, 256)
	server := lsp.NewServer(&lsp.ServerOptions{
		In:                 messageQueue{incoming},
		Out:                messageSink(onMessage),
		Err:                io.Discard,
		Cwd:                projectRoot,
		FS:                 fs,
		DefaultLibraryPath: bundled.LibPath(),
		ParseCache:         parseCache,
		NpmInstall:         npmUnavailable,
	})

	ctx, cancel := context.WithCancel(context.Background())
	server.SetCompilerOptionsForInferredProjects(ctx, options)

	s := &LanguageServer{
		incoming: incoming,
		cancel:   cancel,
		done:     make(chan struct{}),
	}

	go func() {
		defer close(s.done)
		server.Run(ctx)
	}()

	return s, nil
}

func (s *LanguageServer) Post(text string) error {
	var msg lsproto.Message
	if err := json.Unmarshal([]byte(text), &msg); err != nil {
		return err
	}
	s.incoming <- &msg
	return nil
}

func (s *LanguageServer) Dispose() {
	s.cancel()
	close(s.incoming)
	<-s.done
}

type messageQueue struct {
	messages <-chan *lsproto.Message
}

func (q messageQueue) Read() (*lsproto.Message, error) {
	msg, ok := <-q.messages
	if !ok {
		return nil, io.EOF
	}
	return msg, nil
}

type messageSink LanguageServerMessageCallback

func (sink messageSink) Write(msg *lsproto.Message) error {
	data, err := json.Marshal(msg)
	if err != nil {
		return err
	}
	sink(string(data))
	return nil
}

func npmUnavailable(cwd string, args []string) ([]byte, error) {
	return nil, errors.New("npm is not available")
}

type projectHost struct {
	fs          vfs.FS
	projectRoot string
}

func (h projectHost) FS() vfs.FS {
	return h.fs
}

func (h projectHost) GetCurrentDirectory() string {
	return h.projectRoot
}

type configlessFS struct {
	vfs.FS
	tsconfigPath string
}

var _ vfs.FS = (*configlessFS)(nil)

func newConfiglessFS(inner vfs.FS, projectRoot string) *configlessFS {
	return &configlessFS{
		FS:           inner,
		tsconfigPath: filepath.Join(projectRoot, "tsconfig.json"),
	}
}

func (c *configlessFS) FileExists(path string) bool {
	if path == c.tsconfigPath {
		return false
	}
	return c.FS.FileExists(path)
}

func (c *configlessFS) ReadFile(path string) (string, bool) {
	if path == c.tsconfigPath {
		return "", false
	}
	return c.FS.ReadFile(path)
}

func (c *configlessFS) Stat(path string) vfs.FileInfo {
	if path == c.tsconfigPath {
		return nil
	}
	return c.FS.Stat(path)
}

func (c *configlessFS) GetAccessibleEntries(path string) vfs.Entries {
	entries := c.FS.GetAccessibleEntries(path)
	if path == filepath.Dir(c.tsconfigPath) {
		entries.Files = withoutTsconfig(entries.Files)
	}
	return entries
}

func withoutTsconfig(files []string) []string {
	result := make([]string, 0, len(files))
	for _, f := range files {
		if f != "tsconfig.json" {
			result = append(result, f)
		}
	}
	return result
}
