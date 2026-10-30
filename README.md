<p align="center">
  <img src="assets/QuickAssist-144x144.png" alt="QuickAssist logo" width="112" height="112">
</p>

<h1 align="center">QuickAssist</h1>

<p align="center">
  Talk to Home Assistant Assist from your Pebble Time 2.
</p>

<p align="center">
  <a href="https://github.com/tylxr59/QuickAssist/actions/workflows/ci.yml"><img alt="Release status" src="https://github.com/tylxr59/QuickAssist/actions/workflows/ci.yml/badge.svg"></a>
  <a href="https://github.com/tylxr59/QuickAssist/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/tylxr59/QuickAssist?display_name=tag&sort=semver"></a>
  <a href="https://apps.repebble.com/9c105d58803741d5b9ec03af"><img alt="Pebble Appstore" src="https://img.shields.io/badge/Pebble%20Appstore-QuickAssist-ff4700"></a>
  <a href="https://www.home-assistant.io/voice_control/"><img alt="Home Assistant" src="https://img.shields.io/badge/Home%20Assistant-Assist-18bcf2?logo=homeassistant&logoColor=white"></a>
  <a href="LICENSE"><img alt="MIT license" src="https://img.shields.io/badge/license-MIT-97ca00"></a>
</p>

<p align="center">
  <a href="#-features">Features</a> ·
  <a href="#-quick-start">Quick start</a> ·
  <a href="#-controls">Controls</a> ·
  <a href="#-troubleshooting">Troubleshooting</a>
</p>

---

Launching the app immediately opens Pebble dictation while QuickAssist checks readiness with the phone companion. If transcription finishes first, the watch holds it until the companion confirms the configured assistant is ready. The recognized text goes to your selected Home Assistant Assist pipeline, and its reply appears on your watch. Press SELECT to continue the conversation.

<p align="center">
  <img src="docs/setup.png" alt="QuickAssist setup screen" width="200">
  <img src="docs/thinking.png" alt="QuickAssist processing a request" width="200">
  <img src="docs/reply.png" alt="QuickAssist showing a reply" width="200">
</p>

<p align="center"><sub>Emulator previews with sample text. See <a href="docs/ui-design.md">UI references</a>.</sub></p>

## 🎯 Features

| Feature                  | What it does                                                                             |
| ------------------------ | ---------------------------------------------------------------------------------------- |
| 🎙️ **Instant dictation** | Starts listening the moment you open the app, with no transcript confirmation step.      |
| 🏠 **Your assistant**    | Talks to the Home Assistant voice assistant you choose by name.                          |
| 💬 **Conversations**     | Scrollable text replies, and context carries over until you exit. Press SELECT to reply. |
| 📳 **Haptic feedback**   | One tap when a reply arrives, two when a request fails.                                  |
| ⚠️ **Clear errors**      | Connection, authentication, and assistant problems are shown on the watch.               |
| ⏱️ **Auto-close**        | Closes 30 seconds after a reply by default, adjustable in settings.                      |

## 📋 Requirements

- Pebble Time 2 (`emery`) and a compatible Pebble phone companion with working dictation.
- A phone network connection that can reach your Home Assistant instance.
- Home Assistant with Assist pipelines configured and a long-lived access token permitted to list and run those pipelines.
- Pebble SDK 4.33.1 or compatible, Node.js 22+, and a host C compiler (`cc`) to build and test locally.

Pebble's speech service performs transcription. The app sends the resulting text to HA's intent stage; it does not use HA's STT or TTS providers. Replies are text only. Select a pipeline whose language matches your Pebble dictation language.

## 🚀 Quick start

