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
} catch {
  proxy_target = "192.168.0.73";
}
var vite_config_default = defineConfig(({ command }) => {
  return {
    plugins: [
      vue(),
      viteCompression({ deleteOriginFile: true, threshold: 0 }),
      cssInjectedByJsPlugin(),
      VueI18nPlugin({
        /* options */
        include: path.resolve(path.dirname(fileURLToPath(__vite_injected_original_import_meta_url)), "./src/locales/**.json"),
        fullInstall: false,
        forceStringify: true,
        strictMessage: false
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
      drop: command !== "serve" ? ["console", "debugger"] : []
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
  };
});
export {
  vite_config_default as default
};
//# sourceMappingURL=data:application/json;base64,ewogICJ2ZXJzaW9uIjogMywKICAic291cmNlcyI6IFsidml0ZS5jb25maWcudHMiXSwKICAic291cmNlc0NvbnRlbnQiOiBbImNvbnN0IF9fdml0ZV9pbmplY3RlZF9vcmlnaW5hbF9kaXJuYW1lID0gXCJDOlxcXFxVc2Vyc1xcXFxQYXBhXFxcXERva3VtZW50ZVxcXFxQbGF0Zm9ybWlvXFxcXEhveW1pbGVzRFRVXFxcXE9wZW5EVFUtT25CYXR0ZXJ5XFxcXHdlYmFwcFwiO2NvbnN0IF9fdml0ZV9pbmplY3RlZF9vcmlnaW5hbF9maWxlbmFtZSA9IFwiQzpcXFxcVXNlcnNcXFxcUGFwYVxcXFxEb2t1bWVudGVcXFxcUGxhdGZvcm1pb1xcXFxIb3ltaWxlc0RUVVxcXFxPcGVuRFRVLU9uQmF0dGVyeVxcXFx3ZWJhcHBcXFxcdml0ZS5jb25maWcudHNcIjtjb25zdCBfX3ZpdGVfaW5qZWN0ZWRfb3JpZ2luYWxfaW1wb3J0X21ldGFfdXJsID0gXCJmaWxlOi8vL0M6L1VzZXJzL1BhcGEvRG9rdW1lbnRlL1BsYXRmb3JtaW8vSG95bWlsZXNEVFUvT3BlbkRUVS1PbkJhdHRlcnkvd2ViYXBwL3ZpdGUuY29uZmlnLnRzXCI7aW1wb3J0IHsgZmlsZVVSTFRvUGF0aCwgVVJMIH0gZnJvbSAnbm9kZTp1cmwnXG5cbmltcG9ydCB7IGRlZmluZUNvbmZpZyB9IGZyb20gJ3ZpdGUnXG5pbXBvcnQgdnVlIGZyb20gJ0B2aXRlanMvcGx1Z2luLXZ1ZSdcblxuaW1wb3J0IHZpdGVDb21wcmVzc2lvbiBmcm9tICd2aXRlLXBsdWdpbi1jb21wcmVzc2lvbic7XG5pbXBvcnQgY3NzSW5qZWN0ZWRCeUpzUGx1Z2luIGZyb20gJ3ZpdGUtcGx1Z2luLWNzcy1pbmplY3RlZC1ieS1qcydcbmltcG9ydCBWdWVJMThuUGx1Z2luIGZyb20gJ0BpbnRsaWZ5L3VucGx1Z2luLXZ1ZS1pMThuL3ZpdGUnXG5cbmltcG9ydCBwYXRoIGZyb20gJ3BhdGgnXG5cbi8vIGV4YW1wbGUgJ3ZpdGUudXNlci50cyc6IGV4cG9ydCBjb25zdCBwcm94eV90YXJnZXQgPSAnMTkyLjE2OC4xNi4xMDcnXG5sZXQgcHJveHlfdGFyZ2V0O1xudHJ5IHtcbiAgICAvLyBlc2xpbnQtZGlzYWJsZS1uZXh0LWxpbmVcbiAgICBwcm94eV90YXJnZXQgPSByZXF1aXJlKCcuL3ZpdGUudXNlci50cycpLnByb3h5X3RhcmdldDtcbn0gY2F0Y2gge1xuICAgIHByb3h5X3RhcmdldCA9ICcxOTIuMTY4LjAuNzMnO1xufVxuXG4vLyBodHRwczovL3ZpdGVqcy5kZXYvY29uZmlnL1xuZXhwb3J0IGRlZmF1bHQgZGVmaW5lQ29uZmlnKCh7IGNvbW1hbmQgfSkgPT4geyByZXR1cm4ge1xuICBwbHVnaW5zOiBbXG4gICAgdnVlKCksXG4gICAgdml0ZUNvbXByZXNzaW9uKHsgZGVsZXRlT3JpZ2luRmlsZTogdHJ1ZSwgdGhyZXNob2xkOiAwIH0pLFxuICAgIGNzc0luamVjdGVkQnlKc1BsdWdpbigpLFxuICAgIFZ1ZUkxOG5QbHVnaW4oe1xuICAgICAgICAvKiBvcHRpb25zICovXG4gICAgICAgIGluY2x1ZGU6IHBhdGgucmVzb2x2ZShwYXRoLmRpcm5hbWUoZmlsZVVSTFRvUGF0aChpbXBvcnQubWV0YS51cmwpKSwgJy4vc3JjL2xvY2FsZXMvKiouanNvbicpLFxuICAgICAgICBmdWxsSW5zdGFsbDogZmFsc2UsXG4gICAgICAgIGZvcmNlU3RyaW5naWZ5OiB0cnVlLFxuICAgICAgICBzdHJpY3RNZXNzYWdlOiBmYWxzZSxcbiAgICB9KSxcbiAgXSxcbiAgcmVzb2x2ZToge1xuICAgIGFsaWFzOiB7XG4gICAgICAnQCc6IGZpbGVVUkxUb1BhdGgobmV3IFVSTCgnLi9zcmMnLCBpbXBvcnQubWV0YS51cmwpKSxcbiAgICAgICd+Ym9vdHN0cmFwJzogcGF0aC5yZXNvbHZlKF9fZGlybmFtZSwgJ25vZGVfbW9kdWxlcy9ib290c3RyYXAnKSxcbiAgICB9XG4gIH0sXG4gIGJ1aWxkOiB7XG4gICAgLy8gUHJldmVudCB2ZW5kb3IuY3NzIGJlaW5nIGNyZWF0ZWRcbiAgICBjc3NDb2RlU3BsaXQ6IGZhbHNlLFxuICAgIG91dERpcjogJy4uL3dlYmFwcF9kaXN0JyxcbiAgICBlbXB0eU91dERpcjogdHJ1ZSxcbiAgICBtaW5pZnk6ICd0ZXJzZXInLFxuICAgIGNodW5rU2l6ZVdhcm5pbmdMaW1pdDogMTAyNCxcbiAgICByb2xsdXBPcHRpb25zOiB7XG4gICAgICBvdXRwdXQ6IHtcbiAgICAgICAgLy8gT25seSBjcmVhdGUgb25lIGpzIGZpbGVcbiAgICAgICAgaW5saW5lRHluYW1pY0ltcG9ydHM6IHRydWUsXG4gICAgICAgIC8vIEdldCByaWQgb2YgaGFzaCBvbiBqcyBmaWxlXG4gICAgICAgIGVudHJ5RmlsZU5hbWVzOiAnanMvYXBwLmpzJyxcbiAgICAgICAgLy8gR2V0IHJpZCBvZiBoYXNoIG9uIGNzcyBmaWxlXG4gICAgICAgIGFzc2V0RmlsZU5hbWVzOiBcImFzc2V0cy9bbmFtZV0uW2V4dF1cIixcbiAgICAgIH0sXG4gICAgfSxcbiAgfSxcbiAgZXNidWlsZDoge1xuICAgIGRyb3A6IGNvbW1hbmQgIT09ICdzZXJ2ZScgPyBbJ2NvbnNvbGUnLCAnZGVidWdnZXInXSA6IFtdXG4gIH0sXG4gIHNlcnZlcjoge1xuICAgIHByb3h5OiB7XG4gICAgICAnXi9hcGknOiB7XG4gICAgICAgIHRhcmdldDogJ2h0dHA6Ly8nICsgcHJveHlfdGFyZ2V0XG4gICAgICB9LFxuICAgICAgJ14vbGl2ZWRhdGEnOiB7XG4gICAgICAgIHRhcmdldDogJ3dzOi8vJyArIHByb3h5X3RhcmdldCxcbiAgICAgICAgd3M6IHRydWUsXG4gICAgICAgIGNoYW5nZU9yaWdpbjogdHJ1ZVxuICAgICAgfSxcbiAgICAgICdeL3ZlZGlyZWN0bGl2ZWRhdGEnOiB7XG4gICAgICAgIHRhcmdldDogJ3dzOi8vJyArIHByb3h5X3RhcmdldCxcbiAgICAgICAgd3M6IHRydWUsXG4gICAgICAgIGNoYW5nZU9yaWdpbjogdHJ1ZVxuICAgICAgfSxcbiAgICAgICdeL2JhdHRlcnlsaXZlZGF0YSc6IHtcbiAgICAgICAgdGFyZ2V0OiAnd3M6Ly8nICsgcHJveHlfdGFyZ2V0LFxuICAgICAgICB3czogdHJ1ZSxcbiAgICAgICAgY2hhbmdlT3JpZ2luOiB0cnVlXG4gICAgICB9LFxuICAgICAgJ14vY29uc29sZSc6IHtcbiAgICAgICAgdGFyZ2V0OiAnd3M6Ly8nICsgcHJveHlfdGFyZ2V0LFxuICAgICAgICB3czogdHJ1ZSxcbiAgICAgICAgY2hhbmdlT3JpZ2luOiB0cnVlXG4gICAgICB9XG4gICAgfVxuICB9XG59IH0pXG4iXSwKICAibWFwcGluZ3MiOiAiOzs7Ozs7OztBQUF1WixTQUFTLGVBQWUsV0FBVztBQUUxYixTQUFTLG9CQUFvQjtBQUM3QixPQUFPLFNBQVM7QUFFaEIsT0FBTyxxQkFBcUI7QUFDNUIsT0FBTywyQkFBMkI7QUFDbEMsT0FBTyxtQkFBbUI7QUFFMUIsT0FBTyxVQUFVO0FBVGpCLElBQU0sbUNBQW1DO0FBQTROLElBQU0sMkNBQTJDO0FBWXRULElBQUk7QUFDSixJQUFJO0FBRUEsaUJBQWUsVUFBUSxnQkFBZ0IsRUFBRTtBQUM3QyxRQUFRO0FBQ0osaUJBQWU7QUFDbkI7QUFHQSxJQUFPLHNCQUFRLGFBQWEsQ0FBQyxFQUFFLFFBQVEsTUFBTTtBQUFFLFNBQU87QUFBQSxJQUNwRCxTQUFTO0FBQUEsTUFDUCxJQUFJO0FBQUEsTUFDSixnQkFBZ0IsRUFBRSxrQkFBa0IsTUFBTSxXQUFXLEVBQUUsQ0FBQztBQUFBLE1BQ3hELHNCQUFzQjtBQUFBLE1BQ3RCLGNBQWM7QUFBQTtBQUFBLFFBRVYsU0FBUyxLQUFLLFFBQVEsS0FBSyxRQUFRLGNBQWMsd0NBQWUsQ0FBQyxHQUFHLHVCQUF1QjtBQUFBLFFBQzNGLGFBQWE7QUFBQSxRQUNiLGdCQUFnQjtBQUFBLFFBQ2hCLGVBQWU7QUFBQSxNQUNuQixDQUFDO0FBQUEsSUFDSDtBQUFBLElBQ0EsU0FBUztBQUFBLE1BQ1AsT0FBTztBQUFBLFFBQ0wsS0FBSyxjQUFjLElBQUksSUFBSSxTQUFTLHdDQUFlLENBQUM7QUFBQSxRQUNwRCxjQUFjLEtBQUssUUFBUSxrQ0FBVyx3QkFBd0I7QUFBQSxNQUNoRTtBQUFBLElBQ0Y7QUFBQSxJQUNBLE9BQU87QUFBQTtBQUFBLE1BRUwsY0FBYztBQUFBLE1BQ2QsUUFBUTtBQUFBLE1BQ1IsYUFBYTtBQUFBLE1BQ2IsUUFBUTtBQUFBLE1BQ1IsdUJBQXVCO0FBQUEsTUFDdkIsZUFBZTtBQUFBLFFBQ2IsUUFBUTtBQUFBO0FBQUEsVUFFTixzQkFBc0I7QUFBQTtBQUFBLFVBRXRCLGdCQUFnQjtBQUFBO0FBQUEsVUFFaEIsZ0JBQWdCO0FBQUEsUUFDbEI7QUFBQSxNQUNGO0FBQUEsSUFDRjtBQUFBLElBQ0EsU0FBUztBQUFBLE1BQ1AsTUFBTSxZQUFZLFVBQVUsQ0FBQyxXQUFXLFVBQVUsSUFBSSxDQUFDO0FBQUEsSUFDekQ7QUFBQSxJQUNBLFFBQVE7QUFBQSxNQUNOLE9BQU87QUFBQSxRQUNMLFNBQVM7QUFBQSxVQUNQLFFBQVEsWUFBWTtBQUFBLFFBQ3RCO0FBQUEsUUFDQSxjQUFjO0FBQUEsVUFDWixRQUFRLFVBQVU7QUFBQSxVQUNsQixJQUFJO0FBQUEsVUFDSixjQUFjO0FBQUEsUUFDaEI7QUFBQSxRQUNBLHNCQUFzQjtBQUFBLFVBQ3BCLFFBQVEsVUFBVTtBQUFBLFVBQ2xCLElBQUk7QUFBQSxVQUNKLGNBQWM7QUFBQSxRQUNoQjtBQUFBLFFBQ0EscUJBQXFCO0FBQUEsVUFDbkIsUUFBUSxVQUFVO0FBQUEsVUFDbEIsSUFBSTtBQUFBLFVBQ0osY0FBYztBQUFBLFFBQ2hCO0FBQUEsUUFDQSxhQUFhO0FBQUEsVUFDWCxRQUFRLFVBQVU7QUFBQSxVQUNsQixJQUFJO0FBQUEsVUFDSixjQUFjO0FBQUEsUUFDaEI7QUFBQSxNQUNGO0FBQUEsSUFDRjtBQUFBLEVBQ0Y7QUFBRSxDQUFDOyIsCiAgIm5hbWVzIjogW10KfQo=
