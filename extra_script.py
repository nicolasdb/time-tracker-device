import os

Import("env")

# Create the components directory if it doesn't exist
if not os.path.exists("components"):
    os.makedirs("components")

# Clone the esp_littlefs repository if it doesn't exist
if not os.path.exists("components/esp_littlefs"):
    env.Execute("git clone --recursive https://github.com/joltwallet/esp_littlefs.git components/esp_littlefs")
