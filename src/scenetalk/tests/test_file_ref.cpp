#include <iomanip>
#include <random>
#include <sstream>
#include <utest/utest.h>
#include "file_ref.h"

using namespace scene_talk;

UTEST_MAIN()

/**
 * @brief Generate a fake SHA1 hash string
 *
 * Creates a random 40-character hex string that mimics a SHA1 hash.
 * This is for testing purposes only and does not represent actual content hashing.
 *
 * @param seed Optional seed for reproducible results (default: random)
 * @return std::string A string representing a fake SHA1 hash (40 hex characters)
 */
inline std::string generate_fake_sha1(unsigned int seed = std::random_device{}()) {
    // SHA1 hashes are 40 hex characters (160 bits)
    constexpr size_t sha1_length = 40;

    // Use Mersenne Twister engine with provided or random seed
    std::mt19937 generator(seed);

    // Generate random hex values (0-15)
    std::uniform_int_distribution<> distribution(0, 15);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // Generate the 40 hex characters
    for (size_t i = 0; i < sha1_length; ++i) {
        ss << distribution(generator);
    }

    return ss.str();
}

/**
 * @brief Generate a fake SHA1 hash based on file content
 *
 * Creates a deterministic SHA1-like hash from a string. While not cryptographically
 * secure, it provides consistent output for the same input string.
 *
 * @param content The content to hash
 * @return std::string A string representing a fake SHA1 hash (40 hex characters)
 */
inline std::string generate_content_based_fake_sha1(const std::string& content) {
    // If content is empty, return a special hash
    if (content.empty()) {
        return std::string(40, '0');  // 40 zeros for empty content
    }

    // Use a simple hashing algorithm to generate a seed from the content
    unsigned int seed = 0;
    for (char c : content) {
        // A simple hash function: multiply by 31 and add character value
        seed = seed * 31 + static_cast<unsigned char>(c);
    }

    return generate_fake_sha1(seed);
}

// Test constructor with required parameters only
UTEST(file_ref, constructor_required) {
    const std::string name = "test.txt";
    const std::string file_id = "abc123";
    const std::string content_hash = generate_content_based_fake_sha1(file_id);

    file_ref ref(name, file_id);

    ASSERT_STREQ(ref.name().c_str(), name.c_str());
    ASSERT_TRUE(ref.file_id().has_value());
    ASSERT_STREQ(ref.file_id().value().c_str(), file_id.c_str());
    ASSERT_FALSE(ref.content_type().has_value());
    ASSERT_FALSE(ref.content_hash().has_value());
    ASSERT_FALSE(ref.size().has_value());
}

// Test constructor with all parameters
UTEST(file_ref, constructor_all_params) {
    const std::string file_id = "xyz789";
    const std::string name = "image.png";
    const std::string content_type = "image/png";
    const std::string content_hash = generate_fake_sha1();
    const size_t size = 1024;

    file_ref ref(name, file_id, content_type, content_hash, size);

    ASSERT_STREQ(ref.name().c_str(), name.c_str());
    ASSERT_TRUE(ref.file_id().has_value());
    ASSERT_STREQ(ref.file_id().value().c_str(), file_id.c_str());
    ASSERT_TRUE(ref.content_type().has_value());
    ASSERT_STREQ(ref.content_type().value().c_str(), content_type.c_str());
    ASSERT_TRUE(ref.content_hash().has_value()); // Not set in constructor
    ASSERT_STREQ(ref.content_hash().value().c_str(), content_hash.c_str());
    ASSERT_TRUE(ref.size().has_value());
    ASSERT_EQ(ref.size().value(), size);
}

// Test constructor with content type only
UTEST(file_ref, content_type_only) {
    const std::string file_id = "def456";
    const std::string name = "document.pdf";
    const std::string content_type = "application/pdf";

    file_ref ref(name, file_id, content_type);

    ASSERT_STREQ(ref.name().c_str(), name.c_str());
    ASSERT_TRUE(ref.file_id().has_value());
    ASSERT_STREQ(ref.file_id().value().c_str(), file_id.c_str());
    ASSERT_TRUE(ref.content_type().has_value());
    ASSERT_STREQ(ref.content_type().value().c_str(), content_type.c_str());
    ASSERT_FALSE(ref.content_hash().has_value());
    ASSERT_FALSE(ref.size().has_value());
}

// Test constructor with size only
UTEST(file_ref, size_only) {
    const std::string name = "data.bin";
    const std::string file_id = "ghi789";
    const size_t size = 2048;

    // Note: To pass only size but not content_type, we need to explicitly use std::nullopt
    file_ref ref(name, file_id, std::nullopt, std::nullopt, size);

    ASSERT_STREQ(ref.name().c_str(), name.c_str());
    ASSERT_TRUE(ref.file_id().has_value());
    ASSERT_STREQ(ref.file_id().value().c_str(), file_id.c_str());
    ASSERT_FALSE(ref.content_type().has_value());
    ASSERT_FALSE(ref.content_hash().has_value());
    ASSERT_TRUE(ref.size().has_value());
    ASSERT_EQ(ref.size().value(), size);
}

// Test with empty file ID
UTEST(file_ref, empty_file_id) {
    const std::string file_id = "";
    const std::string name = "empty.txt";

    file_ref ref(name, file_id);

    ASSERT_STREQ(ref.name().c_str(), name.c_str());
    ASSERT_TRUE(ref.file_id().has_value());
    ASSERT_STREQ(ref.file_id().value().c_str(), "");
}

// Test with empty name
UTEST(file_ref, empty_name) {
    const std::string file_id = "jkl012";
    const std::string name = "";

    file_ref ref(name, file_id);

    ASSERT_STREQ(ref.name().c_str(), "");
    ASSERT_TRUE(ref.file_id().has_value());
    ASSERT_STREQ(ref.file_id().value().c_str(), file_id.c_str());
}

// Test content_hash accessor
UTEST(file_ref, content_hash_access) {
    const std::string file_id = "mno345";
    const std::string name = "hash_test.txt";
    const std::string content_hash = generate_fake_sha1();

    file_ref ref(name, std::nullopt, std::nullopt, content_hash);

    // Verify content_hash is set
    ASSERT_FALSE(ref.file_id().has_value());
    ASSERT_FALSE(ref.content_type().has_value());
    ASSERT_FALSE(ref.size().has_value());
    ASSERT_TRUE(ref.content_hash().has_value());
    ASSERT_EQ(ref.content_hash().value(), content_hash);
}

