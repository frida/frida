#!/usr/bin/env python3
"""Regenerate the backend provider icons bundled by the resource compiler.

Requires `rsvg-convert` (librsvg) on PATH. The icons use accent colors
drawn from SF Symbols and libadwaita so they feel native in both
SwiftUI and GTK/Adwaita consumers. Each icon is rendered to a 96x96
PNG under src/icons/, where `icons.resources` picks it up.
"""
from __future__ import annotations

import subprocess
from pathlib import Path


def main() -> None:
    ICONS_DIR.mkdir(exist_ok=True)
    for name, svg in icons():
        png_path = ICONS_DIR / f"{name}.png"
        render_png(svg, ICON_SIZE, png_path)
        print(f"updated {png_path.relative_to(ROOT)}")


def icons() -> list[tuple[str, str]]:
    return [
        ("local", svg_local()),
        ("simmy", svg_simmy()),
        ("fruity-network", svg_fruity_network()),
        ("droidy", svg_droidy()),
        ("socket", svg_socket()),
        ("barebone", svg_barebone()),
        ("portal", svg_portal()),
    ]


def svg_local() -> str:
    return f"""\
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <linearGradient id="case" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{SLATE_LIGHT}"/>
      <stop offset="1" stop-color="{SLATE_DARK}"/>
    </linearGradient>
  </defs>
  <rect x="6" y="10" width="52" height="36" rx="3.5" fill="url(#case)"/>
  <rect x="9" y="13" width="46" height="30" rx="1.8" fill="{SCREEN}"/>
  <path d="M 17 22 L 23 28 L 17 34" stroke="#FFFFFF" stroke-width="2.8"
        stroke-linecap="round" stroke-linejoin="round" fill="none"/>
  <rect x="27" y="31" width="10" height="2.8" rx="0.7" fill="#FFFFFF" opacity="0.85"/>
  <rect x="28" y="46" width="8"  height="5" fill="url(#case)"/>
  <rect x="16" y="51" width="32" height="4" rx="2" fill="url(#case)"/>
</svg>
"""


def svg_simmy() -> str:
    return f"""\
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <linearGradient id="phone" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{CHROME_LIGHT}"/>
      <stop offset="1" stop-color="{CHROME_DARK}"/>
    </linearGradient>
    <linearGradient id="screen" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{SIM_SCREEN_TOP}"/>
      <stop offset="1" stop-color="{SIM_SCREEN_BOTTOM}"/>
    </linearGradient>
  </defs>
  <rect x="21" y="6"   width="22" height="52" rx="5" fill="url(#phone)"/>
  <rect x="22.5" y="10" width="19" height="42" rx="2.5" fill="url(#screen)"/>
  <circle cx="27.5" cy="14.5" r="1.6" fill="{TL_RED}"/>
  <circle cx="32"   cy="14.5" r="1.6" fill="{TL_YELLOW}"/>
  <circle cx="36.5" cy="14.5" r="1.6" fill="{TL_GREEN}"/>
  <rect x="29" y="54" width="6" height="1.3" rx="0.65" fill="{CHROME_DARK}" opacity="0.7"/>
</svg>
"""


def svg_fruity_network() -> str:
    return f"""\
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <linearGradient id="phone" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{SLATE_LIGHT}"/>
      <stop offset="1" stop-color="{SLATE_DARK}"/>
    </linearGradient>
    <linearGradient id="wave" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{BLUE_LIGHT}"/>
      <stop offset="1" stop-color="{BLUE}"/>
    </linearGradient>
  </defs>
  <g stroke="url(#wave)" stroke-linecap="round" fill="none">
    <path d="M 11 22 Q 32  2 53 22" stroke-width="3.2" opacity="0.45"/>
    <path d="M 17 24 Q 32 11 47 24" stroke-width="3.2" opacity="0.75"/>
    <path d="M 23 26 Q 32 18 41 26" stroke-width="3.2" opacity="1.00"/>
  </g>
  <rect x="23.5" y="28" width="17" height="34" rx="4.5" fill="url(#phone)"/>
  <rect x="25"   y="29.5" width="14" height="31" rx="3.2" fill="{SCREEN}"/>
  <rect x="28.5" y="31" width="7" height="2.4" rx="1.2" fill="{ISLAND}"/>
  <rect x="29"   y="58" width="6" height="1.3" rx="0.65" fill="{DETAIL}" opacity="0.8"/>
</svg>
"""


def svg_droidy() -> str:
    return f"""\
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <linearGradient id="g" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{GREEN_LIGHT}"/>
      <stop offset="1" stop-color="{GREEN_DARK}"/>
    </linearGradient>
  </defs>
  <path d="M 21 16 L 17 9" stroke="{GREEN_DARK}" stroke-width="3" stroke-linecap="round" fill="none"/>
  <path d="M 43 16 L 47 9" stroke="{GREEN_DARK}" stroke-width="3" stroke-linecap="round" fill="none"/>
  <path d="M 11 26 Q 11 12 32 12 Q 53 12 53 26 L 53 47 Q 53 51 49 51 L 15 51 Q 11 51 11 47 Z"
        fill="url(#g)"/>
  <circle cx="23" cy="30" r="3.2" fill="{GREEN_EYE}"/>
  <circle cx="41" cy="30" r="3.2" fill="{GREEN_EYE}"/>
  <path d="M 14 22 Q 20 14 32 14" stroke="#FFFFFF" stroke-width="1.6"
        stroke-linecap="round" fill="none" opacity="0.40"/>
</svg>
"""


