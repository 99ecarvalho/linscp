#!/usr/bin/env python3
"""
Prepare a release: put the new version everywhere the tagged tree carries it.

  prepare_release.py X.Y.Z [--date YYYY-MM-DD] [--check]

Run it on main, with NEWS.md's "## Unreleased" section written (the release
notes: an opening paragraph, then the changes). It:

  CMakeLists.txt         project(linscp VERSION X.Y.Z)
  README.md              "> **Version X.Y.Z.**"
  NEWS.md                "## Unreleased" becomes "## X.Y.Z — DATE" (the
                         release workflow publishes that section)
  metainfo               a <release> entry, its text NEWS.md's opening
                         paragraph (port/packaging/linux)
  RPM spec               Version, and a %changelog entry
  debian/changelog       an X.Y.Z-1 entry (UNRELEASED until a Debian or PPA
                         upload names its distribution)
  doc/man/*.1            regenerated (gen_manpages.py)
  port/packaging/obs     regenerated (gen_obs_debian.py)

Each step is skipped when already done, so it can be run again. --check
changes nothing and fails if any of them is not at X.Y.Z. It then prints
what is left: the commit, the signed tag, and the channels updated after
publishing (they need the published files' hashes).
"""
import argparse, datetime, email.utils, os, re, subprocess, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
MAINTAINER = 'Eduardo Correia <ecorreia@apliant.com.br>'
METAINFO = os.path.join('port', 'packaging', 'linux', 'org.linscp.linscp.metainfo.xml')
SPEC = os.path.join('port', 'packaging', 'rpm', 'linscp.spec')
DEBIAN_CHANGELOG = os.path.join('port', 'packaging', 'debian', 'changelog')


def path(name):
    return os.path.join(ROOT, name)


def read(name):
    with open(path(name), encoding='utf-8') as f:
        return f.read()


