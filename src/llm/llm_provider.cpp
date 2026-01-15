#include "llm/llm_provider.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>

using json = nlohmann::json;

namespace kg {

// ============================================================================
// Helper Functions for HTTP Requests
// ============================================================================

namespace {

// CURL write callback
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// Make HTTP POST request with CURL
std::string http_post(
    const std::string& url,
    const std::string& json_payload,
    const std::vector<std::string>& headers,
    int timeout_seconds = 60
) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;
    struct curl_slist* header_list = nullptr;

    // Add headers
    for (const auto& header : headers) {
        header_list = curl_slist_append(header_list, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);

    CURLcode res = curl_easy_perform(curl);

    if (header_list) {
        curl_slist_free_all(header_list);
    }

    if (res != CURLE_OK) {
        std::string error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error("CURL request failed: " + error);
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code < 200 || http_code >= 300) {
        throw std::runtime_error(
            "HTTP request failed with code " + std::to_string(http_code) +
            ": " + response
        );
    }

    return response;
}

} // anonymous namespace

// ============================================================================
// LLMProvider Base Class
// ============================================================================

std::vector<ExtractionResult> LLMProvider::extract_relations_batch(
    const std::vector<std::string>& texts,
    const std::vector<std::string>& chunk_ids,
    const std::string& system_prompt
) {
    std::vector<ExtractionResult> results;

    for (size_t i = 0; i < texts.size(); ++i) {
        std::string chunk_id = (i < chunk_ids.size()) ? chunk_ids[i] : "";
        results.push_back(extract_relations(texts[i], chunk_id, system_prompt));
    }

    return results;
}

std::vector<ExtractedRelation> LLMProvider::parse_extraction_response(
    const std::string& json_response
) const {
    return parse_relations_json(json_response);
}

template<typename Func>
LLMResponse LLMProvider::retry_call(Func&& func, const std::string& operation_name) {
    int attempts = 0;
    while (attempts < config_.max_retries) {
        try {
            return func();
        } catch (const std::exception& e) {
            attempts++;
            if (attempts >= config_.max_retries) {
                LLMResponse error_response;
                error_response.success = false;
                error_response.error_message = std::string("Failed after ") +
                    std::to_string(config_.max_retries) + " attempts: " + e.what();
                return error_response;
            }

            if (config_.verbose) {
                std::cerr << "Attempt " << attempts << " failed for " << operation_name
                          << ": " << e.what() << ". Retrying..." << std::endl;
            }

            // Exponential backoff
            std::this_thread::sleep_for(
                std::chrono::seconds(static_cast<int>(std::pow(2, attempts - 1)))
            );
        }
    }

    LLMResponse error_response;
    error_response.success = false;
    error_response.error_message = "Max retries exceeded";
    return error_response;
}

// ============================================================================
// OpenAI Provider
// ============================================================================

OpenAIProvider::OpenAIProvider(const std::string& api_key, const std::string& model) {
    config_.api_key = api_key;
    config_.model = model;
    config_.api_base_url = "https://api.openai.com/v1";
}

std::string OpenAIProvider::make_request(
    const std::string& endpoint,
    const std::string& json_payload
) {
    std::string url = config_.api_base_url + endpoint;

    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "Authorization: Bearer " + config_.api_key
    };

    return http_post(url, json_payload, headers, config_.timeout_seconds);
}

std::string OpenAIProvider::build_chat_payload(const std::vector<Message>& messages) {
    json j;
    j["model"] = config_.model;
    j["temperature"] = config_.temperature;
    j["max_tokens"] = config_.max_tokens;

    json messages_array = json::array();
    for (const auto& msg : messages) {
        messages_array.push_back({
            {"role", msg.role_string()},
            {"content", msg.content}
        });
    }
    j["messages"] = messages_array;

    return j.dump();
}

LLMResponse OpenAIProvider::parse_response(const std::string& response_json) {
    LLMResponse response;

    try {
        json j = json::parse(response_json);

        if (j.contains("error")) {
            response.success = false;
            response.error_message = j["error"]["message"];
            return response;
        }

        response.content = j["choices"][0]["message"]["content"];
        response.model = j["model"];

        if (j.contains("usage")) {
            response.prompt_tokens = j["usage"]["prompt_tokens"];
            response.completion_tokens = j["usage"]["completion_tokens"];
            response.total_tokens = j["usage"]["total_tokens"];
        }

        response.success = true;

    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = std::string("Failed to parse response: ") + e.what();
    }

    return response;
}

LLMResponse OpenAIProvider::complete(const std::string& prompt) {
    std::vector<Message> messages = {
        Message(Message::Role::User, prompt)
    };
    return chat(messages);
}

