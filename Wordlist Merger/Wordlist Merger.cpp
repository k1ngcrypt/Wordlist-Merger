#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <filesystem>
#include <memory>

using namespace std;
namespace fs = std::filesystem;

// Simple wildcard matching (* and ?)
static bool wildcardMatch(const string& text, const string& pattern) {
	size_t tIdx = 0, pIdx = 0;
	size_t starIdx = string::npos, matchIdx = 0;

	while (tIdx < text.length()) {
		if (pIdx < pattern.length() && (pattern[pIdx] == '?' || pattern[pIdx] == text[tIdx])) {
			tIdx++;
			pIdx++;
		} else if (pIdx < pattern.length() && pattern[pIdx] == '*') {
			starIdx = pIdx;
			matchIdx = tIdx;
			pIdx++;
		} else if (starIdx != string::npos) {
			pIdx = starIdx + 1;
			matchIdx++;
			tIdx = matchIdx;
		} else {
			return false;
		}
	}

	while (pIdx < pattern.length() && pattern[pIdx] == '*') {
		pIdx++;
	}

	return pIdx == pattern.length();
}

// Expand wildcards and validate files
static vector<string> expandFilePaths(const vector<string>& patterns) {
	vector<string> result;

	for (const string& pattern : patterns) {
		// Check if pattern contains wildcards
		if (pattern.find('*') != string::npos || pattern.find('?') != string::npos) {
			fs::path p(pattern);
			fs::path parentPath = p.parent_path();
			string filename = p.filename().string();

			// If no parent path, use current directory
			if (parentPath.empty()) {
				parentPath = ".";
			}

			try {
				if (fs::exists(parentPath) && fs::is_directory(parentPath)) {
					for (const auto& entry : fs::directory_iterator(parentPath)) {
						if (entry.is_regular_file()) {
							string entryName = entry.path().filename().string();
							if (wildcardMatch(entryName, filename)) {
								result.push_back(entry.path().string());
							}
						}
					}
				} else {
					cerr << "Warning: Directory not found for pattern: " << pattern << '\n';
				}
			} catch (const fs::filesystem_error& e) {
				cerr << "Warning: Error processing pattern '" << pattern << "': " << e.what() << '\n';
			}
		} else {
			// No wildcard, validate and add file
			if (fs::exists(pattern) && fs::is_regular_file(pattern)) {
				result.push_back(pattern);
			} else {
				cerr << "Warning: File not found or not a regular file: " << pattern << '\n';
			}
		}
	}

	return result;
}

// Weave-merge with deduplication
static void weaveMergeDedup(const vector<string>& filePaths, ofstream& out) {
	vector<ifstream> files;
	vector<unique_ptr<char[]>> buffers;
	hash<string> hasher;
	unordered_set<size_t> written; // Track what we've already written
	written.reserve(10'000'000); // Reserve for 10M unique lines

	// Open all files
	for (const string& path : filePaths) {
		files.emplace_back(path, ios::binary);
		if (!files.back().is_open()) {
			cerr << "Warning: Could not open file: " << path << '\n';
			files.pop_back();
			continue;
		}

		// Create unique buffer for each file
		auto buffer = make_unique<char[]>(131072); // 128KB buffer per file
		files.back().rdbuf()->pubsetbuf(buffer.get(), 131072);
		buffers.push_back(move(buffer));
	}

	if (files.empty()) {
		cerr << "Error: No files could be opened for weave-merge\n";
		return;
	}

	cerr << "Weave-merging " << files.size() << " files...\n";

	bool anyAlive = true;
	string line;
	line.reserve(256);

	size_t iterations = 0;

	while (anyAlive) {
		anyAlive = false;
		iterations++;

		for (ifstream& f : files) {
			if (!f.good()) continue;

			if (getline(f, line)) {
				anyAlive = true;

				size_t hash = hasher(line);

				// Only write if not yet written (deduplication)
				if (written.insert(hash).second) {
					out << line << '\n';
				}

				line.clear();
			}
		}

		// Progress indicator for large merges
		if (iterations % 10000 == 0) {
			cerr << "Progress: " << written.size() << " unique lines written...\r" << flush;
		}
	}

	cerr << "\nMerge complete: " << written.size() << " unique lines written\n";
	cerr << "Memory usage: ~" << (written.size() * sizeof(size_t)) / 1024 / 1024 << " MB\n";
}

int main() {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);
	cerr.tie(nullptr);

	if (__argc < 2) {
		cerr << "Usage: " << __argv[0] << " [-o <output>] <file1> <file2> ... <fileN>\n";
		cerr << "  -o, --output <file>  Output file (default: merged.txt)\n";
		cerr << "  Supports wildcards: *.txt, file?.txt, etc.\n";
		return 1;
	}

	// Parse command-line arguments
	string outputFile = "merged.txt"; // Default output file
	vector<string> filePatterns;

	for (int i = 1; i < __argc; i++) {
		string arg = __argv[i];

		if (arg == "-o" || arg == "--output") {
			if (i + 1 < __argc) {
				outputFile = __argv[++i];
			} else {
				cerr << "Error: -o/--output requires a filename\n";
				return 1;
			}
		} else {
			filePatterns.emplace_back(arg);
		}
	}

	if (filePatterns.empty()) {
		cerr << "Error: No input files specified\n";
		return 1;
	}

	// Expand wildcards and validate files
	cerr << "Expanding file patterns...\n";
	vector<string> filePaths = expandFilePaths(filePatterns);

	if (filePaths.empty()) {
		cerr << "Error: No valid input files found\n";
		return 1;
	}

	cerr << "Processing " << filePaths.size() << " files...\n";

	// Warn if opening many files simultaneously (potential file descriptor limit issue)
	if (filePaths.size() > 100) {
		cerr << "Warning: Opening " << filePaths.size() << " files simultaneously.\n";
		cerr << "         If you encounter errors, your OS may have file descriptor limits.\n";
	}

	cerr << "Output file: " << outputFile << '\n';

	// Open output file with large buffer
	ofstream out(outputFile, ios::binary);
	if (!out.is_open()) {
		cerr << "Error: Could not open output file: " << outputFile << '\n';
		return 1;
	}
	auto outBuffer = make_unique<char[]>(1048576); // 1MB output buffer
	out.rdbuf()->pubsetbuf(outBuffer.get(), 1048576);

	// Weave-merge with deduplication
	weaveMergeDedup(filePaths, out);

	cerr << "Output written to: " << outputFile << '\n';

	return 0;
}
