#include "pdf/pdf_processor.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

using namespace kg;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(70, '=') << "\n\n";
}

void print_chunk_preview(const TextChunk& chunk, int max_length = 150) {
    std::string preview = chunk.text;
    if (preview.length() > static_cast<size_t>(max_length)) {
        preview = preview.substr(0, max_length) + "...";
    }
    // Replace newlines with spaces for display
    for (char& c : preview) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    std::cout << "    \"" << preview << "\"" << std::endl;
}

int main() {
    print_separator("PDF Processing Example");

    // Create output directory
    const std::string output_dir = "output_json";
    #ifdef _WIN32
        _mkdir(output_dir.c_str());
    #else
        mkdir(output_dir.c_str(), 0755);
    #endif

    // Check if Poppler is available
    if (!PDFProcessor::is_available()) {
        std::cerr << "Error: Poppler support not available.\n";
        std::cerr << "Please rebuild with Poppler: apt-get install libpoppler-cpp-dev\n";
        return 1;
    }

    std::cout << "Poppler support: ✓ Available\n";

    // Initialize PDF processor
    PDFProcessor processor;
    processor.set_verbose(true);

    // =========================================================================
    // Example 1: Load and Process Single PDF
    // =========================================================================

    print_separator("Example 1: Loading PDF Document");

    std::string pdf_path = "../../tests/1page.pdf";
    std::cout << "Loading: " << pdf_path << "\n\n";

    try {
        PDFDocument doc = processor.load_pdf(pdf_path);

        std::cout << "\nDocument Information:\n";
        std::cout << "  Document ID: " << doc.document_id << "\n";
        std::cout << "  File Path: " << doc.file_path << "\n";
        std::cout << "  Pages: " << doc.metadata.num_pages << "\n";
        std::cout << "  Total Characters: " << doc.get_total_chars() << "\n";
        std::cout << "  Total Words: " << doc.get_total_words() << "\n";

        if (!doc.metadata.title.empty()) {
            std::cout << "  Title: " << doc.metadata.title << "\n";
        }
        if (!doc.metadata.author.empty()) {
            std::cout << "  Author: " << doc.metadata.author << "\n";
        }

        std::cout << "\nPage Statistics:\n";
        for (const auto& page : doc.pages) {
            std::cout << "  Page " << page.page_number << ": "
                      << page.char_count << " chars, "
                      << page.word_count << " words\n";
        }

        // =========================================================================
        // Example 2: Fixed-Size Chunking
        // =========================================================================

        print_separator("Example 2: Fixed-Size Chunking Strategy");

        FixedSizeChunking fixed_chunker(500, 100);  // 500 chars, 100 char overlap
        std::cout << "Strategy: Fixed-size with 500 chars per chunk, 100 char overlap\n\n";

        auto fixed_chunks = processor.chunk_document(doc, fixed_chunker);

        std::cout << "\nCreated " << fixed_chunks.size() << " chunks:\n\n";

        for (size_t i = 0; i < std::min(size_t(3), fixed_chunks.size()); ++i) {
            const auto& chunk = fixed_chunks[i];
            std::cout << "Chunk " << (i + 1) << ":\n";
            std::cout << "  ID: " << chunk.chunk_id << "\n";
            std::cout << "  Length: " << chunk.text.length() << " chars\n";
            std::cout << "  Position: " << chunk.start_position << " - "
                      << chunk.end_position << "\n";
            std::cout << "  Page: " << chunk.page_number << "\n";
            std::cout << "  Preview:\n";
            print_chunk_preview(chunk);
            std::cout << "\n";
        }

        if (fixed_chunks.size() > 3) {
            std::cout << "... and " << (fixed_chunks.size() - 3) << " more chunks\n";
        }

        // =========================================================================
        // Example 3: Page-Based Chunking
        // =========================================================================

        print_separator("Example 3: Page-Based Chunking Strategy");

        PageBasedChunking page_chunker;
        std::cout << "Strategy: One chunk per page\n\n";

        auto page_chunks = processor.chunk_document(doc, page_chunker);

        std::cout << "\nCreated " << page_chunks.size() << " chunks (one per page):\n\n";

        for (const auto& chunk : page_chunks) {
            std::cout << "Chunk from Page " << chunk.page_number << ":\n";
            std::cout << "  ID: " << chunk.chunk_id << "\n";
            std::cout << "  Length: " << chunk.text.length() << " chars\n";
            std::cout << "  Preview:\n";
            print_chunk_preview(chunk, 200);
            std::cout << "\n";
        }

        // =========================================================================
        // Example 4: Paragraph-Based Chunking
        // =========================================================================

        print_separator("Example 4: Paragraph-Based Chunking Strategy");

        ParagraphChunking para_chunker(3, 800);  // Max 3 paragraphs or 800 chars
        std::cout << "Strategy: Group up to 3 paragraphs or 800 chars per chunk\n\n";

        auto para_chunks = processor.chunk_document(doc, para_chunker);

        std::cout << "\nCreated " << para_chunks.size() << " chunks:\n\n";

        for (size_t i = 0; i < std::min(size_t(3), para_chunks.size()); ++i) {
            const auto& chunk = para_chunks[i];
            std::cout << "Chunk " << (i + 1) << ":\n";
            std::cout << "  ID: " << chunk.chunk_id << "\n";
            std::cout << "  Length: " << chunk.text.length() << " chars\n";
            std::cout << "  Preview:\n";
            print_chunk_preview(chunk, 200);
            std::cout << "\n";
        }

        // =========================================================================
        // Example 5: Sentence-Based Chunking
        // =========================================================================

        print_separator("Example 5: Sentence-Based Chunking Strategy");

        SentenceChunking sent_chunker(5, 600);  // Max 5 sentences or 600 chars
        std::cout << "Strategy: Group up to 5 sentences or 600 chars per chunk\n\n";

        auto sent_chunks = processor.chunk_document(doc, sent_chunker);

        std::cout << "\nCreated " << sent_chunks.size() << " chunks:\n\n";

        for (size_t i = 0; i < std::min(size_t(3), sent_chunks.size()); ++i) {
            const auto& chunk = sent_chunks[i];
            std::cout << "Chunk " << (i + 1) << ":\n";
            std::cout << "  ID: " << chunk.chunk_id << "\n";
            std::cout << "  Length: " << chunk.text.length() << " chars\n";
            std::cout << "  Preview:\n";
            print_chunk_preview(chunk, 200);
            std::cout << "\n";
        }

        // =========================================================================
        // Example 6: Export Chunks to JSON
        // =========================================================================

        print_separator("Example 6: Exporting Chunks to JSON");

        // Export fixed-size chunks
        {
            std::ofstream file(output_dir + "/chunks_fixed_size.json");
            file << "{\n  \"document_id\": \"" << doc.document_id << "\",\n";
            file << "  \"strategy\": \"FixedSize\",\n";
            file << "  \"chunk_count\": " << fixed_chunks.size() << ",\n";
            file << "  \"chunks\": [\n";

            for (size_t i = 0; i < fixed_chunks.size(); ++i) {
                const auto& chunk = fixed_chunks[i];
                file << "    {\n";
                file << "      \"chunk_id\": \"" << chunk.chunk_id << "\",\n";
                file << "      \"chunk_index\": " << chunk.chunk_index << ",\n";
                file << "      \"page_number\": " << chunk.page_number << ",\n";
                file << "      \"length\": " << chunk.text.length() << ",\n";
                file << "      \"start_pos\": " << chunk.start_position << ",\n";
                file << "      \"end_pos\": " << chunk.end_position << ",\n";
                // Escape text for JSON
                file << "      \"text\": \"";
                for (char c : chunk.text) {
                    if (c == '"') file << "\\\"";
                    else if (c == '\\') file << "\\\\";
                    else if (c == '\n') file << "\\n";
                    else if (c == '\r') file << "\\r";
                    else if (c == '\t') file << "\\t";
                    else file << c;
                }
                file << "\"\n";
                file << "    }";
                if (i < fixed_chunks.size() - 1) file << ",";
                file << "\n";
            }

            file << "  ]\n}\n";
            file.close();
            std::cout << "✓ Exported fixed-size chunks to: " << output_dir << "/chunks_fixed_size.json\n";
        }

        // Export page-based chunks
        {
            std::ofstream file(output_dir + "/chunks_page_based.json");
            file << "{\n  \"document_id\": \"" << doc.document_id << "\",\n";
            file << "  \"strategy\": \"PageBased\",\n";
            file << "  \"chunk_count\": " << page_chunks.size() << ",\n";
            file << "  \"chunks\": [\n";

            for (size_t i = 0; i < page_chunks.size(); ++i) {
                const auto& chunk = page_chunks[i];
                file << "    {\n";
                file << "      \"chunk_id\": \"" << chunk.chunk_id << "\",\n";
                file << "      \"page_number\": " << chunk.page_number << ",\n";
                file << "      \"length\": " << chunk.text.length() << "\n";
                file << "    }";
                if (i < page_chunks.size() - 1) file << ",";
                file << "\n";
            }

            file << "  ]\n}\n";
            file.close();
            std::cout << "✓ Exported page-based chunks to: " << output_dir << "/chunks_page_based.json\n";
        }

        // =========================================================================
        // Example 7: Find All PDFs in Directory
        // =========================================================================

        print_separator("Example 7: Batch Processing PDFs");

        std::cout << "Finding all PDFs in tests/ directory...\n\n";
        auto pdf_files = find_pdf_files("tests", false);

        std::cout << "Found " << pdf_files.size() << " PDF file(s):\n";
        for (const auto& path : pdf_files) {
            std::cout << "  - " << path << "\n";
        }

        if (pdf_files.size() > 1) {
            std::cout << "\nBatch processing all PDFs...\n";
            processor.set_verbose(false);  // Reduce verbosity for batch

            auto documents = processor.load_multiple(pdf_files);

            std::cout << "\nSuccessfully loaded " << documents.size() << " documents:\n";
            for (const auto& d : documents) {
                std::cout << "  - " << d.document_id << " ("
                          << d.metadata.num_pages << " pages, "
                          << d.get_total_words() << " words)\n";
            }
        }

        // =========================================================================
        // Summary
        // =========================================================================

        print_separator("Summary");

        std::cout << "This example demonstrated:\n";
        std::cout << "  ✓ Loading PDF documents with Poppler\n";
        std::cout << "  ✓ Extracting text and metadata\n";
        std::cout << "  ✓ Fixed-size chunking with overlap\n";
        std::cout << "  ✓ Page-based chunking\n";
        std::cout << "  ✓ Paragraph-based chunking\n";
        std::cout << "  ✓ Sentence-based chunking\n";
        std::cout << "  ✓ Exporting chunks to JSON\n";
        std::cout << "  ✓ Batch processing multiple PDFs\n";

        std::cout << "\nChunking strategies comparison:\n";
        std::cout << "  Fixed-size:   " << fixed_chunks.size() << " chunks (uniform size)\n";
        std::cout << "  Page-based:   " << page_chunks.size() << " chunks (natural boundaries)\n";
        std::cout << "  Paragraph:    " << para_chunks.size() << " chunks (semantic units)\n";
        std::cout << "  Sentence:     " << sent_chunks.size() << " chunks (granular)\n";

        std::cout << "\nNext steps:\n";
        std::cout << "  1. Integrate with LLM providers\n";
        std::cout << "  2. Extract knowledge graphs from chunks\n";
        std::cout << "  3. Build complete extraction pipeline\n";

        print_separator("End of PDF Processing Example");

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