def svg_socket() -> str:
    return f"""\
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <radialGradient id="g" cx="0.35" cy="0.30" r="0.95">
      <stop offset="0"    stop-color="{BLUE_LIGHT}"/>
      <stop offset="0.55" stop-color="{BLUE}"/>
      <stop offset="1"    stop-color="{BLUE_DARK}"/>
    </radialGradient>
  </defs>
  <circle cx="32" cy="32" r="25" fill="url(#g)"/>
  <g fill="none" stroke="#FFFFFF" stroke-width="1.6" opacity="0.92" stroke-linecap="round">
    <ellipse cx="32" cy="32" rx="25" ry="10"/>
    <ellipse cx="32" cy="32" rx="10" ry="25"/>
    <ellipse cx="32" cy="32" rx="20" ry="25" opacity="0.55"/>
    <ellipse cx="32" cy="32" rx="25" ry="20" opacity="0.55"/>
    <line x1="7" y1="32" x2="57" y2="32"/>
  </g>
  <circle cx="32" cy="32" r="25" fill="none" stroke="{BLUE_DARK}" stroke-width="1" opacity="0.35"/>
</svg>
"""


def svg_barebone() -> str:
    return f"""\
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <linearGradient id="chip" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{CHIP_LIGHT}"/>
      <stop offset="1" stop-color="{CHIP_DARK}"/>
    </linearGradient>
    <linearGradient id="pin" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{PIN_LIGHT}"/>
      <stop offset="1" stop-color="{PIN_DARK}"/>
    </linearGradient>
    <radialGradient id="led" cx="0.5" cy="0.5" r="0.5">
      <stop offset="0"    stop-color="{LED_BRIGHT}"/>
      <stop offset="0.55" stop-color="{LED}"/>
      <stop offset="1"    stop-color="{LED_DARK}"/>
    </radialGradient>
  </defs>
  <g fill="url(#pin)">
    <rect x="3"  y="19" width="9" height="4" rx="1"/>
    <rect x="3"  y="30" width="9" height="4" rx="1"/>
    <rect x="3"  y="41" width="9" height="4" rx="1"/>
    <rect x="52" y="19" width="9" height="4" rx="1"/>
    <rect x="52" y="30" width="9" height="4" rx="1"/>
    <rect x="52" y="41" width="9" height="4" rx="1"/>
  </g>
  <rect x="12" y="12" width="40" height="40" rx="3.5" fill="url(#chip)"/>
  <circle cx="32" cy="12" r="3" fill="{NOTCH}"/>
  <circle cx="32" cy="36" r="5" fill="url(#led)"/>
  <circle cx="32" cy="36" r="1.6" fill="#FFF7DC"/>
</svg>
"""


def svg_portal() -> str:
    return f"""\
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <radialGradient id="hub" cx="0.40" cy="0.35" r="0.85">
      <stop offset="0"    stop-color="{PURPLE_LIGHT}"/>
      <stop offset="0.65" stop-color="{PURPLE}"/>
      <stop offset="1"    stop-color="{PURPLE_DARK}"/>
    </radialGradient>
    <linearGradient id="node" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="{TEAL_LIGHT}"/>
      <stop offset="1" stop-color="{TEAL_DARK}"/>
    </linearGradient>
  </defs>
  <g stroke="{TEAL}" stroke-width="3" stroke-linecap="round" opacity="0.85">
    <line x1="32" y1="32" x2="13" y2="13"/>
    <line x1="32" y1="32" x2="51" y2="13"/>
    <line x1="32" y1="32" x2="13" y2="51"/>
    <line x1="32" y1="32" x2="51" y2="51"/>
  </g>
  <circle cx="13" cy="13" r="6.5" fill="url(#node)"/>
  <circle cx="51" cy="13" r="6.5" fill="url(#node)"/>
  <circle cx="13" cy="51" r="6.5" fill="url(#node)"/>
  <circle cx="51" cy="51" r="6.5" fill="url(#node)"/>
  <circle cx="32" cy="32" r="11.5" fill="url(#hub)"/>
  <circle cx="32" cy="32" r="4.5" fill="#FFFFFF" opacity="0.95"/>
</svg>
"""


def render_png(svg: str, size: int, out: Path) -> None:
    subprocess.run(
        ["rsvg-convert", "-w", str(size), "-h", str(size), "-o", str(out)],
        input=svg.encode("utf-8"),
        check=True,
    )


ROOT = Path(__file__).resolve().parent.parent
ICON_SIZE = 96
ICONS_DIR = ROOT / "src" / "icons"

BLUE = "#2E77E4"
BLUE_LIGHT = "#5AAEF5"
BLUE_DARK = "#1A5FB4"

CHROME_LIGHT = "#EFEEF1"
CHROME_DARK = "#9A9996"
SIM_SCREEN_TOP = "#3D3846"
SIM_SCREEN_BOTTOM = "#1F1B2B"
TL_RED = "#FF5F56"
TL_YELLOW = "#FFBD2E"
TL_GREEN = "#27C93F"

SLATE_LIGHT = "#8B8A95"
SLATE_DARK = "#4E4C58"
SCREEN = "#17131F"
ISLAND = "#050208"
DETAIL = "#DCDADF"

GREEN_LIGHT = "#57E389"
GREEN_DARK = "#26A269"
GREEN_EYE = "#0F3B20"

CHIP_LIGHT = "#4E4C58"
CHIP_DARK = "#1F1B2B"
PIN_LIGHT = "#B5B3BB"
PIN_DARK = "#6E6D75"
NOTCH = "#0F0B17"
LED_BRIGHT = "#FFD89B"
LED = "#FFA348"
LED_DARK = "#C65B10"

PURPLE = "#9141AC"
PURPLE_LIGHT = "#DC8ADD"
PURPLE_DARK = "#613583"

TEAL = "#33C7DE"
TEAL_LIGHT = "#5EDDEE"
TEAL_DARK = "#1D8DA8"


if __name__ == "__main__":
    main()
