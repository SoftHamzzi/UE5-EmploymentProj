"""
UE5 API Documentation Fetcher
Usage: python fetch-ue-api.py <query>
Example: python fetch-ue-api.py ACharacter
         python fetch-ue-api.py "UAbilitySystemComponent GetActiveAbilities"
         python fetch-ue-api.py "GameplayEffect replication"

Searches DuckDuckGo for UE5 API docs and extracts concise results.
Outputs structured text to stdout for Claude context injection.
"""

import sys
import re
import requests
from bs4 import BeautifulSoup

SEARCH_URL = "https://html.duckduckgo.com/html/"
MAX_RESULTS = 5
HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
}


def search_ue_docs(query: str) -> list[dict]:
    """Search DuckDuckGo for UE5 API documentation."""
    search_query = f"site:dev.epicgames.com unreal engine API {query}"

    try:
        resp = requests.post(
            SEARCH_URL,
            data={"q": search_query, "b": ""},
            headers=HEADERS,
            timeout=15,
        )
        resp.raise_for_status()
    except requests.RequestException as e:
        print(f"[ERROR] Search failed: {e}", file=sys.stderr)
        return []

    soup = BeautifulSoup(resp.text, "html.parser")
    results = []

    for item in soup.select(".result"):
        title_tag = item.select_one(".result__a")
        snippet_tag = item.select_one(".result__snippet")
        url_tag = item.select_one(".result__url")

        if not title_tag:
            continue

        title = title_tag.get_text(strip=True)
        snippet = snippet_tag.get_text(strip=True) if snippet_tag else ""
        url = url_tag.get_text(strip=True) if url_tag else ""

        # Filter to only UE API related results
        if any(kw in title.lower() or kw in url.lower() for kw in ["unreal", "epic", "api"]):
            results.append({"title": title, "snippet": snippet, "url": url})

        if len(results) >= MAX_RESULTS:
            break

    return results


def search_general(query: str) -> list[dict]:
    """Fallback: broader search without site restriction."""
    search_query = f"UE5 C++ API {query}"

    try:
        resp = requests.post(
            SEARCH_URL,
            data={"q": search_query, "b": ""},
            headers=HEADERS,
            timeout=15,
        )
        resp.raise_for_status()
    except requests.RequestException:
        return []

    soup = BeautifulSoup(resp.text, "html.parser")
    results = []

    for item in soup.select(".result"):
        title_tag = item.select_one(".result__a")
        snippet_tag = item.select_one(".result__snippet")
        url_tag = item.select_one(".result__url")

        if not title_tag:
            continue

        title = title_tag.get_text(strip=True)
        snippet = snippet_tag.get_text(strip=True) if snippet_tag else ""
        url = url_tag.get_text(strip=True) if url_tag else ""

        results.append({"title": title, "snippet": snippet, "url": url})

        if len(results) >= MAX_RESULTS:
            break

    return results


def format_output(query: str, results: list[dict]) -> str:
    lines = []
    lines.append(f"# UE5 API: {query}")
    lines.append(f"Results: {len(results)}")
    lines.append("")

    for i, r in enumerate(results, 1):
        lines.append(f"## [{i}] {r['title']}")
        if r["url"]:
            lines.append(f"URL: {r['url']}")
        if r["snippet"]:
            # Clean up snippet
            snippet = re.sub(r"\s+", " ", r["snippet"]).strip()
            lines.append(f"{snippet}")
        lines.append("")

    return "\n".join(lines)


def main():
    if len(sys.argv) < 2:
        print("Usage: python fetch-ue-api.py <query>")
        print('Example: python fetch-ue-api.py ACharacter')
        print('         python fetch-ue-api.py "FHitResult members"')
        sys.exit(1)

    query = " ".join(sys.argv[1:]).strip()

    # Try site-specific search first
    results = search_ue_docs(query)

    # Fallback to general search
    if not results:
        results = search_general(query)

    if not results:
        print(f"[NO RESULTS] No documentation found for: {query}")
        print("Try a different class name or broader query.")
        sys.exit(1)

    print(format_output(query, results))


if __name__ == "__main__":
    main()
