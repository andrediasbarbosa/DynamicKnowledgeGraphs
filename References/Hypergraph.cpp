#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <sstream>
#include <map>
#include <set>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <poppler-document.h>
#include <poppler-page.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ==========================================
// 1. Hypergraph Data Structure
// ==========================================
struct HyperEdge {
    std::vector<std::string> sources;
    std::string relation;
    std::vector<std::string> targets;
};

class Hypergraph {
public:
    void add_edge(const std::vector<std::string>& sources, 
                  const std::string& relation, 
                  const std::vector<std::string>& targets) {
        edges.push_back({sources, relation, targets});
        for (const auto& node : sources) nodes.insert(node);
        for (const auto& node : targets) nodes.insert(node);
    }

    // Exports to Graphviz DOT format
    // For hypergraphs, we represent the "Relation" as a diamond node
    // connecting source nodes to target nodes.
    void save_to_dot(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) return;

        file << "digraph G {\n";
        file << "  rankdir=LR;\n";
        file << "  node [shape=ellipse, style=filled, color=lightblue];\n";

        int rel_counter = 0;
        for (const auto& edge : edges) {
            std::string rel_node_id = "rel_" + std::to_string(rel_counter++);
            
            // Draw relation node (diamond shape)
            file << "  " << rel_node_id << " [shape=diamond, style=filled, color=orange, label=\"" << edge.relation << "\"];\n";

            // Draw edges from Sources -> Relation Node
            for (const auto& src : edge.sources) {
                file << "  \"" << src << "\" -> " << rel_node_id << ";\n";
            }

            // Draw edges from Relation Node -> Targets
            for (const auto& tgt : edge.targets) {
                file << "  " << rel_node_id << " -> \"" << tgt << "\";\n";
            }
        }
        file << "}\n";
        file.close();
        std::cout << "[+] Hypergraph saved to " << filename << std::endl;
    }

private:
    std::set<std::string> nodes;
    std::vector<HyperEdge> edges;
};

// ==========================================
// 2. PDF Extraction (poppler-cpp)
// ==========================================
class PDFExtractor {
public:
    static std::string extract_text(const std::string& file_path) {
        std::unique_ptr<poppler::document> doc(poppler::document::load_from_file(file_path));
        if (!doc) {
            std::cerr << "[-] Failed to load PDF: " << file_path << std::endl;
            return "";
        }

        std::string full_text;
        int pages = doc->pages();
        for (int i = 0; i < pages; ++i) {
            std::unique_ptr<poppler::page> p(doc->create_page(i));
            if (p) {
                // Determine layout analysis implies physical layout
                std::vector<char> txt = p->text().to_utf8(); 
                full_text += std::string(txt.begin(), txt.end()) + "\n";
            }
        }
        return full_text;
    }
};

// ==========================================
// 3. LLM Client (OpenAI & Gemini)
// ==========================================
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class LLMClient {
protected:
    std::string api_key;
    std::string model;
    
    std::string perform_request(const std::string& url, const json& payload, const std::vector<std::string>& headers_list) {
        CURL* curl;
        CURLcode res;
        std::string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            struct curl_slist* headers = NULL;
            for (const auto& h : headers_list) {
                headers = curl_slist_append(headers, h.c_str());
            }

            std::string json_str = payload.dump();

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            }

            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }
        return readBuffer;
    }

public:
    LLMClient(std::string key, std::string mdl) : api_key(key), model(mdl) {}
    virtual ~LLMClient() = default;
    virtual std::string extract_relations(const std::string& text) = 0;
};

class OpenAIClient : public LLMClient {
public:
    OpenAIClient(std::string key, std::string mdl = "gpt-4o") : LLMClient(key, mdl) {}

    std::string extract_relations(const std::string& text) override {
        std::string url = "https://api.openai.com/v1/chat/completions";
        
        // System prompt to enforce JSON output
        std::string prompt = R"(
            You are a scientific knowledge extractor. Analyze the text and extract knowledge triples.
            Supports hyper-relations (multiple sources/targets).
            Return ONLY raw JSON. Format:
            {
              "events": [
                { "sources": ["A", "B"], "relation": "interacts with", "targets": ["C"] }
              ]
            }
        )";

