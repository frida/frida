//go:build frida_compiler_backend_executable

package main

import (
	"bufio"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"sync"
)

type BackendRequest struct {
	Type             string   `json:"type"`
	ID               uint     `json:"id,omitempty"`
	SessionID        uint     `json:"session_id,omitempty"`
	ProjectRoot      string   `json:"project_root,omitempty"`
	Entrypoint       string   `json:"entrypoint,omitempty"`
	OutputFormat     string   `json:"output_format,omitempty"`
	BundleFormat     string   `json:"bundle_format,omitempty"`
	DisableTypeCheck bool     `json:"disable_type_check,omitempty"`
	SourceMap        bool     `json:"source_map,omitempty"`
	Compress         bool     `json:"compress,omitempty"`
	Platform         string   `json:"platform,omitempty"`
	Externals        []string `json:"externals,omitempty"`
	Text             string   `json:"text,omitempty"`
}

type BackendEvent struct {
	Type      string `json:"type"`
	ID        uint   `json:"id,omitempty"`
	SessionID uint   `json:"session_id,omitempty"`

	Bundle string `json:"bundle,omitempty"`
	Error  string `json:"error,omitempty"`
	Text   string `json:"text,omitempty"`

	Category  string `json:"category,omitempty"`
	Code      int    `json:"code,omitempty"`
	Path      string `json:"path,omitempty"`
	Line      int    `json:"line,omitempty"`
	Character int    `json:"character,omitempty"`
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err.Error())
		os.Exit(1)
	}
}

