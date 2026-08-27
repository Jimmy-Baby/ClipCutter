# Manual regression checks

1. Start ClipCutter with no queue. Confirm clip-dependent toolbar actions, rename, processing, skipping, and **Use Selected** are disabled.
2. Invoke every toolbar shortcut before loading a clip. Confirm no crash or state change.
3. Open an empty folder, then a folder containing only unsupported files. Confirm one useful message appears and any existing queue remains unchanged.
4. Place a regular file named `ClipCutterOutput` beside supported videos and open that folder. Confirm one error includes the full output path and the existing queue remains unchanged.
5. Add a keyword with surrounding spaces. Confirm it is trimmed. Try blank and differently-cased duplicate keywords; confirm both are rejected.
6. Select keyword rows without pressing **Use Selected**. Confirm the current clip name does not change.
7. Use, change, and toggle off a keyword on several clips. Confirm only the current clip has **Yes** and each displayed output name matches its assigned keyword.
8. Remove a keyword used by multiple clips. Confirm every affected clip returns to its own unprefixed output name.
9. Select the first and last clips and invoke previous/next shortcuts. Confirm navigation remains in bounds.
10. Exercise play/pause, stop, skip, set-start, set-end, rename, and keyword controls with a loaded clip.
