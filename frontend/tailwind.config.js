/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        wood: {
          light: '#DEB887',
          DEFAULT: '#C19A6B',
          dark: '#8B6914',
          grain: '#A0784C',
        },
      },
    },
  },
  plugins: [],
}
