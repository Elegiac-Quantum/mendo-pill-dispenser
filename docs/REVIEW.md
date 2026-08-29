# Release-candidate review

Status: local review candidate, not a safety certification or clinical validation.

## Verified during this review

- Existing host C test suite: 14/14 pass after the final motion-safety change.
- Added volume regression executed real production getter/setter with an NVS
  test backend enforcing the 15-character key limit. Before the fix, lowering
  volume to 35 reverted to 100 on simulated reboot. After the fix, 35 and mute
  persist. The setter also records completion of the volume migration.
- Development ESP-IDF firmware rebuilt successfully after this change.
- Added an alignment contract regression. Overshoot outside the same 1.5-degree
  settling tolerance is now reported as `overshot_target`, rather than success.
- Thirteen selected STL files passed closed, positive-volume mesh checks. The
  generated 3MF was reopened and contains 13 separate objects with triangles.
- No physical dispensing or firmware flashing was performed in this review.

## Findings that limit use

1. **Uncollected doses can accumulate.** The next scheduled occurrence proceeds
   even when the prior reminder was not acknowledged. There is no cup-contents
   detector. Never interpret button acknowledgement as verified ingestion.
2. **The overshoot software fault is corrected but needs physical retesting.**
   The controller now fails closed outside 1.5 degrees and never reverses. This
   can turn a formerly reported success into a visible dispense fault, which is
   intentional. The changed build was not flashed or physically exercised here.
3. **Duplicate suppression has limited history.** NVS `dose_state/latest` stores
   only one occurrence; reminder `last_N` keys use list indices. Clock rollback,
   reordering/deleting schedules or interrupted multi-schedule processing need
   persistent per-occurrence coverage. Current tests do not prove those cases.
4. **Scheduling and playback share a task.** Removing the endless alarm loop
   prevents that known lockup, but synchronous playback can still delay a later
   schedule. The exact-minute matcher needs a catch-up policy before long uploads
   or many simultaneous schedules can be considered reliable.
5. **Local security is limited.** Caregiver PIN is optional, local HTTP is not
   encrypted, and the default hotspot password is derived from the MAC address.
   Use a unique hotspot password and PIN on a trusted network; do not forward
   the device's HTTP port to the Internet. Remote control needs a separately
   secured HTTPS service; no live service credentials are published here.
6. **Audio still requires in-room testing.** The 12x software gain saturates
   peaks. A successful build cannot establish audibility, intelligibility,
   absence of distortion or suitability for a particular person's hearing.
7. **AI is not a verified feature of this candidate.** The stability snapshot
   disables the wake task after prior failures. The online service is separate.

These findings must not be replaced with a blanket statement that all functions
are working. Use placebo contents for engineering tests. A caregiver must review
the tray, cup, time, loading order and power-recovery state.

## Excluded from public files

SDK caches, binaries/flashed images, NVS data, device account bindings, Wi-Fi
settings, fleet credentials, logs, patient schedules, family recordings, APK
signing material, vendor PDFs, presentation files and obsolete prototype trees.
