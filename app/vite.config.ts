import { defineConfig, loadEnv } from "vite";
import preact from "@preact/preset-vite";
import { viteSingleFile } from "vite-plugin-singlefile";

// Dev: `VITE_BOX=http://192.168.1.50 npm run dev` proxies /api to a real box;
// the default is the mock box from `npm run mock`.
export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), "");
  const box = env.VITE_BOX || "http://localhost:8787";
  return {
    plugins: [preact(), viteSingleFile({ removeViteModuleLoader: true })],
    server: { host: true, proxy: { "/api": { target: box, changeOrigin: true } } },
    build: {
      target: "es2019",
      cssCodeSplit: false,
      assetsInlineLimit: 100000000,
      reportCompressedSize: false,
      modulePreload: false,
    },
  };
});
