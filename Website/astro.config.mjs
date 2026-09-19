import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
  output: 'static',
  integrations: [
    starlight({
      title: 'Nocturne Engine',
      description: 'Technical documentation for Nocturne Engine.',
      pagefind: true,
      customCss: ['./src/styles/nocturne.css'],
      components: {
        SiteTitle: './src/components/docs/NocturneSiteTitle.astro',
        Search: './src/components/docs/NocturneSearch.astro',
        PageTitle: './src/components/docs/NocturnePageTitle.astro',
        ThemeSelect: './src/components/docs/NocturneThemeSelect.astro'
      },
      sidebar: [
        {
          label: 'Nocturne',
          items: [
            { slug: 'docs', label: 'Documentation Home' },
            { label: 'Website', link: '/' },
            { label: 'Downloads', link: '/download' }
          ]
        },
        {
          label: 'Architecture',
          items: [
            { slug: 'docs/architecture/overview', label: 'Overview' }
          ]
        },
        {
          label: 'Systems',
          items: [
            { slug: 'docs/systems/runtime' },
            { slug: 'docs/systems/resources' },
            { slug: 'docs/systems/rendering' },
            { slug: 'docs/systems/world-ecs' },
            { slug: 'docs/systems/editor' }
          ]
        },
        {
          label: 'Development',
          items: [
            { slug: 'docs/development/documentation-model' },
            { slug: 'docs/development/production-engineering-standard' },
            { slug: 'docs/development/website-specification' }
          ]
        },
        {
          label: 'Development History',
          collapsed: true,
          items: [
            { autogenerate: { directory: 'docs/history', collapsed: true } }
          ]
        }
      ]
    })
  ]
});