        json payload = {
            {"model", model},
            {"messages", {
                {{"role", "system"}, {"content", prompt}},
                {{"role", "user"}, {"content", text.substr(0, 15000)}} // Truncate to avoid token limits for demo
            }},
            {"temperature", 0},
            {"response_format", {{"type", "json_object"}}} 
        };

        std::vector<std::string> headers = {
            "Content-Type: application/json",
            "Authorization: Bearer " + api_key
        };

        std::string response = perform_request(url, payload, headers);
        
        try {
            auto j = json::parse(response);
            return j["choices"][0]["message"]["content"];
        } catch (...) {
            return "{}";
        }
    }
};

class GeminiClient : public LLMClient {
public:
    GeminiClient(std::string key, std::string mdl = "gemini-1.5-flash") : LLMClient(key, mdl) {}

    std::string extract_relations(const std::string& text) override {
        std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent?key=" + api_key;

        std::string prompt = R"(
            Extract scientific knowledge hypergraphs from the following text.
            Return a JSON object with a key 'events'. Each event has 'sources' (list of strings), 'relation' (string), and 'targets' (list of strings).
            Output strictly JSON.
            Text: 
        )";

        json payload = {
            {"contents", {{
                {"parts", {{
                    {"text", prompt + text.substr(0, 30000)} // Gemini handles larger contexts
                }}}
            }}},
            {"generationConfig", {
                {"response_mime_type", "application/json"}
            }}
        };

        std::vector<std::string> headers = {
            "Content-Type: application/json"
        };

        std::string response = perform_request(url, payload, headers);
        
        try {
            auto j = json::parse(response);
            return j["candidates"][0]["content"]["parts"][0]["text"];
        } catch (const std::exception& e) {
            std::cerr << "Gemini Parse Error: " << e.what() << " Response: " << response << std::endl;
            return "{}";
        }
    }
};

// ==========================================
// 4. Main Driver
// ==========================================
int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Usage: ./hg_agent <provider: openai|gemini> <api_key> <path_to_pdf_folder>\n";
        return 1;
    }

    std::string provider = argv[1];
    std::string api_key = argv[2];
    std::string folder_path = argv[3];

    std::unique_ptr<LLMClient> llm;
    if (provider == "openai") {
        llm = std::make_unique<OpenAIClient>(api_key);
    } else if (provider == "gemini") {
        llm = std::make_unique<GeminiClient>(api_key);
    } else {
        std::cerr << "Unknown provider. Use 'openai' or 'gemini'.\n";
        return 1;
    }

    Hypergraph kg;

    // Iterate over PDFs
    for (const auto& entry : fs::directory_iterator(folder_path)) {
        if (entry.path().extension() == ".pdf") {
            std::cout << "[*] Processing: " << entry.path().filename() << std::endl;
            
            // 1. Extract Text
            std::string text = PDFExtractor::extract_text(entry.path().string());
            if (text.empty()) continue;

            // 2. Extract Relations via LLM
            std::cout << "    ... Querying LLM" << std::endl;
            std::string json_resp = llm->extract_relations(text);

            // 3. Build Graph
            try {
                auto data = json::parse(json_resp);
                if (data.contains("events")) {
                    for (const auto& item : data["events"]) {
                        kg.add_edge(
                            item["sources"].get<std::vector<std::string>>(),
                            item["relation"].get<std::string>(),
                            item["targets"].get<std::vector<std::string>>()
                        );
                    }
                    std::cout << "    ... Added " << data["events"].size() << " relations." << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "    [-] JSON Error: " << e.what() << std::endl;
            }
        }
    }

    // 4. Visualize
    kg.save_to_dot("hypergraph.dot");
    std::cout << "[*] Done. Run 'dot -Tpng hypergraph.dot -o graph.png' to visualize." << std::endl;

    return 0;
}