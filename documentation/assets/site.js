(function () {
  const REPO_URL = 'https://github.com/ci-bus/bmk';
  const THEME_KEY = 'bmk-theme';

  const NAV = [
    {
      title: 'Getting started',
      pages: [
        { file: 'index.html', title: 'Introduction', keys: 'overview features home supported keyboards' },
        { file: 'installation.html', title: 'Installation', keys: 'docker devcontainer vs code west zephyr setup volumes' },
        { file: 'building.html', title: 'Building', keys: 'west build compile uf2 pristine board keyboard cmake' },
        { file: 'flashing.html', title: 'Flashing', keys: 'uf2 bootloader hid_boot hid_reset west flash' },
      ],
    },
    {
      title: 'Keymaps',
      pages: [
        { file: 'keymap.html', title: 'Keymaps and layers', keys: 'keymap.c layers fn_mo fn_to hid_key_trans hid_key_none matrix index' },
        { file: 'keycodes.html', title: 'Keycodes', keys: 'hid_key modifiers media consumer special copy' },
        { file: 'tap-hold.html', title: 'Tap-hold', keys: 'th_lshift th_rctrl mod tap tap_hold_press_delay tap_hold_release_delay' },
        { file: 'encoders.html', title: 'Encoders', keys: 'rotary encoder pin_encoders volume debounce_encoder' },
      ],
    },
    {
      title: 'Features',
      pages: [
        { file: 'bluetooth.html', title: 'Bluetooth', keys: 'ble pairing bond advertising hid_bt_clear tx power' },
        { file: 'usb.html', title: 'USB', keys: 'usb hid vbus transport priority vid pid 6kro' },
        { file: 'power.html', title: 'Sleep and power', keys: 'sleep deep sleep timeout power_ext hid_deep_sleep consumption' },
        { file: 'rgb.html', title: 'RGB lighting', keys: 'ws2812 leds rgb_port rgb_pin rgb_leds rgb_effects hid_key_rgb' },
        { file: 'battery.html', title: 'Battery', keys: 'battery level percentage bat_update vddh' },
        { file: 'gaming.html', title: 'Gaming mode', keys: 'hid_gaming_on hid_gaming_off scan rate gaming_cycle_delay' },
        { file: 'dongle.html', title: 'USB dongle', keys: 'dongle central wireless receiver free mark' },
      ],
    },
    {
      title: 'Reference',
      pages: [
        { file: 'configuration.html', title: 'Configuration options', keys: 'keyboard.h defines debounce cycle_delay sleep_timeout logs prj.conf options' },
        { file: 'project-structure.html', title: 'Project structure', keys: 'files folders source tree' },
      ],
    },
    {
      title: 'Development',
      pages: [
        { file: 'new-keyboard.html', title: 'Adding a keyboard', keys: 'new keyboard.h keyboard.c keymap.c pins matrix diodes pin_cols pin_rows' },
        { file: 'hardware.html', title: 'Boards and hardware', keys: 'nrf52840 nrf52833 e73 flash partitions bootloader clock' },
        { file: 'logging.html', title: 'Logs', keys: 'logs debug serial cdc acm log_level' },
      ],
    },
    {
      title: 'Help',
      pages: [
        { file: 'troubleshooting.html', title: 'Troubleshooting', keys: 'problems errors faq pairing build fails' },
      ],
    },
  ];

  function readTheme() {
    try {
      return localStorage.getItem(THEME_KEY);
    } catch (e) {
      return null;
    }
  }

  function saveTheme(theme) {
    try {
      localStorage.setItem(THEME_KEY, theme);
    } catch (e) {
      return;
    }
  }

  const root = document.documentElement;
  root.dataset.theme = readTheme() || 'dark';

  function el(tag, props, children) {
    const node = document.createElement(tag);
    Object.assign(node, props || {});
    (children || []).forEach((child) => node.append(child));
    return node;
  }

  let toastTimer;

  function toast(text) {
    let box = document.querySelector('.toast');
    if (!box) {
      box = el('div', { className: 'toast' });
      document.body.append(box);
    }
    box.textContent = text;
    box.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => box.classList.remove('show'), 1800);
  }

  function copyText(text) {
    const done = () => toast(text + ' copied');
    if (navigator.clipboard && window.isSecureContext) {
      navigator.clipboard.writeText(text).then(done, () => fallbackCopy(text, done));
    } else {
      fallbackCopy(text, done);
    }
  }

  function fallbackCopy(text, done) {
    const area = el('textarea', { value: text });
    area.style.position = 'fixed';
    area.style.opacity = '0';
    document.body.append(area);
    area.select();
    const ok = document.execCommand('copy');
    area.remove();
    if (ok) {
      done();
    }
  }

  window.bmk = { copyText, toast };

  const pages = NAV.flatMap((group) => group.pages);
  const currentFile = decodeURIComponent(location.pathname.split('/').pop()) || 'index.html';

  function buildTopbar() {
    const menu = el('button', { className: 'icon-btn menu-btn', type: 'button', textContent: '☰' });
    menu.setAttribute('aria-label', 'Toggle menu');
    menu.addEventListener('click', () => document.body.classList.toggle('nav-open'));

    const brand = el('a', { className: 'brand', href: 'index.html', textContent: 'BMK' }, []);
    brand.append(el('span', { textContent: 'Bluetooth Mechanical Keyboard' }));

    const search = el('input', { className: 'search', type: 'search', placeholder: 'Filter pages…' });
    search.setAttribute('aria-label', 'Filter pages');

    const theme = el('button', { className: 'icon-btn', type: 'button' });
    const paintTheme = () => {
      theme.textContent = root.dataset.theme === 'dark' ? 'Light' : 'Dark';
    };
    paintTheme();
    theme.addEventListener('click', () => {
      root.dataset.theme = root.dataset.theme === 'dark' ? 'light' : 'dark';
      saveTheme(root.dataset.theme);
      paintTheme();
    });

    const repo = el('a', { className: 'icon-btn', href: REPO_URL, textContent: 'GitHub' });

    return { bar: el('header', { className: 'topbar' }, [menu, brand, el('div', { className: 'grow' }), search, theme, repo]), search };
  }

  function buildSidebar(search) {
    const side = el('nav', { className: 'sidebar' });
    const empty = el('div', { className: 'nav-empty', textContent: 'No pages match.', hidden: true });
    const groups = NAV.map((group) => {
      const links = group.pages.map((page) => {
        const link = el('a', { className: 'nav-link', href: page.file, textContent: page.title });
        if (page.file === currentFile) {
          link.classList.add('active');
        }
        return { link, haystack: (page.title + ' ' + page.keys).toLowerCase() };
      });
      const box = el('div', { className: 'nav-group' }, [el('p', { className: 'nav-title', textContent: group.title })]);
      links.forEach((item) => box.append(item.link));
      side.append(box);
      return { box, links };
    });
    side.append(empty);

    const visibleLinks = () => groups.flatMap((group) => group.links).filter((item) => !item.link.hidden);

    search.addEventListener('input', () => {
      const words = search.value.toLowerCase().split(/\s+/).filter(Boolean);
      groups.forEach((group) => {
        group.links.forEach((item) => {
          item.link.hidden = !words.every((word) => item.haystack.includes(word));
        });
        group.box.hidden = group.links.every((item) => item.link.hidden);
      });
      empty.hidden = visibleLinks().length > 0;
      if (words.length) {
        document.body.classList.add('nav-open');
      }
    });

    search.addEventListener('keydown', (event) => {
      const first = visibleLinks()[0];
      if (event.key === 'Enter' && search.value.trim() && first) {
        location.href = first.link.href;
      }
    });

    return side;
  }

  function slugify(text, used) {
    const base = text.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '') || 'section';
    let slug = base;
    let n = 2;
    while (used.has(slug)) {
      slug = base + '-' + n++;
    }
    used.add(slug);
    return slug;
  }

  function buildToc(main) {
    const used = new Set();
    const heads = Array.from(main.querySelectorAll('h2, h3'));
    const labels = heads.map((head) => head.textContent);
    heads.forEach((head) => {
      if (!head.id) {
        head.id = slugify(head.textContent, used);
      } else {
        used.add(head.id);
      }
      head.append(el('a', { className: 'anchor', href: '#' + head.id, textContent: '#' }));
    });
    if (heads.length < 2 || main.classList.contains('wide')) {
      return null;
    }
    const toc = el('aside', { className: 'toc' }, [el('p', { className: 'toc-title', textContent: 'On this page' })]);
    const links = heads.map((head, i) => {
      const link = el('a', { href: '#' + head.id, textContent: labels[i] });
      if (head.tagName === 'H3') {
        link.classList.add('sub');
      }
      toc.append(link);
      return link;
    });
    const mark = () => {
      let active = 0;
      heads.forEach((head, i) => {
        if (head.getBoundingClientRect().top < 120) {
          active = i;
        }
      });
      links.forEach((link, i) => link.classList.toggle('current', i === active));
    };
    window.addEventListener('scroll', mark, { passive: true });
    mark();
    return toc;
  }

  function buildPager() {
    const index = pages.findIndex((page) => page.file === currentFile);
    if (index < 0) {
      return null;
    }
    const pager = el('div', { className: 'pager' });
    const prev = pages[index - 1];
    const next = pages[index + 1];
    if (prev) {
      pager.append(el('a', { href: prev.file }, [el('small', { textContent: 'Previous' }), el('b', { textContent: prev.title })]));
    }
    if (next) {
      pager.append(el('a', { className: 'next', href: next.file }, [el('small', { textContent: 'Next' }), el('b', { textContent: next.title })]));
    }
    return pager;
  }

  function enhanceContent(main) {
    main.querySelectorAll('pre').forEach((pre) => {
      const button = el('button', { className: 'copy-btn', type: 'button', textContent: 'Copy' });
      button.addEventListener('click', () => copyText(pre.querySelector('code').textContent.trim()));
      pre.append(button);
    });
    main.querySelectorAll('table').forEach((table) => {
      const wrap = el('div', { className: 'table-wrap' });
      table.replaceWith(wrap);
      wrap.append(table);
    });
    main.querySelectorAll('.copyable').forEach((node) => {
      node.title = 'Click to copy';
      node.addEventListener('click', () => copyText(node.textContent.trim()));
    });
  }

  document.addEventListener('DOMContentLoaded', () => {
    const main = document.querySelector('main');
    if (!main) {
      return;
    }
    const { bar, search } = buildTopbar();
    const sidebar = buildSidebar(search);
    const backdrop = el('div', { className: 'backdrop' });
    backdrop.addEventListener('click', () => document.body.classList.remove('nav-open'));

    enhanceContent(main);
    const pager = buildPager();
    if (pager) {
      main.append(pager);
    }
    const toc = buildToc(main);

    const page = el('div', { className: 'page' });
    main.replaceWith(page);
    page.append(main);
    if (toc) {
      page.append(toc);
    }
    document.body.prepend(bar, sidebar, backdrop);

    const active = sidebar.querySelector('.active');
    if (active) {
      active.scrollIntoView({ block: 'nearest' });
    }
    if (location.hash) {
      const target = document.getElementById(location.hash.slice(1));
      if (target) {
        target.scrollIntoView();
      }
    }
  });
})();
