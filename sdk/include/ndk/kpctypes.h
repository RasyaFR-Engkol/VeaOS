// VEAOS KPC TYPES
#pragma once

#include <procbind.h>
#include <ultypes2.h>
#include <ks.h>
#include <pttypes.h>

//
// KPC Port Type
//

typedef enum _KPC_PORT_TYPE
{
    KpcUnconnectedServerPort = 1,   
    KpcServerCommunicationPort,  
    KpcClientCommunicationPort    
} KPC_PORT_TYPE;

//
// KPC Port Mode
//

typedef enum _KPC_PORT_MODE
{
    KPC_MODE_MESSAGE = 0,          
    KPC_MODE_STREAM  = 1     
} KPC_PORT_MODE;

//
// KPC Message Header
//

typedef struct _KPC_MESSAGE_HEADER
{
    USHORT DataLength;          
    USHORT TotalLength;         
    USHORT MessageType;            
    ULONG_PTR DataInfoOffset;        
    CLIENT_ID ClientId;         
    ULONG MessageId;           
    ULONG CallbackId;            
} KPC_MESSAGE_HEADER, *PKPC_MESSAGE_HEADER;

//
// KPC Message Node
//

typedef struct _KPC_MESSAGE_NODE
{
    LIST_ENTRY QueueListEntry;    
    PKTHREAD RequestingThread;      
    ULONG ReadOffset;   
    PVOID ReplyBuffer;           
    KPC_MESSAGE_HEADER Header;   
    UCHAR MessageData[1];  
} KPC_MESSAGE_NODE, *PKPC_MESSAGE_NODE;

//
// KPC Port Object
//

typedef struct _KPC_PORT_OBJECT
{
    KPC_PORT_TYPE Type;
    KPC_PORT_MODE Mode;
    struct _KPC_PORT_OBJECT *ConnectedPort;
    struct _KPC_PORT_OBJECT *ServerProcessPort;
    PEPROCESS CreatorProcess;
    KSEMAPHORE MesssageSemaphore;
    KSPIN_LOCK QueueLock;
    LIST_ENTRY MessageQueueHead;
    LIST_ENTRY LpcReplyChain;
    ULONG MaxMessageLength;
    ULONG MaxConnectionInfoLength;
    SECURITY_QUALITY_OF_SERVICE SecurityQos;
} KPC_PORT_OBJECT, *PKPC_PORT_OBJECT;

//
// KPC Connect Packet
//

typedef struct _KPC_CONNECT_PACKET
{
    KPC_MESSAGE_HEADER Header;
    KPC_PORT_MODE RequestedMode;
    ULONG ConnectionInfoLength;
    UCHAR ConnectionInfo[1];     
} KPC_CONNECT_PACKET, *PKPC_CONNECT_PACKET;

//
// KPC Object Access Mask
//

#define PORT_CONNECT                    0x1
#define PORT_ALL_ACCESS                 (VEA_STANDARD_RIGHTS_REQUIRED | \
                                         VEA_SYNCHRONIZE | \
                                         PORT_CONNECT)

extern POBJECT_TYPE KpcPortObjectType;

//
// KPC Section View
//

typedef struct _KPC_SECTION_WRITE
{
    ULONG Length;            
    HANDLE SectionHandle;  
    ULONG SectionOffset;       
    SIZE_T ViewSize;       
    PVOID ViewBase;            
    PVOID ViewRemoteBase;    
} KPC_SECTION_WRITE, *PKPC_SECTION_WRITE;

//
// KPC Remote Section View
//

typedef struct _KPC_SECTION_READ
{
    ULONG Length;               
    SIZE_T ViewSize;      
    PVOID ViewBase;         
} KPC_SECTION_READ, *PKPC_SECTION_READ;

//
// KPC Message Types (NT-Style LPC Definitions)
//
#define KPC_REQUEST                 1  
#define KPC_REPLY                   2   
#define KPC_DATAGRAM                3   
#define KPC_LOST_REPLY              4  
#define KPC_PORT_CLOSED             5  
#define KPC_CLIENT_DIED             6   
#define KPC_EXCEPTION               7  
#define KPC_DEBUG_EVENT             8 
#define KPC_ERROR_EVENT             9   
#define KPC_CONNECTION_REQUEST      0xFFFF 

/* EOF */