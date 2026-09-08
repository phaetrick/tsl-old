import base64

# 1. Paste your Google Play Public Key here
raw_key = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAs2Dy41ouuL6GC+ZtaCMSjixpe/E97tEA3Ndghe75D7XQTU5xIO9akq2ecVdBP8NFI27yf28Og3QQT2hNTelH/NYw8cWYAJFdYw8jkRR1vpB4dgixqjCpVx6bA5WIMdWgrfsxp/zRlzI9WG3stNNXlCPv4aiZZ0aAbLnPxnPavJWvy+feS8PboYxS2HFamu6dKWtcClqlUEi8zGjYcULLmyoI2vYgls/IVibV2I4TfqbFeYfF44J6s+yARGz+QsRbP62BgSYMOr2BdlCws0M7Nwzn63oyIsmpCiBeUReEXISu0wmcSgg0pmVhRicwBQ9bmPoiQ/KLF2JbsaORQjqZHwIDAQAB" 

# 2. Use the same salt as your C++ code (0x55)
SALT = 0x55

def generate_hex_array(key_str):
    # XOR each character with the salt
    obfuscated = [ord(c) ^ SALT for c in key_str]
    
    # Format as C++ hex array
    hex_format = ", ".join([f"0x{b:02X}" for b in obfuscated])
    
    print(f"static const unsigned char obfuscated_key[] = {{ {hex_format} }};")
    print(f"static const int key_length = {len(obfuscated)};")

generate_hex_array(raw_key)
