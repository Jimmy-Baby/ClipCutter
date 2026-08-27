# Manual regression checks

1. Start with no queue. Confirm clip actions, rename, processing, skipping, and **Use Selected** are disabled; invoke every toolbar shortcut and confirm no crash.
2. Import supported videos through **Open File(s)** and **Open Folder**. Repeat the imports and confirm canonical duplicates are skipped with a non-blocking summary while existing rows remain.
3. Drop supported videos, unsupported files, a local folder, and a web URL. Confirm local videos append, unsupported/duplicate/remote items are summarized, and drag-over status is visible.
4. Drop a folder with recursive import off, then on. Confirm nested videos import only when recursion is enabled and probing remains asynchronous.
5. Import videos from multiple directories. Select **Beside each source**, retain `ClipCutterOutput`, and confirm each preview points beside its own source. Confirm no directory is created until export preflight succeeds.
6. Switch to **Fixed directory**, browse to a writable directory, and confirm every preview changes. Try an empty path, a regular file, and an unwritable path; confirm export cannot queue.
7. Create output collisions, switch destination/profile/name, and confirm collision status/preflight refreshes. Exercise Ask, Auto Rename, Skip, and Overwrite.
8. Restart after moving/resizing the window, splitter, volume, profile, destination, metadata, recursion, prefixes, and template. Confirm all settings restore. Invoke **Reset Settings** and confirm defaults return after restart.
9. Resize the window to its minimum and on a high-DPI display. Confirm preview and queue remain resizable and important controls remain reachable.
10. Apply every naming token, multiple index widths, malformed/unknown tokens, invalid filename characters, and a duplicate-rendering template. Confirm live selected-row and batch previews agree with export paths and extensions remain profile-controlled.
11. Exercise every **Batch** action with multiple selected rows and the whole queue. Confirm the clear-queue confirmation appears once and operations retain stable row IDs/order.
12. Filter by source, output, prefix, each export status, keep, and skip. Edit a filtered row and confirm the underlying stable segment changes. Confirm active-filter text shows the visible and total counts.
13. Create a session, import and edit rows, Save, close, reopen, and compare order, IDs, paths, trim ranges, names/templates, prefixes, skip flags, profile, and destination workflow.
14. Keep session sources below the session directory, move the whole directory tree, and reopen. Confirm relative sources resolve at the new location.
15. Delete a session source and reopen. Confirm the missing row remains, other rows load, and **Relink Missing Sources** restores it and starts asynchronous probing.
16. Modify a saved and an unnamed session, wait for debounce, then terminate without a clean close. Restart and recover. Confirm recovery is offered only when newer and never overwrites the explicit session file.
17. Start export in each destination mode after accepting the batch-path preview. Confirm correct final paths, responsive UI, state/progress updates, cancellation cleanup, retries, and inspectable diagnostics.
