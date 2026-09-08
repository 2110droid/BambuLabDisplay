const boards = {
  "c3-supermini": {
    id: "c3-supermini",
    name: "ESP32-C3 Super Mini",
    chip: "ESP32-C3",
    description: "Tiny USB-C board — ideal for a compact display build.",
    manifest: "manifests/c3-supermini.json",
    image: "assets/esp32-c3-supermini-pinout.jpg",
    pins: [
      ["VCC", "3.3V"],
      ["GND", "GND"],
      ["SCL / CLK", "GPIO 4"],
      ["SDA / DIN / MOSI", "GPIO 5"],
      ["DC", "GPIO 6"],
      ["CS", "GPIO 7"],
      ["RST / RES", "GPIO 10"],
      ["BL / BLK", "GPIO 3"]
    ],
    notes: [
      "MISO is not required by the GC9A01 display.",
      "GPIO 2, 8 and 9 are intentionally left unused by the display.",
      "GPIO 20 and 21 remain available for future expansion."
    ]
  },

  "s3-devkit": {
    id: "s3-devkit",
    name: "ESP32-S3 DevKit",
    chip: "ESP32-S3",
    description: "Generic ESP32-S3 DevKitC-style board with plenty of GPIO.",
    manifest: "manifests/s3-devkit.json",
    pins: [
      ["VCC", "3.3V"],
      ["GND", "GND"],
      ["SCL / CLK", "GPIO 12"],
      ["SDA / DIN / MOSI", "GPIO 11"],
      ["DC", "GPIO 10"],
      ["CS", "GPIO 9"],
      ["RST / RES", "GPIO 14"],
      ["BL / BLK", "GPIO 13"]
    ],
    notes: [
      "This profile targets a generic ESP32-S3 DevKitC-style board.",
      "Check that these GPIO pins are broken out on your exact S3 board before wiring.",
      "MISO is not required."
    ]
  },

  "esp32-wroom": {
    id: "esp32-wroom",
    name: "ESP32 DevKit / WROOM",
    chip: "ESP32",
    description: "Classic ESP32-WROOM DevKit profile using common safe GPIO.",
    manifest: "manifests/esp32-wroom.json",
    pins: [
      ["VCC", "3.3V"],
      ["GND", "GND"],
      ["SCL / CLK", "GPIO 18"],
      ["SDA / DIN / MOSI", "GPIO 23"],
      ["DC", "GPIO 16"],
      ["CS", "GPIO 17"],
      ["RST / RES", "GPIO 19"],
      ["BL / BLK", "GPIO 21"]
    ],
    notes: [
      "This profile is intended for common ESP32-WROOM DevKit boards.",
      "Avoid assuming every clone exposes exactly the same labels; verify the GPIO numbers.",
      "MISO is not required."
    ]
  }
};

let selectedBoardId = "c3-supermini";

const boardOptions = document.querySelector("#board-options");
const boardSummary = document.querySelector("#board-summary");
const installButton = document.querySelector("#install-button");

const wiringCard = document.querySelector("#wiring-card");
const wiringBoardName = document.querySelector("#wiring-board-name");
const wiringChip = document.querySelector("#wiring-chip");
const pinoutTable = document.querySelector("#pinout-table");
const wiringNotes = document.querySelector("#wiring-notes");
const boardVisual = document.querySelector("#board-visual");

const serialStatus = document.querySelector("#serial-status");
const serialLog = document.querySelector("#serial-log");

const form = document.querySelector("#config-form");

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function renderBoardOptions() {
  boardOptions.innerHTML = Object.values(boards).map(board => `
    <button
      type="button"
      class="board-option ${board.id === selectedBoardId ? "selected" : ""}"
      data-board="${board.id}"
      role="radio"
      aria-checked="${board.id === selectedBoardId}"
    >
      <span class="board-chip">${escapeHtml(board.chip)}</span>
      <strong>${escapeHtml(board.name)}</strong>
      <span>${escapeHtml(board.description)}</span>
    </button>
  `).join("");

  boardOptions.querySelectorAll(".board-option").forEach(button => {
    button.addEventListener("click", () => {
      selectBoard(button.dataset.board);
    });
  });
}

