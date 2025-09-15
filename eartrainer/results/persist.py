from __future__ import annotations

"""Simple, durable JSON persistence for progress over time by category/degree.

Schema (v2):
{
  "schema": 2,
  "categories": {
    "note": {
      "major": {"degrees": {"1": {"total": {asked, correct}, "timeline": [ {ts, asked_total, correct_total, delta_asked, delta_correct, ...} ] }, ...}},
      "natural_minor": { ... }
    },
    "chord": { ... }
  }
}

Notes:
- New entries are prepended to timeline (newest first).
- Timeline entries store cumulative totals to make plotting simpler without re-scan.
"""

from typing import Any, Dict
from pathlib import Path
import json
from datetime import datetime


def _load(path: str) -> Dict[str, Any]:
    p = Path(path)
    if not p.exists():
        return {"schema": 2, "categories": {}}
    try:
        with p.open("r", encoding="utf-8") as f:
            data = json.load(f)
            if not isinstance(data, dict):
                return {"schema": 2, "categories": {}}
            if int(data.get("schema", 0)) != 2:
                return {"schema": 2, "categories": {}}
            data.setdefault("categories", {})
            return data
    except Exception:
        return {"schema": 2, "categories": {}}


def _save(path: str, data: Dict[str, Any]) -> None:
    p = Path(path)
    try:
        with p.open("w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
    except Exception:
        # Best-effort; swallow for now
        pass


def persist_progress(
    path: str,
    *,
    category: str,
    scale: str,
    per_degree: Dict[str, Any],
    detail: str = "basic",
) -> None:
    """Persist per-degree results into a category/scale timeline with cumulative totals.

    - category: "note" | "chord" (future: "interval")
    - scale: e.g., "major" or "natural_minor"
    - per_degree: mapping "degree" -> {asked, correct, meta?}
    - detail: "basic" (just asked/correct) or "rich" (include assist/time deltas)
    """
    data = _load(path)
    cats = data.setdefault("categories", {})
    cat = cats.setdefault(category, {})
    scale_obj = cat.setdefault(scale, {"degrees": {}})
    degs = scale_obj.setdefault("degrees", {})
    ts = datetime.utcnow().isoformat()

    for degree, stats in (per_degree or {}).items():
        asked = int(stats.get("asked", 0))
        correct = int(stats.get("correct", 0))
        if asked <= 0:
            continue
        node = degs.setdefault(str(degree), {"total": {"asked": 0, "correct": 0}, "timeline": []})
        tot = node.setdefault("total", {"asked": 0, "correct": 0})
        tot["asked"] = int(tot.get("asked", 0)) + asked
        tot["correct"] = int(tot.get("correct", 0)) + correct

        entry = {
            "ts": ts,
            "asked_total": tot["asked"],
            "correct_total": tot["correct"],
            "delta_asked": asked,
            "delta_correct": correct,
        }
        if detail == "rich":
            meta = stats.get("meta", {}) or {}
            ac = meta.get("assist_counts", {}) or {}
            entry["assist_delta"] = {k: int(ac.get(k, 0)) for k in ("r", "s", "c", "p", "t")}
            answers = int(meta.get("answers", 0) or 0)
            if answers > 0:
                entry["avg_ms_this_session"] = int(meta.get("time_ms_total", 0)) // answers

        # Prepend new entry
        timeline = node.setdefault("timeline", [])
        timeline.insert(0, entry)
        node["timeline"] = timeline
        node["total"] = tot
        degs[str(degree)] = node

    cat[scale] = {"degrees": degs}
    cats[category] = cat
    data["categories"] = cats
    data["schema"] = 2
    _save(path, data)


def persist_compact_session(
    path: str,
    *,
    scale: str,
    key: str | None,
    categories: Dict[str, Any],
    detail: str = "basic",
) -> None:
    """Persist one compact session entry with per-category totals and per-degree deltas.

    categories shape (caller supplies only nonzero fields):
    {
      "note": {"Q": int, "C": int, "D": {"1": {"Q": int, "C": int, "A"?: {...}, "T"?: int}}},
      "chord": { ... }
    }
    """
    # Load or init schema v3 document
    from pathlib import Path
    p = Path(path)
    raw_text: str | None = None
    if p.exists():
        try:
            raw_text = p.read_text(encoding="utf-8")
            data = json.loads(raw_text)
        except Exception:
            data = {}
    else:
        data = {}

    if not isinstance(data, dict) or int(data.get("schema", 0)) != 3:
        # Backup previous file once when upgrading schema
        if raw_text is not None:
            try:
                backup_name = f"{p.stem}.backup-{datetime.utcnow().strftime('%Y%m%d-%H%M%S')}{p.suffix}"
                p.with_name(backup_name).write_text(raw_text, encoding="utf-8")
            except Exception:
                pass
        data = {"schema": 3, "sessions": [], "totals": {}}

    sessions = data.setdefault("sessions", [])
    totals = data.setdefault("totals", {})

    # Build session entry
    ts = datetime.utcnow().strftime("%Y-%m-%d %H:%M")
    entry = {"ts": ts, "S": str(scale), "cats": {}}
    if key:
        entry["K"] = str(key)

    for cat_name, cat_val in (categories or {}).items():
        if not cat_val:
            continue
        # Minimal copy; ensure correct types
        cat_entry: Dict[str, Any] = {}
        q = int(cat_val.get("Q", 0))
        c = int(cat_val.get("C", 0))
        if q > 0:
            cat_entry["Q"] = q
        if c > 0 or q > 0:
            cat_entry["C"] = c
        degs = {}
        for deg, st in (cat_val.get("D", {}) or {}).items():
            dq = int(st.get("Q", 0))
            if dq <= 0:
                continue
            dc = int(st.get("C", 0))
            node: Dict[str, Any] = {"Q": dq}
            if dc > 0:
                node["C"] = dc
            if detail == "rich":
                # assistance sparse copy
                ac_src = st.get("A", {}) or {}
                a_sparse = {k: int(v) for k, v in ac_src.items() if int(v) > 0}
                if a_sparse:
                    node["A"] = a_sparse
                t = int(st.get("T", 0))
                if t > 0:
                    node["T"] = t
            degs[str(deg)] = node
        if degs:
            cat_entry["D"] = degs
        if cat_entry:
            entry["cats"][cat_name] = cat_entry

        # Update totals
        if degs:
            tcat = totals.setdefault(cat_name, {})
            tscale = tcat.setdefault(str(scale), {"D": {}})
            tdegs = tscale.setdefault("D", {})
            for dkey, node in degs.items():
                td = tdegs.setdefault(dkey, {"Q": 0, "C": 0})
                td["Q"] = int(td.get("Q", 0)) + int(node.get("Q", 0))
                td["C"] = int(td.get("C", 0)) + int(node.get("C", 0))
                if detail == "rich":
                    if "A" in node:
                        ta = td.setdefault("A", {})
                        for k, v in node["A"].items():
                            ta[k] = int(ta.get(k, 0)) + int(v)
                    if "T" in node:
                        td["T"] = int(td.get("T", 0)) + int(node.get("T", 0))
                tdegs[dkey] = td
            tscale["D"] = tdegs
            tcat[str(scale)] = tscale
            totals[cat_name] = tcat

    # Only append if there is any category content
    if entry.get("cats"):
        sessions.insert(0, entry)
        data["sessions"] = sessions
        data["totals"] = totals
        with p.open("w", encoding="utf-8") as f:
            json.dump(data, f, separators=(",", ":"))
