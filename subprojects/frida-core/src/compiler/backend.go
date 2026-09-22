package main

import (
	"fmt"
	"path"
	"path/filepath"
	"strings"
	"sync"

	esbuild "github.com/evanw/esbuild/pkg/api"
	"github.com/frida/TypeScript/tsc/pkg/ast"
	"github.com/frida/TypeScript/tsc/pkg/locale"
	tsscanner "github.com/frida/TypeScript/tsc/pkg/scanner"
)

type BuildOptions struct {
	ProjectRoot      string
	Entrypoint       string
	OutputFormat     OutputFormat
	BundleFormat     BundleFormat
	DisableTypeCheck bool
	SourceMap        bool
	Compress         bool
	Platform         esbuild.Platform
	Externals        []string
}

type Diagnostic struct {
	category        string
	code            int
	path            string
	line, character int
	text            string
}

type BuildEventCallbacks struct {
	OnStart        BuildStartCallback
	OnEnd          BuildEndCallback
	OnOutput       BuildOutputCallback
	OnDiagnostic   BuildDiagnosticCallback
	OnConfigChange ConfigChangeCallback
}

type BuildStartCallback func()
type BuildEndCallback func()
type BuildOutputCallback func(bundle string)
type BuildDiagnosticCallback func(d Diagnostic)

func build(options BuildOptions, onDiagnostic BuildDiagnosticCallback) (bundle string, err error) {
	callbacks := BuildEventCallbacks{
		OnOutput: func(b string) {
			bundle = b
		},
		OnDiagnostic: onDiagnostic,
	}

	ctx, err := makeContext(options, callbacks)
	if err != nil {
		return
	}
	defer ctx.Dispose()

	result := ctx.Rebuild()

	if len(result.Errors) != 0 {
		err = fmt.Errorf("Compilation failed")
	}
	return
}

type WatchSession struct {
	mu        sync.Mutex
	options   BuildOptions
	callbacks BuildEventCallbacks
	ctx       *buildContext
	onDispose SessionDisposeHandler
}

type SessionDisposeHandler func()

func NewWatchSession(opts BuildOptions, onDispose SessionDisposeHandler, callbacks BuildEventCallbacks) (session *WatchSession, err error) {
	cbs := BuildEventCallbacks{
		OnStart:      callbacks.OnStart,
		OnEnd:        callbacks.OnEnd,
		OnOutput:     callbacks.OnOutput,
		OnDiagnostic: callbacks.OnDiagnostic,
		OnConfigChange: func() {
			if callbacks.OnConfigChange != nil {
				callbacks.OnConfigChange()
			}
			session.onConfigChange()
		},
	}

	ctx, err := makeContext(opts, cbs)
	if err != nil {
		return
	}

	ctx.Watch(esbuild.WatchOptions{})

	session = &WatchSession{
		options:   opts,
		callbacks: cbs,
		ctx:       ctx,
		onDispose: onDispose,
	}
	return
}

func (s *WatchSession) Dispose() {
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.ctx != nil {
		s.ctx.Dispose()
		s.ctx = nil
	}

	s.onDispose()
}

func (s *WatchSession) onConfigChange() {
	go func() {
		s.mu.Lock()
		defer s.mu.Unlock()

		if s.ctx != nil {
			s.ctx.Dispose()
			s.ctx = nil
		}

		ctx, err := makeContext(s.options, s.callbacks)
		if err != nil {
			return
		}
		s.ctx = ctx

		ctx.Watch(esbuild.WatchOptions{})
	}()
}

