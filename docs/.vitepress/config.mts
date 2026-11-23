import { defineConfig } from 'vitepress'

export default defineConfig({
  title: "UE_Cambriana",
  description: "TODO - Replace with your project description.",
  base: '/UE_Cambriana/',
  themeConfig: {
    nav: [
      { text: 'Home', link: '/' },
      {
        text: 'Unreal',
        items: [
          { text: 'Getting Started', link: '/unreal/getting-started' },
          { text: 'Grammar Assets', link: '/unreal/grammar-assets' },
          { text: 'Grammar Editor', link: '/unreal/grammar-editor' },
          { text: 'Live Actor', link: '/unreal/live-actor' },
          { text: 'Baking', link: '/unreal/baking' },
          { text: 'Data Channels', link: '/unreal/data-channels' }
        ]
      },
      {
        text: 'Grammar',
        items: [
          { text: 'Syntax Guide', link: '/grammar-guide' },
          { text: 'Format Specification', link: '/grammar-spec' }
        ]
      },
      { text: 'GitHub', link: 'https://github.com/ScottKirvan/UE_Cambriana' }
    ],
    sidebar: [
      {
        text: 'Using Cambriana in Unreal',
        items: [
          { text: 'Getting Started', link: '/unreal/getting-started' },
          { text: 'Grammar Assets', link: '/unreal/grammar-assets' },
          { text: 'Grammar Editor', link: '/unreal/grammar-editor' },
          { text: 'Live Actor', link: '/unreal/live-actor' },
          { text: 'Baking', link: '/unreal/baking' },
          { text: 'Data Channels', link: '/unreal/data-channels' }
        ]
      },
      {
        text: 'Grammar',
        items: [
          { text: 'Syntax Guide', link: '/grammar-guide' },
          { text: 'Format Specification', link: '/grammar-spec' }
        ]
      }
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
