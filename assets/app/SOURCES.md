# linscp's application icon

`linscp.svg` is linscp's own: an original tile (linscp's, under linscp's
license) around the `folder-sync` glyph of Lucide (npm package
`lucide-static` 1.49.0, ISC, `LICENSE.lucide` here), its strokes recoloured.
It does not derive from WinSCP's application icon, whose images may not be
reused in other software (WinSCP's license.txt, part B).

`port/tools/make_icon.py` renders it to `port/qt/icons/linscp.ico` (the
executable, the installer and the windows) and to `linscp-<size>.png` here
(the Linux packages' hicolor icons).
