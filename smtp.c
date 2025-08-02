#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include "smtp.h"

typedef struct {
    const char *extension;
    const char *mime_type;
} MimeMapping;

// Comprehensive list of MIME types
static const MimeMapping mime_types[] = {
    // Text and Documents
    {".txt", "text/plain"},
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".csv", "text/csv"},
    {".md", "text/markdown"},
    {".rtf", "application/rtf"},

    // Images
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".bmp", "image/bmp"},
    {".svg", "image/svg+xml"},
    {".webp", "image/webp"},
    {".ico", "image/x-icon"},
    {".tiff", "image/tiff"},
    {".psd", "image/vnd.adobe.photoshop"},

    // Audio
    {".mp3", "audio/mpeg"},
    {".wav", "audio/wav"},
    {".ogg", "audio/ogg"},
    {".flac", "audio/flac"},
    {".aac", "audio/aac"},
    {".mid", "audio/midi"},
    {".midi", "audio/midi"},
    {".m4a", "audio/mp4"},

    // Video
    {".mp4", "video/mp4"},
    {".webm", "video/webm"},
    {".avi", "video/x-msvideo"},
    {".mov", "video/quicktime"},
    {".wmv", "video/x-ms-wmv"},
    {".flv", "video/x-flv"},
    {".mkv", "video/x-matroska"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},

    // Archives
    {".zip", "application/zip"},
    {".rar", "application/x-rar-compressed"},
    {".tar", "application/x-tar"},
    {".gz", "application/gzip"},
    {".7z", "application/x-7z-compressed"},
    {".bz2", "application/x-bzip2"},

    // Documents
    {".pdf", "application/pdf"},
    {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {".odp", "application/vnd.oasis.opendocument.presentation"},

    // Programming
    {".c", "text/x-c"},
    {".h", "text/x-c"},
    {".cpp", "text/x-c++src"},
    {".hpp", "text/x-c++hdr"},
    {".java", "text/x-java-source"},
    {".py", "text/x-python"},
    {".sh", "application/x-sh"},
    {".pl", "application/x-perl"},
    {".php", "application/x-php"},
    {".rb", "application/x-ruby"},
    {".go", "text/x-go"},
    {".swift", "text/x-swift"},

    // System
    {".exe", "application/x-msdownload"},
    {".dll", "application/x-msdownload"},
    {".so", "application/x-sharedlib"},
    {".deb", "application/x-debian-package"},
    {".rpm", "application/x-rpm"},
    {".apk", "application/vnd.android.package-archive"},
    {".dmg", "application/x-apple-diskimage"},

    // Fonts
    {".ttf", "font/ttf"},
    {".otf", "font/otf"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},

    // Default fallback
    {NULL, "application/octet-stream"}
};

static const char *get_mime_type(const char *filename) {
    const char *extension = strrchr(filename, '.');
    if (!extension) {
        return mime_types[sizeof(mime_types)/sizeof(MimeMapping) - 1].mime_type;
    }

    // Case-insensitive comparison
    for (int i = 0; mime_types[i].extension; i++) {
        if (strcasecmp(extension, mime_types[i].extension) == 0) {
            return mime_types[i].mime_type;
        }
    }

    return mime_types[sizeof(mime_types)/sizeof(MimeMapping) - 1].mime_type;
}

static char* read_file(char* path, size_t* length)
{
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    struct stat st;
    stat(path, &st);
    *length = st.st_size;

    char* content = malloc(*length + 1);
    fread(content, 1, *length, file);
    content[*length] = '\0';
    fclose(file);
    return content;
}

static void base64_encode(char* dest, char* src)
{
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    BIO_write(bio, src, strlen(src));
    BIO_flush(bio);

    BIO_get_mem_ptr(bio, &bufferPtr);
    strcpy(dest, bufferPtr->data);

    BIO_free_all(bio);
}

void insert_attachement(MailMessage *message, Attachement attachement)
{
    AttachementListNode* new = malloc(sizeof(AttachementListNode));
    strcpy(new->attachement.fileName, attachement.fileName);
    strcpy(new->attachement.filePath, attachement.filePath);

    new->next = (*message).attachementList.head;
    (*message).attachementList.head = new;

    (*message).attachementList.numberOfElements++;
}

