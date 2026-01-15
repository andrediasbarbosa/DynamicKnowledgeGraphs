# Gemini API Setup Guide

## Current Configuration

The system now uses **Gemini 1.5 Flash** (also known as Flash 2.5) as the default model for the Gemini provider.

## API Endpoint

- **Base URL**: `https://generativelanguage.googleapis.com/v1`
- **Model**: `gemini-1.5-flash`

## Environment Variables

```bash
export GEMINI_API_KEY='your-api-key-here'
export KG_LLM_PROVIDER='gemini'
export KG_LLM_MODEL='gemini-1.5-flash'  # Optional, this is the default
```

## Configuration File

In `pipeline_config.json`:

```json
{
  "llm_provider": "gemini",
  "llm_api_key": "your-api-key-here",
  "llm_model": "gemini-1.5-flash",
  "llm_temperature": 0.0,
  "llm_max_tokens": 2000
}
```

## Available Gemini Models

You can use any of these models by setting `llm_model`:

- `gemini-1.5-flash` (default) - Fast and efficient, good for most tasks
- `gemini-1.5-flash-8b` - Smaller, faster version
- `gemini-1.5-pro` - More capable, slower, higher cost
- `gemini-2.0-flash-exp` - Latest experimental version

## Getting an API Key

1. Go to [Google AI Studio](https://makersuite.google.com/app/apikey)
2. Click "Get API Key"
3. Create a new API key or use an existing one
4. Copy the key and set it in your environment

## Testing Your Setup

```bash
# Set your API key
export GEMINI_API_KEY='your-key'

# Run the LLM extraction example
cd build
./bin/llm_extraction_example

# Or run the full pipeline
./bin/pipeline_example
```

## Code Example

```cpp
#include "llm/llm_provider.hpp"

// Create Gemini provider
auto provider = std::make_unique<GeminiProvider>(
    "your-api-key",
    "gemini-1.5-flash"  // or any other model
);

// Extract relations
auto result = provider->extract_relations(text, "chunk_id");

for (const auto& rel : result.relations) {
    std::cout << "Found: " << rel.sources[0] 
              << " -> " << rel.relation 
              << " -> " << rel.targets[0] << "\n";
}
```

## Troubleshooting

### 404 Error: Model Not Found

**Error**: `models/gemini-pro is not found for API version v1beta`

**Solution**: Update to use `gemini-1.5-flash` instead of the deprecated `gemini-pro`:

```bash
export KG_LLM_MODEL='gemini-1.5-flash'
```

### Authentication Error

**Error**: `API key not valid`

**Solution**: 
1. Verify your API key is correct
2. Make sure you've set the `GEMINI_API_KEY` environment variable
3. Check that your API key has the Gemini API enabled

### Rate Limiting

If you hit rate limits, adjust the delay in your config:

```json
{
  "rate_limit_delay_ms": 2000  // 2 seconds between requests
}
```

## API Quotas

Gemini 1.5 Flash free tier:
- 15 requests per minute
- 1 million tokens per minute
- 1500 requests per day

Adjust `rate_limit_delay_ms` to stay within limits:
- 15 RPM = ~4000ms delay minimum
- 60 RPM (paid tier) = ~1000ms delay

## Differences from OpenAI

1. **System Messages**: Gemini prepends system messages to the first user message
2. **Response Format**: Uses `candidates` array instead of `choices`
3. **Token Counting**: Field names are different (`promptTokenCount` vs `prompt_tokens`)
4. **API Structure**: Uses `/v1/models/{model}:generateContent` endpoint

All these differences are handled automatically by the `GeminiProvider` class.

