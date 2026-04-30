import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// All API calls go through FastAPI
const API = 'http://localhost:8000'

export default defineConfig({
  root: '.',
  base: './',
  plugins: [react()],
  server: {
    host: '0.0.0.0',
    allowedHosts: [
      '0.0.0.0',
    ],
    proxy: {
      '/auth': { target: API, changeOrigin: true },
      '/game': { target: API, changeOrigin: true },
      '/leaderboard': { target: API, changeOrigin: true },
      '/multiplayer': { target: API, changeOrigin: true },
      '/user': { target: API, changeOrigin: true },
      '/health': { target: API, changeOrigin: true },
    },
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
  },
  test: {
    globals: true,
    environment: 'jsdom',
    setupFiles: './src/test-setup.ts',
    css: true,
  },
})