void send_email(SMTPClient client, MailMessage message, int enableLogs)
{
    char buffer[4096];
    char req[4096];

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in smtp_server_addr;
    struct addrinfo hints = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM};
    struct addrinfo *res;

    char port[10] = {0};
    sprintf(port, "%d", client.port);

    char dateStr[128];

    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);

    strftime(dateStr, sizeof(dateStr), "%a, %d %b %Y %H:%M:%S +0000", tm_info);

    if (client.port == 25)
    {
        perror("Port 25 is no longer used");
        exit(1);
    }
    else if (client.port == 465)
    {
        getaddrinfo(client.mailServer, port, &hints, &res);
        connect(clientfd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);

        OPENSSL_init_ssl(0, NULL);
        SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, clientfd);

        SSL_connect(ssl);

        if (enableLogs)
        {
            SSL_read(ssl, buffer, sizeof(buffer));
            printf("S: %s", buffer);
        }

        strcpy(req, "EHLO localhost\r\n");

        SSL_write(ssl, req, strlen(req));
        if (enableLogs)
        {
            printf("C: %s", req);
            SSL_read(ssl, buffer, sizeof(buffer));
            printf("S: %s", buffer);
        }

        if (client.authType == LOGIN)
        {
            strcpy(req, "AUTH LOGIN\r\n");
            SSL_write(ssl, req, strlen(req));
            if (enableLogs)
            {
                printf("C: %s", req);
                SSL_read(ssl, buffer, sizeof(buffer));
                printf("S: %s", buffer);
            }

            base64_encode(req, client.emailAdress);
            strcat(req, "\r\n");
            SSL_write(ssl, req, strlen(req));

            if (enableLogs)
            {
                printf("C: %s", req);
                SSL_read(ssl, buffer, sizeof(buffer));
                printf("S: %s", buffer);
            }

            base64_encode(req, client.secretCode);
            strcat(req, "\r\n");
            SSL_write(ssl, req, strlen(req));

            if (enableLogs)
            {
                printf("C: %s", req);
                SSL_read(ssl, buffer, sizeof(buffer));
                printf("S: %s", buffer);
            }
        }
        else
        {
            const char xoauth2_prefix[] = "AUTH XOAUTH2 ";
            char temp[4069] = {0};

            snprintf(temp, sizeof(temp), "user=%s%cauth=Bearer %s\%c%c", client.emailAdress,  0x01, client.secretCode, 0x01, 0x01);
            printf("%s\n", temp);
            BIO *bio, *b64;
            BUF_MEM *bufferPtr;

            b64 = BIO_new(BIO_f_base64());
            bio = BIO_new(BIO_s_mem());
            bio = BIO_push(b64, bio);

            BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

            BIO_write(bio, temp, strlen(temp));
            BIO_flush(bio);

            BIO_get_mem_ptr(bio, &bufferPtr);
            memcpy(req, bufferPtr->data, bufferPtr->length);
            req[bufferPtr->length] = '\0';

            BIO_free_all(bio);
            printf("%s\n", req);
            strcat(req, "\r\n");
            sprintf(temp, "%s%s", xoauth2_prefix, req);
            SSL_write(ssl, temp, strlen(temp));

            if (enableLogs)
            {
                printf("C: %s", temp);
                SSL_read(ssl, buffer, sizeof(buffer));
                printf("S: %s", buffer);
            }
        }

        sprintf(req, "MAIL FROM: <%s>\r\n", client.emailAdress);
        SSL_write(ssl, req, strlen(req));

        if (enableLogs)
        {
            printf("C: %s", req);
            SSL_read(ssl, buffer, sizeof(buffer));
            printf("S: %s", buffer);
        }

        sprintf(req, "RCPT TO: <%s>\r\n", message.receiverEmailAdress);
        SSL_write(ssl, req, strlen(req));

        if (enableLogs)
        {
            printf("C: %s", req);
            SSL_read(ssl, buffer, sizeof(buffer));
            printf("S: %s", buffer);
        }

        strcpy(req, "DATA\r\n");
        SSL_write(ssl, req, strlen(req));

        if (enableLogs)
        {
            printf("C: %s", req);
            SSL_read(ssl, buffer, sizeof(buffer));
            printf("S: %s", buffer);
        }

        if (!message.attachementList.numberOfElements)
        {
            sprintf(req,"Date: %s\r\n"
                        "From: <%s>\r\n"
                        "To: Recipient <%s>\r\n"
                        "MIME-Version: 1.0\r\n"
                        "Content-Type: text/%s; charset=\"ISO-8859-1\"\r\n"
                        "Subject: %s\r\n\r\n"
                        "%s\r\n.\r\n",
                        dateStr,
                        client.emailAdress,
                        message.receiverEmailAdress,
                        message.isBodyHtml? "html" : "plain",
                        message.subject,
                        message.body);

            SSL_write(ssl, req, strlen(req));
            if (enableLogs)
            {
                printf("C: %s", req);
                SSL_read(ssl, buffer, sizeof(buffer));
                printf("S: %s", buffer);
            }
        }
        else
        {
            sprintf(req,"Date: %s\r\n"
                        "From: <%s>\r\n"
                        "To: Recipient <%s>\r\n"
                        "MIME-Version: 1.0\r\n"
                        "Content-Type: multipart/mixed; boundary=\"123456789\"\r\n"
                        "Subject: %s\r\n\r\n"
                        "--123456789\r\n"
                        "Content-Type: text/%s; charset=\"ISO-8859-1\"\r\n"
                        "Content-Transfer-Encoding: quoted-printable\r\n\r\n"
                        "%s\r\n\r\n"
                        "--123456789\r\n",
                        dateStr,
                        client.emailAdress,
                        message.receiverEmailAdress,
                        message.subject,
                        message.isBodyHtml? "html" : "plain",
                        message.body);
            SSL_write(ssl, req, strlen(req));

            AttachementListNode* current = message.attachementList.head;
            char *file;
            size_t* length = malloc(sizeof(size_t));

            char *encoded_file;
            size_t encoded_length;

            for (int i = 0; i < message.attachementList.numberOfElements; i++)
            {
                sprintf(req,"Content-Disposition: attachment; filename=\"%s\"\r\n"
                            "Content-Type: %s; name=\"%s\"\r\n"
                            "Content-Transfer-Encoding: base64\r\n\r\n",
                            (current->attachement).fileName,
                            get_mime_type((current->attachement).fileName),
                            (current->attachement).fileName
                );

                size_t total_sent;
                SSL_write(ssl, req, strlen(req));

                file = read_file((current->attachement).filePath, length);

                encoded_length = 4 * ceil((*length) / 3);

                encoded_file = malloc(encoded_length + 1);

                base64_encode(encoded_file, file);

                free(file);

                total_sent = 0;
                while (total_sent < encoded_length) {
                    int chunk = (encoded_length - total_sent > 16384)
                                ? 16384
                                : encoded_length - total_sent;

                    int ret = SSL_write(ssl, encoded_file + total_sent, chunk);
                    total_sent += ret;
                }

                free(encoded_file);

                sprintf(req,"\r\n\r\n"
                            "--123456789\r\n");
                SSL_write(ssl, req, strlen(req));
            }

            sprintf(req,".\r\n");
            SSL_write(ssl, req, strlen(req));
            free(length);
        }

        strcpy(req, "QUIT\r\n");
        SSL_write(ssl, req, strlen(req));

        if (enableLogs)
        {
            printf("C: %s", req);
            SSL_read(ssl, buffer, sizeof(buffer));
            printf("S: %s", buffer);
        }

        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(clientfd);
    }
    else if(client.port == 587 || client.port == 2525)
    {
        getaddrinfo(client.mailServer, port, &hints, &res);
        connect(clientfd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);

        send(clientfd, "EHLO localhost\r\n", 16, 0);

        if (enableLogs)
        {
            printf("C: EHLO localhost\n");
            recv(clientfd, buffer, sizeof(buffer), 0);
            printf("S: %s", buffer);
        }

        if (client.enableSSL)
        {
            printf("%d\n", message.attachementList.numberOfElements);
            send(clientfd, "STARTTLS\r\n", 10, 0);

            recv(clientfd, buffer, sizeof(buffer), 0);

            while (!strstr(buffer, "220")) {
                recv(clientfd, buffer, sizeof(buffer), 0);
            }

            OPENSSL_init_ssl(0, NULL);
            SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
            SSL *ssl = SSL_new(ctx);
            SSL_set_fd(ssl, clientfd);

            SSL_connect(ssl);

            if (enableLogs)
            {
                printf("C: STARTTLS\r\n");
                printf("S: %s", buffer);
            }

            strcpy(req, "EHLO localhost\r\n");

            SSL_write(ssl, req, strlen(req));

            if (enableLogs)
            {
                printf("C: %s", req);
                SSL_read(ssl, buffer, sizeof(buffer));
                printf("S: %s", buffer);
            }

            if (client.authType == LOGIN)
            {
                strcpy(req, "AUTH LOGIN\r\n");
                SSL_write(ssl, req, strlen(req));
                if (enableLogs)
                {
                    printf("C: %s", req);
                    SSL_read(ssl, buffer, sizeof(buffer));
                    printf("S: %s", buffer);
                }

                base64_encode(req, client.emailAdress);
                strcat(req, "\r\n");
                SSL_write(ssl, req, strlen(req));

                if (enableLogs)
                {
                    printf("C: %s", req);
                    SSL_read(ssl, buffer, sizeof(buffer));
                    printf("S: %s", buffer);
                }

                base64_encode(req, client.secretCode);
                strcat(req, "\r\n");
                SSL_write(ssl, req, strlen(req));

                if (enableLogs)
                {
                    printf("C: %s", req);
                    SSL_read(ssl, buffer, sizeof(buffer));
                    printf("S: %s", buffer);
                }
            }
            else
            {
                const char xoauth2_prefix[] = "AUTH XOAUTH2 ";
                char temp[4069] = {0};

                snprintf(temp, sizeof(temp), "user=%s%cauth=Bearer %s\%c%c", client.emailAdress,  0x01, client.secretCode, 0x01, 0x01);
                printf("%s\n", temp);
                BIO *bio, *b64;
                BUF_MEM *bufferPtr;

                b64 = BIO_new(BIO_f_base64());
                bio = BIO_new(BIO_s_mem());
                bio = BIO_push(b64, bio);

                BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

                BIO_write(bio, temp, strlen(temp));
                BIO_flush(bio);

                BIO_get_mem_ptr(bio, &bufferPtr);
                memcpy(req, bufferPtr->data, bufferPtr->length);
                req[bufferPtr->length] = '\0';

                BIO_free_all(bio);
                printf("%s\n", req);
                strcat(req, "\r\n");
                sprintf(temp, "%s%s", xoauth2_prefix, req);
                SSL_write(ssl, temp, strlen(temp));

                if (enableLogs)
                {
                    printf("C: %s", temp);
                    SSL_read(ssl, buffer, sizeof(buffer));
                    printf("S: %s", buffer);
                }
            }

            sprintf(req, "MAIL FROM: <%s>\r\n", client.emailAdress);
            SSL_write(ssl, req, strlen(req));

            if (enableLogs)
            {
                printf("C: %s", req);
                SSL_read(ssl, buffer, sizeof(buffer));
                printf("S: %s", buffer);
            }

            sprintf(req, "RCPT TO: <%s>\r\n", message.receiverEmailAdress);
            SSL_write(ssl, req, strlen(req));

            if (enableLogs)
            {
                printf("C: %s", req);
                SSL_read(ssl, buffer, sizeof(buffer));
                printf("S: %s", buffer);
            }

            strcpy(req, "DATA\r\n");
            SSL_write(ssl, req, strlen(req));

            if (enableLogs)
            {
                printf("C: %s", req);
                SSL_read(ssl, buffer, sizeof(buffer));
                printf("S: %s", buffer);
            }

            if (!message.attachementList.numberOfElements)
            {
                sprintf(req,"Date: %s\r\n"
                            "From: <%s>\r\n"
                            "To: Recipient <%s>\r\n"
                            "MIME-Version: 1.0\r\n"
                            "Content-Type: text/%s; charset=\"ISO-8859-1\"\r\n"
                            "Subject: %s\r\n\r\n"
                            "%s\r\n.\r\n",
                            dateStr,
                            client.emailAdress,
                            message.receiverEmailAdress,
                            message.isBodyHtml? "html" : "plain",
                            message.subject,
                            message.body);

                SSL_write(ssl, req, strlen(req));
                if (enableLogs)
                {
                    printf("C: %s", req);
                    SSL_read(ssl, buffer, sizeof(buffer));
                    printf("S: %s", buffer);
                }
            }
            else
            {
                sprintf(req,"Date: %s\r\n"
                            "From: <%s>\r\n"
                            "To: Recipient <%s>\r\n"
                            "MIME-Version: 1.0\r\n"
                            "Content-Type: multipart/mixed; boundary=\"123456789\"\r\n"
                            "Subject: %s\r\n\r\n"
                            "--123456789\r\n"
                            "Content-Type: text/%s; charset=\"ISO-8859-1\"\r\n"
                            "Content-Transfer-Encoding: quoted-printable\r\n\r\n"
                            "%s\r\n\r\n"
                            "--123456789\r\n",
                            dateStr,
                            client.emailAdress,
                            message.receiverEmailAdress,
                            message.subject,
                            message.isBodyHtml? "html" : "plain",
                            message.body);
                SSL_write(ssl, req, strlen(req));

                AttachementListNode* current = message.attachementList.head;
                char *file;
                size_t* length = malloc(sizeof(size_t));

                char *encoded_file;
                size_t encoded_length;

                for (int i = 0; i < message.attachementList.numberOfElements; i++)
                {
                    sprintf(req,"Content-Disposition: attachment; filename=\"%s\"\r\n"
                                "Content-Type: %s; name=\"%s\"\r\n"
                                "Content-Transfer-Encoding: base64\r\n\r\n",
                                (current->attachement).fileName,
                                get_mime_type((current->attachement).fileName),
                                (current->attachement).fileName
                    );

                    size_t total_sent;
                    SSL_write(ssl, req, strlen(req));

                    file = read_file((current->attachement).filePath, length);

                    encoded_length = 4 * ceil((*length) / 3);

                    encoded_file = malloc(encoded_length + 1);

                    base64_encode(encoded_file, file);

                    free(file);

                    total_sent = 0;
                    while (total_sent < encoded_length) {
                        int chunk = (encoded_length - total_sent > 16384)
                                    ? 16384
                                    : encoded_length - total_sent;

                        int ret = SSL_write(ssl, encoded_file + total_sent, chunk);
                        total_sent += ret;
                    }

                    free(encoded_file);

                    sprintf(req,"\r\n\r\n"
                                "--123456789\r\n");
                    SSL_write(ssl, req, strlen(req));
                }

                sprintf(req,".\r\n");
                SSL_write(ssl, req, strlen(req));
                free(length);
            }

            strcpy(req, "QUIT\r\n");
            SSL_write(ssl, req, strlen(req));

            if (enableLogs)
            {
                    printf("C: %s", req);
                SSL_read(ssl, buffer, sizeof(buffer));
                printf("S: %s", buffer);
            }

            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(clientfd);
        }
        else {
            if (client.authType == LOGIN)
            {
                strcpy(req, "AUTH LOGIN\r\n");
                send(clientfd, req, strlen(req), 0);
                if (enableLogs)
                {
                    printf("C: %s", req);
                    recv(clientfd, buffer, sizeof(buffer), 0);
                    printf("S: %s", buffer);
                }

                base64_encode(req, client.emailAdress);
                strcat(req, "\r\n");
                send(clientfd, req, strlen(req), 0);

                if (enableLogs)
                {
                    printf("C: %s", req);
                    recv(clientfd, buffer, sizeof(buffer), 0);
                    printf("S: %s", buffer);
                }

                base64_encode(req, client.secretCode);
                strcat(req, "\r\n");
                send(clientfd, req, strlen(req), 0);

                if (enableLogs)
                {
                    printf("C: %s", req);
                    recv(clientfd, buffer, sizeof(buffer), 0);
                    printf("S: %s", buffer);
                }
            }
            else
            {
                const char xoauth2_prefix[] = "AUTH XOAUTH2 ";
                char temp[4069] = {0};

                snprintf(temp, sizeof(temp), "user=%s%cauth=Bearer %s\%c%c", client.emailAdress,  0x01, client.secretCode, 0x01, 0x01);
                printf("%s\n", temp);
                BIO *bio, *b64;
                BUF_MEM *bufferPtr;

                b64 = BIO_new(BIO_f_base64());
                bio = BIO_new(BIO_s_mem());
                bio = BIO_push(b64, bio);

                BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

                BIO_write(bio, temp, strlen(temp));
                BIO_flush(bio);

                BIO_get_mem_ptr(bio, &bufferPtr);
                memcpy(req, bufferPtr->data, bufferPtr->length);
                req[bufferPtr->length] = '\0';

                BIO_free_all(bio);
                printf("%s\n", req);
                strcat(req, "\r\n");
                sprintf(temp, "%s%s", xoauth2_prefix, req);
                send(clientfd, temp, strlen(temp), 0);

                if (enableLogs)
                {
                    printf("C: %s", temp);
                    recv(clientfd, buffer, sizeof(buffer), 0);
                    printf("S: %s", buffer);
                }
            }

            sprintf(req, "MAIL FROM: <%s>\r\n", client.emailAdress);
            send(clientfd, req, strlen(req), 0);

            if (enableLogs)
            {
                printf("C: %s", req);
                recv(clientfd, buffer, sizeof(buffer), 0);
                printf("S: %s", buffer);
            }

            sprintf(req, "RCPT TO: <%s>\r\n", message.receiverEmailAdress);
            send(clientfd, req, strlen(req), 0);

            if (enableLogs)
            {
                printf("C: %s", req);
                recv(clientfd, buffer, sizeof(buffer), 0);
                printf("S: %s", buffer);
            }

            strcpy(req, "DATA\r\n");
            send(clientfd, req, strlen(req), 0);

            if (enableLogs)
            {
                printf("C: %s", req);
                recv(clientfd, buffer, sizeof(buffer), 0);
                printf("S: %s", buffer);
            }

            if (!message.attachementList.numberOfElements)
            {
                sprintf(req,"Date: %s\r\n"
                            "From: <%s>\r\n"
                            "To: Recipient <%s>\r\n"
                            "MIME-Version: 1.0\r\n"
                            "Content-Type: text/%s; charset=\"ISO-8859-1\"\r\n"
                            "Subject: %s\r\n\r\n"
                            "%s\r\n.\r\n",
                            dateStr,
                            client.emailAdress,
                            message.receiverEmailAdress,
                            message.isBodyHtml? "html" : "plain",
                            message.subject,
                            message.body);

                send(clientfd, req, strlen(req), 0);
                if (enableLogs)
                {
                    printf("C: %s", req);
                    recv(clientfd, buffer, sizeof(buffer), 0);
                    printf("S: %s", buffer);
                }
            }
            else
            {
                sprintf(req,"Date: %s\r\n"
                            "From: <%s>\r\n"
                            "To: Recipient <%s>\r\n"
                            "MIME-Version: 1.0\r\n"
                            "Content-Type: multipart/mixed; boundary=\"123456789\"\r\n"
                            "Subject: %s\r\n\r\n"
                            "--123456789\r\n"
                            "Content-Type: text/%s; charset=\"ISO-8859-1\"\r\n"
                            "Content-Transfer-Encoding: quoted-printable\r\n\r\n"
                            "%s\r\n\r\n"
                            "--123456789\r\n",
                            dateStr,
                            client.emailAdress,
                            message.receiverEmailAdress,
                            message.subject,
                            message.isBodyHtml? "html" : "plain",
                            message.body);
                send(clientfd, req, strlen(req), 0);

                AttachementListNode* current = message.attachementList.head;
                char *file;
                size_t* length = malloc(sizeof(size_t));

                char *encoded_file;
                size_t encoded_length;

                for (int i = 0; i < message.attachementList.numberOfElements; i++)
                {
                    sprintf(req,"Content-Disposition: attachment; filename=\"%s\"\r\n"
                                "Content-Type: %s; name=\"%s\"\r\n"
                                "Content-Transfer-Encoding: base64\r\n\r\n",
                                (current->attachement).fileName,
                                get_mime_type((current->attachement).fileName),
                                (current->attachement).fileName
                    );

                    size_t total_sent;
                    send(clientfd, req, strlen(req), 0);

                    file = read_file((current->attachement).filePath, length);

                    encoded_length = 4 * ceil((*length) / 3);

                    encoded_file = malloc(encoded_length + 1);

                    base64_encode(encoded_file, file);

                    free(file);

                    total_sent = 0;
                    while (total_sent < encoded_length) {
                        int chunk = (encoded_length - total_sent > 16384)
                                    ? 16384
                                    : encoded_length - total_sent;

                        int ret = send(clientfd, encoded_file + total_sent, chunk, 0);
                        total_sent += ret;
                    }

                    free(encoded_file);

                    sprintf(req,"\r\n\r\n"
                                "--123456789\r\n");
                    send(clientfd, req, strlen(req), 0);
                }

                sprintf(req,".\r\n");
                send(clientfd, req, strlen(req), 0);

                free(length);
            }

            strcpy(req, "QUIT\r\n");
            send(clientfd, req, strlen(req), 0);

            if (enableLogs)
            {
                printf("C: %s", req);
                recv(clientfd, buffer, sizeof(buffer), 0);
                printf("S: %s", buffer);
            }
        }
    }
    else
    {
        perror("Unknown smtp port\n");
        exit(1);
    }
}