def write(name, text):
    with open(path(name), 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


def plain(markdown):
    """NEWS.md's Markdown as plain text for the metainfo: no emphasis, code or
    link markup, XML-escaped."""
    text = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', markdown)
    text = re.sub(r'[*`]', '', text)
    text = ' '.join(text.split())
    return text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


class Release:
    def __init__(self, version, date, check):
        self.version, self.date, self.check = version, date, check
        self.pending = []

    def change(self, name, old, new, what):
        if old == new:
            print(f'  ✔ {what}')
            return
        if self.check:
            self.pending.append(what)
            print(f'  ✘ {what}')
            return
        write(name, new)
        print(f'  ✎ {what}')

    def cmake(self):
        text = read('CMakeLists.txt')
        new = re.sub(r'^(project\(linscp VERSION )[0-9.]+', rf'\g<1>{self.version}', text, count=1, flags=re.M)
        self.change('CMakeLists.txt', text, new, f'CMakeLists.txt: VERSION {self.version}')

    def readme(self):
        text = read('README.md')
        new = re.sub(r'^> \*\*Version [0-9.]+\.\*\*', f'> **Version {self.version}.**', text, count=1, flags=re.M)
        self.change('README.md', text, new, f'README.md: Version {self.version}')

    def news(self):
        """Returns the release's opening paragraph."""
        text = read('NEWS.md')
        heading = re.search(rf'^## {re.escape(self.version)} [—-] (\d{{4}}-\d{{2}}-\d{{2}})\s*$', text, re.M)
        if heading:
            self.date = heading.group(1)
            print(f'  ✔ NEWS.md: ## {self.version} — {self.date}')
            start = heading.end()
        else:
            unreleased = re.search(r'^## Unreleased\s*$', text, re.M)
            if not unreleased:
                sys.exit('NEWS.md has neither "## Unreleased" nor a section for ' + self.version)
            body = text[unreleased.end():].split('\n## ', 1)[0]
            if not body.strip():
                sys.exit('NEWS.md: "## Unreleased" is empty; write the release notes first')
            new = text[:unreleased.start()] + f'## {self.version} — {self.date}' + text[unreleased.end():]
            self.change('NEWS.md', text, new, f'NEWS.md: ## {self.version} — {self.date}')
            start = unreleased.end()
        body = text[start:].split('\n## ', 1)[0].strip()
        first = body.split('\n\n', 1)[0]
        if first.lstrip().startswith(('-', '*')):
            sys.exit('NEWS.md: the release notes need an opening paragraph before the list '
                     '(it becomes the metainfo release description)')
        return plain(first)

    def metainfo(self, summary):
        text = read(METAINFO)
        if f'<release version="{self.version}"' in text:
            print(f'  ✔ metainfo: release {self.version}')
            return
        entry = (f'    <release version="{self.version}" date="{self.date}">\n'
                 f'      <description>\n        <p>{summary}</p>\n      </description>\n    </release>\n')
        new = text.replace('  <releases>\n', '  <releases>\n' + entry, 1)
        self.change(METAINFO, text, new, f'metainfo: release {self.version}')

    def spec(self):
        text = read(SPEC)
        new = re.sub(r'^(Version:\s+)\S+', rf'\g<1>{self.version}', text, count=1, flags=re.M)
        new = re.sub(r'^(Release:\s+)\d+', r'\g<1>1', new, count=1, flags=re.M)
        if f' - {self.version}-1\n' not in new:
            day = datetime.date.fromisoformat(self.date).strftime('%a %b %d %Y')
            new = new.replace('%changelog\n', f'%changelog\n* {day} {MAINTAINER} - {self.version}-1\n'
                                               f'- Update to {self.version}\n\n', 1)
        self.change(SPEC, text, new, f'RPM spec: {self.version}-1')

    def debian(self):
        text = read(DEBIAN_CHANGELOG)
        if text.startswith(f'linscp ({self.version}-'):
            print(f'  ✔ debian/changelog: {self.version}-1')
            return
        when = email.utils.format_datetime(datetime.datetime.combine(
            datetime.date.fromisoformat(self.date), datetime.time(12, 0),
            tzinfo=datetime.timezone(datetime.timedelta(hours=-3))))
        entry = (f'linscp ({self.version}-1) UNRELEASED; urgency=medium\n\n'
                 f'  * New upstream release.\n\n -- {MAINTAINER}  {when}\n\n')
        self.change(DEBIAN_CHANGELOG, text, entry + text, f'debian/changelog: {self.version}-1')

    def generated(self, script, what):
        tool = path(os.path.join('port', 'tools', script))
        current = subprocess.run([sys.executable, tool, '--check'], cwd=ROOT, capture_output=True).returncode == 0
        if current:
            print(f'  ✔ {what}')
        elif self.check:
            self.pending.append(what)
            print(f'  ✘ {what}')
        else:
            subprocess.run([sys.executable, tool], cwd=ROOT, check=True, capture_output=True)
            print(f'  ✎ {what}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0].strip())
    ap.add_argument('version', help='X.Y.Z')
    ap.add_argument('--date', default=datetime.date.today().isoformat(), help='the release date (default: today)')
    ap.add_argument('--check', action='store_true', help='change nothing; fail if anything is not at VERSION')
    a = ap.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+', a.version):
        sys.exit('the version is X.Y.Z')
    r = Release(a.version, a.date, a.check)
    print(f'linscp {a.version}')
    r.cmake()
    r.readme()
    summary = r.news()
    r.metainfo(summary)
    r.spec()
    r.debian()
    # Both read the version from CMakeLists.txt, so they follow the steps above.
    r.generated('gen_manpages.py', 'doc/man: manual pages')
    r.generated('gen_obs_debian.py', 'port/packaging/obs: the OBS Debian recipe')
    if r.pending:
        print(f'\nnot at {a.version}: ' + ', '.join(r.pending))
        return 1
    if a.check:
        return 0
    print(f'''
Next:
  git commit -a -m "chore: release {a.version}"   # after reviewing git diff
  git push origin main                # wait for CI to pass on main
  git tag -s v{a.version} -m "linscp {a.version}"
  git push origin v{a.version}        # the release workflow drafts the release
Then review and publish the draft on GitHub, and update the channels that
need the published files' hashes (CLAUDE.md, "Releases").''')
    return 0


if __name__ == '__main__':
    sys.exit(main())
