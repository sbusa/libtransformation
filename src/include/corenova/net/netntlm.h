/* 
 * File:   ntlm.h
 * Author: alexv
 *
 * Created on December 9, 2010, 10:50 PM
 */

#ifndef _corenova_net_NTLM_H_
#define	_corenova_net_NTLM_H_

#include <corenova/interface.h>
#include <ntlm.h>

#define FLAG_NEGOTIATE_UNICODE 0x00000001	// Negotiate Unicode 	Indicates that Unicode strings are supported for use in security buffer data.
#define FLAG_NEGOTIATE_OEM 0x00000002	// Negotiate OEM 	Indicates that OEM strings are supported for use in security buffer data.
#define FLAG_REQUEST_TARGET 0x00000004	// Request Target 	Requests that the server's authentication realm be included in the Type 2 message.
#define FLAG_NEGOTIATE_SIGN 0x00000010	// Negotiate Sign 	Specifies that authenticated communication between the client and server should carry a digital signature (message integrity).
#define FLAG_NEGOTIATE_SEAL 0x00000020	// Negotiate Seal 	Specifies that authenticated communication between the client and server should be encrypted (message confidentiality).
#define FLAG_NEGOTIATE_DATAGRAM 0x00000040	 // Negotiate Datagram Style 	Indicates that datagram authentication is being used.
#define FLAG_NEGOTIATE_LM_KEY 0x00000080	// Negotiate Lan Manager Key 	Indicates that the Lan Manager Session Key should be used for signing and sealing authenticated communications.
#define FLAG_NEGOTIATE_NETWARE 0x00000100	// Negotiate Netware 	This flag's usage has not been identified.
#define FLAG_NEGOTIATE_NTLM 0x00000200	// Negotiate NTLM 	Indicates that NTLM authentication is being used.
#define FLAG_NEGOTIATE_ANONYMOUS 0x00000800	// Negotiate Anonymous 	Sent by the client in the Type 3 message to indicate that an anonymous context has been established. This also affects the response fields (as detailed in the "Anonymous Response" section).
#define FLAG_NEGOTIATE_DOMAIN_SUPPLIED 0x00001000	// Negotiate Domain Supplied 	Sent by the client in the Type 1 message to indicate that the name of the domain in which the client workstation has membership is included in the message. This is used by the server to determine whether the client is eligible for local authentication.
#define FLAG_NEGOTIATE_WKS_SUPPLIED 0x00002000 //Negotiate Workstation Supplied 	Sent by the client in the Type 1 message to indicate that the client workstation's name is included in the message. This is used by the server to determine whether the client is eligible for local authentication.
#define FLAG_NEGOTIATE_LOCAL_CALL 0x00004000	// Negotiate Local Call 	Sent by the server to indicate that the server and client are on the same machine. Implies that the client may use the established local credentials for authentication instead of calculating a response to the challenge.
#define FLAG_NEGOTIATE_ALWAYS_SIGN 0x00008000	// Negotiate Always Sign 	Indicates that authenticated communication between the client and server should be signed with a "dummy" signature.
#define FLAG_TARGET_DOMAIN 0x00010000	// Target Type Domain 	Sent by the server in the Type 2 message to indicate that the target authentication realm is a domain.
#define FLAG_TARGET_SERVER 0x00020000  //Target Type Server 	Sent by the server in the Type 2 message to indicate that the target authentication realm is a server.
#define FLAG_TARGET_SHARE 0x00040000	// Target Type Share 	Sent by the server in the Type 2 message to indicate that the target authentication realm is a share. Presumably, this is for share-level authentication. Usage is unclear.
#define FLAG_NEGOTIATE_NTLM2 0x00080000	// Negotiate NTLM2 Key 	Indicates that the NTLM2 signing and sealing scheme should be used for protecting authenticated communications. Note that this refers to a particular session security scheme, and is not related to the use of NTLMv2 authentication. This flag can, however, have an effect on the response calculations (as detailed in the "NTLM2 Session Response" section).
#define FLAG_INIT_RESPONSE 0x00100000	// Request Init Response 	This flag's usage has not been identified.
#define FLAG_ACCEPT_RESPONSE 0x00200000	// Request Accept Response 	This flag's usage has not been identified.
#define FLAG_REQUEST_NON_NT_SESSION_KEY 0x00400000	// Request Non-NT Session Key 	This flag's usage has not been identified.
#define FLAG_NEGOTIATE_TARGET_INFO 0x00800000	// Negotiate Target Info 	Sent by the server in the Type 2 message to indicate that it is including a Target Information block in the message. The Target Information block is used in the calculation of the NTLMv2 response.
#define FLAG_NEGOTIATE_128 0x20000000	// Negotiate 128 	Indicates that 128-bit encryption is supported.
#define FLAG_NEGOTIATE_KEY_EXCHANGE 0x40000000	// Negotiate Key Exchange 	Indicates that the client will provide an encrypted master key in the "Session Key" field of the Type 3 message.
#define FLAG_NEGOTIATE_56 0x80000000	// Negotiate 56 	Indicates that 56-bit encryption is supported.

enum NTLM_TYPE {
    AUTH_REQUEST = 1,
    AUTH_CHALLENGE = 2,
    AUTH_RESPONSE = 3

};

typedef struct {
    tSmbNtlmAuthRequest request;
    tSmbNtlmAuthChallenge challenge;
    tSmbNtlmAuthResponse response;

} ntlm_handle_t;

DEFINE_INTERFACE(NetNTLM) {

    void (*destroy) (ntlm_handle_t **);
    ntlm_handle_t * (*new) ();

    void (*buildAuthRequest) (ntlm_handle_t *, char *user, char *domain);
    void (*buildAuthResponse) (ntlm_handle_t *, char *user, char *password);
    void (*buildAuthChallenge) (ntlm_handle_t *, char *domain, char *challenge_string);

    void (*dumpAuthRequest) (ntlm_handle_t *);
    void (*dumpAuthResponse) (ntlm_handle_t *);
    void (*dumpAuthChallenge) (ntlm_handle_t *);
    
    char* (*encodeAuthRequest) (ntlm_handle_t *);
    char* (*encodeAuthResponse) (ntlm_handle_t *);
    char* (*encodeAuthChallenge) (ntlm_handle_t *);

    int (*decode) (ntlm_handle_t *, char *buf);

    char* (*getResponseUser) (ntlm_handle_t *);
    char* (*getResponseHost) (ntlm_handle_t *);
    char* (*getResponseDomain) (ntlm_handle_t *);


};


#endif

