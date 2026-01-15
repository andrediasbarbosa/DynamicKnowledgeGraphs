# PDF Processing Component

## Overview

The PDF processing component provides robust text extraction from PDF documents using Poppler, along with multiple chunking strategies for preparing text for downstream LLM processing.

## Components

### 1. PDFProcessor
Main class for loading and processing PDF documents.

**Key Features:**
- Extract text from PDFs using Poppler
- Extract metadata (title, author, pages, etc.)
- Batch processing of multiple PDFs
- Clean and sanitize extracted text
- Verbose mode for debugging

**API:**
```cpp
PDFProcessor processor;
processor.set_verbose(true);

// Load single PDF
PDFDocument doc = processor.load_pdf("tests/1page.pdf");

// Load and chunk in one step
FixedSizeChunking strategy(500, 100);
auto chunks = processor.load_and_chunk("tests/1page.pdf", strategy);

// Batch processing
auto files = find_pdf_files("tests/", false);
auto documents = processor.load_multiple(files);
```

### 2. Data Structures

**PDFDocument**
- `file_path`: Original file location
- `document_id`: Sanitized identifier
- `metadata`: PDFMetadata struct
- `pages`: Vector of PDFPage structs
- `full_text`: Concatenated text from all pages

**PDFPage**
- `page_number`: 1-indexed page number
- `text`: Extracted text content
- `char_count`: Number of characters
- `word_count`: Number of words

**TextChunk**
- `text`: Chunk text content
- `document_id`: Source document
- `chunk_id`: Unique identifier
- `page_number`: Source page
- `chunk_index`: Position in document
- `start_position`, `end_position`: Character positions
- `metadata`: Additional key-value pairs

### 3. Chunking Strategies

All strategies implement the `ChunkingStrategy` interface, allowing easy swapping and extension.

#### FixedSizeChunking
**Best for:** Consistent processing, controlled token counts

```cpp
FixedSizeChunking strategy(
    500,  // chunk_size: characters per chunk
    100   // overlap: overlapping characters between chunks
);
```

**Characteristics:**
- Uniform chunk sizes (approximately)
- Overlap prevents information loss at boundaries
- Breaks at word boundaries when possible
- Good for: LLM processing with token limits, parallel processing

**Example:** 2320 char document → 6 chunks (~500 chars each)

#### PageBasedChunking
**Best for:** Preserving document structure, natural boundaries

```cpp
PageBasedChunking strategy;
```

**Characteristics:**
- One chunk per PDF page
- Preserves page-level context
- Variable chunk sizes
- Good for: Document understanding, page-level analysis

**Example:** 1-page document → 1 chunk (2320 chars)

#### ParagraphChunking
**Best for:** Semantic coherence, topic modeling

```cpp
ParagraphChunking strategy(
    3,     // max_paragraphs: paragraphs per chunk
    800    // max_chars: maximum characters per chunk
);
```

**Characteristics:**
- Groups paragraphs together
- Maintains semantic units
- Respects natural text structure
- Good for: Topic extraction, semantic search

**Example:** Depends on paragraph structure in document

#### SentenceChunking
**Best for:** Fine-grained control, granular extraction

```cpp
SentenceChunking strategy(
    5,      // max_sentences: sentences per chunk
    600     // max_chars: maximum characters per chunk
);
```

**Characteristics:**
- Groups sentences together
- Sentence-level granularity
- Respects sentence boundaries
- Good for: Fact extraction, detailed analysis

**Example:** 2320 char document → 5 chunks (~380-500 chars each)

## Usage Examples

### Basic PDF Loading

```cpp
#include "pdf/pdf_processor.hpp"

PDFProcessor processor;
PDFDocument doc = processor.load_pdf("document.pdf");

std::cout << "Pages: " << doc.metadata.num_pages << "\n";
std::cout << "Words: " << doc.get_total_words() << "\n";
```

### Chunking with Different Strategies

```cpp
// Strategy 1: Fixed-size
FixedSizeChunking fixed_strategy(1000, 200);
auto fixed_chunks = processor.chunk_document(doc, fixed_strategy);

// Strategy 2: Page-based
PageBasedChunking page_strategy;
auto page_chunks = processor.chunk_document(doc, page_strategy);

// Strategy 3: Paragraph
ParagraphChunking para_strategy(5, 1500);
auto para_chunks = processor.chunk_document(doc, para_strategy);
```

### Batch Processing

```cpp
// Find all PDFs
auto pdf_files = find_pdf_files("corpus/", true);  // recursive

// Load all
auto documents = processor.load_multiple(pdf_files);

// Chunk all with same strategy
FixedSizeChunking strategy(800, 150);
for (const auto& doc : documents) {
    auto chunks = processor.chunk_document(doc, strategy);
    // Process chunks...
}
```

### Export Chunks to JSON

```cpp
// Export chunks for LLM processing
for (const auto& chunk : chunks) {
    nlohmann::json j;
    j["chunk_id"] = chunk.chunk_id;
    j["text"] = chunk.text;
    j["page_number"] = chunk.page_number;
    j["metadata"] = chunk.metadata;

    // Save or send to LLM...
}
```

## Integration with Knowledge Extraction

The PDF processor is designed to feed the LLM extraction pipeline:

```
PDF Document
    ↓
[PDFProcessor]
    ↓
Text Chunks (with metadata)
    ↓
[LLM Provider] ← Next component to implement
    ↓
Extracted Relations
    ↓
[Hypergraph Builder]
    ↓
Knowledge Hypergraph
```

### Recommended Workflow

1. **Load PDF**: Extract text and metadata
2. **Choose Strategy**: Select chunking based on use case
3. **Process Chunks**: Send to LLM for extraction
4. **Build Graph**: Convert extracted relations to hyperedges
5. **Deduplicate**: Merge similar nodes using embeddings

## Performance Considerations

### Chunking Strategy Selection

| Use Case | Recommended Strategy | Reasoning |
|----------|---------------------|-----------|
| LLM API calls | FixedSizeChunking | Control token counts, consistent costs |
| Document QA | PageBasedChunking | Preserve page context for citations |
| Topic extraction | ParagraphChunking | Semantic coherence |
| Fact extraction | SentenceChunking | Fine-grained control |
| Large documents | FixedSizeChunking with overlap | Prevent information loss |

### Memory Usage

- **Text extraction**: O(document_size)
- **Chunking**: O(document_size) with negligible overhead
- **Batch processing**: Processes one document at a time

### Optimization Tips

1. **Overlap tuning**: 10-20% of chunk size prevents boundary issues
2. **Chunk size**: 500-1500 chars works well for most LLMs
3. **Batch size**: Process 10-50 documents per batch for efficiency
4. **Caching**: Store processed chunks to avoid re-processing

## Testing

Run the complete example:

```bash
cd build
./bin/pdf_processing_example
```

This demonstrates:
- Loading the test PDF (tests/1page.pdf)
- All four chunking strategies
- Metadata extraction
- Batch processing
- JSON export

## Dependencies

**Required:**
- Poppler C++ library (`libpoppler-cpp-dev`)
- C++17 compiler

**Install on Ubuntu/WSL:**
```bash
sudo apt-get install libpoppler-cpp-dev
```

## File Structure

```
include/pdf/
  └── pdf_processor.hpp          # Interface definitions

src/pdf/
  └── pdf_processor.cpp           # Implementation

examples/
  └── pdf_processing_example.cpp  # Usage demonstrations

output_json/
  ├── chunks_fixed_size.json      # Fixed-size chunks
  └── chunks_page_based.json      # Page-based chunks
```

## API Reference

### PDFProcessor

```cpp
class PDFProcessor {
public:
    // Load PDF and extract text
    PDFDocument load_pdf(const std::string& file_path);

    // Extract only metadata
    PDFMetadata extract_metadata(const std::string& file_path);

    // Chunk a loaded document
    std::vector<TextChunk> chunk_document(
        const PDFDocument& document,
        ChunkingStrategy& strategy
    );

    // Load and chunk in one step
    std::vector<TextChunk> load_and_chunk(
        const std::string& file_path,
        ChunkingStrategy& strategy
    );

    // Batch processing
    std::vector<PDFDocument> load_multiple(
        const std::vector<std::string>& file_paths
    );

    // Configuration
    void set_verbose(bool verbose);

    // Check availability
    static bool is_available();
};
```

### Utility Functions

```cpp
// Find PDFs in directory
std::vector<std::string> find_pdf_files(
    const std::string& directory_path,
    bool recursive = false
);

// Text utilities
size_t count_words(const std::string& text);
std::string sanitize_text(const std::string& text);
```

## Next Steps

With PDF processing complete, the next components to implement are:

1. **LLM Provider Interface** - Abstract interface for OpenAI, Gemini
2. **Extraction Strategies** - Use LLMs to extract relations from chunks
3. **Pipeline Integration** - Connect PDF → Chunks → LLM → Hypergraph

## Examples

### Example Output: Fixed-Size Chunks

From tests/1page.pdf (2320 chars, 325 words):

```
Chunk 1: 497 chars
"History thread: movement of texts and institutions The House of
Wisdom in Baghdad sponsored translators. Those translators produced
Arabic versions of..."

Chunk 2: 493 chars
"...hosted multilingual workshops under Norman administration. A
workshop there produced a Latin copy, and the copy entered a
cathedral library..."

... (6 chunks total)
```

### Example Output: Sentence Chunks

```
Chunk 1: 382 chars (5 sentences)
"History thread: movement of texts and institutions The House of
Wisdom in Baghdad sponsored translators Those translators produced
Arabic versions of Greek geometry..."

Chunk 2: 376 chars (5 sentences)
"Sicily hosted multilingual workshops under Norman administration
A workshop there produced a Latin copy, and the copy entered a
cathedral library..."

... (5 chunks total)
```

## Comparison with Paper

The chunking strategies support Algorithm 1 from the paper:
- Process documents in manageable chunks
- Maintain provenance (document ID, page number, chunk position)
- Enable incremental hypergraph construction
- Support batch processing (every f documents)

## Status

✅ **Complete and Production-Ready**
- All core functionality implemented
- Multiple chunking strategies available
- Robust error handling
- Comprehensive examples
- Full documentation

---

**See Also:**
- [README.md](README.md) - Project overview
- [DESIGN.md](DESIGN.md) - Complete system architecture
- [examples/pdf_processing_example.cpp](examples/pdf_processing_example.cpp) - Usage examples
