"""Drops each result under deps/ from the SARIF files named on the command line, in place.

Each checkout under deps/ is another repository, and its own analysis reports its findings.
"""

import json
import sys


def under_deps(result):
    """True where a location of the result is a file under deps/."""
    for location in result.get("locations", []):
        uri = location.get("physicalLocation", {}).get("artifactLocation", {}).get("uri", "")
        if uri.startswith("deps/"):
            return True
    return False


def main(paths):
    for path in paths:
        with open(path, encoding="utf-8") as handle:
            log = json.load(handle)
        dropped = 0
        for run in log.get("runs", []):
            kept = [result for result in run.get("results", []) if not under_deps(result)]
            dropped += len(run.get("results", [])) - len(kept)
            run["results"] = kept
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(log, handle)
        print("%s: dropped %d results under deps/" % (path, dropped))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
