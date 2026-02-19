Import("env")
import os
import struct

def make_ota_image(source, target, env):
    firmware_path = str(source[0])
    ota_image_path = firmware_path.replace(".bin", ".zigbee")
    
    # Zigbee OTA Header Constants
    OTA_UPGRADE_FILE_ID = 0x0BEEF11E
    HEADER_VERSION = 0x0100
    HEADER_LENGTH = 56 # Fixed for now
    FIELD_CONTROL = 0x0000
    MANUFACTURER_CODE = 0x1001 # Test Manufacturer
    IMAGE_TYPE = 0x0000        # Application
    FILE_VERSION = 0x00000001  # V1
    STACK_VERSION = 0x0002
    
    # Read firmware binary
    with open(firmware_path, "rb") as f:
        firmware_data = f.read()
        
    total_image_size = HEADER_LENGTH + len(firmware_data)
    
    # Pack Header (Little Endian)
    # 4s: ID
    # H: Header Version
    # H: Header Length
    # H: Field Control
    # H: Manufacturer Code
    # H: Image Type
    # I: File Version
    # H: Stack Version
    # 32s: Header String (Optional)
    # I: Total Image Size
    
    header = struct.pack("<IHHHHHIH32sI", 
        OTA_UPGRADE_FILE_ID, 
        HEADER_VERSION, 
        HEADER_LENGTH, 
        FIELD_CONTROL, 
        MANUFACTURER_CODE, 
        IMAGE_TYPE, 
        FILE_VERSION, 
        STACK_VERSION, 
        b"Zigbee OTA Image", 
        total_image_size
    )
    
    # Write OTA Image
    with open(ota_image_path, "wb") as f:
        f.write(header)
        f.write(firmware_data)
        
    print(f"Generada imagen OTA Zigbee: {ota_image_path}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", make_ota_image)