func makeContext(options BuildOptions, callbacks BuildEventCallbacks) (ctx *buildContext, err error) {
	var e error

	var projectRoot string
	if projectRoot, e = filepath.EvalSymlinks(options.ProjectRoot); e != nil {
		err = fmt.Errorf("Failed to resolve project root: %w", e)
		return
	}

	var entrypoint string
	if filepath.IsAbs(options.Entrypoint) {
		entrypoint = options.Entrypoint
	} else {
		entrypoint = filepath.Join(projectRoot, options.Entrypoint)
	}
	if entrypoint, e = filepath.EvalSymlinks(entrypoint); e != nil {
		err = fmt.Errorf("Failed to resolve entrypoint: %w", e)
		return
	}
	rel, e := filepath.Rel(projectRoot, entrypoint)
	if e != nil {
		err = fmt.Errorf("Could not compute entrypoint path relative to project root: %w", e)
		return
	}
	if strings.HasPrefix(rel, "..") {
		err = fmt.Errorf("Entrypoint must be inside the project root")
		return
	}

	isTS := strings.HasSuffix(entrypoint, ".ts")

	var tsconfigCache *TSConfigCache
	var tsconfigText string
	var tsCompiler *TSCompiler

	if isTS {
		tsconfigCache = NewTSConfigCache(projectRoot, options.SourceMap, callbacks.OnConfigChange)

		tsCompiler = NewTSCompiler(projectRoot, entrypoint, tsconfigCache.GetCompilerOptions)

		_, tsconfigText, err = tsconfigCache.GetCompilerOptions(tsCompiler)
		if err != nil {
			err = fmt.Errorf("Failed to load tsconfig options: %w", err)
			return
		}
	}

	sourcemapOption := esbuild.SourceMapNone
	if options.SourceMap {
		if options.BundleFormat == BundleFormatESM {
			sourcemapOption = esbuild.SourceMapExternal
		} else {
			sourcemapOption = esbuild.SourceMapInline
		}
	}

	minifyWhitespace := false
	minifyIdentifiers := false
	minifySyntax := false
	if options.Compress {
		minifyWhitespace = true
		minifyIdentifiers = true
		minifySyntax = true
	}

	var format esbuild.Format
	if options.BundleFormat == BundleFormatESM {
		format = esbuild.FormatESModule
	} else {
		format = esbuild.FormatIIFE
	}

	plugins := []esbuild.Plugin{
		makeBuildObserverPlugin(projectRoot, entrypoint, options, callbacks),
	}

	if isTS && !options.DisableTypeCheck {
		plugins = append(plugins, makeTypeScriptPlugin(tsCompiler))
	}

	plugins = append(plugins, makeFridaShimsPlugin())

	buildOpts := esbuild.BuildOptions{
		Sourcemap:         sourcemapOption,
		SourcesContent:    esbuild.SourcesContentExclude,
		Target:            esbuild.ES2022,
		MinifyWhitespace:  minifyWhitespace,
		MinifyIdentifiers: minifyIdentifiers,
		MinifySyntax:      minifySyntax,
		LegalComments:     esbuild.LegalCommentsNone,
		Bundle:            true,
		Outdir:            projectRoot,
		AbsWorkingDir:     projectRoot,
		Platform:          options.Platform,
		Format:            format,
		Inject:            []string{"frida-builtins:///node-globals.js"},
		External:          options.Externals,
		EntryPoints:       []string{entrypoint},
		Write:             false,
		Plugins:           plugins,
	}

	if isTS {
		buildOpts.TsconfigRaw = tsconfigText
	}

	if buildCtx, ctxErr := esbuild.Context(buildOpts); ctxErr == nil {
		ctx = &buildContext{buildCtx, tsCompiler}
	} else {
		for _, e := range ctxErr.Errors {
			emitDiagnostic("error", e, callbacks.OnDiagnostic)
		}
		err = fmt.Errorf("Failed to create ESBuild context")
	}
	return
}

type buildContext struct {
	esbuild.BuildContext
	compiler *TSCompiler
}

func (c *buildContext) Dispose() {
	c.BuildContext.Dispose()
	if c.compiler != nil {
		c.compiler.Dispose()
	}
}

func emitDiagnostic(category string, message esbuild.Message, onDiagnostic BuildDiagnosticCallback) {
	d := Diagnostic{
		category: category,
		code:     -1,
		text:     message.Text,
	}

	l := message.Location
	if l != nil {
		d.path = l.File
		d.line = l.Line
		d.character = l.Column
	}

	if message.PluginName == "frida-custom-ts" && len(message.Notes) == 2 {
		d.category = message.Notes[0].Text
		fmt.Sscan(message.Notes[1].Text, &d.code)
	}

	onDiagnostic(d)
}

func changeToJS(p string) string {
	ext := path.Ext(p)
	base := p[:len(p)-len(ext)]
	return base + ".js"
}

