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
      sidebar: [
        {
          label: 'Nocturne',
          items: [
            { label: 'Website', link: '/' },
            { label: 'Downloads', link: '/download' },
            { slug: 'docs' }
          ]
        },
        {
          label: 'Documentation',
          items: [
            { autogenerate: { directory: 'docs', collapsed: true } }
          ]
        }
      ]
    })
  ]
});
