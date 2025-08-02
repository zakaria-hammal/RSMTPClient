# RSMTPClient

A robust SMTP client library written in C for sending emails with attachments via SSL/TLS connections.

## Features

- Supports both SSL (port 465) and STARTTLS (port 587) connections
- Handles multiple file attachments with automatic MIME type detection
- Includes comprehensive MIME type mapping for 80+ file extensions
- Provides detailed logging for debugging SMTP transactions
- Memory-safe implementation with proper error handling

## Installation

### Prerequisites
- OpenSSL development libraries
- C compiler (GCC, Clang, etc.)

```bash
# On Ubuntu/Debian
sudo apt-get install libssl-dev

# On CentOS/RHEL
sudo yum install openssl-devel
```
### Building
```bash
gcc -o myapp myapp.c smtp.c -lssl -lcrypto
```

---
## Usage

### Basic Email
```c
#include "smtp.h"

int main() {
    SMTPClient client = {
        .mailServer = "smtp.gmail.com", // example with gmail
        .emailAdress = "your@gmail.com",
        .secretCode = "yourpassword",  // Or app password
        .enableSSL = 1,
        .port = 587
    };

    MailMessage message = {
        .receiverEmailAdress = "recipient@example.com",
        .subject = "Test Email",
        .body = "This is a test message",
        .isBodyHtml = 0
    };

    send_email(client, message, 1);  // Enable logs
    return 0;
}
```

### Email with Attachment
```c
// ... client setup ...

MailMessage message = {
    .receiverEmailAdress = "recipient@example.com",
    .subject = "With Attachment",
    .body = "See attached file",
    .isBodyHtml = 0
};

Attachement attachment = {
    .fileName = "document.pdf",
    .filePath = "/path/to/document.pdf"
};

insert_attachement(&message, attachment);
send_email(client, message, 1);
```

### Supported MIME Types

| Extension | MIME Type |
|-----------|-----------|
| **Text/Documents** ||
| `.txt` | `text/plain` |
| `.html`, `.htm` | `text/html` |
| `.css` | `text/css` |
| `.js` | `text/javascript` |
| `.json` | `application/json` |
| `.xml` | `application/xml` |
| `.csv` | `text/csv` |
| `.md` | `text/markdown` |
| `.rtf` | `application/rtf` |
| **Images** ||
| `.jpg`, `.jpeg` | `image/jpeg` |
| `.png` | `image/png` |
| `.gif` | `image/gif` |
| `.bmp` | `image/bmp` |
| `.svg` | `image/svg+xml` |
| `.webp` | `image/webp` |
| `.ico` | `image/x-icon` |
| `.tiff` | `image/tiff` |
| `.psd` | `image/vnd.adobe.photoshop` |
| **Audio** ||
| `.mp3` | `audio/mpeg` |
| `.wav` | `audio/wav` |
| `.ogg` | `audio/ogg` |
| `.flac` | `audio/flac` |
| `.aac` | `audio/aac` |
| `.mid`, `.midi` | `audio/midi` |
| `.m4a` | `audio/mp4` |
| **Video** ||
| `.mp4` | `video/mp4` |
| `.webm` | `video/webm` |
| `.avi` | `video/x-msvideo` |
| `.mov` | `video/quicktime` |
| `.wmv` | `video/x-ms-wmv` |
| `.flv` | `video/x-flv` |
| `.mkv` | `video/x-matroska` |
| `.mpeg`, `.mpg` | `video/mpeg` |
| **Archives** ||
| `.zip` | `application/zip` |
| `.rar` | `application/x-rar-compressed` |
| `.tar` | `application/x-tar` |
| `.gz` | `application/gzip` |
| `.7z` | `application/x-7z-compressed` |
| `.bz2` | `application/x-bzip2` |
| **Office Documents** ||
| `.pdf` | `application/pdf` |
| `.doc` | `application/msword` |
| `.docx` | `application/vnd.openxmlformats-officedocument.wordprocessingml.document` |
| `.xls` | `application/vnd.ms-excel` |
| `.xlsx` | `application/vnd.openxmlformats-officedocument.spreadsheetml.sheet` |
| `.ppt` | `application/vnd.ms-powerpoint` |
| `.pptx` | `application/vnd.openxmlformats-officedocument.presentationml.presentation` |
| `.odt` | `application/vnd.oasis.opendocument.text` |
| `.ods` | `application/vnd.oasis.opendocument.spreadsheet` |
| `.odp` | `application/vnd.oasis.opendocument.presentation` |
| **Programming** ||
| `.c`, `.h` | `text/x-c` |
| `.cpp`, `.hpp` | `text/x-c++src`, `text/x-c++hdr` |
| `.java` | `text/x-java-source` |
| `.py` | `text/x-python` |
| `.sh` | `application/x-sh` |
| `.pl` | `application/x-perl` |
| `.php` | `application/x-php` |
| `.rb` | `application/x-ruby` |
| `.go` | `text/x-go` |
| `.swift` | `text/x-swift` |
| **System Files** ||
| `.exe`, `.dll` | `application/x-msdownload` |
| `.so` | `application/x-sharedlib` |
| `.deb` | `application/x-debian-package` |
| `.rpm` | `application/x-rpm` |
| `.apk` | `application/vnd.android.package-archive` |
| `.dmg` | `application/x-apple-diskimage` |
| **Fonts** ||
| `.ttf` | `font/ttf` |
| `.otf` | `font/otf` |
| `.woff` | `font/woff` |
| `.woff2` | `font/woff2` |
| **Default** ||
| *Others* | `application/octet-stream` |

---
## Security Considerations

### Authentication
- 🔐 Always use **app passwords** instead of your real email password
- 🔑 For Gmail, enable 2FA and generate app-specific passwords
- 🚫 Never hardcode credentials in source files

### Data Protection
- 🔒 Consider encrypting sensitive attachments before sending
- 📄 The library validates server certificates by default
- ⚠️ Plaintext authentication is used - recommend TLS 1.2+ only

### Best Practices
- 🛡️ Use environment variables for sensitive data:
  ```c
  // Recommended credential handling
  char* email = getenv("SMTP_EMAIL");
  char* pass = getenv("SMTP_PASSWORD");
  ```
- 🔄 Rotate credentials regularly
- 📝 Audit logs should monitor SMTP transactions

---
## Limitations

### Size Restrictions
- 📦 Maximum attachment size: 25MB (Gmail limit)
- 📈 Base64 encoding adds ~33% overhead
- 🧮 Theoretical maximum: ~18.7MB binary → 25MB encoded

### Protocol Support
- 🔌 SSLv3 and TLS 1.0/1.1 are disabled (TLS 1.2+ only)
- 📨 No support for SMTPUTF8 (ASCII-only emails)

### Technical Constraints
- ⏳ No async I/O - operations block during transmission
- 💾 Memory-intensive for large attachments
- 🖥️ Single-threaded implementation

### Feature Gaps
- 📎 No chunked transfer encoding (BDAT) support
- ✉️ No DKIM/DMARC signing capabilities
- 📆 No scheduling/delayed send functionality
 ---
## Special Thanks
A heartfelt thank you to **[Rayan Tribeche](https://github.com/Rayantrbh)** for his invaluable support and encouragement during the developement of this project.
