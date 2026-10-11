#!/usr/bin/env bash
# Regenerate plan.html from the markdown sources. Requires pandoc.
set -euo pipefail
cd "$(dirname "$0")"
{ for f in README PLAN TASKS TRACKER ESTIMATES; do
    sed -e 's|](PLAN\.md[^)]*)|](#plan)|g' -e 's|](TASKS\.md[^)]*)|](#tasks)|g' \
        -e 's|](TRACKER\.md[^)]*)|](#tracker)|g' -e 's|](ESTIMATES\.md[^)]*)|](#estimates)|g' \
        -e 's|](README\.md)|](#linscp--qt-port-of-winscp)|g' -e 's|](doc/qt/)|](#)|g' "$f.md"
    printf '\n\n'
  done
} | pandoc -f markdown-tex_math_dollars -o plan.html --standalone --toc --toc-depth=3 \
      --metadata title="linscp — Qt port of WinSCP" \
      --metadata subtitle="Plan, tasks, tracker and estimates" \
      --include-in-header=<(printf '<style>\n%s\n</style>\n' "$(cat style.css)") -
echo "wrote $(pwd)/plan.html"
