import polyfills from "@frida/rollup-plugin-node-polyfills";
import terser from "@rollup/plugin-terser";
import typescript from "@rollup/plugin-typescript";
import resolve from "@rollup/plugin-node-resolve";
import { defineConfig } from "rollup";
import type { RollupOptions } from "rollup";

const BRIDGES = ["objc", "swift", "java"];

export default defineConfig(BRIDGES.map(name => {
    return {
        input: `${name}.ts`,
        output: {
            file: `${name}.js`,
            format: "iife",
            name: "bridge",
            generatedCode: {
                preset: "es2015",
            },
            strict: false,
        },
        plugins: [
             ({
                name: "disable-treeshake",
                transform (code, id) {
                    if (/node_modules\/frida-objc-bridge/.test(id)) {
                        return {
                            code,
                            map: null,
                            moduleSideEffects: "no-treeshake",
                        };
                    }

                    return null;
                },
            }),
            typescript(),
            polyfills(),
            resolve(),
            terser({ ecma: 2022 }),
        ],
    } as RollupOptions;
}));
