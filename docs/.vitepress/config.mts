import { defineConfig } from 'vitepress'

export default defineConfig({
  title: "UE_Cambriana",
  description: "TODO - Replace with your project description.",
  base: '/UE_Cambriana/',
  themeConfig: {
    nav: [
      { text: 'Home', link: '/' },
      { text: 'GitHub', link: 'https://github.com/ScottKirvan/UE_Cambriana' }
    ],
    socialLinks: [
      { icon: 'github', link: 'https://github.com/ScottKirvan/UE_Cambriana' },
      { icon: 'discord', link: 'https://discord.gg/TN6XJSNK5Y' }
    ],
    footer: {
      message: 'Released under the MIT License.',
      copyright: 'Copyright © Scott Kirvan'
    }
  }
})
