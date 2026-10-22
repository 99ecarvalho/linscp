# Scoop manifest

`linscp.json` installs the portable package (`linscp-X.Y.Z-portable.zip`):
`linscp-cli` on the path, a Start menu shortcut to `linscp`, and `linscp.ini`
— where the portable package keeps its configuration, beside `linscp.exe` —
persisted across updates in `~\scoop\persist\linscp\`.

`checkver` follows the latest GitHub release; `autoupdate` takes the hash from
the release's `SHA256SUMS`.

## Testing a change

```powershell
scoop install .\linscp.json        # from this folder
linscp-cli /help
scoop uninstall linscp             # add -p to drop the persisted linscp.ini too
```

## Publishing it in a bucket

The Extras bucket takes applications with about 100 stars or 50 forks; until
then linscp has a bucket of its own.

1. Create a public repository `99ecarvalho/scoop-bucket` from the template
   <https://github.com/ScoopInstaller/BucketTemplate> ("Use this template").
2. Delete the template's sample manifest, copy `linscp.json` to
   `bucket/linscp.json`, and push.
3. The template's Excavator workflow (`.github/workflows/excavator.yml`) runs
   `checkver` and `autoupdate` on a schedule and commits new versions. It needs
   the repository's Actions to have write permission: Settings → Actions →
   General → Workflow permissions → "Read and write permissions".
4. Users then install with:

   ```powershell
   scoop bucket add linscp https://github.com/99ecarvalho/scoop-bucket
   scoop install linscp/linscp
   ```

5. Keep this folder's `linscp.json` as the reference copy: a change to the
   manifest's structure goes here and to the bucket.

Once linscp meets the Extras criteria, open a pull request adding
`bucket/linscp.json` to <https://github.com/ScoopInstaller/Extras> (its
contribution guide asks for the same `checkver`/`autoupdate`), and point users
to `scoop install extras/linscp`.
