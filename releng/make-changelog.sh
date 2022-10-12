#!/bin/sh

submodules="frida-gum frida-core frida-python frida-node frida-qml frida-clr"

from=$1
to=$2
if [ -z "$from" -o -z "$to" ]; then
  echo "usage: $0 <from> <to>"
  exit 1
fi
range=${from}..${to}

set -e

intdir="$(mktemp -d /tmp/make-changelog.XXXXXX)"
scratch=$intdir/scratch
summary=$intdir/summary
log=$intdir/log
authors=$intdir/authors

cleanup ()
{
  rm -rf "$intdir"
}
trap cleanup EXIT

summarize_repo_changes ()
{
  local repo=$1
  local from=$2
  local to=$3
  local type=$4

  echo
  echo "## $repo"
  echo "   > https://github.com/frida/$repo/compare/$from...$to"
  git \
      --no-pager \
      log \
      "--format=%ae<%an<%s" \
      --no-decorate \
      --color=never \
      ${from}..${to} \
      | sort -t "<" -k 3 \
      | grep -v "test: " \
      | grep -v "tests: " \
      > "$summary" || true
  if [ "$repo" == "frida" ]; then
    grep -Ev "(submodules: Bump outdated|deps: Bump)" "$summary" > "${summary}-filtered" || true
    mv "${summary}-filtered" "$summary"
  fi

  export IFS="<"
  cat "$summary" | while read author_email author_name title; do
    printf -- '- %s.' $title
    if [ $type == internal ] && echo $author_email | grep -Evq "dependabot|oleavr"; then
      printf " Thanks [$author_name][]!"
    fi
    echo
  done
  unset IFS
}

append_log ()
{
  git --no-pager log --decorate=short --color=never $1 >> "$log"
}

append_log $range
echo "Released $(head -3 "$log" | grep "^Date: " | cut -c 9-)"
summarize_repo_changes frida $from $to internal

for module in frida-gum frida-core frida-python frida-node frida-qml frida-clr; do
  git --no-pager diff $range $module > "$scratch"
  if grep -q "Subproject commit" "$scratch"; then
    mod_from=$(grep -E "^-Subproject" "$scratch" | cut -f3 -d" ")
    mod_to=$(grep -E "^\+Subproject" "$scratch" | cut -f3 -d" ")
    mod_range=${mod_from}..${mod_to}
    pushd $module > /dev/null
    append_log $mod_range
    summarize_repo_changes $module $mod_from $mod_to internal
    if [ $module == frida-gum ]; then
      git diff $mod_range -- bindings/gumjs/generate-runtime.py > "$intdir/bridge-changes"
      for bridge in $(grep "^-" "$intdir/bridge-changes" | grep -- '-bridge": "' | cut -d '"' -f 2); do
        bridge_from=$(grep "^-" "$intdir/bridge-changes" | grep '"'$bridge'": "' | cut -d '"' -f 4)
        bridge_to=$(grep "^+" "$intdir/bridge-changes" | grep '"'$bridge'": "' | cut -d '"' -f 4)
        pushd ~/src/$bridge > /dev/null
        summarize_repo_changes $bridge v$bridge_from v$bridge_to internal
        popd > /dev/null
      done
    fi
    popd > /dev/null
  fi
done

git --no-pager diff $range releng/deps.mk > "$scratch"
bumped_deps=$(grep "_version = " "$scratch" \
    | grep "^-" \
    | cut -c 2- \
    | awk '{ print $1 }' \
    | grep -Ev "frida_(deps|bootstrap)_version" \
    | sed -e 's,_version$,,')
for id in $bumped_deps; do
  case $id in
    bison|depot_tools|flex|gn|v8_api)
      continue
      ;;
  esac
  repo=$(echo $id | tr -s "_" "-")
  dep_from=$(grep "${id}_version = " "$scratch" | grep "^-" | awk '{ print $3 }')
  dep_to=$(grep "${id}_version = " "$scratch" | grep "^+" | awk '{ print $3 }')
  if [ ! -d deps/$repo ]; then
    make -f Makefile.sdk.mk deps/.${repo}-stamp
  fi
  pushd deps/$repo > /dev/null
  summarize_repo_changes $repo $dep_from $dep_to external
  popd > /dev/null
done

echo
echo "# Summary"

releases=$(grep "(tag: " "$log" | wc -l)
grep "^Author: " "$log" | cut -d : -f 2- | cut -c 2- | grep -v dependabot | sort -u > "$authors"
echo
echo "- Releases: $releases"
echo "- Commits: $(grep "^commit " "$log" | wc -l)"
echo "- Contributors: $(wc -l "$authors" | awk '{ print $1 }')"
echo
cat "$authors" | awk '{ print "> " $0 }'
