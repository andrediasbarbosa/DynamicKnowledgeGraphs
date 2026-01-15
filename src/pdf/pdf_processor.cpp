#include "pdf/pdf_processor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cctype>
#include <cmath>

#ifdef HAVE_POPPLER
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#endif

#include <sys/stat.h>
#include <dirent.h>

namespace kg {

// ============================================================================
// PDFMetadata
// ============================================================================

std::map<std::string, std::string> PDFMetadata::to_map() const {
    return {
        {"title", title},
        {"author", author},
        {"subject", subject},
        {"keywords", keywords},
        {"creator", creator},
        {"producer", producer},
        {"creation_date", creation_date},
        {"modification_date", modification_date},
        {"num_pages", std::to_string(num_pages)}
    };
}

// ============================================================================
// TextChunk
// ============================================================================

std::string TextChunk::generate_chunk_id(const std::string& doc_id, int index) {
    return doc_id + "_chunk_" + std::to_string(index);
}

// ============================================================================
// PDFPage
// ============================================================================

void PDFPage::compute_word_count() {
    word_count = count_words(text);
    char_count = text.length();
}

// ============================================================================
// PDFDocument
// ============================================================================

std::optional<PDFPage> PDFDocument::get_page(int page_number) const {
    if (page_number < 1 || page_number > static_cast<int>(pages.size())) {
        return std::nullopt;
    }
    return pages[page_number - 1];
}

size_t PDFDocument::get_total_chars() const {
    return full_text.length();
}

size_t PDFDocument::get_total_words() const {
    size_t total = 0;
    for (const auto& page : pages) {
        total += page.word_count;
    }
    return total;
}

// ============================================================================
// FixedSizeChunking
// ============================================================================

FixedSizeChunking::FixedSizeChunking(size_t chunk_size, size_t overlap)
    : chunk_size_(chunk_size), overlap_(overlap) {
    if (overlap >= chunk_size) {
        throw std::invalid_argument("Overlap must be less than chunk size");
    }
}

std::vector<TextChunk> FixedSizeChunking::chunk(const PDFDocument& document) {
    std::vector<TextChunk> chunks;
    const std::string& text = document.full_text;

    if (text.empty()) {
        return chunks;
    }

    size_t pos = 0;
    int chunk_index = 0;

    while (pos < text.length()) {
        size_t end = std::min(pos + chunk_size_, text.length());

        // Try to break at word boundary
        if (end < text.length()) {
            size_t last_space = text.rfind(' ', end);
            if (last_space != std::string::npos && last_space > pos) {
                end = last_space;
            }
        }

        TextChunk chunk;
        chunk.text = text.substr(pos, end - pos);
        chunk.document_id = document.document_id;
        chunk.chunk_id = TextChunk::generate_chunk_id(document.document_id, chunk_index);
        chunk.chunk_index = chunk_index;
        chunk.start_position = pos;
        chunk.end_position = end;

        // Determine page number (approximate)
        chunk.page_number = -1;
        size_t char_pos = 0;
        for (const auto& page : document.pages) {
            char_pos += page.text.length();
            if (pos < char_pos) {
                chunk.page_number = page.page_number;
                break;
            }
        }

        chunks.push_back(chunk);
        chunk_index++;

        // Move position forward, accounting for overlap
        if (end >= text.length()) {
            break;
        }
        pos = end > overlap_ ? end - overlap_ : end;
    }

    return chunks;
}

// ============================================================================
// PageBasedChunking
// ============================================================================

std::vector<TextChunk> PageBasedChunking::chunk(const PDFDocument& document) {
    std::vector<TextChunk> chunks;

    for (int i = 0; i < static_cast<int>(document.pages.size()); ++i) {
        const auto& page = document.pages[i];

        if (page.text.empty()) {
            continue;
        }

        TextChunk chunk;
        chunk.text = page.text;
        chunk.document_id = document.document_id;
        chunk.chunk_id = TextChunk::generate_chunk_id(document.document_id, i);
        chunk.chunk_index = i;
        chunk.page_number = page.page_number;
        chunk.start_position = 0;
        chunk.end_position = page.text.length();

        chunks.push_back(chunk);
    }

    return chunks;
}

// ============================================================================
// ParagraphChunking
// ============================================================================

ParagraphChunking::ParagraphChunking(int max_paragraphs, size_t max_chars)
    : max_paragraphs_(max_paragraphs), max_chars_(max_chars) {}

std::vector<std::string> ParagraphChunking::split_paragraphs(const std::string& text) const {
    std::vector<std::string> paragraphs;
    std::istringstream stream(text);
    std::string line;
    std::string current_paragraph;

    while (std::getline(stream, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) {
            // Empty line marks paragraph boundary
            if (!current_paragraph.empty()) {
                paragraphs.push_back(current_paragraph);
                current_paragraph.clear();
            }
        } else {
            if (!current_paragraph.empty()) {
                current_paragraph += " ";
            }
            current_paragraph += line;
        }
    }

    // Add final paragraph
    if (!current_paragraph.empty()) {
        paragraphs.push_back(current_paragraph);
    }

    return paragraphs;
}

std::vector<TextChunk> ParagraphChunking::chunk(const PDFDocument& document) {
    std::vector<TextChunk> chunks;
    auto paragraphs = split_paragraphs(document.full_text);

    if (paragraphs.empty()) {
        return chunks;
    }

    int chunk_index = 0;
    std::string current_chunk;
    int paragraph_count = 0;
    size_t start_pos = 0;
    size_t current_pos = 0;

    for (const auto& paragraph : paragraphs) {
        // Check if adding this paragraph would exceed limits
        bool would_exceed = (paragraph_count >= max_paragraphs_) ||
                           (current_chunk.length() + paragraph.length() > max_chars_);

        if (would_exceed && !current_chunk.empty()) {
            // Save current chunk
            TextChunk chunk;
            chunk.text = current_chunk;
            chunk.document_id = document.document_id;
            chunk.chunk_id = TextChunk::generate_chunk_id(document.document_id, chunk_index);
            chunk.chunk_index = chunk_index;
            chunk.start_position = start_pos;
            chunk.end_position = current_pos;
            chunk.page_number = -1;  // Could be improved with position tracking

            chunks.push_back(chunk);
            chunk_index++;

            // Start new chunk
            current_chunk.clear();
            paragraph_count = 0;
            start_pos = current_pos;
        }

        if (!current_chunk.empty()) {
            current_chunk += "\n\n";
        }
        current_chunk += paragraph;
        paragraph_count++;
        current_pos += paragraph.length() + 2;  // +2 for newlines
    }

    // Add final chunk
    if (!current_chunk.empty()) {
        TextChunk chunk;
        chunk.text = current_chunk;
        chunk.document_id = document.document_id;
        chunk.chunk_id = TextChunk::generate_chunk_id(document.document_id, chunk_index);
        chunk.chunk_index = chunk_index;
        chunk.start_position = start_pos;
        chunk.end_position = current_pos;
        chunk.page_number = -1;

        chunks.push_back(chunk);
    }

    return chunks;
}

// ============================================================================
// SentenceChunking
// ============================================================================

SentenceChunking::SentenceChunking(int max_sentences, size_t max_chars)
    : max_sentences_(max_sentences), max_chars_(max_chars) {}

std::vector<std::string> SentenceChunking::split_sentences(const std::string& text) const {
    std::vector<std::string> sentences;

    // Simple sentence splitting (could be improved with NLP)
    std::regex sentence_end(R"([.!?]+\s+)");
    std::sregex_token_iterator iter(text.begin(), text.end(), sentence_end, -1);
    std::sregex_token_iterator end;

    for (; iter != end; ++iter) {
        std::string sentence = iter->str();
        // Trim
        sentence.erase(0, sentence.find_first_not_of(" \t\r\n"));
        sentence.erase(sentence.find_last_not_of(" \t\r\n") + 1);

        if (!sentence.empty()) {
            sentences.push_back(sentence);
        }
    }

    return sentences;
}

std::vector<TextChunk> SentenceChunking::chunk(const PDFDocument& document) {
    std::vector<TextChunk> chunks;
    auto sentences = split_sentences(document.full_text);

    if (sentences.empty()) {
        return chunks;
    }

    int chunk_index = 0;
    std::string current_chunk;
    int sentence_count = 0;
    size_t start_pos = 0;
    size_t current_pos = 0;

    for (const auto& sentence : sentences) {
        bool would_exceed = (sentence_count >= max_sentences_) ||
                           (current_chunk.length() + sentence.length() > max_chars_);

        if (would_exceed && !current_chunk.empty()) {
            TextChunk chunk;
            chunk.text = current_chunk;
            chunk.document_id = document.document_id;
            chunk.chunk_id = TextChunk::generate_chunk_id(document.document_id, chunk_index);
            chunk.chunk_index = chunk_index;
            chunk.start_position = start_pos;
            chunk.end_position = current_pos;
            chunk.page_number = -1;

            chunks.push_back(chunk);
            chunk_index++;

            current_chunk.clear();
            sentence_count = 0;
            start_pos = current_pos;
        }

        if (!current_chunk.empty()) {
            current_chunk += " ";
        }
        current_chunk += sentence;
        sentence_count++;
        current_pos += sentence.length() + 1;
    }

    if (!current_chunk.empty()) {
        TextChunk chunk;
        chunk.text = current_chunk;
        chunk.document_id = document.document_id;
        chunk.chunk_id = TextChunk::generate_chunk_id(document.document_id, chunk_index);
        chunk.chunk_index = chunk_index;
        chunk.start_position = start_pos;
        chunk.end_position = current_pos;
        chunk.page_number = -1;

        chunks.push_back(chunk);
    }

    return chunks;
}

// ============================================================================
// PDFProcessor
// ============================================================================

PDFProcessor::PDFProcessor() {}

PDFProcessor::~PDFProcessor() {}

bool PDFProcessor::is_available() {
#ifdef HAVE_POPPLER
    return true;
#else
    return false;
#endif
}

std::string PDFProcessor::generate_document_id(const std::string& file_path) const {
    // Extract filename without extension
    size_t last_slash = file_path.find_last_of("/\\");
    size_t last_dot = file_path.find_last_of('.');

    std::string filename = file_path;
    if (last_slash != std::string::npos) {
        filename = file_path.substr(last_slash + 1);
    }
    if (last_dot != std::string::npos && last_dot > last_slash) {
        filename = filename.substr(0, last_dot - (last_slash + 1));
    }

    // Sanitize: replace non-alphanumeric with underscore
    std::string doc_id;
    for (char c : filename) {
        if (std::isalnum(c) || c == '_' || c == '-') {
            doc_id += c;
        } else {
            doc_id += '_';
        }
    }

    return doc_id;
}

std::string PDFProcessor::clean_text(const std::string& raw_text) const {
    return sanitize_text(raw_text);
}

PDFDocument PDFProcessor::load_pdf(const std::string& file_path) {
#ifndef HAVE_POPPLER
    throw std::runtime_error("Poppler support not available. Rebuild with Poppler.");
#else
    if (verbose_) {
        std::cout << "Loading PDF: " << file_path << std::endl;
    }

    // Load PDF document
    std::unique_ptr<poppler::document> doc(
        poppler::document::load_from_file(file_path)
    );

    if (!doc) {
        throw std::runtime_error("Failed to load PDF: " + file_path);
    }

    if (doc->is_locked()) {
        throw std::runtime_error("PDF is password protected: " + file_path);
    }

    PDFDocument pdf_doc;
    pdf_doc.file_path = file_path;
    pdf_doc.document_id = generate_document_id(file_path);

    // Extract metadata
    pdf_doc.metadata.num_pages = doc->pages();

    // Extract metadata strings
    auto get_info = [&](const std::string& key) -> std::string {
        poppler::ustring info = doc->info_key(key);
        std::vector<char> utf8_data = info.to_utf8();
        return std::string(utf8_data.begin(), utf8_data.end());
    };

    pdf_doc.metadata.title = get_info("Title");
    pdf_doc.metadata.author = get_info("Author");
    pdf_doc.metadata.subject = get_info("Subject");
    pdf_doc.metadata.keywords = get_info("Keywords");
    pdf_doc.metadata.creator = get_info("Creator");
    pdf_doc.metadata.producer = get_info("Producer");
    pdf_doc.metadata.creation_date = get_info("CreationDate");
    pdf_doc.metadata.modification_date = get_info("ModDate");

    if (verbose_) {
        std::cout << "  Pages: " << pdf_doc.metadata.num_pages << std::endl;
        if (!pdf_doc.metadata.title.empty()) {
            std::cout << "  Title: " << pdf_doc.metadata.title << std::endl;
        }
    }

    // Extract text from each page
    std::ostringstream full_text_stream;

    for (int i = 0; i < doc->pages(); ++i) {
        std::unique_ptr<poppler::page> page(doc->create_page(i));
        if (!page) {
            continue;
        }

        poppler::ustring page_text = page->text();
        std::vector<char> utf8_data = page_text.to_utf8();
        std::string text_utf8(utf8_data.begin(), utf8_data.end());

        PDFPage pdf_page;
        pdf_page.page_number = i + 1;  // 1-indexed
        pdf_page.text = clean_text(text_utf8);
        pdf_page.compute_word_count();

        pdf_doc.pages.push_back(pdf_page);
        full_text_stream << pdf_page.text << "\n\n";

        if (verbose_) {
            std::cout << "  Page " << (i + 1) << ": "
                      << pdf_page.char_count << " chars, "
                      << pdf_page.word_count << " words" << std::endl;
        }
    }

    pdf_doc.full_text = full_text_stream.str();

    if (verbose_) {
        std::cout << "  Total: " << pdf_doc.get_total_chars() << " chars, "
                  << pdf_doc.get_total_words() << " words" << std::endl;
    }

    return pdf_doc;
#endif
}

PDFMetadata PDFProcessor::extract_metadata(const std::string& file_path) {
    PDFDocument doc = load_pdf(file_path);
    return doc.metadata;
}

std::vector<TextChunk> PDFProcessor::chunk_document(
    const PDFDocument& document,
    ChunkingStrategy& strategy
) {
    if (verbose_) {
        std::cout << "Chunking document: " << document.document_id
                  << " using " << strategy.get_name() << " strategy" << std::endl;
    }

    auto chunks = strategy.chunk(document);

    if (verbose_) {
        std::cout << "  Created " << chunks.size() << " chunks" << std::endl;
    }

    return chunks;
}

std::vector<TextChunk> PDFProcessor::load_and_chunk(
    const std::string& file_path,
    ChunkingStrategy& strategy
) {
    PDFDocument doc = load_pdf(file_path);
    return chunk_document(doc, strategy);
}

std::vector<PDFDocument> PDFProcessor::load_multiple(
    const std::vector<std::string>& file_paths
) {
    std::vector<PDFDocument> documents;

    for (const auto& path : file_paths) {
        try {
            documents.push_back(load_pdf(path));
        } catch (const std::exception& e) {
            std::cerr << "Error loading " << path << ": " << e.what() << std::endl;
        }
    }

    return documents;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::vector<std::string> find_pdf_files(
    const std::string& directory_path,
    bool recursive
) {
    std::vector<std::string> pdf_files;

    DIR* dir = opendir(directory_path.c_str());
    if (!dir) {
        return pdf_files;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;

        if (name == "." || name == "..") {
            continue;
        }

        std::string full_path = directory_path + "/" + name;

        // Check if it's a directory
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (recursive) {
                    auto sub_files = find_pdf_files(full_path, true);
                    pdf_files.insert(pdf_files.end(), sub_files.begin(), sub_files.end());
                }
            } else if (S_ISREG(st.st_mode)) {
                // Check if it's a PDF file
                if (name.length() >= 4 &&
                    name.substr(name.length() - 4) == ".pdf") {
                    pdf_files.push_back(full_path);
                }
            }
        }
    }

    closedir(dir);
    std::sort(pdf_files.begin(), pdf_files.end());

    return pdf_files;
}

size_t count_words(const std::string& text) {
    if (text.empty()) {
        return 0;
    }

    size_t count = 0;
    bool in_word = false;

    for (char c : text) {
        if (std::isspace(c)) {
            if (in_word) {
                count++;
                in_word = false;
            }
        } else {
            in_word = true;
        }
    }

    if (in_word) {
        count++;
    }

    return count;
}

std::string sanitize_text(const std::string& text) {
    std::string result;
    result.reserve(text.length());

    bool last_was_space = false;

    for (char c : text) {
        // Remove control characters except newlines and tabs
        if (std::iscntrl(c) && c != '\n' && c != '\t') {
            continue;
        }

        // Normalize whitespace
        if (std::isspace(c)) {
            if (!last_was_space) {
                result += ' ';
                last_was_space = true;
            }
        } else {
            result += c;
            last_was_space = false;
        }
    }

    // Trim trailing whitespace
    while (!result.empty() && std::isspace(result.back())) {
        result.pop_back();
    }

    return result;
}

} // namespace kg
