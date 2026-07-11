#!/usr/bin/env python3
"""Stream selected parquet rows as CSV.

This helper keeps book-validate free of an Arrow C++ dependency.  It is a thin
I/O adapter only: market semantics stay in the C++ checker and mappers.
"""

from __future__ import annotations

import argparse
import csv
import signal
import sys
from pathlib import Path

import pyarrow.compute as pc
import pyarrow.dataset as ds
import pyarrow.parquet as pq


def stream_rows(path: Path, symbol: str) -> None:
    dataset = ds.dataset(path, format="parquet")
    columns = list(dataset.schema.names)
    writer = csv.writer(sys.stdout, lineterminator="\n")
    writer.writerow(columns)

    scanner = dataset.scanner(
        columns=columns,
        filter=pc.field("SecurityID") == symbol,
        batch_size=262_144,
    )
    for batch in scanner.to_batches():
        for row in batch.to_pylist():
            writer.writerow(["" if row[name] is None else row[name] for name in columns])


def list_symbols(path: Path) -> None:
    seen: set[str] = set()
    pf = pq.ParquetFile(path)
    for batch in pf.iter_batches(columns=["SecurityID"], batch_size=1_000_000):
        for value in batch.column(0).to_pylist():
            if value:
                seen.add(value.strip())
    for symbol in sorted(seen):
        print(symbol)


def main() -> int:
    if hasattr(signal, "SIGPIPE"):
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)

    parser = argparse.ArgumentParser()
    parser.add_argument("--file")
    parser.add_argument("--symbol")
    parser.add_argument("--list-symbols", action="store_true")
    args = parser.parse_args()

    if not args.file:
        parser.error("--file is required")
    path = Path(args.file)
    if args.list_symbols:
        list_symbols(path)
        return 0
    if not args.symbol:
        parser.error("--symbol is required unless --list-symbols is used")
    stream_rows(path, args.symbol)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except BrokenPipeError:
        raise SystemExit(0)
