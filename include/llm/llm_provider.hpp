#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <functional>

namespace kg {

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Configuration for LLM provider
 */
struct LLMConfig {
    std::string api_key;                    ///< API key for authentication
    std::string model;                      ///< Model name/ID
    std::string api_base_url;               ///< Base URL for API (optional)
    double temperature = 0.0;               ///< Sampling temperature (0.0-1.0)
    int max_tokens = 2000;                  ///< Maximum tokens in response
    int timeout_seconds = 60;               ///< Request timeout
    int max_retries = 3;                    ///< Max retry attempts on failure
    bool verbose = false;                   ///< Enable verbose logging

    // Additional parameters
    std::map<std::string, std::string> extra_params;
};

/**
 * @brief Message in a conversation
 */
struct Message {
    enum class Role {
        System,
        User,
        Assistant
    };

    Role role;
    std::string content;

    Message(Role r, const std::string& c) : role(r), content(c) {}

    std::string role_string() const {
        switch (role) {
            case Role::System: return "system";
            case Role::User: return "user";
            case Role::Assistant: return "assistant";
            default: return "user";
        }
    }
};

/**
 * @brief Response from LLM
 */
struct LLMResponse {
    std::string content;                    ///< Generated text
    std::string model;                      ///< Model that generated response
    int prompt_tokens = 0;                  ///< Tokens in prompt
    int completion_tokens = 0;              ///< Tokens in completion
    int total_tokens = 0;                   ///< Total tokens used
    double latency_ms = 0.0;                ///< Response latency
    bool success = false;                   ///< Whether request succeeded
    std::string error_message;              ///< Error message if failed

    // Additional response metadata
    std::map<std::string, std::string> metadata;
};

/**
 * @brief Extracted relation from text
 */
struct ExtractedRelation {
    std::vector<std::string> sources;       ///< Source entities
    std::string relation;                   ///< Relation type/label
    std::vector<std::string> targets;       ///< Target entities
    double confidence = 1.0;                ///< Confidence score (0.0-1.0)
    std::string source_text;                ///< Original text snippet
    std::map<std::string, std::string> properties;  ///< Additional properties
};

/**
 * @brief Result of extraction from text chunk
 */
struct ExtractionResult {
    std::string chunk_id;                   ///< Source chunk identifier
    std::vector<ExtractedRelation> relations; ///< Extracted relations
    LLMResponse llm_response;               ///< Raw LLM response
    bool success = false;                   ///< Whether extraction succeeded
    std::string error_message;              ///< Error message if failed
};

// ============================================================================
// LLM Provider Interface
// ============================================================================

/**
 * @brief Abstract base class for LLM providers
 *
 * This interface allows swapping between different LLM providers
 * (OpenAI, Gemini, etc.) while maintaining a consistent API.
 */
class LLMProvider {
public:
    virtual ~LLMProvider() = default;

    /**
     * @brief Complete a single prompt
     *
     * @param prompt The prompt text
     * @return LLM response
     */
    virtual LLMResponse complete(const std::string& prompt) = 0;

    /**
     * @brief Chat completion with message history
     *
     * @param messages Conversation messages
     * @return LLM response
     */
    virtual LLMResponse chat(const std::vector<Message>& messages) = 0;

    /**
     * @brief Extract relations from text
     *
     * @param text Input text to extract from
     * @param chunk_id Identifier for the text chunk
     * @param system_prompt Optional system prompt override
     * @return Extraction result
     */
    virtual ExtractionResult extract_relations(
        const std::string& text,
        const std::string& chunk_id = "",
        const std::string& system_prompt = ""
    ) = 0;

    /**
     * @brief Batch extraction from multiple texts
     *
     * @param texts Vector of input texts
     * @param chunk_ids Identifiers for each chunk
     * @param system_prompt Optional system prompt override
     * @return Vector of extraction results
     */
    virtual std::vector<ExtractionResult> extract_relations_batch(
        const std::vector<std::string>& texts,
        const std::vector<std::string>& chunk_ids = {},
        const std::string& system_prompt = ""
    );

    /**
     * @brief Get provider name
     */
    virtual std::string get_provider_name() const = 0;

    /**
     * @brief Get current model
     */
    virtual std::string get_model() const = 0;

    /**
     * @brief Check if provider is configured correctly
     */
    virtual bool is_configured() const = 0;

    /**
     * @brief Set configuration
     */
    virtual void set_config(const LLMConfig& config) = 0;

    /**
     * @brief Get current configuration
     */
    virtual LLMConfig get_config() const = 0;

protected:
    LLMConfig config_;

    /**
     * @brief Parse JSON response into extracted relations
     */
    std::vector<ExtractedRelation> parse_extraction_response(
        const std::string& json_response
    ) const;

    /**
     * @brief Retry logic for API calls
     */
    template<typename Func>
    LLMResponse retry_call(Func&& func, const std::string& operation_name);
};

// ============================================================================
// OpenAI Provider
// ============================================================================

/**
 * @brief OpenAI API provider (GPT-4, GPT-3.5, etc.)
 */
class OpenAIProvider : public LLMProvider {
public:
    /**
     * @brief Constructor
     *
     * @param api_key OpenAI API key
     * @param model Model name (default: gpt-4)
     */
    explicit OpenAIProvider(
        const std::string& api_key,
        const std::string& model = "gpt-4"
    );

    LLMResponse complete(const std::string& prompt) override;
    LLMResponse chat(const std::vector<Message>& messages) override;

