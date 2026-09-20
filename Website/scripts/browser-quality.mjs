import { chromium } from 'playwright';
import { writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const websiteRoot = path.resolve(scriptDir, '..');
const baseUrl = (process.env.NOCTURNE_PREVIEW_URL || 'http://127.0.0.1:4322').replace(/\/$/, '');

const routes = [
  '/',
  '/download/',
  '/docs/',
  '/docs/systems/runtime/',
  '/docs/systems/editor/'
];

const viewports = [
  { name: 'mobile', width: 375, height: 812 },
  { name: 'tablet', width: 768, height: 1024 },
  { name: 'desktop', width: 1440, height: 900 }
];

const browser = await chromium.launch({ headless: true });
const results = [];

try {
  for (const viewport of viewports) {
    const context = await browser.newContext({
      viewport: { width: viewport.width, height: viewport.height },
      reducedMotion: 'reduce'
    });

    for (const route of routes) {
      const page = await context.newPage();
      const pageErrors = [];
      page.on('pageerror', (error) => pageErrors.push(String(error)));

      const response = await page.goto(baseUrl + route, {
        waitUntil: 'domcontentloaded',
        timeout: 30000
      });

      if (!response || response.status() >= 400) {
        throw new Error(`${viewport.name} ${route}: HTTP ${response?.status() ?? 'no response'}`);
      }

      await page.waitForTimeout(200);

      const audit = await page.evaluate(() => {
        const visible = (element) => {
          const style = getComputedStyle(element);
          const rect = element.getBoundingClientRect();
          return style.visibility !== 'hidden' &&
            style.display !== 'none' &&
            rect.width > 0 &&
            rect.height > 0;
        };

        const accessibleName = (element) => {
          const ariaLabel = element.getAttribute('aria-label')?.trim();
          if (ariaLabel) return ariaLabel;

          const labelledBy = element.getAttribute('aria-labelledby');
          if (labelledBy) {
            const value = labelledBy
              .split(/\s+/)
              .map((id) => document.getElementById(id)?.textContent?.trim() || '')
              .join(' ')
              .trim();
            if (value) return value;
          }

          const text = element.textContent?.trim();
          if (text) return text;

          const title = element.getAttribute('title')?.trim();
          if (title) return title;

          const imageAlt = element.querySelector('img[alt]')?.getAttribute('alt')?.trim();
          return imageAlt || '';
        };

        const ids = [...document.querySelectorAll('[id]')].map((el) => el.id);
        const duplicateIds = [...new Set(ids.filter((id, index) => ids.indexOf(id) !== index))];

        const unnamedInteractive = [...document.querySelectorAll('a[href],button')]
          .filter(visible)
          .filter((element) => accessibleName(element).length === 0)
          .map((element) => element.outerHTML.slice(0, 180));

        return {
          lang: document.documentElement.lang,
          title: document.title,
          mainCount: document.querySelectorAll('main').length,
          h1Count: document.querySelectorAll('h1').length,
          missingAltCount: document.querySelectorAll('img:not([alt])').length,
          positiveTabIndexCount: [...document.querySelectorAll('[tabindex]')]
            .filter((element) => Number(element.getAttribute('tabindex')) > 0).length,
          duplicateIds,
          unnamedInteractive,
          scrollWidth: document.documentElement.scrollWidth,
          viewportWidth: window.innerWidth,
          mainWidth: document.querySelector('main')?.getBoundingClientRect().width || 0
        };
      });

      if (!audit.lang) throw new Error(`${route}: missing html lang`);
      if (!audit.title) throw new Error(`${route}: missing document title`);
      if (audit.mainCount !== 1) throw new Error(`${route}: expected exactly one main element`);
      if (audit.h1Count < 1) throw new Error(`${route}: missing h1`);
      if (audit.missingAltCount > 0) throw new Error(`${route}: image without alt attribute`);
      if (audit.positiveTabIndexCount > 0) throw new Error(`${route}: positive tabindex found`);
      if (audit.duplicateIds.length > 0) {
        throw new Error(`${route}: duplicate ids: ${audit.duplicateIds.join(', ')}`);
      }
      if (audit.unnamedInteractive.length > 0) {
        throw new Error(
          `${route}: unnamed visible interactive element(s): ${audit.unnamedInteractive.join(' | ')}`
        );
      }
      if (audit.scrollWidth > audit.viewportWidth + 2) {
        throw new Error(
          `${viewport.name} ${route}: horizontal overflow ${audit.scrollWidth}px > ${audit.viewportWidth}px`
        );
      }
      if (audit.mainWidth < Math.min(280, audit.viewportWidth - 20)) {
        throw new Error(`${viewport.name} ${route}: main content width is unexpectedly small`);
      }
      if (pageErrors.length > 0) {
        throw new Error(`${route}: browser page error(s): ${pageErrors.join(' | ')}`);
      }

      if (viewport.name === 'desktop') {
        const seen = new Set();
        let visibleFocusIndicators = 0;

        for (let i = 0; i < 8; ++i) {
          await page.keyboard.press('Tab');
          const focus = await page.evaluate(() => {
            const element = document.activeElement;
            if (!(element instanceof HTMLElement) || element === document.body) {
              return null;
            }
            const style = getComputedStyle(element);
            const rect = element.getBoundingClientRect();
            return {
              signature: [
                element.tagName,
                element.id,
                element.getAttribute('href'),
                element.getAttribute('aria-label'),
                element.textContent?.trim().slice(0, 60)
              ].join('|'),
              visible: rect.width > 0 && rect.height > 0 &&
                style.visibility !== 'hidden' && style.display !== 'none',
              indicator:
                (style.outlineStyle !== 'none' && parseFloat(style.outlineWidth) > 0) ||
                style.boxShadow !== 'none'
            };
          });

          if (focus?.visible) {
            seen.add(focus.signature);
            if (focus.indicator) visibleFocusIndicators += 1;
          }
        }

        if (seen.size < 3) {
          throw new Error(`${route}: keyboard Tab navigation reached fewer than three visible targets`);
        }
        if (visibleFocusIndicators < 1) {
          throw new Error(`${route}: no visible keyboard focus indicator detected`);
        }
      }

      if (viewport.name === 'desktop' && route === '/') {
        await page.goto(baseUrl + '/', { waitUntil: 'domcontentloaded' });
        await page.keyboard.press('Tab');
        const firstFocus = await page.evaluate(() => ({
          text: document.activeElement?.textContent?.trim() || '',
          href: document.activeElement?.getAttribute?.('href') || ''
        }));

        if (!/skip to content/i.test(firstFocus.text)) {
          throw new Error('Landing page first keyboard target must be the Skip to content link.');
        }

        await page.keyboard.press('Enter');
        const skipWorked = await page.evaluate(() =>
          location.hash === '#main-content' ||
          document.activeElement?.id === 'main-content'
        );
        if (!skipWorked) throw new Error('Landing page skip link did not move to main content.');
      }

      results.push({ viewport: viewport.name, route, audit });
      await page.close();
    }

    await context.close();
  }
} finally {
  await browser.close();
}

await writeFile(
  path.join(websiteRoot, 'dist', 'browser-quality-report.json'),
  JSON.stringify({ schemaVersion: 1, baseUrl, results }, null, 2) + '\n',
  'utf8'
);

console.log(`Browser quality passed for ${routes.length} routes across ${viewports.length} viewport profiles.`);
