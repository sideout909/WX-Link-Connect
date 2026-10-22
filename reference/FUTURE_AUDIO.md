# Archived audio concept for a future hardware version

The owner removed sound alerts from the current PanelLan device on October 21,
2026. There is no speaker indicator, sound setting or sound driver in the
current firmware. Audio may be reconsidered for a larger-display hardware
version; this archived design is not an implementation task for SC05_X.

## Earlier behavioral study

Add a speaker and suitable amplifier, or use a verified existing board audio circuit. The proposed ESP32-S3 baseline can use an I²S amplifier such as **MAX98357A** plus a matched small speaker; reserve bit-clock, word-select, and data GPIO, and optionally an amplifier-enable pin. Verify module input levels, supply, speaker impedance/rating, and the whole pin budget before wiring. A speaker must not be driven directly by an ESP32 GPIO. A driven piezo buzzer is an alternative if simple beeps and fewer audio features are preferred. See [PARTS.md](../PARTS.md).

Proposed behavior after the user enables and tests audio:

- A newly received actual watch plays one short, original two-note chime.
- A newly received actual warning plays a distinct original three-pulse tone; immediate severe/extreme warnings may repeat every 60 seconds until silenced, cancelled, or no longer confirmed fresh.
- Advisories/statements remain silent by default; an optional setting enables their chime. Unknown classifications remain visible and do not inherit a guessed tone.
- The alert detail page opens when sound starts and provides a large **Silence this alert** button. Stop playback immediately; retain the alert, count, and background. A separate **Mute all** setting persists; a trailing speaker/mute icon after APRS/CWOP shows audio state. Unconfigured audio uses the mute icon.
- Provide volume, enable/disable, a deliberately invoked **Test sound** button, and an optional quiet-hours setting. Quiet hours suppress sound unless an explicit warning override is enabled; never assume they override mute. Make the selected behavior visible in settings.
- Deduplicate by alert identity/update lineage. Routine polling, reconnects, and unchanged updates do not replay tones. A watch-to-warning escalation or materially increased hazard may re-alert, including after the previous version was silenced; document those triggers and persist bounded acknowledgement state to avoid restart loops. A newly discovered fresh alert after reboot can still announce once.
- Stop repeats on stale/unavailable NWS data, message expiry, cancellation, mute, or a location change. Cached alerts and invalid-time startup cannot trigger fresh-alert tones. Resume only after fresh validated data, without replaying an unchanged acknowledged event.

Generate tones locally with bounded nonblocking playback; no cloud speech or SD audio files are required. Use original chimes, not a recording or imitation of the EAS/NOAA warning attention signals. Volume and audibility require an enclosure bench test. Audio remains optional and must not stall RF capture, history updates, rendering, UART output, or networking.


These requirements would need a new verified pin, power and enclosure budget
if a future version revives them. Original links below are relative to the
project root: see [parts](../PARTS.md) and [Home](../HOME_SCREEN.md).
