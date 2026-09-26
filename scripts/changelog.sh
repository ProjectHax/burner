#!/bin/bash
# changelog.sh - Generate Markdown release notes from git history
# Usage: ./scripts/changelog.sh [ref]
#
# Lists the commits between the previous v* tag and <ref> (default: HEAD).
# Commits using Conventional Commit prefixes (feat:, fix:, perf:) are grouped
# into their own sections; everything else is listed under "Changes".
# Merge commits are skipped.
#
# Examples:
#   ./scripts/changelog.sh          # Preview notes for the next release
#   ./scripts/changelog.sh v1.1.0   # Notes for an existing tag

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$(dirname "$SCRIPT_DIR")"

TARGET="${1:-HEAD}"
if ! git rev-parse --verify --quiet "$TARGET^{commit}" > /dev/null; then
    echo "Error: unknown ref '$TARGET'" >&2
    exit 1
fi

# Link commits to GitHub: use the Actions environment when available,
# otherwise derive the URL from the origin remote
if [ -n "${GITHUB_REPOSITORY:-}" ]; then
    REPO_URL="${GITHUB_SERVER_URL:-https://github.com}/$GITHUB_REPOSITORY"
else
    REPO_URL=$(git remote get-url origin 2>/dev/null | sed -E \
        -e 's#^git@([^:]+):#https://\1/#' \
        -e 's#^ssh://git@([^/]+)/#https://\1/#' \
        -e 's#\.git$##')
fi

# Find the previous release tag. A stable release skips pre-release tags
# (v1.1.0-rc1) so its notes cover everything since the last stable release.
DESCRIBE_ARGS=(--tags --abbrev=0 --match 'v[0-9]*')
if [[ "$TARGET" != *-* ]]; then
    DESCRIBE_ARGS+=(--exclude 'v*-*')
fi
PREVIOUS=$(git describe "${DESCRIBE_ARGS[@]}" "$TARGET^" 2> /dev/null || true)

RANGE="$TARGET"
if [ -n "$PREVIOUS" ]; then
    RANGE="$PREVIOUS..$TARGET"
fi

features=()
fixes=()
performance=()
other=()

while IFS=$'\t' read -r sha subject; do
    link="([${sha:0:7}]($REPO_URL/commit/$sha))"

    # type(scope)!: description
    if [[ "$subject" =~ ^([a-zA-Z]+)(\(([^\)]+)\))?!?:[[:space:]]+(.+)$ ]]; then
        type="${BASH_REMATCH[1],,}"
        scope="${BASH_REMATCH[3]}"
        entry="- ${scope:+**$scope:** }${BASH_REMATCH[4]} $link"
        case "$type" in
            feat) features+=("$entry") ;;
            fix)  fixes+=("$entry") ;;
            perf) performance+=("$entry") ;;
            *)    other+=("$entry") ;;
        esac
    else
        other+=("- $subject $link")
    fi
done < <(git log --no-merges --format='%H%x09%s' "$RANGE")

print_section() {
    local title="$1"
    shift
    [ $# -eq 0 ] && return
    echo "### $title"
    echo ""
    printf '%s\n' "$@"
    echo ""
}

echo "## What's Changed"
echo ""

if [ $(( ${#features[@]} + ${#fixes[@]} + ${#performance[@]} + ${#other[@]} )) -eq 0 ]; then
    echo "No changes."
    echo ""
fi

print_section "Features" "${features[@]}"
print_section "Bug Fixes" "${fixes[@]}"
print_section "Performance" "${performance[@]}"
if [ $(( ${#features[@]} + ${#fixes[@]} + ${#performance[@]} )) -eq 0 ]; then
    print_section "Changes" "${other[@]}"
else
    print_section "Other Changes" "${other[@]}"
fi

# Name the target by tag when it is one, otherwise by commit
TARGET_NAME="$TARGET"
if ! git show-ref --verify --quiet "refs/tags/$TARGET"; then
    TARGET_NAME=$(git rev-parse "$TARGET")
fi

if [ -n "$PREVIOUS" ]; then
    echo "**Full Changelog**: $REPO_URL/compare/$PREVIOUS...$TARGET_NAME"
else
    echo "**Full Changelog**: $REPO_URL/commits/$TARGET_NAME"
fi