function selectBoard(boardId) {
  selectedBoardId = boardId;
  const board = boards[boardId];

  renderBoardOptions();

  boardSummary.classList.add("visible");
  boardSummary.innerHTML = `
    <strong>${escapeHtml(board.name)}</strong> selected.
    The installer will use the <code>${escapeHtml(board.chip)}</code>
    firmware and show its pinout after configuration.
  `;

  installButton.setAttribute("manifest", board.manifest);

  renderWiring(board, !wiringCard.classList.contains("locked"));
}

function renderWiring(board, unlock = false) {
  wiringBoardName.textContent = board.name;
  wiringChip.textContent = board.chip;

  pinoutTable.innerHTML = board.pins.map(([from, to]) => `
    <div class="pin-row">
      <span class="from">${escapeHtml(from)}</span>
      <span class="arrow">→</span>
      <span class="to">${escapeHtml(to)}</span>
    </div>
  `).join("");

  wiringNotes.innerHTML = board.notes.map(note => `
    <div class="wiring-note">${escapeHtml(note)}</div>
  `).join("");

  if (board.image) {
    boardVisual.innerHTML = `
      <img src="${escapeHtml(board.image)}" alt="${escapeHtml(board.name)} pinout">
    `;
  } else {
    boardVisual.innerHTML = `
      <div class="generic-board" data-label="${escapeHtml(board.name)}"></div>
    `;
  }

  if (unlock) {
    wiringCard.classList.remove("locked");
  }
}

function setStatus(type, title, message) {
  serialStatus.innerHTML = `
    <span class="status-icon ${type}"></span>
    <div>
      <strong>${escapeHtml(title)}</strong>
      <p>${escapeHtml(message)}</p>
    </div>
  `;
}

function setLog(text) {
  serialLog.textContent = text || "No output.";
  serialLog.scrollTop = serialLog.scrollHeight;
}

function appendLog(text) {
  if (serialLog.textContent === "No serial session yet.") {
    serialLog.textContent = "";
  }

  serialLog.textContent += text;
  serialLog.scrollTop = serialLog.scrollHeight;
}

function getConfigPayload() {
  if (!form.reportValidity()) {
    throw new Error("Please complete the required fields first.");
  }

  return {
    cmd: "config",
    ssid: document.querySelector("#ssid").value.trim(),
    wifi_password: document.querySelector("#wifi-password").value,
    printer_ip: document.querySelector("#printer-ip").value.trim(),
    printer_serial: document.querySelector("#printer-serial").value.trim(),
    access_code: document.querySelector("#access-code").value.trim(),
    hostname: document.querySelector("#hostname").value.trim(),
    timezone: document.querySelector("#timezone").value
  };
}

async function openSerialAndSend(payload, expectedEvents, timeoutMs = 12000) {
  if (!("serial" in navigator)) {
    throw new Error(
      "This browser does not expose Web Serial. Use a compatible desktop browser over HTTPS."
    );
  }

  const port = await navigator.serial.requestPort();

  await port.open({
    baudRate: 115200
  });

  const encoder = new TextEncoder();
  const decoder = new TextDecoder();

  const writer = port.writable.getWriter();
  const reader = port.readable.getReader();

  let buffer = "";
  let allText = "";

  try {
    // Give native-USB boards a moment after the serial port opens.
    await new Promise(resolve => setTimeout(resolve, 500));

    const line = JSON.stringify(payload) + "\n";
    await writer.write(encoder.encode(line));

    appendLog(`> ${line}`);

    const deadline = Date.now() + timeoutMs;

    while (Date.now() < deadline) {
      const remaining = deadline - Date.now();

      const result = await Promise.race([
        reader.read(),
        new Promise(resolve => {
          setTimeout(
            () => resolve({ timeout: true }),
            Math.min(remaining, 800)
          );
        })
      ]);

      if (result?.timeout) {
        continue;
      }

      if (result.done) {
        break;
      }

      const chunk = decoder.decode(result.value, { stream: true });
      buffer += chunk;
      allText += chunk;
      appendLog(chunk);

      const lines = buffer.split(/\r?\n/);
      buffer = lines.pop() ?? "";

      for (const rawLine of lines) {
        const trimmed = rawLine.trim();

        if (!trimmed.startsWith("{")) {
          continue;
        }

        try {
          const message = JSON.parse(trimmed);

          if (
            message.event &&
            expectedEvents.includes(message.event)
          ) {
            return {
              message,
              allText
            };
          }
        } catch {
          // Ignore ordinary boot/log lines.
        }
      }
    }

    throw new Error(
      "No expected response was received. The board may be rebooting or the wrong serial port was selected."
    );
  } finally {
    try { reader.releaseLock(); } catch {}
    try { writer.releaseLock(); } catch {}
    try { await port.close(); } catch {}
  }
}

