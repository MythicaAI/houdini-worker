#include "frame_decoder.h"
#include "encoder.h"
#include "file_ref.h"
#include <iostream>
#include <iomanip>


#include <vector>
#include <string>

using json = nlohmann::json;
constexpr auto FLAG_PARTIAL = 0x1;

// Helper function to create a simple test frame with CBOR encoded payload
std::vector<uint8_t> createTestFrame(char frameType, bool partial, const json& payload) {
    std::vector<uint8_t> frame;
    
    // Convert payload to CBOR
    std::vector<uint8_t> payloadCbor = json::to_cbor(payload);
    
    // Build header
    frame.push_back(static_cast<uint8_t>(frameType));  // Frame type
    frame.push_back(partial ? FLAG_PARTIAL : 0);       // Flags
    
    // Length (little endian)
    uint16_t length = static_cast<uint16_t>(payloadCbor.size());
    frame.push_back(length & 0xFF);
    frame.push_back((length >> 8) & 0xFF);
    
    // Add payload
    frame.insert(frame.end(), payloadCbor.begin(), payloadCbor.end());
    
    return frame;
}

// Utility to print bytes in hex format
void printBytes(const std::vector<uint8_t>& data) {
    for (const auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
}

void example_decoder() {

    std::vector<uint8_t> stream;
    scene_talk::encoder encoder([&stream](const scene_talk::frame &f) {
        f.serialize(stream);
    });
    scene_talk::reader reader;


    
    // Create some test frames
    json payload1 = {
        {"action", "login"},
        {"user", "john_doe"},
        {"timestamp", 1635765432}
    };
    
    json payload2 = {
        {"status", "success"},
        {"message", "User authenticated"},
        {"session", "a1b2c3d4"}
    };
    
    // Create complete frames
    auto frame1 = createTestFrame('A', false, payload1);
    auto frame2 = createTestFrame('B', false, payload2);
    
    // Create partial frames
    json bigData = {{"data", std::vector<int>(1000, 42)}};
    auto partFrame1 = createTestFrame('C', true, json{{"part", 1}});
    auto partFrame2 = createTestFrame('C', true, json{{"part", 2}});
    auto partFrame3 = createTestFrame('C', false, json{{"part", 3}, {"final", true}});
    
    std::cout << "Adding first frame..." << std::endl;
    decoder.append(frame1.data(), frame1.size());
    
    // Process frames
    auto frames = read_all(decoder);
    std::cout << "Extracted " << frames.size() << " frames" << std::endl;
    
    for (const auto& [header, payload] : frames) {
        std::cout << "Frame: " << header.toString() << std::endl;
        
        if (std::holds_alternative<json>(payload)) {
            std::cout << "JSON Payload: " << std::get<json>(payload).dump(2) << std::endl;
        } else {
            std::cout << "Raw Payload (bytes): ";
            printBytes(std::get<std::vector<uint8_t>>(payload));
        }
    }
    
    std::cout << "\nAdding multiple frames..." << std::endl;
    buffer.appendBytes(frame2.data(), frame2.size());
    buffer.appendBytes(partFrame1.data(), partFrame1.size());
    buffer.appendBytes(partFrame2.data(), partFrame2.size());
    
    // Process frames again
    frames = buffer.readFrames();
    std::cout << "Extracted " << frames.size() << " frames" << std::endl;
    
    for (const auto& [header, payload] : frames) {
        std::cout << "Frame: " << header.toString() << std::endl;
        
        if (std::holds_alternative<json>(payload)) {
            std::cout << "JSON Payload: " << std::get<json>(payload).dump(2) << std::endl;
        } else {
            std::cout << "Raw Payload (bytes): ";
            printBytes(std::get<std::vector<uint8_t>>(payload));
        }
    }
    
    // Demonstrate flush callback
    std::cout << "\nDemonstrating flush callback..." << std::endl;
    buffer.appendBytes(partFrame3.data(), partFrame3.size());
    
    buffer.flush([](scene_talk::frame_payload data) {
        std::cout << "Flushed " << data.size() << " bytes" << std::endl;
        
        // Print the first few bytes
        std::cout << "First bytes: ";
        for (size_t i = 0; i < std::min<size_t>(16, data.size()); i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(data[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    });
    
    std::cout << "Buffer should be empty now!" << std::endl;
}

// Helper function to print byte vectors in hex format
void print_bytes(const scene_talk::frame &f) {
    std::cout << "["  << f.type << "], partial: " <<
            (f.flags & FLAG_PARTIAL ? "y" : "n") <<
            ", len: " << f.payload.size() << std::endl;
    for (uint8_t b : f.payload) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    }
    std::cout << std::dec << std::endl;
}

void example_encoder() {
    // Create an encoder instance
    scene_talk::encoder encoder([](const scene_talk::frame& frame) {
        print_bytes(frame);
    });

    // Example: Create a BEGIN frame
    std::cout << "BEGIN frame example:" << std::endl;
    encoder.begin("test_entity", "test_name");

    // Example: Create an END frame
    std::cout << "END frame example:" << std::endl;
    encoder.end();

    // Example: Create an attribute frame
    std::cout << "ATTRIBUTE frame example:" << std::endl;
    json value = {{"key1", "value1"}, {"key2", 42}};
    encoder.attr("test_attr", "object", value);

    // Example: Create a file reference frame
    std::cout << "FILE frame example:" << std::endl;
    scene_talk::file_ref ref("file123", "test.txt", "text/plain", "sha1:hash", 1024);
    encoder.file(ref);

    // Example: Create a hello frame
    std::cout << "HELLO frame example:" << std::endl;
    encoder.hello("test_client", "auth_token_123");
}

int main() {
    example_encoder();
    example_decoder();
}
