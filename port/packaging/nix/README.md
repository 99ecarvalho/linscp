# nixpkgs package

`package.nix` is the package as it would go into nixpkgs at
`pkgs/by-name/li/linscp/package.nix`. It builds the release tarball with
clang, against the system's Qt 6, OpenSSL and expat.

## Building it locally

From a checkout of nixpkgs, with the file copied to
`pkgs/by-name/li/linscp/package.nix`:

```sh
nix-build -A linscp
QT_QPA_PLATFORM=offscreen timeout 10 result/bin/linscp
result/bin/linscp-cli /info
```

Or, without a nixpkgs checkout, against a pinned nixpkgs:

```nix
# default.nix
let
  pkgs = import (builtins.fetchTarball "https://github.com/NixOS/nixpkgs/archive/<rev>.tar.gz") { };
in
pkgs.callPackage ./package.nix { }
```

To test a tarball that is not published yet (`build/dist/`, from the release
tooling), add it to the store first; `fetchurl` then finds it by its hash
instead of downloading it:

```sh
nix-store --add-fixed sha256 build/dist/linscp-X.Y.Z.tar.xz
```

The `hash` must be the published tarball's (`nix-prefetch-url` or the
release's `SHA256SUMS`). Format the file with `nixfmt` (nixfmt-rfc-style)
before submitting.

## The nixpkgs pull request

- Two commits: `maintainers: add <handle>` (an entry in
  `maintainers/maintainer-list.nix`, alphabetical), then `linscp: init at
  X.Y.Z` with the package, and the handle in `meta.maintainers`.
- The pull request is titled `linscp: init at X.Y.Z`, with the template's
  checklist filled in and a `nixpkgs-review` report
  (`nixpkgs-review pr <number> --post-result`, or `nixpkgs-review rev HEAD`
  before opening it).
- nixpkgs prefers packages proposed by their users rather than their
  authors, so the pull request waits until linscp has real users on NixOS.
