var __require = /* @__PURE__ */ ((x) => typeof require !== "undefined" ? require : typeof Proxy !== "undefined" ? new Proxy(x, {
  get: (a, b) => (typeof require !== "undefined" ? require : a)[b]
}) : x)(function(x) {
  if (typeof require !== "undefined") return require.apply(this, arguments);
  throw Error('Dynamic require of "' + x + '" is not supported');
});

// vite.config.ts
import { fileURLToPath, URL } from "node:url";
import { defineConfig } from "file:///C:/Users/Papa/Dokumente/Platformio/HoymilesDTU/OpenDTU-OnBattery/webapp/node_modules/vite/dist/node/index.js";
import vue from "file:///C:/Users/Papa/Dokumente/Platformio/HoymilesDTU/OpenDTU-OnBattery/webapp/node_modules/@vitejs/plugin-vue/dist/index.mjs";
import viteCompression from "file:///C:/Users/Papa/Dokumente/Platformio/HoymilesDTU/OpenDTU-OnBattery/webapp/node_modules/vite-plugin-compression/dist/index.mjs";
import cssInjectedByJsPlugin from "file:///C:/Users/Papa/Dokumente/Platformio/HoymilesDTU/OpenDTU-OnBattery/webapp/node_modules/vite-plugin-css-injected-by-js/dist/esm/index.js";
import VueI18nPlugin from "file:///C:/Users/Papa/Dokumente/Platformio/HoymilesDTU/OpenDTU-OnBattery/webapp/node_modules/@intlify/unplugin-vue-i18n/lib/vite.mjs";
import path from "path";
var __vite_injected_original_dirname = "C:\\Users\\Papa\\Dokumente\\Platformio\\HoymilesDTU\\OpenDTU-OnBattery\\webapp";
var __vite_injected_original_import_meta_url = "file:///C:/Users/Papa/Dokumente/Platformio/HoymilesDTU/OpenDTU-OnBattery/webapp/vite.config.ts";
var proxy_target;
try {
  proxy_target = __require("./vite.user.ts").proxy_target;
} catch (error) {
  proxy_target = "192.168.0.73";
}
var vite_config_default = defineConfig({
  plugins: [
    vue(),
    viteCompression({ deleteOriginFile: true, threshold: 0 }),
    cssInjectedByJsPlugin(),
    VueI18nPlugin({
      /* options */
      include: path.resolve(path.dirname(fileURLToPath(__vite_injected_original_import_meta_url)), "./src/locales/**.json"),
      fullInstall: false,
      forceStringify: true,
      strictMessage: false,
      jitCompilation: false
    })
  ],
  resolve: {
    alias: {
      "@": fileURLToPath(new URL("./src", __vite_injected_original_import_meta_url)),
      "~bootstrap": path.resolve(__vite_injected_original_dirname, "node_modules/bootstrap")
    }
  },
  build: {
    // Prevent vendor.css being created
    cssCodeSplit: false,
    outDir: "../webapp_dist",
    emptyOutDir: true,
    minify: "terser",
    chunkSizeWarningLimit: 1024,
    rollupOptions: {
      output: {
        // Only create one js file
        inlineDynamicImports: true,
        // Get rid of hash on js file
        entryFileNames: "js/app.js",
        // Get rid of hash on css file
        assetFileNames: "assets/[name].[ext]"
      }
    }
  },
  esbuild: {
    //    drop: ['console', 'debugger'],
    drop: ["debugger"]
  },
  server: {
    proxy: {
      "^/api": {
        target: "http://" + proxy_target
      },
      "^/livedata": {
        target: "ws://" + proxy_target,
        ws: true,
        changeOrigin: true
      },
      "^/vedirectlivedata": {
        target: "ws://" + proxy_target,
        ws: true,
        changeOrigin: true
      },
      "^/batterylivedata": {
        target: "ws://" + proxy_target,
        ws: true,
        changeOrigin: true
      },
      "^/console": {
        target: "ws://" + proxy_target,
        ws: true,
        changeOrigin: true
      }
    }
  }
});
export {
  vite_config_default as default
};
//# sourceMappingURL=data:application/json;base64,ewogICJ2ZXJzaW9uIjogMywKICAic291cmNlcyI6IFsidml0ZS5jb25maWcudHMiXSwKICAic291cmNlc0NvbnRlbnQiOiBbImNvbnN0IF9fdml0ZV9pbmplY3RlZF9vcmlnaW5hbF9kaXJuYW1lID0gXCJDOlxcXFxVc2Vyc1xcXFxQYXBhXFxcXERva3VtZW50ZVxcXFxQbGF0Zm9ybWlvXFxcXEhveW1pbGVzRFRVXFxcXE9wZW5EVFUtT25CYXR0ZXJ5XFxcXHdlYmFwcFwiO2NvbnN0IF9fdml0ZV9pbmplY3RlZF9vcmlnaW5hbF9maWxlbmFtZSA9IFwiQzpcXFxcVXNlcnNcXFxcUGFwYVxcXFxEb2t1bWVudGVcXFxcUGxhdGZvcm1pb1xcXFxIb3ltaWxlc0RUVVxcXFxPcGVuRFRVLU9uQmF0dGVyeVxcXFx3ZWJhcHBcXFxcdml0ZS5jb25maWcudHNcIjtjb25zdCBfX3ZpdGVfaW5qZWN0ZWRfb3JpZ2luYWxfaW1wb3J0X21ldGFfdXJsID0gXCJmaWxlOi8vL0M6L1VzZXJzL1BhcGEvRG9rdW1lbnRlL1BsYXRmb3JtaW8vSG95bWlsZXNEVFUvT3BlbkRUVS1PbkJhdHRlcnkvd2ViYXBwL3ZpdGUuY29uZmlnLnRzXCI7aW1wb3J0IHsgZmlsZVVSTFRvUGF0aCwgVVJMIH0gZnJvbSAnbm9kZTp1cmwnXG5cbmltcG9ydCB7IGRlZmluZUNvbmZpZyB9IGZyb20gJ3ZpdGUnXG5pbXBvcnQgdnVlIGZyb20gJ0B2aXRlanMvcGx1Z2luLXZ1ZSdcblxuaW1wb3J0IHZpdGVDb21wcmVzc2lvbiBmcm9tICd2aXRlLXBsdWdpbi1jb21wcmVzc2lvbic7XG5pbXBvcnQgY3NzSW5qZWN0ZWRCeUpzUGx1Z2luIGZyb20gJ3ZpdGUtcGx1Z2luLWNzcy1pbmplY3RlZC1ieS1qcydcbmltcG9ydCBWdWVJMThuUGx1Z2luIGZyb20gJ0BpbnRsaWZ5L3VucGx1Z2luLXZ1ZS1pMThuL3ZpdGUnXG5cbmltcG9ydCBwYXRoIGZyb20gJ3BhdGgnXG5cbi8vIGV4YW1wbGUgJ3ZpdGUudXNlci50cyc6IGV4cG9ydCBjb25zdCBwcm94eV90YXJnZXQgPSAnMTkyLjE2OC4xNi4xMDcnXG5sZXQgcHJveHlfdGFyZ2V0O1xudHJ5IHtcbiAgICAvLyBlc2xpbnQtZGlzYWJsZS1uZXh0LWxpbmVcbiAgICBwcm94eV90YXJnZXQgPSByZXF1aXJlKCcuL3ZpdGUudXNlci50cycpLnByb3h5X3RhcmdldDtcbn0gY2F0Y2ggKGVycm9yKSB7XG4gICAgcHJveHlfdGFyZ2V0ID0gJzE5Mi4xNjguMC43Myc7XG59XG5cbi8vIGh0dHBzOi8vdml0ZWpzLmRldi9jb25maWcvXG5leHBvcnQgZGVmYXVsdCBkZWZpbmVDb25maWcoe1xuICBwbHVnaW5zOiBbXG4gICAgdnVlKCksXG4gICAgdml0ZUNvbXByZXNzaW9uKHsgZGVsZXRlT3JpZ2luRmlsZTogdHJ1ZSwgdGhyZXNob2xkOiAwIH0pLFxuICAgIGNzc0luamVjdGVkQnlKc1BsdWdpbigpLFxuICAgIFZ1ZUkxOG5QbHVnaW4oe1xuICAgICAgICAvKiBvcHRpb25zICovXG4gICAgICAgIGluY2x1ZGU6IHBhdGgucmVzb2x2ZShwYXRoLmRpcm5hbWUoZmlsZVVSTFRvUGF0aChpbXBvcnQubWV0YS51cmwpKSwgJy4vc3JjL2xvY2FsZXMvKiouanNvbicpLFxuICAgICAgICBmdWxsSW5zdGFsbDogZmFsc2UsXG4gICAgICAgIGZvcmNlU3RyaW5naWZ5OiB0cnVlLFxuICAgICAgICBzdHJpY3RNZXNzYWdlOiBmYWxzZSxcbiAgICAgICAgaml0Q29tcGlsYXRpb246IGZhbHNlLFxuICAgIH0pLFxuICBdLFxuICByZXNvbHZlOiB7XG4gICAgYWxpYXM6IHtcbiAgICAgICdAJzogZmlsZVVSTFRvUGF0aChuZXcgVVJMKCcuL3NyYycsIGltcG9ydC5tZXRhLnVybCkpLFxuICAgICAgJ35ib290c3RyYXAnOiBwYXRoLnJlc29sdmUoX19kaXJuYW1lLCAnbm9kZV9tb2R1bGVzL2Jvb3RzdHJhcCcpLFxuICAgIH1cbiAgfSxcbiAgYnVpbGQ6IHtcbiAgICAvLyBQcmV2ZW50IHZlbmRvci5jc3MgYmVpbmcgY3JlYXRlZFxuICAgIGNzc0NvZGVTcGxpdDogZmFsc2UsXG4gICAgb3V0RGlyOiAnLi4vd2ViYXBwX2Rpc3QnLFxuICAgIGVtcHR5T3V0RGlyOiB0cnVlLFxuICAgIG1pbmlmeTogJ3RlcnNlcicsXG4gICAgY2h1bmtTaXplV2FybmluZ0xpbWl0OiAxMDI0LFxuICAgIHJvbGx1cE9wdGlvbnM6IHtcbiAgICAgIG91dHB1dDoge1xuICAgICAgICAvLyBPbmx5IGNyZWF0ZSBvbmUganMgZmlsZVxuICAgICAgICBpbmxpbmVEeW5hbWljSW1wb3J0czogdHJ1ZSxcbiAgICAgICAgLy8gR2V0IHJpZCBvZiBoYXNoIG9uIGpzIGZpbGVcbiAgICAgICAgZW50cnlGaWxlTmFtZXM6ICdqcy9hcHAuanMnLFxuICAgICAgICAvLyBHZXQgcmlkIG9mIGhhc2ggb24gY3NzIGZpbGVcbiAgICAgICAgYXNzZXRGaWxlTmFtZXM6IFwiYXNzZXRzL1tuYW1lXS5bZXh0XVwiLFxuICAgICAgfSxcbiAgICB9LFxuICB9LFxuICBlc2J1aWxkOiB7XG4vLyAgICBkcm9wOiBbJ2NvbnNvbGUnLCAnZGVidWdnZXInXSxcbiAgICBkcm9wOiBbJ2RlYnVnZ2VyJ10sXG4gIH0sXG4gIHNlcnZlcjoge1xuICAgIHByb3h5OiB7XG4gICAgICAnXi9hcGknOiB7XG4gICAgICAgIHRhcmdldDogJ2h0dHA6Ly8nICsgcHJveHlfdGFyZ2V0XG4gICAgICB9LFxuICAgICAgJ14vbGl2ZWRhdGEnOiB7XG4gICAgICAgIHRhcmdldDogJ3dzOi8vJyArIHByb3h5X3RhcmdldCxcbiAgICAgICAgd3M6IHRydWUsXG4gICAgICAgIGNoYW5nZU9yaWdpbjogdHJ1ZVxuICAgICAgfSxcbiAgICAgICdeL3ZlZGlyZWN0bGl2ZWRhdGEnOiB7XG4gICAgICAgIHRhcmdldDogJ3dzOi8vJyArIHByb3h5X3RhcmdldCxcbiAgICAgICAgd3M6IHRydWUsXG4gICAgICAgIGNoYW5nZU9yaWdpbjogdHJ1ZVxuICAgICAgfSxcbiAgICAgICdeL2JhdHRlcnlsaXZlZGF0YSc6IHtcbiAgICAgICAgdGFyZ2V0OiAnd3M6Ly8nICsgcHJveHlfdGFyZ2V0LFxuICAgICAgICB3czogdHJ1ZSxcbiAgICAgICAgY2hhbmdlT3JpZ2luOiB0cnVlXG4gICAgICB9LFxuICAgICAgJ14vY29uc29sZSc6IHtcbiAgICAgICAgdGFyZ2V0OiAnd3M6Ly8nICsgcHJveHlfdGFyZ2V0LFxuICAgICAgICB3czogdHJ1ZSxcbiAgICAgICAgY2hhbmdlT3JpZ2luOiB0cnVlXG4gICAgICB9XG4gICAgfVxuICB9XG59KVxuIl0sCiAgIm1hcHBpbmdzIjogIjs7Ozs7Ozs7QUFBdVosU0FBUyxlQUFlLFdBQVc7QUFFMWIsU0FBUyxvQkFBb0I7QUFDN0IsT0FBTyxTQUFTO0FBRWhCLE9BQU8scUJBQXFCO0FBQzVCLE9BQU8sMkJBQTJCO0FBQ2xDLE9BQU8sbUJBQW1CO0FBRTFCLE9BQU8sVUFBVTtBQVRqQixJQUFNLG1DQUFtQztBQUE0TixJQUFNLDJDQUEyQztBQVl0VCxJQUFJO0FBQ0osSUFBSTtBQUVBLGlCQUFlLFVBQVEsZ0JBQWdCLEVBQUU7QUFDN0MsU0FBUyxPQUFPO0FBQ1osaUJBQWU7QUFDbkI7QUFHQSxJQUFPLHNCQUFRLGFBQWE7QUFBQSxFQUMxQixTQUFTO0FBQUEsSUFDUCxJQUFJO0FBQUEsSUFDSixnQkFBZ0IsRUFBRSxrQkFBa0IsTUFBTSxXQUFXLEVBQUUsQ0FBQztBQUFBLElBQ3hELHNCQUFzQjtBQUFBLElBQ3RCLGNBQWM7QUFBQTtBQUFBLE1BRVYsU0FBUyxLQUFLLFFBQVEsS0FBSyxRQUFRLGNBQWMsd0NBQWUsQ0FBQyxHQUFHLHVCQUF1QjtBQUFBLE1BQzNGLGFBQWE7QUFBQSxNQUNiLGdCQUFnQjtBQUFBLE1BQ2hCLGVBQWU7QUFBQSxNQUNmLGdCQUFnQjtBQUFBLElBQ3BCLENBQUM7QUFBQSxFQUNIO0FBQUEsRUFDQSxTQUFTO0FBQUEsSUFDUCxPQUFPO0FBQUEsTUFDTCxLQUFLLGNBQWMsSUFBSSxJQUFJLFNBQVMsd0NBQWUsQ0FBQztBQUFBLE1BQ3BELGNBQWMsS0FBSyxRQUFRLGtDQUFXLHdCQUF3QjtBQUFBLElBQ2hFO0FBQUEsRUFDRjtBQUFBLEVBQ0EsT0FBTztBQUFBO0FBQUEsSUFFTCxjQUFjO0FBQUEsSUFDZCxRQUFRO0FBQUEsSUFDUixhQUFhO0FBQUEsSUFDYixRQUFRO0FBQUEsSUFDUix1QkFBdUI7QUFBQSxJQUN2QixlQUFlO0FBQUEsTUFDYixRQUFRO0FBQUE7QUFBQSxRQUVOLHNCQUFzQjtBQUFBO0FBQUEsUUFFdEIsZ0JBQWdCO0FBQUE7QUFBQSxRQUVoQixnQkFBZ0I7QUFBQSxNQUNsQjtBQUFBLElBQ0Y7QUFBQSxFQUNGO0FBQUEsRUFDQSxTQUFTO0FBQUE7QUFBQSxJQUVQLE1BQU0sQ0FBQyxVQUFVO0FBQUEsRUFDbkI7QUFBQSxFQUNBLFFBQVE7QUFBQSxJQUNOLE9BQU87QUFBQSxNQUNMLFNBQVM7QUFBQSxRQUNQLFFBQVEsWUFBWTtBQUFBLE1BQ3RCO0FBQUEsTUFDQSxjQUFjO0FBQUEsUUFDWixRQUFRLFVBQVU7QUFBQSxRQUNsQixJQUFJO0FBQUEsUUFDSixjQUFjO0FBQUEsTUFDaEI7QUFBQSxNQUNBLHNCQUFzQjtBQUFBLFFBQ3BCLFFBQVEsVUFBVTtBQUFBLFFBQ2xCLElBQUk7QUFBQSxRQUNKLGNBQWM7QUFBQSxNQUNoQjtBQUFBLE1BQ0EscUJBQXFCO0FBQUEsUUFDbkIsUUFBUSxVQUFVO0FBQUEsUUFDbEIsSUFBSTtBQUFBLFFBQ0osY0FBYztBQUFBLE1BQ2hCO0FBQUEsTUFDQSxhQUFhO0FBQUEsUUFDWCxRQUFRLFVBQVU7QUFBQSxRQUNsQixJQUFJO0FBQUEsUUFDSixjQUFjO0FBQUEsTUFDaEI7QUFBQSxJQUNGO0FBQUEsRUFDRjtBQUNGLENBQUM7IiwKICAibmFtZXMiOiBbXQp9Cg==