    ExtractionResult extract_relations(
        const std::string& text,
        const std::string& chunk_id = "",
        const std::string& system_prompt = ""
    ) override;

    std::string get_provider_name() const override { return "OpenAI"; }
    std::string get_model() const override { return config_.model; }
    bool is_configured() const override { return !config_.api_key.empty(); }
    void set_config(const LLMConfig& config) override { config_ = config; }
    LLMConfig get_config() const override { return config_; }

private:
    /**
     * @brief Make HTTP POST request to OpenAI API
     */
    std::string make_request(
        const std::string& endpoint,
        const std::string& json_payload
    );

    /**
     * @brief Build JSON payload for chat completion
     */
    std::string build_chat_payload(const std::vector<Message>& messages);

    /**
     * @brief Parse OpenAI API response
     */
    LLMResponse parse_response(const std::string& response_json);
};

// ============================================================================
// Gemini Provider
// ============================================================================

/**
 * @brief Google Gemini API provider
 */
class GeminiProvider : public LLMProvider {
public:
    /**
     * @brief Constructor
     *
     * @param api_key Gemini API key
     * @param model Model name (default: gemini-1.5-flash)
     */
    explicit GeminiProvider(
        const std::string& api_key,
        const std::string& model = "gemini-1.5-flash"
    );

    LLMResponse complete(const std::string& prompt) override;
    LLMResponse chat(const std::vector<Message>& messages) override;

    ExtractionResult extract_relations(
        const std::string& text,
        const std::string& chunk_id = "",
        const std::string& system_prompt = ""
    ) override;

    std::string get_provider_name() const override { return "Gemini"; }
    std::string get_model() const override { return config_.model; }
    bool is_configured() const override { return !config_.api_key.empty(); }
    void set_config(const LLMConfig& config) override { config_ = config; }
    LLMConfig get_config() const override { return config_; }

private:
    /**
     * @brief Make HTTP POST request to Gemini API
     */
    std::string make_request(
        const std::string& endpoint,
        const std::string& json_payload
    );

    /**
     * @brief Build JSON payload for Gemini API
     */
    std::string build_gemini_payload(const std::vector<Message>& messages);

    /**
     * @brief Parse Gemini API response
     */
    LLMResponse parse_response(const std::string& response_json);
};

// ============================================================================
// LLM Provider Factory
// ============================================================================

/**
 * @brief Factory for creating LLM providers
 */
class LLMProviderFactory {
public:
    enum class ProviderType {
        OpenAI,
        Gemini
    };

    /**
     * @brief Create LLM provider from type
     *
     * @param type Provider type
     * @param config Configuration
     * @return Unique pointer to provider
     */
    static std::unique_ptr<LLMProvider> create(
        ProviderType type,
        const LLMConfig& config
    );

    /**
     * @brief Create provider from string name
     *
     * @param provider_name "openai" or "gemini"
     * @param config Configuration
     * @return Unique pointer to provider
     */
    static std::unique_ptr<LLMProvider> create(
        const std::string& provider_name,
        const LLMConfig& config
    );

    /**
     * @brief Create provider from environment variables
     *
     * Looks for:
     * - KG_LLM_PROVIDER (openai/gemini)
     * - KG_OPENAI_API_KEY or OPENAI_API_KEY
     * - KG_GEMINI_API_KEY or GEMINI_API_KEY
     * - KG_LLM_MODEL (optional)
     *
     * @return Unique pointer to provider, or nullptr if not configured
     */
    static std::unique_ptr<LLMProvider> create_from_env();

    /**
     * @brief Create provider from JSON config file
     *
     * Tries multiple locations in order:
     * 1. Provided config_path
     * 2. .llm_config.json (in current directory)
     * 3. ../.llm_config.json (from build/)
     * 4. ../../.llm_config.json (from build/bin/)
     * 5. Environment variables (fallback)
     *
     * Config file format:
     * {
     *   "provider": "openai" or "gemini",
     *   "api_key": "your-key",
     *   "model": "gpt-4" or "gemini-1.5-flash",
     *   "temperature": 0.0,
     *   "max_tokens": 2000
     * }
     *
     * @param config_path Optional path to config file
     * @return Unique pointer to provider, or nullptr if not configured
     */
    static std::unique_ptr<LLMProvider> create_from_config_file(
        const std::string& config_path = ""
    );
};

// ============================================================================
// Prompt Templates
// ============================================================================

/**
 * @brief Prompt templates for knowledge extraction
 */
class PromptTemplates {
public:
    /**
     * @brief System prompt for relation extraction
     */
    static std::string relation_extraction_system_prompt();

    /**
     * @brief User prompt for relation extraction
     */
    static std::string relation_extraction_user_prompt(const std::string& text);

    /**
     * @brief System prompt for hypergraph extraction (dual-pass)
     */
    static std::string hypergraph_extraction_system_prompt();

    /**
     * @brief User prompt for hypergraph extraction
     */
    static std::string hypergraph_extraction_user_prompt(const std::string& text);

    /**
     * @brief Format instructions for JSON output
     */
    static std::string json_format_instructions();
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Parse JSON string into extracted relations
 */
std::vector<ExtractedRelation> parse_relations_json(const std::string& json_str);

/**
 * @brief Convert extracted relations to JSON string
 */
std::string relations_to_json(const std::vector<ExtractedRelation>& relations);

/**
 * @brief Load API key from environment variable
 */
std::string get_api_key_from_env(const std::string& env_var_name);

} // namespace kg
