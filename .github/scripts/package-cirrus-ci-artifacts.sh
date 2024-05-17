#!/bin/bash

sha=$1
if [ -z "$sha" ]; then
  echo "Usage: $0 <sha>" > /dev/stderr
  exit 1
fi

set -e

build_id=""
while [ -z "$build_id" ]; do
  response=$(curl https://api.cirrus-ci.com/graphql --silent --data \
        '{
          "query": "query BuildBySHAQuery($owner: String!, $name: String!, $SHA: String) { searchBuilds(repositoryOwner: $owner, repositoryName: $name, SHA: $SHA) { id, status } }",
          "variables": {
            "owner": "frida",
            "name": "frida",
            "SHA": "'$sha'"
          }
        }'
      );
  pending=0
  errors=0
  for build in $(echo $response | jq -r '.data.searchBuilds[] | { id, status } | join("|")'); do
    id=$(echo $build | cut -d'|' -f1)
    status=$(echo $build | cut -d'|' -f2)
    case $status in
      COMPLETED)
        build_id=$id
        ;;
      FAILED|ABORTED|ERRORED)
        errors=$((errors+1))
        ;;
      *)
        pending=$((pending+1))
        ;;
    esac
  done
  if [ -z "$build_id" ]; then
    if [ $errors -gt 0 ] && [ $pending -eq 0 ]; then
      echo "Cirrus CI build failed :(" > /dev/stderr
      exit 1
    else
      echo "Cirrus CI build not yet completed. Checking back in 5 minutes."
      sleep 300
    fi
  fi
done

output_dir=$PWD/build/release-assets
mkdir -p "$output_dir"

export XZ_OPT="-T 0"

for task in freebsd_x86_64; do
  host=$(echo $task | sed -e 's,_,-,')

  enter_artifact ()
  {
    local name=$1
    rm -rf build/pkg
    mkdir -p build/pkg
    pushd build/pkg > /dev/null
    local url=https://api.cirrus-ci.com/v1/artifact/build/$build_id/${name}_${task}.zip
    echo "Processing $url"
    curl $url -sLo artifact.zip
    unzip -qq artifact.zip
    rm artifact.zip
  }

  leave_artifact ()
  {
    popd > /dev/null
    rm -rf build/pkg
  }

  for component in gum gumjs core; do
    enter_artifact frida_${component}_devkit

    mv dist/lib/frida/devkits/$component/* .
    rm -rf build
    tar -cJf "$output_dir/frida-$component-devkit-$host.tar.xz" .

    leave_artifact
  done

  for component in server portal inject gadget; do
    enter_artifact frida_${component}

    readarray -t files < <(find . -type f)
    n=${#files[@]}
    if [ $n -ne 1 ]; then
      echo "The frida-$component artifact should only contain a single file" > /dev/stderr
      exit 1
    fi
    file=${files[0]}
    name=$(basename $file)
    if echo $name | grep -q '\.'; then
      extension=".$(echo $name | cut -d'.' -f2-).xz"
    else
      extension=".xz"
    fi
    xz -c -T 0 "$file" > "$output_dir/frida-$component-$host$extension"

    leave_artifact
  done

  enter_artifact frida_python
  mv dist/wheels/* "$output_dir"
  leave_artifact

  enter_artifact frida_node
  mv dist/prebuilds/* "$output_dir"
  leave_artifact
done
