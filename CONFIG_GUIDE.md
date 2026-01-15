# LLM Configuration Guide

This guide explains how to configure API keys for OpenAI and Gemini providers across all examples.

## Configuration Methods

The examples support multiple configuration methods, tried in this order:

1. **Config file** (`.llm_config.json`) - **Recommended**
2. **Environment variables** - Fallback option
3. **Demo mode** - Runs without API keys (limited functionality)

## Method 1: Config File (Recommended)

### For OpenAI

1. Copy the example config:
   ```bash
   cp .llm_config.json.example .llm_config.json
   ```

2. Edit `.llm_config.json`:
   ```json
   {
     "provider": "openai",
     "api_key": "sk-your-actual-openai-key-here",
     "model": "gpt-4",
     "temperature": 0.0,
     "max_tokens": 2000,
     "timeout_seconds": 60,
     "max_retries": 3,
     "verbose": false
   }
   ```

3. Run any example:
   ```bash
   ./build/bin/llm_extraction_example
   ./build/bin/pipeline_example
   ```

### For Gemini

1. Copy the Gemini example:
   ```bash
   cp .llm_config_gemini.json.example .llm_config.json
   ```

2. Edit `.llm_config.json`:
   ```json
   {
     "provider": "gemini",
     "api_key": "your-actual-gemini-key-here",
     "model": "gemini-1.5-flash",
     "temperature": 0.0,
     "max_tokens": 2000,
     "timeout_seconds": 60,
     "max_retries": 3,
     "verbose": false
   }
   ```

3. Run any example:
   ```bash
   ./build/bin/llm_extraction_example
   ./build/bin/pipeline_example
   ```

## Method 2: Environment Variables

If no config file is found, the system falls back to environment variables.

### For OpenAI

```bash
export OPENAI_API_KEY='sk-your-actual-key-here'
export KG_LLM_MODEL='gpt-4'  # Optional, defaults to gpt-4
./build/bin/llm_extraction_example
```

### For Gemini

```bash
export GEMINI_API_KEY='your-actual-key-here'
export KG_LLM_PROVIDER='gemini'
export KG_LLM_MODEL='gemini-1.5-flash'  # Optional
./build/bin/llm_extraction_example
```

## Config File Locations

The system automatically searches for config files in multiple locations:

1. `.llm_config.json` (current directory)
2. `../.llm_config.json` (from build/ directory)
3. `../../.llm_config.json` (from build/bin/ directory)

This means you can run examples from any of these locations:

```bash
# From project root
./build/bin/llm_extraction_example

# From build/bin directory
cd build/bin
./llm_extraction_example

# From build directory
cd build
./bin/llm_extraction_example
```

All will find the `.llm_config.json` file in the project root!

## Available Models

### OpenAI Models
- `gpt-4` (default) - Most capable
- `gpt-4-turbo` - Faster, cheaper
- `gpt-3.5-turbo` - Fast, economical

### Gemini Models
- `gemini-1.5-flash` (default) - Fast, efficient
- `gemini-1.5-flash-8b` - Smaller, faster
- `gemini-1.5-pro` - More capable
- `gemini-2.0-flash-exp` - Latest experimental

## Security

**IMPORTANT**: Never commit `.llm_config.json` to version control!

The `.gitignore` file is configured to:
- ✅ Ignore `.llm_config.json` (contains your API key)
- ✅ Keep `.llm_config*.example` files (safe templates)

Always use the example files as templates and copy them to create your config.

## Examples Configuration

### LLM Extraction Example

```bash
# 1. Create config
cp .llm_config.json.example .llm_config.json
# Edit with your API key

# 2. Run with default text
./build/bin/llm_extraction_example

# 3. Or provide custom text
./build/bin/llm_extraction_example "Your text to analyze"
```

### Pipeline Example

The pipeline example uses the same `.llm_config.json` file, or you can provide a full pipeline config:

```bash
# Option 1: Use .llm_config.json
cp .llm_config.json.example .llm_config.json
# Edit with your API key
./build/bin/pipeline_example

# Option 2: Use custom pipeline config
./build/bin/pipeline_example --config example_pipeline_config.json

# Option 3: Process specific PDFs
./build/bin/pipeline_example path/to/document.pdf
```

## Pipeline Config File Format

For advanced users, you can create a complete pipeline config:

```json
{
  "llm_provider": "openai",
  "llm_api_key": "your-key-here",
  "llm_model": "gpt-4",
  "llm_temperature": 0.0,
  "llm_max_tokens": 2000,
  "llm_timeout_seconds": 60,
  "llm_max_retries": 3,

  "chunking_strategy": "sentence",
  "max_sentences": 5,
  "max_chars_per_chunk": 800,

  "batch_size": 10,
  "rate_limit_delay_ms": 1000,

  "enable_deduplication": true,
  "similarity_threshold": 0.85,

  "output_directory": "output_json",
  "save_intermediate": true,
  "save_extractions": true,
  "verbose": true
}
```

## Switching Between Providers

To switch from OpenAI to Gemini (or vice versa), just edit `.llm_config.json`:

```bash
# Edit the file
nano .llm_config.json

# Change "provider" and "api_key"
# From:
#   "provider": "openai",
#   "api_key": "sk-...",
# To:
#   "provider": "gemini",
#   "api_key": "your-gemini-key",
#   "model": "gemini-1.5-flash"

# Run any example - it will use the new provider
./build/bin/llm_extraction_example
```

## Troubleshooting

### "No API key found"
- Check that `.llm_config.json` exists in project root
- Verify the JSON syntax is valid
- Ensure `api_key` field is not empty

### "Configuration error: LLM API key is required"
- The config file was found but `api_key` is missing or empty
- Edit `.llm_config.json` and add your actual API key

### "HTTP request failed with code 401"
- Your API key is invalid or expired
- For OpenAI: Check https://platform.openai.com/api-keys
- For Gemini: Check https://aistudio.google.com/app/apikey

### "HTTP request failed with code 404"
- The model name might be incorrect
- For Gemini, use `gemini-1.5-flash` (not `gemini-pro`)
- For OpenAI, use `gpt-4` or `gpt-3.5-turbo`

### Config file not found from build/bin
- The system automatically searches multiple locations
- Place `.llm_config.json` in the project root (not in build/)
- The examples will find it from any subdirectory

## Getting API Keys

### OpenAI
1. Go to https://platform.openai.com/signup
2. Navigate to API keys section
3. Create new secret key
4. Copy the key (starts with `sk-`)

### Gemini
1. Go to https://aistudio.google.com/app/apikey
2. Click "Create API Key"
3. Copy the key
4. See [GEMINI_SETUP.md](GEMINI_SETUP.md) for detailed setup

## Quick Start

```bash
# 1. Get an API key (OpenAI or Gemini)

# 2. Create config file
cp .llm_config.json.example .llm_config.json

# 3. Edit with your key
nano .llm_config.json

# 4. Run an example
./build/bin/llm_extraction_example

# 5. Check output
ls output_json/
```

That's it! All examples will now use your configured API key.