LLMResponse OpenAIProvider::chat(const std::vector<Message>& messages) {
    auto start_time = std::chrono::high_resolution_clock::now();

    auto call_api = [&]() -> LLMResponse {
        std::string payload = build_chat_payload(messages);

        if (config_.verbose) {
            std::cout << "OpenAI API Request to " << config_.model << std::endl;
        }

        std::string response_str = make_request("/chat/completions", payload);
        LLMResponse response = parse_response(response_str);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );
        response.latency_ms = duration.count();

        if (config_.verbose && response.success) {
            std::cout << "  Tokens: " << response.total_tokens
                      << " (prompt: " << response.prompt_tokens
                      << ", completion: " << response.completion_tokens << ")" << std::endl;
            std::cout << "  Latency: " << response.latency_ms << " ms" << std::endl;
        }

        return response;
    };

    return retry_call(call_api, "OpenAI chat");
}

ExtractionResult OpenAIProvider::extract_relations(
    const std::string& text,
    const std::string& chunk_id,
    const std::string& system_prompt
) {
    ExtractionResult result;
    result.chunk_id = chunk_id;

    // Build messages
    std::vector<Message> messages;

    if (!system_prompt.empty()) {
        messages.push_back(Message(Message::Role::System, system_prompt));
    } else {
        messages.push_back(Message(
            Message::Role::System,
            PromptTemplates::relation_extraction_system_prompt()
        ));
    }

    messages.push_back(Message(
        Message::Role::User,
        PromptTemplates::relation_extraction_user_prompt(text)
    ));

    // Call API
    LLMResponse llm_response = chat(messages);
    result.llm_response = llm_response;

    if (!llm_response.success) {
        result.success = false;
        result.error_message = llm_response.error_message;
        return result;
    }

    // Parse extracted relations
    try {
        result.relations = parse_extraction_response(llm_response.content);
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Failed to parse relations: ") + e.what();
    }

    return result;
}

// ============================================================================
// Gemini Provider
// ============================================================================

GeminiProvider::GeminiProvider(const std::string& api_key, const std::string& model) {
    config_.api_key = api_key;
    config_.model = model;
    config_.api_base_url = "https://generativelanguage.googleapis.com/v1";
}

std::string GeminiProvider::make_request(
    const std::string& endpoint,
    const std::string& json_payload
) {
    std::string url = config_.api_base_url + endpoint + "?key=" + config_.api_key;

    std::vector<std::string> headers = {
        "Content-Type: application/json"
    };

    return http_post(url, json_payload, headers, config_.timeout_seconds);
}

std::string GeminiProvider::build_gemini_payload(const std::vector<Message>& messages) {
    json j;

    // Gemini uses "contents" array
    json contents = json::array();

    for (const auto& msg : messages) {
        std::string role = (msg.role == Message::Role::User) ? "user" : "model";

        // System messages get prepended to first user message
        if (msg.role == Message::Role::System) {
            continue;  // Handle separately
        }

        contents.push_back({
            {"role", role},
            {"parts", json::array({{{"text", msg.content}}})}
        });
    }

    // Prepend system message to first user message if present
    for (const auto& msg : messages) {
        if (msg.role == Message::Role::System && !contents.empty()) {
            std::string combined = msg.content + "\n\n" +
                contents[0]["parts"][0]["text"].get<std::string>();
            contents[0]["parts"][0]["text"] = combined;
            break;
        }
    }

    j["contents"] = contents;

    // Generation config
    j["generationConfig"] = {
        {"temperature", config_.temperature},
        {"maxOutputTokens", config_.max_tokens}
    };

    return j.dump();
}

LLMResponse GeminiProvider::parse_response(const std::string& response_json) {
    LLMResponse response;

    try {
        json j = json::parse(response_json);

        if (j.contains("error")) {
            response.success = false;
            response.error_message = j["error"]["message"];
            return response;
        }

        if (j.contains("candidates") && !j["candidates"].empty()) {
            response.content = j["candidates"][0]["content"]["parts"][0]["text"];
        }

        response.model = config_.model;

        // Gemini token counts
        if (j.contains("usageMetadata")) {
            response.prompt_tokens = j["usageMetadata"]["promptTokenCount"];
            response.completion_tokens = j["usageMetadata"]["candidatesTokenCount"];
            response.total_tokens = j["usageMetadata"]["totalTokenCount"];
        }

        response.success = true;

    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = std::string("Failed to parse response: ") + e.what();
    }

    return response;
}

