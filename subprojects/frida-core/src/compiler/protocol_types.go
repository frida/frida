package main

import (
	"fmt"

	esbuild "github.com/evanw/esbuild/pkg/api"
)

type OutputFormat int

const (
	OutputFormatUnescaped OutputFormat = iota
	OutputFormatHexBytes
	OutputFormatCString
)

func outputFormatFromNick(s string) (OutputFormat, error) {
	switch s {
	case "unescaped":
		return OutputFormatUnescaped, nil
	case "hex-bytes":
		return OutputFormatHexBytes, nil
	case "c-string":
		return OutputFormatCString, nil
	default:
		return 0, fmt.Errorf("unknown output format: %q", s)
	}
}

type BundleFormat int

const (
	BundleFormatESM BundleFormat = iota
	BundleFormatIIFE
)

func bundleFormatFromNick(s string) (BundleFormat, error) {
	switch s {
	case "esm":
		return BundleFormatESM, nil
	case "iife":
		return BundleFormatIIFE, nil
	default:
		return 0, fmt.Errorf("unknown bundle format: %q", s)
	}
}

func platformFromFrida(s string) esbuild.Platform {
	switch s {
	case "gum":
		return esbuild.PlatformNode
	case "browser":
		return esbuild.PlatformBrowser
	case "neutral":
		return esbuild.PlatformNeutral
	default:
		panic(fmt.Sprintf("Unknown platform: %q", s))
	}
}
