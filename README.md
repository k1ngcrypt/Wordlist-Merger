# Wordlist Merger

A fast CLI tool that merges multiple wordlist files into a single deduplicated output.

## Features

- Weave-merges any number of input files (round-robin line interleaving)
- Deduplicates entries using hash-based tracking
- Wildcard support for input paths (`*.txt`, `file?.txt`, etc.)
- Configurable output file via `-o` / `--output`
- Large I/O buffers for high throughput

## Usage

```
"Wordlist Merger" [-o <output>] <file1> <file2> ... <fileN>
```

| Flag | Description |
|------|-------------|
| `-o`, `--output <file>` | Output file (default: `merged.txt`) |

### Examples

```bash
# Merge two files
"Wordlist Merger" list1.txt list2.txt

# Merge all .txt files into custom output
"Wordlist Merger" -o combined.txt *.txt
```

## Build

Open `Wordlist Merger.slnx` in **Visual Studio** and build the solution (C++17 or later required).