LLMResponse GeminiProvider::complete(const std::string& prompt) {
    std::vector<Message> messages = {
        Message(Message::Role::User, prompt)
    };
    return chat(messages);
}

LLMResponse GeminiProvider::chat(const std::vector<Message>& messages) {
    auto start_time = std::chrono::high_resolution_clock::now();

    auto call_api = [&]() -> LLMResponse {
        std::string payload = build_gemini_payload(messages);

        if (config_.verbose) {
            std::cout << "Gemini API Request to " << config_.model << std::endl;
        }

        std::string endpoint = "/models/" + config_.model + ":generateContent";
        std::string response_str = make_request(endpoint, payload);
        LLMResponse response = parse_response(response_str);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );
        response.latency_ms = duration.count();

        if (config_.verbose && response.success) {
            std::cout << "  Tokens: " << response.total_tokens
                      << " (prompt: " << response.prompt_tokens
                      << ", completion: " << response.completion_tokens << ")" << std::endl;
            std::cout << "  Latency: " << response.latency_ms << " ms" << std::endl;
        }

        return response;
    };

    return retry_call(call_api, "Gemini chat");
}

ExtractionResult GeminiProvider::extract_relations(
    const std::string& text,
    const std::string& chunk_id,
    const std::string& system_prompt
) {
    ExtractionResult result;
    result.chunk_id = chunk_id;

    // Build messages
    std::vector<Message> messages;

    if (!system_prompt.empty()) {
        messages.push_back(Message(Message::Role::System, system_prompt));
    } else {
        messages.push_back(Message(
            Message::Role::System,
            PromptTemplates::relation_extraction_system_prompt()
        ));
    }

    messages.push_back(Message(
        Message::Role::User,
        PromptTemplates::relation_extraction_user_prompt(text)
    ));

    // Call API
    LLMResponse llm_response = chat(messages);
    result.llm_response = llm_response;

    if (!llm_response.success) {
        result.success = false;
        result.error_message = llm_response.error_message;
        return result;
    }

    // Parse extracted relations
    try {
        result.relations = parse_extraction_response(llm_response.content);
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Failed to parse relations: ") + e.what();
    }

    return result;
}

// ============================================================================
// LLM Provider Factory
// ============================================================================

std::unique_ptr<LLMProvider> LLMProviderFactory::create(
    ProviderType type,
    const LLMConfig& config
) {
    switch (type) {
        case ProviderType::OpenAI:
            return std::make_unique<OpenAIProvider>(config.api_key, config.model);

        case ProviderType::Gemini:
            return std::make_unique<GeminiProvider>(config.api_key, config.model);

        default:
            throw std::invalid_argument("Unknown provider type");
    }
}

std::unique_ptr<LLMProvider> LLMProviderFactory::create(
    const std::string& provider_name,
    const LLMConfig& config
) {
    std::string name_lower = provider_name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

    if (name_lower == "openai") {
        return create(ProviderType::OpenAI, config);
    } else if (name_lower == "gemini") {
        return create(ProviderType::Gemini, config);
    } else {
        throw std::invalid_argument("Unknown provider name: " + provider_name);
    }
}

std::unique_ptr<LLMProvider> LLMProviderFactory::create_from_env() {
    // Check for provider selection
    std::string provider = get_api_key_from_env("KG_LLM_PROVIDER");
    if (provider.empty()) {
        provider = "openai";  // Default
    }

    LLMConfig config;

    // Get API key based on provider
    if (provider == "openai") {
        config.api_key = get_api_key_from_env("KG_OPENAI_API_KEY");
        if (config.api_key.empty()) {
            config.api_key = get_api_key_from_env("OPENAI_API_KEY");
        }
        config.model = get_api_key_from_env("KG_LLM_MODEL");
        if (config.model.empty()) {
            config.model = "gpt-4";
        }
    } else if (provider == "gemini") {
        config.api_key = get_api_key_from_env("KG_GEMINI_API_KEY");
        if (config.api_key.empty()) {
            config.api_key = get_api_key_from_env("GEMINI_API_KEY");
        }
        config.model = get_api_key_from_env("KG_LLM_MODEL");
        if (config.model.empty()) {
            config.model = "gemini-1.5-flash";
        }
    }

    if (config.api_key.empty()) {
        return nullptr;
    }

    return create(provider, config);
}