async function saveSettings() {
  try {
    const payload = getConfigPayload();

    setLog("");
    setStatus(
      "working",
      "Connecting over USB",
      "Select the serial port that belongs to your flashed ESP32."
    );

    const { message } = await openSerialAndSend(
      payload,
      ["config_saved"]
    );

    if (!message.ok) {
      throw new Error(
        message.error || "The ESP32 rejected the configuration."
      );
    }

    setStatus(
      "success",
      "Configuration saved",
      "The ESP32 is restarting with your Wi-Fi and printer settings."
    );

    renderWiring(
      boards[selectedBoardId],
      true
    );

    wiringCard.scrollIntoView({
      behavior: "smooth",
      block: "center"
    });
  } catch (error) {
    setStatus(
      "error",
      "Could not save settings",
      error.message
    );
  }
}

async function readSettings() {
  try {
    setLog("");
    setStatus(
      "working",
      "Reading device",
      "Select the ESP32 serial port."
    );

    const { message } = await openSerialAndSend(
      { cmd: "read" },
      ["config"]
    );

    if (!message.ok) {
      throw new Error("The device did not return a valid configuration.");
    }

    if (
      message.board_id &&
      boards[message.board_id]
    ) {
      selectBoard(message.board_id);
    }

    document.querySelector("#ssid").value =
      message.ssid ?? "";

    document.querySelector("#printer-ip").value =
      message.printer_ip ?? "";

    document.querySelector("#printer-serial").value =
      message.printer_serial ?? "";

    document.querySelector("#hostname").value =
      message.hostname ?? "Bambu-P2S-Display";

    if (message.timezone) {
      const timezoneSelect =
        document.querySelector("#timezone");

      const matchingOption =
        [...timezoneSelect.options]
          .find(option => option.value === message.timezone);

      if (matchingOption) {
        timezoneSelect.value = message.timezone;
      }
    }

    document.querySelector("#wifi-password").value = "";
    document.querySelector("#access-code").value = "";

    setStatus(
      "success",
      "Settings read",
      "Secrets are intentionally not returned. Re-enter the Wi-Fi password and LAN Access Code before saving changes."
    );

    renderWiring(
      boards[selectedBoardId],
      true
    );
  } catch (error) {
    setStatus(
      "error",
      "Could not read settings",
      error.message
    );
  }
}

async function clearSettings() {
  const confirmed = window.confirm(
    "Clear all saved Wi-Fi and Bambu printer settings from the ESP32?"
  );

  if (!confirmed) {
    return;
  }

  try {
    setLog("");
    setStatus(
      "working",
      "Clearing settings",
      "Select the ESP32 serial port."
    );

    const { message } = await openSerialAndSend(
      { cmd: "clear" },
      ["config_cleared"]
    );

    if (!message.ok) {
      throw new Error("The device could not clear its settings.");
    }

    setStatus(
      "success",
      "Settings cleared",
      "The ESP32 is restarting and will wait for a new configuration."
    );

    wiringCard.classList.add("locked");
  } catch (error) {
    setStatus(
      "error",
      "Could not clear settings",
      error.message
    );
  }
}

document.querySelector("#send-config")
  .addEventListener("click", saveSettings);

document.querySelector("#read-config")
  .addEventListener("click", readSettings);

document.querySelector("#clear-config")
  .addEventListener("click", clearSettings);

document.querySelectorAll("[data-reveal]")
  .forEach(button => {
    button.addEventListener("click", () => {
      const input =
        document.querySelector(
          `#${button.dataset.reveal}`
        );

      const show = input.type === "password";

      input.type = show ? "text" : "password";
      button.textContent = show ? "Hide" : "Show";
    });
  });

renderBoardOptions();
selectBoard(selectedBoardId);
