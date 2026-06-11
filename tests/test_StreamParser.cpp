// test_StreamParser.cpp
#include "../src/StreamParser.h"
#include <iostream>
#include <cassert>
#include <vector>

int main()
{
    // ── Ollama mode ──────────────────────────────────────────────────────────
    {
        StreamParser parser(StreamParser::Mode::Ollama);
        std::vector<std::string> tokens;
        bool finished = false;

        parser.on_token  = [&](const std::string& t) { tokens.push_back(t); };
        parser.on_finish = [&]() { finished = true; };

        parser.feed(R"({"response":"Hello","done":false})" "\n");
        parser.feed(R"({"response":" world","done":false})" "\n");
        parser.feed(R"({"response":"!","done":true})" "\n");

        assert(tokens.size() == 3);
        assert(tokens[0] == "Hello");
        assert(tokens[1] == " world");
        assert(tokens[2] == "!");
        assert(finished);

        std::cout << "Ollama: " << tokens[0] << tokens[1] << tokens[2] << "\n";
        std::cout << "Ollama streaming test passed.\n";
    }

    // ── OpenAI SSE mode ──────────────────────────────────────────────────────
    {
        StreamParser parser(StreamParser::Mode::OpenAI);
        std::vector<std::string> tokens;
        bool finished = false;

        parser.on_token  = [&](const std::string& t) { tokens.push_back(t); };
        parser.on_finish = [&]() { finished = true; };

        // Simulate two SSE events.
        std::string chunk1 =
            "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\"},\"index\":0}]}\n\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\" there\"},\"index\":0}]}\n\n";
        parser.feed(chunk1);

        std::string chunk2 =
            "data: {\"choices\":[{\"delta\":{\"content\":\"!\"},\"index\":0,\"finish_reason\":\"stop\"}]}\n\n"
            "data: [DONE]\n\n";
        parser.feed(chunk2);

        assert(tokens.size() == 3);
        assert(tokens[0] == "Hi");
        assert(tokens[1] == " there");
        assert(tokens[2] == "!");
        assert(finished);

        std::cout << "OpenAI: " << tokens[0] << tokens[1] << tokens[2] << "\n";
        std::cout << "OpenAI streaming test passed.\n";
    }

    std::cout << "\nAll StreamParser tests passed.\n";
    return 0;
}