std::unique_ptr<LLMProvider> LLMProviderFactory::create_from_config_file(
    const std::string& config_path
) {
    std::vector<std::string> paths_to_try;

    // If specific path provided, try it first
    if (!config_path.empty()) {
        paths_to_try.push_back(config_path);
    }

    // Try standard locations
    paths_to_try.push_back(".llm_config.json");           // Current directory
    paths_to_try.push_back("../.llm_config.json");        // From build/
    paths_to_try.push_back("../../.llm_config.json");     // From build/bin/

    std::string found_path;
    std::ifstream file;

    for (const auto& path : paths_to_try) {
        file.open(path);
        if (file.is_open()) {
            found_path = path;
            break;
        }
    }

    // If no config file found, fall back to environment variables
    if (!file.is_open()) {
        return create_from_env();
    }

    // Parse JSON config
    nlohmann::json config_json;
    try {
        file >> config_json;
        file.close();
    } catch (const std::exception& e) {
        file.close();
        // Fall back to environment if JSON parsing fails
        return create_from_env();
    }

    // Extract configuration
    LLMConfig config;

    if (config_json.contains("provider")) {
        std::string provider = config_json["provider"];

        if (config_json.contains("api_key")) {
            config.api_key = config_json["api_key"];
        }

        if (config_json.contains("model")) {
            config.model = config_json["model"];
        } else {
            // Set default model based on provider
            if (provider == "openai") {
                config.model = "gpt-4";
            } else if (provider == "gemini") {
                config.model = "gemini-1.5-flash";
            }
        }

        if (config_json.contains("temperature")) {
            config.temperature = config_json["temperature"];
        }

        if (config_json.contains("max_tokens")) {
            config.max_tokens = config_json["max_tokens"];
        }

        if (config_json.contains("timeout_seconds")) {
            config.timeout_seconds = config_json["timeout_seconds"];
        }

        if (config_json.contains("max_retries")) {
            config.max_retries = config_json["max_retries"];
        }

        if (config_json.contains("verbose")) {
            config.verbose = config_json["verbose"];
        }

        if (config.api_key.empty()) {
            // No API key in config, fall back to environment
            return create_from_env();
        }

        return create(provider, config);
    }

    // Invalid config format, fall back to environment
    return create_from_env();
}

// ============================================================================
// Prompt Templates
// ============================================================================

std::string PromptTemplates::relation_extraction_system_prompt() {
    return R"(You are an expert at extracting knowledge relations from text.

Your task is to identify entities and the relationships between them.
Extract higher-order relationships where multiple entities can be sources or targets.

Output JSON format:
{
  "relations": [
    {
      "sources": ["entity1", "entity2"],
      "relation": "relationship_type",
      "targets": ["entity3"],
      "confidence": 0.95
    }
  ]
}

Guidelines:
- Extract ALL meaningful relationships, not just the most obvious ones
- Use clear, concise entity names
- Use descriptive relation labels (verbs or verb phrases)
- Set confidence based on how explicit the relationship is
- For multi-entity relationships, keep all sources and targets
- Do not collapse relationships into pairwise connections
)";
}

std::string PromptTemplates::relation_extraction_user_prompt(const std::string& text) {
    return "Extract all knowledge relations from the following text:\n\n" + text +
           "\n\n" + json_format_instructions();
}

std::string PromptTemplates::hypergraph_extraction_system_prompt() {
    return R"(You are an expert at extracting hypergraph structures from scientific text.

Your task is to identify higher-order relationships where edges connect multiple nodes.
Focus on n-ary relationships (n > 2) that cannot be decomposed into pairwise edges.

Output JSON format:
{
  "relations": [
    {
      "sources": ["entity1", "entity2", "entity3"],
      "relation": "relationship_type",
      "targets": ["entity4", "entity5"],
      "confidence": 0.95,
      "properties": {
        "key": "value"
      }
    }
  ]
}

Guidelines:
- Prioritize multi-entity relationships over pairwise ones
- Extract compositional relationships (multiple entities combine to form something)
- Extract process relationships (multiple inputs, multiple outputs)
- Include properties when relevant (measurements, conditions, etc.)
- Set confidence based on relationship explicitness
)";
}

std::string PromptTemplates::hypergraph_extraction_user_prompt(const std::string& text) {
    return "Extract hypergraph relationships from the following text:\n\n" + text +
           "\n\n" + json_format_instructions();
}

std::string PromptTemplates::json_format_instructions() {
    return R"(IMPORTANT:
- Respond ONLY with valid JSON (no markdown, no explanation)
- Keep entity names SHORT (1-4 words max)
- Use simple relation verbs
- Limit to 5-10 most important relations
- Ensure JSON is complete and properly closed with all brackets)";
}

// ============================================================================
// Utility Functions
// ============================================================================

