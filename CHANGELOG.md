# Changelog

## 1.4.0

- Added haptic feedback when a request finishes: one short tap when a reply arrives, and two taps when a request fails or times out.

## 1.3.0

- Added automatic closing after a successful Home Assistant reply, with a default inactivity timeout of 30 seconds.
- Added a phone setting to customize the timeout from 5 to 300 seconds in 5-second increments.
- Kept replies open while you interact: button presses and scrolling restart the timer, and follow-up dictation cancels it until the next successful reply.

## 1.2.0

- Gave QuickAssist its own app identity and phone settings storage, separate from HomeAssist and QuickAssist 1.0/1.1. Enter your Home Assistant connection again after installing; the new app may appear alongside the old one. Previous companion settings are not migrated or erased.

## 1.1.0

- Rebranded the app as QuickAssist and added a house-and-voice launcher icon with a transparent background and black house outline.
- Started dictation while connecting to the phone, so you can speak sooner. Recognized speech waits until the companion is ready and is discarded if the connection times out or settings change.
- Added listing copy and setup requirements for the first Pebble Appstore submission.

## 1.0.0

- Added launch-to-dictation access to Home Assistant Assist on Pebble Time 2, with immediate command submission and scrollable text replies.
- Added phone settings for token authentication and selection of a configured voice assistant.
- Added follow-up conversation context until app exit and visible recovery instructions for connection and assistant failures.
- Fixed assistant discovery getting stuck on “Connecting” in browser-based phone companions and repaired reply-transfer timers.
- Improved onboarding with in-page connection checks and assistant selection, without closing and reopening settings.
- Refreshed the watch UI with wider replies, vertically centered short responses, and a compact voice area, preserving Pebble’s gray palette, Gothic fonts, and native clock bar.
