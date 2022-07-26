#include <tests.h>

TEST_CASE( "[test 1] Test hash and rolling hash", "[test 1]")
{
    SECTION("compare hash with rolling hash")
    {
        std::string thelightside = "gandalf";
        std::string thedarkside  = "sauron";
        
        uint32_t hashLightBase = HashService::hash(reinterpret_cast<uint8_t*>(const_cast<char *>(thelightside.c_str())), 6);
        uint32_t hashLightFull = HashService::hash(reinterpret_cast<uint8_t*>(const_cast<char *>(thelightside.c_str() + 1)), 6);
        uint32_t hashLightRoll = HashService::rolling_hash(reinterpret_cast<uint8_t*>(const_cast<char *>(thelightside.c_str())), 6, hashLightBase);
    
        CHECK(hashLightFull == hashLightRoll);

        uint32_t hashDarkBase = HashService::hash(reinterpret_cast<uint8_t*>(const_cast<char *>(thedarkside.c_str())), 4);
        uint32_t hashDarkFull = HashService::hash(reinterpret_cast<uint8_t*>(const_cast<char *>(thedarkside.c_str() + 1)), 4);
        uint32_t hashDarkRoll = HashService::rolling_hash(reinterpret_cast<uint8_t*>(const_cast<char *>(thedarkside.c_str())), 4, hashDarkBase);
    }
}