Install QuickAssist from the [Pebble Appstore](https://apps.repebble.com/9c105d58803741d5b9ec03af), download the latest PBW from [GitHub Releases](https://github.com/tylxr59/QuickAssist/releases), or build locally using the instructions below.

1. In your Home Assistant profile, create a **Long-Lived Access Token**. Treat it like a password.
2. Open QuickAssist's settings in the Pebble phone companion.
3. Enter your HA base URL (for example `https://home.example.com` or `http://192.168.1.20:8123`) and token, then tap **Connect**.
4. Wait for the connection result on the same page. Choose your voice assistant from the populated picker.
5. Optionally adjust **Close after inactivity (seconds)** from 5 to 300 seconds in 5-second increments (default: 30), then tap **Save**.
6. Launch QuickAssist on the watch and speak.

Connect keeps settings open and displays errors inline. Save closes the page and applies the selected assistant; the phone companion then validates the saved connection. Reopening existing settings automatically reconnects and preserves your assistant if it is still available. Changing URL or token requires reconnecting and selecting an assistant before saving. Cancelling the page leaves your saved configuration unchanged.

Use the base URL, without `/api/websocket`, query parameters, URL credentials, or a fragment. Path prefixes such as `https://example.com/homeassistant` are preserved. HTTP is supported for trusted local networks; HTTPS encrypts the connection. A local URL works only when the phone can reach that network (directly or through a VPN). Self-signed or untrusted certificates may be rejected by the companion.

### Upgrading from HomeAssist or QuickAssist 1.0/1.1

Version 1.2.0 introduces a new QuickAssist app identity and phone storage keys. When upgrading from HomeAssist or QuickAssist 1.0/1.1, enter your HA connection again. The new app can appear alongside the old one; remove the older watch app if needed. Old companion settings are not migrated or erased.

## 🎮 Controls

- **Launch:** starts dictation while checking companion readiness.
- **SELECT:** starts another dictation, preserving the current conversation.
- **UP/DOWN:** scrolls the reply or setup/error text.
- **BACK:** closes dictation using the system controls, or exits the app from the result screen.

After a successful reply, the app closes after the configured period without interaction. Button presses and scrolling restart the timer, including presses at the ends of a reply. Starting another dictation cancels it; the next successful reply starts a new timer. Setup, processing, and errors do not start an inactivity timer.

Exiting and reopening starts a fresh conversation. Saving settings also resets conversation context. Cancelling dictation submits nothing. Commands are sent immediately after recognition and may control devices without further confirmation. While processing, SELECT does not submit another command.

## 🩺 Troubleshooting

- **Setup instructions:** enter URL/token, tap Connect, select an assistant, and save.
- **Authentication failed:** replace an invalid or revoked token, tap Connect, select an assistant, and save.
- **No assistants / permissions:** configure a voice assistant in HA and check that the token's user can list and run pipelines.
- **Assistant removed:** open settings, let it reconnect or tap Refresh assistants, then select an available assistant and save.
- **Dictation unavailable / failed:** check Bluetooth, companion dictation support, speech-service availability, and your phone's connection. The emulator cannot verify real speech recognition.
- **Connection failed:** test the base URL from your phone; check VPN, reverse-proxy WebSocket support, and certificates.
- **Timeout / lost connection / reply delivery failed:** a command may already have executed. Check HA before speaking it again. QuickAssist never automatically repeats an uncertain command.
- **Long messages:** transcripts over 1,024 UTF-8 bytes are rejected. Replies are limited to approximately 8 KB and visibly marked when truncated.

## 🔐 Privacy and security

QuickAssist has no server of its own. The phone companion connects directly to the Home Assistant URL you configure.

Credentials and cached assistant metadata are stored in the phone's PebbleKitJS local storage and Clay settings storage. They are included in the bundled local settings page when you open it, never sent to the watch or a hosted configuration service, and never deliberately logged. Storage is not an encrypted credential vault. Clearing companion app data removes the configuration. Clear the URL/token and save to remove saved credentials from this app.

Commands run as soon as dictation finishes and may control devices without further confirmation.

## 👋 Development

```bash
npm ci
npm test
npm run build
npm run install:emery
```

The install script launches the emulator with VNC. The PBW is `build/QuickAssist.pbw`. To install on a real watch, enable the phone companion's developer connection and run:

```bash
pebble install --phone PHONE_IP build/QuickAssist.pbw
```

Tests use mocked HA WebSockets and companion transports; no real HA credentials are needed. See [DEVELOPMENT.md](DEVELOPMENT.md) for the protocol and hardware acceptance checklist.

### Project layout

- `src/c/`: native watch UI and dictation.
- `src/pkjs/`: phone networking, Clay settings, and reply transfer.
- `test/`: HA, settings, and protocol tests.
- `assets/`: source QuickAssist logo.
- `resources/images/`: Pebble launcher icon.
- `docs/`: emulator screenshots.
- `package.json`, `wscript`: Pebble build configuration.
- `pebble-appstore.md`: listing copy and requirements for the first Pebble Appstore submission.

## 📦 Releases

A matching `v<package version>` tag builds and attaches the PBW to a [GitHub Release](https://github.com/tylxr59/QuickAssist/releases). Release notes start with that version's `CHANGELOG.md` entry, followed by GitHub's generated notes (merged PRs, new contributors, and a compare link). Ordinary commits and pull requests do not trigger publishing.

To repair an existing release: **Actions → Release PBW → Run workflow**, enter an existing tag. The workflow checks out that exact tag. New code requires a new version and tag. No release or tag is created by local builds.

## 🤝 Acknowledgements

Inspired by [Home Assistant on your Wrist](https://github.com/caco3/pebble-home-assistant-on-your-wrist). QuickAssist uses a separate native C implementation focused on immediate dictation and named Assist pipelines. Implementation assisted by OpenAI Codex.

## 📃 License

QuickAssist is available under the [MIT License](LICENSE).
