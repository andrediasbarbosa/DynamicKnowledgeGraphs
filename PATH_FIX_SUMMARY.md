# File Path Resolution Fix

## Issue
The example programs ([llm_extraction_example.cpp](examples/llm_extraction_example.cpp) and [pipeline_example.cpp](examples/pipeline_example.cpp)) failed to find `tests/1page.pdf` when run from directories other than the project root, resulting in errors like:

```
poppler/error: Couldn't open file 'tests/1page.pdf': No such file or directory
```

## Solution
Added intelligent path resolution that tries multiple locations:

1. **`tests/1page.pdf`** - from project root
2. **`../tests/1page.pdf`** - from build/ directory
3. **`../../tests/1page.pdf`** - from build/bin/ directory

The examples now automatically detect which path works and use it.

## Changes Made

### llm_extraction_example.cpp
Added path resolution logic at [llm_extraction_example.cpp:260-277](examples/llm_extraction_example.cpp#L260-L277):

```cpp
// Find PDF path (try multiple locations)
std::string pdf_path = "tests/1page.pdf";
struct stat buffer;

if (stat(pdf_path.c_str(), &buffer) != 0) {
    // Try from build directory
    pdf_path = "../tests/1page.pdf";
    if (stat(pdf_path.c_str(), &buffer) != 0) {
        // Try from build/bin directory
        pdf_path = "../../tests/1page.pdf";
        if (stat(pdf_path.c_str(), &buffer) != 0) {
            throw std::runtime_error(
                "Could not find tests/1page.pdf. "
                "Please run from project root directory."
            );
        }
    }
}
```

### pipeline_example.cpp
Added similar logic for directory scanning at [pipeline_example.cpp:123-146](examples/pipeline_example.cpp#L123-L146):

```cpp
// Process test directory (try multiple locations)
std::string test_dir = "tests";
pdf_paths = find_pdf_files(test_dir, false);

if (pdf_paths.empty()) {
    // Try from build directory
    test_dir = "../tests";
    pdf_paths = find_pdf_files(test_dir, false);
}

if (pdf_paths.empty()) {
    // Try from build/bin directory
    test_dir = "../../tests";
    pdf_paths = find_pdf_files(test_dir, false);
}
```

## Usage

### Running from Project Root (Recommended)
```bash
./build/bin/llm_extraction_example
./build/bin/pipeline_example
```

### Running from build/bin Directory
```bash
cd build/bin
./llm_extraction_example
./pipeline_example
```

### Running from build Directory
```bash
cd build
./bin/llm_extraction_example
./bin/pipeline_example
```

All of these locations now work correctly!

## Testing

The fix has been tested from:
- ✅ Project root directory
- ✅ build/bin directory
- ✅ build directory

Both examples run successfully in demo mode (without API keys) from all locations.

## Note

When you have API keys configured and want to run the full extraction, the examples will now correctly find and process the test PDFs regardless of your current working directory.

To run with real LLM extraction:
```bash
# Create config file (recommended)
cp .llm_config.json.example .llm_config.json
# Edit .llm_config.json with your API key

./build/bin/llm_extraction_example
./build/bin/pipeline_example
```

Or use environment variables:
```bash
export OPENAI_API_KEY='your-key-here'
# OR
export GEMINI_API_KEY='your-key-here'
export KG_LLM_PROVIDER='gemini'

./build/bin/llm_extraction_example
```

See [CONFIG_GUIDE.md](CONFIG_GUIDE.md) for detailed configuration options.
