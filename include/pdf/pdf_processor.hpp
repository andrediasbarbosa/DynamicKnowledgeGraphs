#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace kg {

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Metadata extracted from a PDF document
 */
struct PDFMetadata {
    std::string title;
    std::string author;
    std::string subject;
    std::string keywords;
    std::string creator;
    std::string producer;
    std::string creation_date;
    std::string modification_date;
    int num_pages = 0;

    std::map<std::string, std::string> to_map() const;
};

/**
 * @brief A chunk of text from a PDF with associated metadata
 */
struct TextChunk {
    std::string text;              ///< The text content
    std::string document_id;       ///< Source document identifier
    std::string chunk_id;          ///< Unique chunk identifier
    int page_number = -1;          ///< Page number (1-indexed)
    int chunk_index = 0;           ///< Chunk index within document
    size_t start_position = 0;     ///< Start position in original text
    size_t end_position = 0;       ///< End position in original text

    // Optional metadata
    std::map<std::string, std::string> metadata;

    /**
     * @brief Create chunk ID from document and index
     */
    static std::string generate_chunk_id(const std::string& doc_id, int index);
};

/**
 * @brief A page of text from a PDF
 */
struct PDFPage {
    int page_number;               ///< Page number (1-indexed)
    std::string text;              ///< Full text content of the page
    size_t char_count = 0;         ///< Number of characters
    size_t word_count = 0;         ///< Number of words

    /**
     * @brief Get word count from text
     */
    void compute_word_count();
};

/**
 * @brief Complete PDF document with all extracted data
 */
struct PDFDocument {
    std::string file_path;         ///< Path to PDF file
    std::string document_id;       ///< Unique document identifier
    PDFMetadata metadata;          ///< Document metadata
    std::vector<PDFPage> pages;    ///< All pages
    std::string full_text;         ///< Concatenated text from all pages

    /**
     * @brief Get page by number (1-indexed)
     */
    std::optional<PDFPage> get_page(int page_number) const;

    /**
     * @brief Get total character count
     */
    size_t get_total_chars() const;

    /**
     * @brief Get total word count
     */
    size_t get_total_words() const;
};

// ============================================================================
// Chunking Strategies
// ============================================================================

/**
 * @brief Abstract base class for text chunking strategies
 */
class ChunkingStrategy {
public:
    virtual ~ChunkingStrategy() = default;

    /**
     * @brief Chunk a PDF document into text chunks
     *
     * @param document The PDF document to chunk
     * @return Vector of text chunks
     */
    virtual std::vector<TextChunk> chunk(const PDFDocument& document) = 0;

    /**
     * @brief Get strategy name
     */
    virtual std::string get_name() const = 0;
};

/**
 * @brief Fixed-size character chunking with overlap
 */
class FixedSizeChunking : public ChunkingStrategy {
public:
    /**
     * @brief Constructor
     *
     * @param chunk_size Number of characters per chunk
     * @param overlap Number of overlapping characters between chunks
     */
    FixedSizeChunking(size_t chunk_size = 1000, size_t overlap = 200);

    std::vector<TextChunk> chunk(const PDFDocument& document) override;
    std::string get_name() const override { return "FixedSize"; }

private:
    size_t chunk_size_;
    size_t overlap_;
};

/**
 * @brief Page-based chunking (one chunk per page)
 */
class PageBasedChunking : public ChunkingStrategy {
public:
    PageBasedChunking() = default;

    std::vector<TextChunk> chunk(const PDFDocument& document) override;
    std::string get_name() const override { return "PageBased"; }
};

/**
 * @brief Paragraph-based chunking with grouping
 */
class ParagraphChunking : public ChunkingStrategy {
public:
    /**
     * @brief Constructor
     *
     * @param max_paragraphs Maximum paragraphs per chunk
     * @param max_chars Maximum characters per chunk
     */
    ParagraphChunking(int max_paragraphs = 5, size_t max_chars = 2000);

    std::vector<TextChunk> chunk(const PDFDocument& document) override;
    std::string get_name() const override { return "Paragraph"; }

private:
    int max_paragraphs_;
    size_t max_chars_;

    /**
     * @brief Split text into paragraphs
     */
    std::vector<std::string> split_paragraphs(const std::string& text) const;
};

/**
 * @brief Sentence-based chunking with grouping
 */
class SentenceChunking : public ChunkingStrategy {
public:
    /**
     * @brief Constructor
     *
     * @param max_sentences Maximum sentences per chunk
     * @param max_chars Maximum characters per chunk
     */
    SentenceChunking(int max_sentences = 10, size_t max_chars = 1500);

    std::vector<TextChunk> chunk(const PDFDocument& document) override;
    std::string get_name() const override { return "Sentence"; }

private:
    int max_sentences_;
    size_t max_chars_;

    /**
     * @brief Split text into sentences
     */
    std::vector<std::string> split_sentences(const std::string& text) const;
};

// ============================================================================
// PDF Processor
// ============================================================================

/**
 * @brief PDF processing and text extraction
 *
 * This class uses Poppler to extract text from PDF files and provides
 * various chunking strategies for downstream processing.
 */
class PDFProcessor {
public:
    PDFProcessor();
    ~PDFProcessor();

    /**
     * @brief Load and process a PDF file
     *
     * @param file_path Path to PDF file
     * @return Processed PDF document
     * @throws std::runtime_error if file cannot be loaded
     */
    PDFDocument load_pdf(const std::string& file_path);

    /**
     * @brief Extract metadata from PDF
     *
     * @param file_path Path to PDF file
     * @return PDF metadata
     * @throws std::runtime_error if file cannot be loaded
     */
    PDFMetadata extract_metadata(const std::string& file_path);

    /**
     * @brief Chunk a document using specified strategy
     *
     * @param document The PDF document
     * @param strategy Chunking strategy to use
     * @return Vector of text chunks
     */
    std::vector<TextChunk> chunk_document(
        const PDFDocument& document,
        ChunkingStrategy& strategy
    );

    /**
     * @brief Load PDF and chunk in one step
     *
     * @param file_path Path to PDF file
     * @param strategy Chunking strategy
     * @return Vector of text chunks
     */
    std::vector<TextChunk> load_and_chunk(
        const std::string& file_path,
        ChunkingStrategy& strategy
    );

    /**
     * @brief Process multiple PDF files
     *
     * @param file_paths Vector of PDF file paths
     * @return Vector of processed documents
     */
    std::vector<PDFDocument> load_multiple(
        const std::vector<std::string>& file_paths
    );

    /**
     * @brief Set verbose output
     */
    void set_verbose(bool verbose) { verbose_ = verbose; }

    /**
     * @brief Check if Poppler support is available
     */
    static bool is_available();

private:
    bool verbose_ = false;

    /**
     * @brief Generate unique document ID from file path
     */
    std::string generate_document_id(const std::string& file_path) const;

    /**
     * @brief Clean extracted text (remove extra whitespace, etc.)
     */
    std::string clean_text(const std::string& raw_text) const;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Find all PDF files in a directory
 *
 * @param directory_path Path to directory
 * @param recursive Search recursively
 * @return Vector of PDF file paths
 */
std::vector<std::string> find_pdf_files(
    const std::string& directory_path,
    bool recursive = false
);

/**
 * @brief Count words in text
 */
size_t count_words(const std::string& text);

/**
 * @brief Sanitize text (remove control characters, normalize whitespace)
 */
std::string sanitize_text(const std::string& text);

} // namespace kg