func run() error {
	reader := bufio.NewReaderSize(os.Stdin, 128*1024)
	writer := bufio.NewWriterSize(os.Stdout, 128*1024)

	var outputMu sync.Mutex
	var sessionsMu sync.Mutex
	sessions := make(map[uint]*WatchSession)
	languageServers := make(map[uint]*LanguageServer)

	emit := func(ev BackendEvent) {
		outputMu.Lock()
		defer outputMu.Unlock()

		if err := writeMessage(writer, ev); err != nil {
			fmt.Fprintln(os.Stderr, err.Error())
		}
	}

	for {
		var req BackendRequest
		if err := readMessage(reader, &req); err != nil {
			if errors.Is(err, io.EOF) {
				return nil
			}
			return err
		}

		switch req.Type {
		case "build":
			go func(req BackendRequest) {
				options, err := buildOptionsFromRequest(req)
				if err != nil {
					emit(BackendEvent{
						Type:  "build:complete",
						ID:    req.ID,
						Error: err.Error(),
					})
					return
				}

				onDiagnostic := func(d Diagnostic) {
					ev := BackendEvent{
						Type:      "build:diagnostic",
						ID:        req.ID,
						Category:  d.category,
						Code:      d.code,
						Path:      d.path,
						Line:      d.line,
						Character: d.character,
						Text:      d.text,
					}
					emit(ev)
				}

				bundle, err := build(options, onDiagnostic)

				ev := BackendEvent{
					Type: "build:complete",
					ID:   req.ID,
				}
				if err != nil {
					ev.Error = err.Error()
				} else {
					ev.Bundle = bundle
				}
				emit(ev)
			}(req)

		case "watch":
			go func(req BackendRequest) {
				options, err := buildOptionsFromRequest(req)
				if err != nil {
					emit(BackendEvent{
						Type:      "watch:ready",
						SessionID: req.SessionID,
						Error:     err.Error(),
					})
					return
				}

				callbacks := BuildEventCallbacks{
					OnStart: func() {
						emit(BackendEvent{
							Type:      "watch:starting",
							SessionID: req.SessionID,
						})
					},
					OnEnd: func() {
						emit(BackendEvent{
							Type:      "watch:finished",
							SessionID: req.SessionID,
						})
					},
					OnOutput: func(bundle string) {
						emit(BackendEvent{
							Type:      "watch:output",
							SessionID: req.SessionID,
							Bundle:    bundle,
						})
					},
					OnDiagnostic: func(d Diagnostic) {
						emit(BackendEvent{
							Type:      "watch:diagnostic",
							SessionID: req.SessionID,
							Category:  d.category,
							Code:      d.code,
							Path:      d.path,
							Line:      d.line,
							Character: d.character,
							Text:      d.text,
						})
					},
				}

				var session *WatchSession
				onDispose := func() {
					sessionsMu.Lock()
					delete(sessions, req.SessionID)
					sessionsMu.Unlock()
				}

				session, err = NewWatchSession(options, onDispose, callbacks)
				if err != nil {
					emit(BackendEvent{
						Type:      "watch:ready",
						SessionID: req.SessionID,
						Error:     err.Error(),
					})
					return
				}

				sessionsMu.Lock()
				sessions[req.SessionID] = session
				sessionsMu.Unlock()

				emit(BackendEvent{
					Type:      "watch:ready",
					SessionID: req.SessionID,
				})
			}(req)

		case "dispose":
			sessionsMu.Lock()
			session := sessions[req.SessionID]
			delete(sessions, req.SessionID)
			sessionsMu.Unlock()

			if session != nil {
				session.Dispose()
			}

		case "language-server:open":
			server, err := NewLanguageServer(req.ProjectRoot, func(text string) {
				emit(BackendEvent{
					Type:      "language-server:message",
					SessionID: req.SessionID,
					Text:      text,
				})
			})
			if err != nil {
				emit(BackendEvent{
					Type:      "language-server:ready",
					SessionID: req.SessionID,
					Error:     err.Error(),
				})
				continue
			}

			sessionsMu.Lock()
			languageServers[req.SessionID] = server
			sessionsMu.Unlock()

			emit(BackendEvent{
				Type:      "language-server:ready",
				SessionID: req.SessionID,
			})

		case "language-server:close":
			sessionsMu.Lock()
			server := languageServers[req.SessionID]
			delete(languageServers, req.SessionID)
			sessionsMu.Unlock()

			server.Dispose()

		case "language-server:post":
			sessionsMu.Lock()
			server := languageServers[req.SessionID]
			sessionsMu.Unlock()

			if err := server.Post(req.Text); err != nil {
				emit(BackendEvent{
					Type:      "language-server:error",
					SessionID: req.SessionID,
					Error:     err.Error(),
				})
			}

		default:
			return fmt.Errorf("unsupported request type: %q", req.Type)
		}
	}
}

func buildOptionsFromRequest(req BackendRequest) (BuildOptions, error) {
	outputFormat, err := outputFormatFromNick(req.OutputFormat)
	if err != nil {
		return BuildOptions{}, err
	}

	bundleFormat, err := bundleFormatFromNick(req.BundleFormat)
	if err != nil {
		return BuildOptions{}, err
	}

	return BuildOptions{
		ProjectRoot:      req.ProjectRoot,
		Entrypoint:       req.Entrypoint,
		OutputFormat:     outputFormat,
		BundleFormat:     bundleFormat,
		DisableTypeCheck: req.DisableTypeCheck,
		SourceMap:        req.SourceMap,
		Compress:         req.Compress,
		Platform:         platformFromFrida(req.Platform),
		Externals:        req.Externals,
	}, nil
}

func readMessage(r io.Reader, out any) error {
	var size uint32
	if err := binary.Read(r, binary.BigEndian, &size); err != nil {
		return err
	}

	buf := make([]byte, size)
	if _, err := io.ReadFull(r, buf); err != nil {
		return err
	}

	return json.Unmarshal(buf, out)
}

func writeMessage(w *bufio.Writer, msg any) error {
	buf, err := json.Marshal(msg)
	if err != nil {
		return err
	}

	if err := binary.Write(w, binary.BigEndian, uint32(len(buf))); err != nil {
		return err
	}

	if _, err := w.Write(buf); err != nil {
		return err
	}

	return w.Flush()
}
