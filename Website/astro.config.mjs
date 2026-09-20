import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

const site = process.env.NOCTURNE_SITE_URL?.trim();

export default defineConfig({
  ...(site ? { site } : {}),
  output: 'static',
  integrations: [
    starlight({
      title: 'Nocturne Engine',
      description: 'Technical documentation for Nocturne Engine.',
      favicon: '/nocturne-logo.png',
      pagefind: true,
      disable404Route: true,
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
            { slug: 'docs/development/c-api-reference', label: 'C++ API Reference' },
            { slug: 'docs/development/production-engineering-standard' },
            { slug: 'docs/development/release-process' },
            { slug: 'docs/development/website-quality-deployment', label: 'Website Quality & Deployment' },
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
