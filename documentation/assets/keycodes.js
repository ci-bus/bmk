(function () {
  const ROWS = [
    [
      { l: 'Esc', z: 'HID_KEY_ESC', c: 0x29 },
      { s: 1 },
      { l: 'F1', z: 'HID_KEY_F1', c: 0x3a }, { l: 'F2', z: 'HID_KEY_F2', c: 0x3b }, { l: 'F3', z: 'HID_KEY_F3', c: 0x3c }, { l: 'F4', z: 'HID_KEY_F4', c: 0x3d },
      { s: 0.5 },
      { l: 'F5', z: 'HID_KEY_F5', c: 0x3e }, { l: 'F6', z: 'HID_KEY_F6', c: 0x3f }, { l: 'F7', z: 'HID_KEY_F7', c: 0x40 }, { l: 'F8', z: 'HID_KEY_F8', c: 0x41 },
      { s: 0.5 },
      { l: 'F9', z: 'HID_KEY_F9', c: 0x42 }, { l: 'F10', z: 'HID_KEY_F10', c: 0x43 }, { l: 'F11', z: 'HID_KEY_F11', c: 0x44 }, { l: 'F12', z: 'HID_KEY_F12', c: 0x45 },
      { s: 0.25 },
      { l: 'PrtSc', z: 'HID_KEY_SYSRQ', c: 0x46 }, { l: 'Scroll', z: 'HID_KEY_SCROLLLOCK', c: 0x47 }, { l: 'Pause', z: 'HID_KEY_PAUSE', c: 0x48 },
    ],
    [
      { l: '`\n~', z: 'HID_KEY_GRAVE', c: 0x35 },
      { l: '1\n!', z: 'HID_KEY_1', c: 0x1e }, { l: '2\n@', z: 'HID_KEY_2', c: 0x1f }, { l: '3\n#', z: 'HID_KEY_3', c: 0x20 }, { l: '4\n$', z: 'HID_KEY_4', c: 0x21 },
      { l: '5\n%', z: 'HID_KEY_5', c: 0x22 }, { l: '6\n^', z: 'HID_KEY_6', c: 0x23 }, { l: '7\n&', z: 'HID_KEY_7', c: 0x24 }, { l: '8\n*', z: 'HID_KEY_8', c: 0x25 },
      { l: '9\n(', z: 'HID_KEY_9', c: 0x26 }, { l: '0\n)', z: 'HID_KEY_0', c: 0x27 },
      { l: '-\n_', z: 'HID_KEY_MINUS', c: 0x2d }, { l: '=\n+', z: 'HID_KEY_EQUAL', c: 0x2e },
      { l: 'Backspace', z: 'HID_KEY_BACKSPACE', c: 0x2a, w: 2 },
      { s: 0.25 },
      { l: 'Ins', z: 'HID_KEY_INSERT', c: 0x49 }, { l: 'Home', z: 'HID_KEY_HOME', c: 0x4a }, { l: 'PgUp', z: 'HID_KEY_PAGEUP', c: 0x4b },
      { s: 0.25 },
      { l: 'Num', z: 'HID_KEY_NUMLOCK', c: 0x53 }, { l: '/', z: 'HID_KEY_KPSLASH', c: 0x54 }, { l: '*', z: 'HID_KEY_KPASTERISK', c: 0x55 }, { l: '-', z: 'HID_KEY_KPMINUS', c: 0x56 },
    ],
    [
      { l: 'Tab', z: 'HID_KEY_TAB', c: 0x2b, w: 1.5 },
      { l: 'Q', z: 'HID_KEY_Q', c: 0x14 }, { l: 'W', z: 'HID_KEY_W', c: 0x1a }, { l: 'E', z: 'HID_KEY_E', c: 0x08 }, { l: 'R', z: 'HID_KEY_R', c: 0x15 },
      { l: 'T', z: 'HID_KEY_T', c: 0x17 }, { l: 'Y', z: 'HID_KEY_Y', c: 0x1c }, { l: 'U', z: 'HID_KEY_U', c: 0x18 }, { l: 'I', z: 'HID_KEY_I', c: 0x0c },
      { l: 'O', z: 'HID_KEY_O', c: 0x12 }, { l: 'P', z: 'HID_KEY_P', c: 0x13 },
      { l: '[\n{', z: 'HID_KEY_LEFTBRACE', c: 0x2f }, { l: ']\n}', z: 'HID_KEY_RIGHTBRACE', c: 0x30 },
      { l: '\\\n|', z: 'HID_KEY_BACKSLASH', c: 0x31, w: 1.5 },
      { s: 0.25 },
      { l: 'Del', z: 'HID_KEY_DELETE', c: 0x4c }, { l: 'End', z: 'HID_KEY_END', c: 0x4d }, { l: 'PgDn', z: 'HID_KEY_PAGEDOWN', c: 0x4e },
      { s: 0.25 },
      { l: '7', z: 'HID_KEY_KP_7', c: 0x5f }, { l: '8', z: 'HID_KEY_KP_8', c: 0x60 }, { l: '9', z: 'HID_KEY_KP_9', c: 0x61 }, { l: '+', z: 'HID_KEY_KPPLUS', c: 0x57 },
    ],
    [
      { l: 'Caps', z: 'HID_KEY_CAPSLOCK', c: 0x39, w: 1.75 },
      { l: 'A', z: 'HID_KEY_A', c: 0x04 }, { l: 'S', z: 'HID_KEY_S', c: 0x16 }, { l: 'D', z: 'HID_KEY_D', c: 0x07 }, { l: 'F', z: 'HID_KEY_F', c: 0x09 },
      { l: 'G', z: 'HID_KEY_G', c: 0x0a }, { l: 'H', z: 'HID_KEY_H', c: 0x0b }, { l: 'J', z: 'HID_KEY_J', c: 0x0d }, { l: 'K', z: 'HID_KEY_K', c: 0x0e },
      { l: 'L', z: 'HID_KEY_L', c: 0x0f },
      { l: ';\n:', z: 'HID_KEY_SEMICOLON', c: 0x33 }, { l: '\'\n"', z: 'HID_KEY_APOSTROPHE', c: 0x34 },
      { l: 'Enter', z: 'HID_KEY_ENTER', c: 0x28, w: 2.25 },
      { s: 3.5 },
      { l: '4', z: 'HID_KEY_KP_4', c: 0x5c }, { l: '5', z: 'HID_KEY_KP_5', c: 0x5d }, { l: '6', z: 'HID_KEY_KP_6', c: 0x5e },
    ],
    [
      { l: 'Shift', z: 'HID_KEY_LSHIFT', c: 0xe1, w: 2.25, mod: true },
      { l: 'Z', z: 'HID_KEY_Z', c: 0x1d }, { l: 'X', z: 'HID_KEY_X', c: 0x1b }, { l: 'C', z: 'HID_KEY_C', c: 0x06 }, { l: 'V', z: 'HID_KEY_V', c: 0x19 },
      { l: 'B', z: 'HID_KEY_B', c: 0x05 }, { l: 'N', z: 'HID_KEY_N', c: 0x11 }, { l: 'M', z: 'HID_KEY_M', c: 0x10 },
      { l: ',\n<', z: 'HID_KEY_COMMA', c: 0x36 }, { l: '.\n>', z: 'HID_KEY_DOT', c: 0x37 }, { l: '/\n?', z: 'HID_KEY_SLASH', c: 0x38 },
      { l: 'Shift', z: 'HID_KEY_RSHIFT', c: 0xe5, w: 2.75, mod: true },
      { s: 1.25 },
      { l: '↑', z: 'HID_KEY_UP', c: 0x52 },
      { s: 1.25 },
      { l: '1', z: 'HID_KEY_KP_1', c: 0x59 }, { l: '2', z: 'HID_KEY_KP_2', c: 0x5a }, { l: '3', z: 'HID_KEY_KP_3', c: 0x5b },
      { l: 'Enter', z: 'HID_KEY_KPENTER', c: 0x58 },
    ],
    [
      { l: 'Ctrl', z: 'HID_KEY_LCTRL', c: 0xe0, w: 1.25, mod: true },
      { l: 'GUI', z: 'HID_KEY_LGUI', c: 0xe3, w: 1.25, mod: true },
      { l: 'Alt', z: 'HID_KEY_LALT', c: 0xe2, w: 1.25, mod: true },
      { l: 'Space', z: 'HID_KEY_SPACE', c: 0x2c, w: 6.25 },
      { l: 'Alt', z: 'HID_KEY_RALT', c: 0xe6, w: 1.25, mod: true },
      { l: 'GUI', z: 'HID_KEY_RGUI', c: 0xe7, w: 1.25, mod: true },
      { l: 'Menu', z: '0x65', c: 0x65, w: 1.25, raw: true },
      { l: 'Ctrl', z: 'HID_KEY_RCTRL', c: 0xe4, w: 1.25, mod: true },
      { s: 0.25 },
      { l: '←', z: 'HID_KEY_LEFT', c: 0x50 }, { l: '↓', z: 'HID_KEY_DOWN', c: 0x51 }, { l: '→', z: 'HID_KEY_RIGHT', c: 0x4f },
      { s: 0.25 },
      { l: '0', z: 'HID_KEY_KP_0', c: 0x62, w: 2 }, { l: '.', z: '0x63', c: 0x63, raw: true },
    ],
  ];

  const GAP = 4;

  function hex(value) {
    return '0x' + value.toString(16).toUpperCase().padStart(2, '0');
  }

  function line(className, text) {
    const span = document.createElement('span');
    span.className = className;
    span.textContent = text;
    return span;
  }

  document.addEventListener('DOMContentLoaded', () => {
    const board = document.getElementById('keyboard');
    const info = document.getElementById('key-info');
    if (!board || !info) {
      return;
    }
    const unit = parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--u')) || 42;
    const width = (units) => units * unit + (units - 1) * GAP + 'px';

    ROWS.forEach((row) => {
      const rowBox = document.createElement('div');
      rowBox.className = 'row';
      row.forEach((key) => {
        if (key.s !== undefined) {
          const spacer = document.createElement('div');
          spacer.style.width = width(key.s);
          rowBox.append(spacer);
          return;
        }
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'key' + (key.mod ? ' mod' : '') + (key.raw ? ' raw' : '');
        button.style.width = width(key.w || 1);
        key.l.split('\n').forEach((part, i) => {
          if (i) {
            button.append(document.createElement('br'));
          }
          button.append(part);
        });
        button.addEventListener('click', () => {
          board.querySelectorAll('.key.active').forEach((node) => node.classList.remove('active'));
          button.classList.add('active');
          window.bmk.copyText(key.z);
          info.replaceChildren(
            line('name', key.z),
            line('hint', 'HID usage: ' + hex(key.c)),
            line('hint', key.raw ? 'No named constant: use the value directly in the keymap.' : key.mod ? 'Modifier key.' : 'Standard key.')
          );
        });
        rowBox.append(button);
      });
      board.append(rowBox);
    });
  });
})();
