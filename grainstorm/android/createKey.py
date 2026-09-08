import hashlib

# 1. PASTE YOUR SHA-256 HEX HERE
# Get this from Google Play Console -> Setup -> App Integrity
# Example: "F1:E2:D3:C4..."
hex_input = "4F:C6:69:B4:E7:E4:52:B3:8E:B2:10:7F:DA:1A:33:1C:F3:79:F0:64:1A:48:06:6A:30:14:BF:66:17:6C:93:93"

# Clean the string (remove colons and spaces)
clean_hex = hex_input.replace(":", "").replace(" ", "").lower()
raw_bytes = bytes.fromhex(clean_hex)

# 2. XOR KEY (Must match the 0x7F in your C++ SecureHash class)
XOR_KEY = 0x7F

scrambled = [b ^ XOR_KEY for b in raw_bytes]

print("// Copy this into your C++ MY_CERT_HASH array:")
print("static constexpr uint8_t MY_CERT_HASH[32] = {")
print("    " + ", ".join([hex(b) for b in raw_bytes]))
print("};")