func makeBuildObserverPlugin(projectRoot, entrypoint string, options BuildOptions, callbacks BuildEventCallbacks) esbuild.Plugin {
	return esbuild.Plugin{
		Name: "frida-build-observer",
		Setup: func(build esbuild.PluginBuild) {
			if callbacks.OnStart != nil {
				build.OnStart(func() (esbuild.OnStartResult, error) {
					callbacks.OnStart()
					return esbuild.OnStartResult{}, nil
				})
			}

			build.OnEnd(func(result *esbuild.BuildResult) (esbuild.OnEndResult, error) {
				if len(result.Errors) == 0 {
					var output string
					if options.BundleFormat == BundleFormatESM {
						output = makeESMBundle(result.OutputFiles, projectRoot, entrypoint)
					} else {
						output = string(result.OutputFiles[0].Contents)
					}

					switch options.OutputFormat {
					case OutputFormatHexBytes:
						output = encodeStringToHexBytes(output)
					case OutputFormatCString:
						output = encodeStringToCString(output)
					}

					callbacks.OnOutput(output)
				} else {
					for _, e := range result.Errors {
						emitDiagnostic("error", e, callbacks.OnDiagnostic)
					}
					for _, e := range result.Warnings {
						emitDiagnostic("warning", e, callbacks.OnDiagnostic)
					}
				}

				if callbacks.OnEnd != nil {
					callbacks.OnEnd()
				}

				return esbuild.OnEndResult{}, nil
			})
		},
	}
}

func makeESMBundle(files []esbuild.OutputFile, projectRoot, entrypoint string) string {
	entrypointJS, entrypointMap := outputForEntrypoint(projectRoot, entrypoint)
	relEntryp, err := filepath.Rel(projectRoot, entrypoint)
	if err != nil {
		panic(fmt.Sprintf("Cannot compute entrypoint relative path: %v", err))
	}
	entrySubDir := filepath.Dir(relEntryp)

	entryIndexJS := -1
	entryIndexMap := -1
	for i, of := range files {
		if of.Path == entrypointJS {
			entryIndexJS = i
		}
		if of.Path == entrypointMap {
			entryIndexMap = i
		}
	}
	if entryIndexJS < 0 {
		panic(fmt.Sprintf("Entrypoint JS %q not found in files", entrypointJS))
	}

	orderedFiles := make([]esbuild.OutputFile, 0, len(files))
	orderedFiles = append(orderedFiles, files[entryIndexJS])
	if entryIndexMap >= 0 {
		orderedFiles = append(orderedFiles, files[entryIndexMap])
	}

	for i, of := range files {
		if i == entryIndexJS || i == entryIndexMap {
			continue
		}
		orderedFiles = append(orderedFiles, of)
	}

	var sb strings.Builder

	sb.WriteString("📦\n")
	for _, of := range orderedFiles {
		size := len(of.Contents)
		relPath, _ := filepath.Rel(projectRoot, of.Path)
		withSub := filepath.Join(entrySubDir, relPath)
		sb.WriteString(fmt.Sprintf("%d /%s\n", size, filepath.ToSlash(withSub)))
	}

	sb.WriteString("✄\n")
	for i, of := range orderedFiles {
		sb.Write(of.Contents)
		if i < len(orderedFiles)-1 {
			sb.WriteString("\n✄\n")
		}
	}

	return sb.String()
}

func outputForEntrypoint(projectRoot, entrypoint string) (jsOut, mapOut string) {
	jsOut = filepath.Join(projectRoot, changeToJS(filepath.Base(entrypoint)))
	mapOut = jsOut + ".map"
	return
}

func encodeStringToHexBytes(s string) string {
	n := len(s)
	if n == 0 {
		return ""
	}

	totalLen := n*4 + (n-1)*2 + 1
	buf := make([]byte, 0, totalLen)

	hexDigits := "0123456789abcdef"
	for i := 0; i < n; i++ {
		b := s[i]
		buf = append(buf, '0', 'x')
		buf = append(buf,
			hexDigits[b>>4],
			hexDigits[b&0xF],
		)

		if i < n-1 {
			if (i+1)%12 == 0 {
				buf = append(buf, ',', '\n')
			} else {
				buf = append(buf, ',', ' ')
			}
		}
	}

	buf = append(buf, '\n')

	return string(buf)
}

