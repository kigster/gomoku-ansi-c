import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// Backend target: use port 10000 for Envoy proxy, or 9500 for direct backend
const BACKEND = 'http://localhost:10000'

export default defineConfig({
  root: '.',
  base: './',
  plugins: [react()],
  server: {
    host: '0.0.0.0',
    allowedHosts: [
      '0.0.0.0',
      '172.16.10.89',
      'macbook-pro-m3-kig.local'
    ],
    proxy: {
      '/gomoku': {
        target: BACKEND,
        changeOrigin: true,
      },
      '/health': {
        target: BACKEND,
        changeOrigin: true,
      },
      '/ready': {
        target: BACKEND,
        changeOrigin: true,
      },
    },
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
  },
})
