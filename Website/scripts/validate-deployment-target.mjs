const raw = process.argv[2] || process.env.NOCTURNE_SITE_URL || '';

if (!raw.trim()) {
  throw new Error('A production site URL is required.');
}

const site = new URL(raw);

if (site.protocol !== 'https:') {
  throw new Error('Production site URL must use https.');
}

if (site.pathname !== '/' || site.search || site.hash) {
  throw new Error(
    'Production site URL must be root-hosted. Subpath deployment is not supported by the current absolute-route contract.'
  );
}

if (
  site.hostname === 'localhost' ||
  site.hostname.endsWith('.invalid') ||
  site.hostname.endsWith('.example')
) {
  throw new Error('Production site URL must be a real public hostname.');
}

const repository = process.env.GITHUB_REPOSITORY || '';
const [owner, name] = repository.split('/');
if (
  owner &&
  name &&
  site.hostname.toLowerCase() === `${owner}.github.io`.toLowerCase() &&
  name.toLowerCase() !== `${owner}.github.io`.toLowerCase()
) {
  throw new Error(
    'This repository would be a GitHub Pages project subpath. Configure a root-hosted custom domain before deployment.'
  );
}

console.log(`Validated root-hosted deployment target: ${site.origin}`);