std::vector<ExtractedRelation> parse_relations_json(const std::string& json_str) {
    std::vector<ExtractedRelation> relations;

    try {
        // Remove markdown code blocks if present
        std::string clean_json = json_str;

        // Trim leading/trailing whitespace
        size_t first = clean_json.find_first_not_of(" \t\n\r");
        if (first != std::string::npos) {
            clean_json = clean_json.substr(first);
        }

        // Remove ```json or ``` markers
        if (clean_json.substr(0, 7) == "```json") {
            clean_json = clean_json.substr(7);
        } else if (clean_json.substr(0, 3) == "```") {
            clean_json = clean_json.substr(3);
        }

        // Remove trailing ``` if present
        size_t last_backticks = clean_json.rfind("```");
        if (last_backticks != std::string::npos && last_backticks > 0) {
            clean_json = clean_json.substr(0, last_backticks);
        }

        // Trim again after removing markers
        first = clean_json.find_first_not_of(" \t\n\r");
        if (first != std::string::npos) {
            clean_json = clean_json.substr(first);
        }
        size_t last = clean_json.find_last_not_of(" \t\n\r");
        if (last != std::string::npos) {
            clean_json = clean_json.substr(0, last + 1);
        }

        // Fix malformed JSON: replace unescaped newlines inside strings
        // This handles cases where LLM returns newlines within string values
        std::string fixed_json;
        fixed_json.reserve(clean_json.size());
        bool in_string = false;
        bool escaped = false;

        for (size_t i = 0; i < clean_json.size(); ++i) {
            char c = clean_json[i];

            if (escaped) {
                fixed_json += c;
                escaped = false;
                continue;
            }

            if (c == '\\') {
                fixed_json += c;
                escaped = true;
                continue;
            }

            if (c == '"') {
                in_string = !in_string;
                fixed_json += c;
                continue;
            }

            if (in_string && (c == '\n' || c == '\r')) {
                // Replace unescaped newlines in strings with space
                if (c == '\n') {
                    fixed_json += ' ';
                }
                // Skip \r characters
                continue;
            }

            fixed_json += c;
        }

        json j;
        try {
            j = json::parse(fixed_json);
        } catch (const json::parse_error& e) {
            // Try to fix truncated JSON by adding missing closing brackets
            std::string repaired_json = fixed_json;

            // Count open brackets
            int open_braces = 0, open_brackets = 0;
            for (char c : repaired_json) {
                if (c == '{') open_braces++;
                else if (c == '}') open_braces--;
                else if (c == '[') open_brackets++;
                else if (c == ']') open_brackets--;
            }

            // Add missing closing brackets
            while (open_brackets > 0) {
                repaired_json += ']';
                open_brackets--;
            }
            while (open_braces > 0) {
                repaired_json += '}';
                open_braces--;
            }

            // Try parsing again
            try {
                j = json::parse(repaired_json);
            } catch (...) {
                // If still fails, re-throw original error
                throw;
            }
        }

        if (!j.contains("relations")) {
            return relations;
        }

        for (const auto& rel_json : j["relations"]) {
            ExtractedRelation rel;

            if (rel_json.contains("sources")) {
                for (const auto& src : rel_json["sources"]) {
                    rel.sources.push_back(src);
                }
            }

            if (rel_json.contains("relation")) {
                rel.relation = rel_json["relation"];
            }

            if (rel_json.contains("targets")) {
                for (const auto& tgt : rel_json["targets"]) {
                    rel.targets.push_back(tgt);
                }
            }

            if (rel_json.contains("confidence")) {
                rel.confidence = rel_json["confidence"];
            }

            if (rel_json.contains("properties")) {
                for (auto& [key, value] : rel_json["properties"].items()) {
                    rel.properties[key] = value.dump();
                }
            }

            relations.push_back(rel);
        }

    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Failed to parse relations JSON: ") + e.what()
        );
    }

    return relations;
}

std::string relations_to_json(const std::vector<ExtractedRelation>& relations) {
    json j;
    json relations_array = json::array();

    for (const auto& rel : relations) {
        json rel_json;
        rel_json["sources"] = rel.sources;
        rel_json["relation"] = rel.relation;
        rel_json["targets"] = rel.targets;
        rel_json["confidence"] = rel.confidence;

        if (!rel.properties.empty()) {
            rel_json["properties"] = rel.properties;
        }

        relations_array.push_back(rel_json);
    }

    j["relations"] = relations_array;
    return j.dump(2);
}

std::string get_api_key_from_env(const std::string& env_var_name) {
    const char* value = std::getenv(env_var_name.c_str());
    return value ? std::string(value) : std::string();
}

} // namespace kg
