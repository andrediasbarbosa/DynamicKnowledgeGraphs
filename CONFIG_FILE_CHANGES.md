# Config File System Implementation

## Summary

Implemented a flexible configuration system that allows API keys to be stored in config files instead of environment variables, while maintaining backward compatibility.

## Changes Made

### 1. New Files Created

#### `.llm_config.json.example`
Template for OpenAI configuration:
```json
{
  "provider": "openai",
  "api_key": "your-openai-key-here",
  "model": "gpt-4",
  "temperature": 0.0,
  "max_tokens": 2000,
  "timeout_seconds": 60,
  "max_retries": 3,
  "verbose": false
}
```

#### `.llm_config_gemini.json.example`
Template for Gemini configuration:
```json
{
  "provider": "gemini",
  "api_key": "your-gemini-key-here",
  "model": "gemini-1.5-flash",
  ...
}
```

#### `CONFIG_GUIDE.md`
Comprehensive guide covering:
- Configuration methods (config file, environment variables, demo mode)
- Setup instructions for both OpenAI and Gemini
- Security best practices
- Troubleshooting
- Quick start guide

### 2. Code Changes

#### `include/llm/llm_provider.hpp`
Added new factory method:
```cpp
static std::unique_ptr<LLMProvider> create_from_config_file(
    const std::string& config_path = ""
);
```

Tries multiple locations automatically:
1. Provided config_path
2. `.llm_config.json` (current directory)
3. `../.llm_config.json` (from build/)
4. `../../.llm_config.json` (from build/bin/)
5. Environment variables (fallback)

#### `src/llm/llm_provider.cpp`
- Added `#include <fstream>` for file I/O
- Implemented `create_from_config_file()` method with intelligent path searching
- JSON parsing with error handling and fallback to environment variables

#### `src/pipeline/extraction_pipeline.cpp`
- Updated default Gemini model from `gemini-pro` to `gemini-1.5-flash`
- Enhanced `load_config_with_fallback()` to check for `.llm_config.json` in multiple locations
- Tries config files before falling back to environment variables

#### `examples/llm_extraction_example.cpp`
Changed from:
```cpp
auto provider = LLMProviderFactory::create_from_env();
```

To:
```cpp
auto provider = LLMProviderFactory::create_from_config_file();
```

Updated user-facing messages to recommend config files over environment variables.

#### `examples/pipeline_example.cpp`
Updated error messages and help text to prominently feature the `.llm_config.json` method as the primary configuration approach.

### 3. Security Updates

#### `.gitignore`
Added protection for API keys:
```gitignore
# API keys and sensitive config (keep examples)
.llm_config.json
!.llm_config*.example
```

Ensures:
- ✅ `.llm_config.json` (with real API keys) is never committed
- ✅ `.llm_config*.example` templates remain in version control

### 4. Documentation Updates

#### `README.md`
- Updated LLM extraction example section to show config file method first
- Updated pipeline example section with new configuration approach
- Added references to [CONFIG_GUIDE.md](CONFIG_GUIDE.md)

#### `PATH_FIX_SUMMARY.md`
- Updated to reference new config file system
- Added link to [CONFIG_GUIDE.md](CONFIG_GUIDE.md)

## Configuration Priority

The system now uses this priority order:

1. **Config file** - `.llm_config.json` (recommended)
2. **Environment variables** - `OPENAI_API_KEY`, `GEMINI_API_KEY`, etc.
3. **Demo mode** - Runs without API keys for testing

## Benefits

### 1. Security
- API keys stored in local files, not shell history
- Protected by `.gitignore` from accidental commits
- No global environment pollution

### 2. Convenience
- No need to export variables every session
- Easy to switch between OpenAI and Gemini (just edit one file)
- Works from any directory (automatic path resolution)

### 3. Flexibility
- Backward compatible with environment variables
- Support for custom config paths
- Multiple config templates provided

### 4. User Experience
- Simpler setup process
- Clear instructions in examples
- Better error messages

## Migration Guide

### For Existing Users

If you were using environment variables:

```bash
# Old method (still works!)
export OPENAI_API_KEY='your-key'
./build/bin/llm_extraction_example
```

New recommended method:
```bash
# Create config once
cp .llm_config.json.example .llm_config.json
nano .llm_config.json  # Add your key

# Use forever
./build/bin/llm_extraction_example
```

### For New Users

Just follow the new quick start:
```bash
cp .llm_config.json.example .llm_config.json
# Edit with your API key
./build/bin/llm_extraction_example
```

## Testing

All changes have been tested:
- ✅ Config file loading from project root
- ✅ Config file loading from build/bin/
- ✅ Fallback to environment variables
- ✅ Demo mode without any configuration
- ✅ Both OpenAI and Gemini configuration templates
- ✅ Examples run successfully with new system

## File Structure

```
project/
├── .llm_config.json.example       # OpenAI template (safe to commit)
├── .llm_config_gemini.json.example # Gemini template (safe to commit)
├── .llm_config.json               # User's actual config (gitignored)
├── CONFIG_GUIDE.md                # Comprehensive configuration guide
├── README.md                      # Updated with config file methods
├── PATH_FIX_SUMMARY.md           # Updated with new config system
└── examples/
    ├── llm_extraction_example.cpp # Uses create_from_config_file()
    └── pipeline_example.cpp       # Updated help text
```

## Code Examples

### Loading Config in C++

```cpp
// Simple - automatic location detection
auto provider = LLMProviderFactory::create_from_config_file();

// Custom path
auto provider = LLMProviderFactory::create_from_config_file("my_config.json");

// Check if loaded
if (!provider) {
    std::cout << "No config found, running in demo mode\n";
}
```

### Config File Format

```json
{
  "provider": "openai",        // or "gemini"
  "api_key": "sk-...",         // Your actual API key
  "model": "gpt-4",            // Model to use
  "temperature": 0.0,          // Optional
  "max_tokens": 2000,          // Optional
  "timeout_seconds": 60,       // Optional
  "max_retries": 3,            // Optional
  "verbose": false             // Optional
}
```

## Next Steps for Users

1. Copy the appropriate template:
   ```bash
   cp .llm_config.json.example .llm_config.json
   # OR
   cp .llm_config_gemini.json.example .llm_config.json
   ```

2. Edit with your API key:
   ```bash
   nano .llm_config.json
   ```

3. Run any example:
   ```bash
   ./build/bin/llm_extraction_example
   ./build/bin/pipeline_example
   ```

4. Read [CONFIG_GUIDE.md](CONFIG_GUIDE.md) for advanced options

## Implementation Notes

- The implementation maintains full backward compatibility
- Environment variables still work as before
- No breaking changes to existing code
- Path resolution handles running from any directory
- Error handling falls back gracefully through the priority chain
- All examples updated consistently
