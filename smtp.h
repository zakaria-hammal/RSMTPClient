#ifndef SMTP
#define SMTP

typedef struct SMTPClient SMTPClient;
struct SMTPClient
{
    char mailServer[1024];
    char emailAdress[1024];
    char secretCode[1024];
    int enableSSL;
    int port;
};

typedef struct Attachement Attachement;
struct Attachement
{
    char fileName[1024];
    char filePath[1024];
};

typedef struct AttachementListNode AttachementListNode;
struct AttachementListNode
{
    Attachement attachement;
    AttachementListNode* next;
};

typedef struct AttachementList AttachementList;
struct AttachementList
{
    AttachementListNode* head;
    int numberOfElements;
};

typedef struct MailMessage MailMessage;
struct MailMessage
{
    char receiverEmailAdress[1024];
    char subject[1024];
    char body[1024];
    int isBodyHtml;
    AttachementList attachementList;
};

void send_email(SMTPClient client, MailMessage message, int enableLogs);
void insert_attachement(MailMessage *message, Attachement attachement);

#endif
