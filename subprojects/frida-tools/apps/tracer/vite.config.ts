import fs from "fs";
import path from "path";
import { defineConfig, Plugin } from "vite";
import react from "@vitejs/plugin-react";

const R2_WASM_PATH = path.join(import.meta.dirname, "node_modules", "@frida", "react-use-r2", "dist", "r2.wasm");

const tracerPortStr = process.env.FRIDA_TRACE_PORT;
const tracerPort = (tracerPortStr !== undefined) ? parseInt(tracerPortStr) : 5172;

const r2WasmPlugin: Plugin = {
    name: "r2-wasm-plugin",
    configureServer(server) {
        server.middlewares.use((req, res, next) => {
            if (req.originalUrl?.endsWith("/r2.wasm")) {
                const data = fs.readFileSync(R2_WASM_PATH);
                res.setHeader("Content-Length", data.length);
                res.setHeader("Content-Type", "application/wasm");
                res.end(data, "binary");
                return;
            }
            next();
        });
    },
};

export default defineConfig({
    plugins: [react(), r2WasmPlugin],
    assetsInclude: "**/*.wasm",
    build: {
        rollupOptions: {
            output: {
                inlineDynamicImports: true,
                entryFileNames: "assets/[name].js",
                chunkFileNames: "assets/[name].js",
                assetFileNames: "assets/[name].[ext]"
            }
        }
    },
    server: {
        port: tracerPort + 1,
    },
});