func encodeStringToCString(s string) string {
	n := len(s)
	if n == 0 {
		return `""`
	}

	hexDigits := "0123456789abcdef"
	var bldr strings.Builder

	openLiteral := func() {
		bldr.WriteByte('"')
	}
	closeLiteral := func() {
		bldr.WriteByte('"')
	}

	inLiteral := false

	for i := 0; i < n; i++ {
		c := s[i]

		if !inLiteral {
			openLiteral()
			inLiteral = true
		}

		if c == '\n' {
			bldr.WriteByte('\\')
			bldr.WriteByte('n')
			closeLiteral()
			inLiteral = false

			if i < n-1 {
				bldr.WriteByte('\n')
				openLiteral()
				inLiteral = true
			}
			continue
		}

		switch c {
		case '"':
			bldr.WriteByte('\\')
			bldr.WriteByte('"')
		case '\\':
			bldr.WriteByte('\\')
			bldr.WriteByte('\\')

		case '\a':
			bldr.WriteByte('\\')
			bldr.WriteByte('a')
		case '\b':
			bldr.WriteByte('\\')
			bldr.WriteByte('b')
		case '\t':
			bldr.WriteByte('\\')
			bldr.WriteByte('t')
		case '\v':
			bldr.WriteByte('\\')
			bldr.WriteByte('v')
		case '\f':
			bldr.WriteByte('\\')
			bldr.WriteByte('f')
		case '\r':
			bldr.WriteByte('\\')
			bldr.WriteByte('r')

		default:
			isPrintableExceptQuoteAndBackslash := c >= 0x20 && c <= 0x7E
			if isPrintableExceptQuoteAndBackslash {
				bldr.WriteByte(c)
			} else {
				bldr.WriteByte('\\')
				bldr.WriteByte('x')
				bldr.WriteByte(hexDigits[c>>4])
				bldr.WriteByte(hexDigits[c&0xF])
			}
		}
	}

	if inLiteral {
		closeLiteral()
	}

	bldr.WriteByte('\n')

	return bldr.String()
}

func makeTypeScriptPlugin(compiler *TSCompiler) esbuild.Plugin {
	return esbuild.Plugin{
		Name: "frida-custom-ts",
		Setup: func(build esbuild.PluginBuild) {
			build.OnStart(func() (esbuild.OnStartResult, error) {
				compiler.EnsureProgramUpToDate()
				return esbuild.OnStartResult{}, nil
			})

			build.OnEnd(func(result *esbuild.BuildResult) (esbuild.OnEndResult, error) {
				if len(result.Errors) == 0 {
					compiler.CommitPendingInputs()
				} else {
					compiler.DropPendingInputs()
				}
				return esbuild.OnEndResult{}, nil
			})

			build.OnLoad(esbuild.OnLoadOptions{Filter: "\\.ts$"}, func(args esbuild.OnLoadArgs) (esbuild.OnLoadResult, error) {
				compiledJS, tsDiagnostics, err := compiler.Compile(args.Path)

				result := esbuild.OnLoadResult{
					WatchFiles: compiler.WatchFiles(),
					WatchDirs:  compiler.WatchDirs(),
				}

				var esbuildMessages []esbuild.Message
				for _, d := range tsDiagnostics {
					esbuildMessages = append(esbuildMessages, esbuild.Message{
						Text:     d.Localize(locale.Default),
						Location: diagnosticLocation(d),
						Notes: []esbuild.Note{
							{Text: d.Category().Name()},
							{Text: fmt.Sprintf("%d", d.Code())},
						},
					})
				}

				if err != nil {
					mainErr := esbuild.Message{Text: err.Error()}
					result.Errors = append([]esbuild.Message{mainErr}, esbuildMessages...)
					return result, nil
				}

				if len(esbuildMessages) > 0 {
					result.Errors = esbuildMessages
					return result, nil
				}

				result.Contents = &compiledJS
				result.Loader = esbuild.LoaderJS
				return result, nil
			})
		},
	}
}

func diagnosticLocation(d *ast.Diagnostic) *esbuild.Location {
	f := d.File()
	if f == nil {
		return nil
	}

	pos := d.Pos()
	line, column := tsscanner.GetECMALineAndUTF16CharacterOfPosition(f, pos)

	return &esbuild.Location{
		File:     f.FileName(),
		Line:     line,
		Column:   int(column),
		Length:   d.Len(),
		LineText: f.Text()[pos:d.End()],
	}
}
